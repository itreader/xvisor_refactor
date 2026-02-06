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
 * @brief source file for page pool subsystem
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

struct vmm_page_pool_entry {
    struct red_black_node rb;
    double_list_t         head;
    virtual_addr_t        base;
    virtual_size_t        size;
    uint32_t              hugepage_count;
    uint32_t              page_count;
    uint32_t              page_avail_count;
    uint64_t             *page_bmap;
};

struct vmm_page_pool_ctrl {
    enum vmm_page_pool_type type;
    vmm_spinlock_t          lock;
    struct red_black_root   root;
    double_list_t           entry_list;
};

static struct vmm_page_pool_ctrl pparr[VMM_PAGE_POOL_MAX];

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
static int __page_pool_find_bmap(struct vmm_page_pool_entry *e, uint32_t page_count)
{
    int      pos = 0;
    uint32_t i, free = 0;

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
static struct vmm_page_pool_entry *__page_pool_find_by_va(struct vmm_page_pool_ctrl *pp, virtual_addr_t va)
{
    struct red_black_node      *n;
    struct vmm_page_pool_entry *ret = NULL;

    n                               = pp->root.red_black_node;

    while (n) {
        struct vmm_page_pool_entry *e = rb_entry(n, struct vmm_page_pool_entry, rb);

        if ((e->base <= va) && (va < (e->base + e->size))) {
            ret = e;
            break;
        } else if (va < e->base) {
            n = n->rb_left;
        } else if ((e->base + e->size) <= va) {
            n = n->rb_right;
        } else {
            vmm_panic("%s: can't find virtual address\n", __func__);
        }
    }

    return ret;
}

/* NOTE: Must be called with pp->lock held */
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
static struct vmm_page_pool_entry *__page_pool_find_alloc_entry(struct vmm_page_pool_ctrl *pp, uint32_t page_count)
{
    struct vmm_page_pool_entry *et;

    list_for_each_entry(et, &pp->entry_list, head)
    {
        if ((page_count < et->page_avail_count) && (__page_pool_find_bmap(et, page_count) > 0)) {
            return et;
        }
    }

    return NULL;
}

/* NOTE: Must be called with pp->lock held */
static struct vmm_page_pool_entry *__page_pool_add_new_entry(struct vmm_page_pool_ctrl *pp, uint32_t page_count)
{
    virtual_addr_t              base;
    virtual_size_t              size;
    uint32_t                    hugepage_count;
    uint32_t                    hugepage_shift = vmm_host_hugepage_shift();
    struct vmm_page_pool_entry *parent_e, *e = NULL;
    struct red_black_node **new = NULL, *parent = NULL;

    size           = page_count * VMM_PAGE_SIZE;
    size           = roundup2_order_size(size, hugepage_shift);
    page_count     = size >> VMM_PAGE_SHIFT;
    hugepage_count = size >> hugepage_shift;
    base           = vmm_host_alloc_hugepages(hugepage_count, __page_pool_type2flags(pp->type));

    e              = vmm_zalloc(sizeof(*e));

    if (!e) {
        vmm_host_free_hugepages(base, hugepage_count);
        return NULL;
    }

    RB_CLEAR_NODE(&e->rb);
    INIT_LIST_HEAD(&e->head);
    e->base             = base;
    e->size             = size;
    e->hugepage_count   = hugepage_count;
    e->page_count       = page_count;
    e->page_avail_count = page_count;
    e->page_bmap        = vmm_zalloc(BITS_TO_LONGS(page_count) * sizeof(*e->page_bmap));

    if (!e->page_bmap) {
        vmm_free(e);
        vmm_host_free_hugepages(base, hugepage_count);
        return NULL;
    }

    new = &(pp->root.red_black_node);

    while (*new) {
        parent   = *new;
        parent_e = rb_entry(parent, struct vmm_page_pool_entry, rb);

        if ((e->base + e->size) <= parent_e->base) {
            new = &parent->rb_left;
        } else if ((parent_e->base + parent_e->size) <= e->base) {
            new = &parent->rb_right;
        } else {
            vmm_panic("%s: can't add entry\n", __func__);
        }
    }

    rb_link_node(&e->rb, parent, new);
    rb_insert_color(&e->rb, &pp->root);

    list_add_tail(&e->head, &pp->entry_list);

    __page_pool_adjust(pp, e);

    return e;
}

/* NOTE: Must be called with pp->lock held */
static void __page_pool_del_entry(struct vmm_page_pool_ctrl *pp, struct vmm_page_pool_entry *e)
{
    rb_erase(&e->rb, &pp->root);
    RB_CLEAR_NODE(&e->rb);
    list_del(&e->head);

    vmm_host_free_hugepages(e->base, e->hugepage_count);
    vmm_free(e->page_bmap);
    vmm_free(e);
}

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

static int page_pool_free(struct vmm_page_pool_ctrl *pp, virtual_addr_t page_va, uint32_t page_count)
{
    int                         page_pos;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    e = __page_pool_find_by_va(pp, page_va);

    if (!e) {
        vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);
        return VMM_ENOTAVAIL;
    }

    if ((e->page_count - e->page_avail_count) < page_count) {
        vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);
        return VMM_ENOTAVAIL;
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

virtual_size_t vmm_page_pool_space(enum vmm_page_pool_type page_type)
{
    virtual_size_t              ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0;
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

uint32_t vmm_page_pool_entry_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0;
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

uint32_t vmm_page_pool_hugepage_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0;
    }

    pp = &pparr[page_type];

    vmm_spin_lock_irq_save_lite(&pp->lock, flags);

    list_for_each_entry(e, &pp->entry_list, head)
    {
        ret += e->hugepage_count;
    }

    vmm_spin_unlock_irq_restore_lite(&pp->lock, flags);

    return ret;
}

uint32_t vmm_page_pool_page_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0;
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

uint32_t vmm_page_pool_page_avail_count(enum vmm_page_pool_type page_type)
{
    uint32_t                    ret = 0;
    irq_flags_t                 flags;
    struct vmm_page_pool_entry *e;
    struct vmm_page_pool_ctrl  *pp;

    if (VMM_PAGE_POOL_MAX <= page_type) {
        return 0;
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

virtual_addr_t vmm_page_pool_alloc(enum vmm_page_pool_type page_type, uint32_t page_count)
{
    if (VMM_PAGE_POOL_MAX <= page_type) {
        vmm_panic("%s: invalid page_type=%d\n", __func__, page_type);
    }

    return page_pool_alloc(&pparr[page_type], page_count);
}

int vmm_page_pool_free(enum vmm_page_pool_type page_type, virtual_addr_t page_va, uint32_t page_count)
{
    if (VMM_PAGE_POOL_MAX <= page_type) {
        vmm_panic("%s: invalid page_type=%d\n", __func__, page_type);
    }

    return page_pool_free(&pparr[page_type], page_va, page_count);
}

int __init vmm_page_pool_init(void)
{
    int                        i;
    struct vmm_page_pool_ctrl *pp;

    for (i = 0; i < VMM_PAGE_POOL_MAX; i++) {
        pp       = &pparr[i];
        pp->type = i;
        INIT_SPIN_LOCK(&pp->lock);
        pp->root = RB_ROOT;
        INIT_LIST_HEAD(&pp->entry_list);
    }

    return VMM_OK;
}
