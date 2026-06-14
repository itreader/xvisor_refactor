/**
 * Copyright (c) 2018 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_page_pool.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 页池子系统源代码
 */

#include <libs/bitmap.h>
#include <libs/list.h>
#include <libs/red_black_tree_augmented.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_page_pool.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

/**
 * @brief 页面池条目结构，条目结构
 */
struct vmm_page_pool_entry {
    red_black_node_t rb; /**< 运行块指针 */
    double_list_t         head; /**< 链表头 */
    virtual_addr_t        base; /**< 基址 */
    virtual_size_t        size; /**< 大小 */
    uint32_t              huge_page_count; /**< huge_page_count成员 */
    uint32_t              page_count; /**< page_count成员 */
    uint32_t              page_avail_count; /**< page_avail_count成员 */
    uint64_t             *page_bmap; /**< page_bmap成员 */
};

/**
 * @brief 页面池控制结构，管理各类型内存页池的状态
 */
struct vmm_page_pool_ctrl {
    enum vmm_page_pool_type type; /**< 类型 */
    vmm_spinlock_t          lock; /**< 自旋锁 */
    red_black_root_t   root; /**< 根节点 */
    double_list_t           entry_list; /**< entry_list成员 */
};

static struct vmm_page_pool_ctrl pparr[VMM_PAGE_POOL_MAX];

/**
 * @brief   页面池类型转换为内存标志
 * @param type 类型标识值
 * @return 页面池类型对应的内存标志
 */
static uint32_t __page_pool_type2flags(enum vmm_page_pool_type type)
{
    switch (type) {
        case VMM_PAGE_POOL_NORMAL:
            return VMM_MEMORY_FLAGS_NORMAL;

        case VMM_PAGE_POOL_NORMAL_NOCACHE:
            return VMM_MEMORY_FLAGS_NORMAL_NOCACHE;

        case VMM_PAGE_POOL_NORMAL_WT:
            return VMM_MEMORY_FLAGS_NORMAL_WT;

        case VMM_PAGE_POOL_DMA_COHERENT:
            return VMM_MEMORY_FLAGS_DMA_COHERENT;

        case VMM_PAGE_POOL_DMA_NONCOHERENT:
            return VMM_MEMORY_FLAGS_DMA_NONCOHERENT;

        case VMM_PAGE_POOL_IO:
            return VMM_MEMORY_FLAGS_IO;

        default:
            break;
    };

    return VMM_MEMORY_FLAGS_NORMAL;
}

/* NOTE: Must be called with pp->lock held */
/**
 * @brief 在页池位图中查找空闲页
 * @param e 事件或元素指针
 * @param page_count 数量
 * @return 查找结果，失败返回错误码
 */
static int __page_pool_find_bmap(struct vmm_page_pool_entry *e, uint32_t page_count)
{
    int      pos = 0;
    uint32_t i;
    uint32_t free = 0;

    if (!page_count) {
        return -1;
    }

    for (i = 0; i < e->page_count; i++) {
        if (bitmap_isset(e->page_bmap, i)) {
            pos  = i + 1;
            free = 0;
        } else {
            free++;
        }

        if (page_count == free) {
            return pos;
        }
    }

    return -1;
}

/* NOTE: Must be called with pp->lock held */
/**
 * @brief 根据虚拟地址查找页池条目
 * @param pp 页池结构体指针
 * @param va 待操作的虚拟地址
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static struct vmm_page_pool_entry *__page_pool_find_by_va(struct vmm_page_pool_ctrl *pp, virtual_addr_t va)
{
    red_black_node_t      *n;
    struct vmm_page_pool_entry *ret = NULL;

    n                               = pp->root.red_black_node;

    while (n) {
        struct vmm_page_pool_entry *e = rb_entry(n, struct vmm_page_pool_entry, rb); /**< rb)成员 */

        if ((e->base <= va) && (va < (e->base + e->size))) {
            ret = e; /**< e */
            break;
        } else if (va < e->base) {
            n = n->rb_left; /**< n->rb_left成员 */
        } else if ((e->base + e->size) <= va) {
            n = n->rb_right; /**< n->rb_right成员 */
        } else {
            vmm_panic("%s: can't find virtual address\n", __func__); /**< __func__)成员 */
        }
    }

    return ret;
}

/* NOTE: Must be called with pp->lock held */
/**
 * @brief 调整页池的大小或参数
 * @param pp 页池结构体指针
 * @param e 事件或元素指针
 */
static void __page_pool_adjust(struct vmm_page_pool_ctrl *pp, struct vmm_page_pool_entry *e)
{
    bool                        found = FALSE;
    struct vmm_page_pool_entry *et;

    list_del(&e->head);

    list_for_each_entry(et, &pp->entry_list, head)
    {
        if (e->page_avail_count < et->page_avail_count) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        list_add_tail(&e->head, &pp->entry_list);
    } else {
        list_add_tail(&e->head, &et->head);
    }
}

/* NOTE: Must be called with pp->lock held */
/**
 * @brief 在页池中查找已分配的条目
 * @param pp 页池结构体指针
 * @param page_count 数量
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static struct vmm_page_pool_entry *__page_pool_find_alloc_entry(struct vmm_page_pool_ctrl *pp, uint32_t page_count)
{
    struct vmm_page_pool_entry *et;

    list_for_each_entry(et, &pp->entry_list, head)
    {
        if ((page_count < et->page_avail_count) && (__page_pool_find_bmap(et, page_count) > 0)) {
            return et; /**< et */
        }
    }

    return NULL;
}

/* NOTE: Must be called with pp->lock held */
/**
 * @brief 向页池添加新条目
 * @param pp 页池结构体指针
 * @param page_count 数量
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static struct vmm_page_pool_entry *__page_pool_add_new_entry(struct vmm_page_pool_ctrl *pp, uint32_t page_count)
{
    virtual_addr_t              base;
    virtual_size_t              size;
    uint32_t                    huge_page_count;
    uint32_t                    huge_page_shift = vmm_host_huge_page_shift();
    struct vmm_page_pool_entry *parent_e = NULL;
    struct vmm_page_pool_entry *e = NULL;
    red_black_node_t **new_node = NULL;
    red_black_node_t *parent = NULL;

    size           = page_count * VMM_PAGE_SIZE;
    size           = roundup2_order_size(size, huge_page_shift);
    page_count     = size >> VMM_PAGE_SHIFT;
    huge_page_count = size >> huge_page_shift;
    base           = vmm_host_alloc_huge_pages(huge_page_count, __page_pool_type2flags(pp->type));

    e              = vmm_zalloc(sizeof(*e));

    if (!e) {
        vmm_host_free_huge_pages(base, huge_page_count);
        return NULL;
    }

    RB_CLEAR_NODE(&e->rb);
    INIT_LIST_HEAD(&e->head);
    e->base             = base;
    e->size             = size;
    e->huge_page_count   = huge_page_count;
    e->page_count       = page_count;
    e->page_avail_count = page_count;
    e->page_bmap        = vmm_zalloc(BITS_TO_LONGS(page_count) * sizeof(*e->page_bmap));

    if (!e->page_bmap) {
        vmm_free(e);
        vmm_host_free_huge_pages(base, huge_page_count);
        return NULL;
    }

    new_node = &(pp->root.red_black_node);

    while (*new_node) {
        parent   = *new_node;
        parent_e = rb_entry(parent, struct vmm_page_pool_entry, rb);

        if ((e->base + e->size) <= parent_e->base) {
            new_node = &parent->rb_left;
        } else if ((parent_e->base + parent_e->size) <= e->base) {
            new_node = &parent->rb_right;
        } else {
            vmm_panic("%s: can't add entry\n", __func__);
        }
    }

    rb_link_node(&e->rb, parent, new_node);
    rb_insert_color(&e->rb, &pp->root);

    list_add_tail(&e->head, &pp->entry_list);

    __page_pool_adjust(pp, e);

    return e;
}

/* NOTE: Must be called with pp->lock held */
/**
 * @brief 从页池中删除条目
 * @param pp 页池结构体指针
 * @param e 事件或元素指针
 */
static void __page_pool_del_entry(struct vmm_page_pool_ctrl *pp, struct vmm_page_pool_entry *e)
{
    rb_erase(&e->rb, &pp->root);
    RB_CLEAR_NODE(&e->rb);
    list_del(&e->head);

    vmm_host_free_huge_pages(e->base, e->huge_page_count);
    vmm_free(e->page_bmap);
    vmm_free(e);
}

/**
 * @brief 从页池中分配物理页
 * @param pp 页池结构体指针
 * @param page_count 数量
 * @return 成功返回分配结果，失败返回错误码
 */
static virtual_addr_t page_pool_alloc(struct vmm_page_pool_ctrl *pp, uint32_t page_count)
{
    int                         page_pos;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    e = __page_pool_find_alloc_entry(pp, page_count);

    if (!e) {
        e = __page_pool_add_new_entry(pp, page_count);
    }

    if (!e) {
        vmm_panic("%s: no page pool entry\n", __func__);
    }

    page_pos = __page_pool_find_bmap(e, page_count);

    if (page_pos < 0) {
    }

    bitmap_set(e->page_bmap, page_pos, page_count);
    e->page_avail_count -= page_count;

    __page_pool_adjust(pp, e);

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return e->base + page_pos * VMM_PAGE_SIZE;
}

/**
 * @brief 将物理页归还到页池
 * @param pp 页池结构体指针
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回分配结果，失败返回错误码
 */
static int page_pool_free(struct vmm_page_pool_ctrl *pp, virtual_addr_t page_va, uint32_t page_count)
{
    int                         page_pos;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    e = __page_pool_find_by_va(pp, page_va);

    if (!e) {
        vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);
        return VMM_ERR_NOTAVAIL;
    }

    if ((e->page_count - e->page_avail_count) < page_count) {
        vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);
        return VMM_ERR_NOTAVAIL;
    }

    page_pos = (page_va - e->base) >> VMM_PAGE_SHIFT;
    bitmap_clear(e->page_bmap, page_pos, page_count);
    e->page_avail_count += page_count;

    if (e->page_count == e->page_avail_count) {
        __page_pool_del_entry(pp, e);
    } else {
        __page_pool_adjust(pp, e);
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return VMM_OK;
}

/**
 * @brief 页 池 名称
 * @param page_type 页面类型标识
 * @return 成功返回目标指针，失败返回NULL
 */
const char *vmm_page_pool_name(enum vmm_page_pool_type page_type)
{
    switch (page_type) {
        case VMM_PAGE_POOL_NORMAL:
            return "NORMAL";

        case VMM_PAGE_POOL_NORMAL_NOCACHE:
            return "NORMAL_NOCACHE";

        case VMM_PAGE_POOL_NORMAL_WT:
            return "NORMAL_WT";

        case VMM_PAGE_POOL_DMA_COHERENT:
            return "DMA_COHERENT";

        case VMM_PAGE_POOL_DMA_NONCOHERENT:
            return "DMA_NONCOHERENT";

        case VMM_PAGE_POOL_IO:
            return "IO";

        default:
            break;
    };

    return NULL;
}

/**
 * @brief 页 池 空间
 * @param page_type 页面类型标识
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_size_t vmm_page_pool_space(enum vmm_page_pool_type page_type)
{
    virtual_size_t              ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0; /**< 0 */
    }

    pp = &pparr[page_type];

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    list_for_each_entry(e, &pp->entry_list, head)
    {
        ret += e->size;
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return ret;
}

/**
 * @brief 获取页面池条目的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_entry_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0; /**< 0 */
    }

    pp = &pparr[page_type];

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    list_for_each_entry(e, &pp->entry_list, head)
    {
        ret++;
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return ret;
}

/**
 * @brief 获取页面池大页的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_huge_page_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0; /**< 0 */
    }

    pp = &pparr[page_type];

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    list_for_each_entry(e, &pp->entry_list, head)
    {
        ret += e->huge_page_count;
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return ret;
}

/**
 * @brief 获取页面池页帧的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_page_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0; /**< 0 */
    }

    pp = &pparr[page_type];

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    list_for_each_entry(e, &pp->entry_list, head)
    {
        ret += e->page_count;
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return ret;
}

/**
 * @brief 获取页面池可用页的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_page_avail_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0; /**< 0 */
    }

    pp = &pparr[page_type];

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    list_for_each_entry(e, &pp->entry_list, head)
    {
        ret += e->page_avail_count;
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return ret;
}

/**
 * @brief 分配页面池
 * @param page_type 页面类型标识
 * @param page_count 数量
 * @return 数量值
 */
virtual_addr_t vmm_page_pool_alloc(enum vmm_page_pool_type page_type, uint32_t page_count)
{
    if (VMM_PAGE_POOL_MAX <= page_type) {
        vmm_panic("%s: invalid page_type=%d\n", __func__, page_type);
    }

    return page_pool_alloc(&pparr[page_type], page_count);
}

/**
 * @brief 释放页面池
 * @param page_type 页面类型标识
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_page_pool_free(enum vmm_page_pool_type page_type, virtual_addr_t page_va, uint32_t page_count)
{
    if (VMM_PAGE_POOL_MAX <= page_type) {
        vmm_panic("%s: invalid page_type=%d\n", __func__, page_type);
    }

    return page_pool_free(&pparr[page_type], page_va, page_count);
}

/**
 * @brief 初始化页面池
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_page_pool_init(void)
{
    int                        i;
    struct vmm_page_pool_ctrl *pp;

    for (i = 0; i < VMM_PAGE_POOL_MAX; i++) {
        pp       = &pparr[i]; /**< pparr成员 */
        pp->type = i; /**< i */
        INIT_SPIN_LOCK(&pp->lock);
        pp->root = RB_ROOT; /**< RB_ROOT成员 */
        INIT_LIST_HEAD(&pp->entry_list);
    }

    return VMM_OK;
}
