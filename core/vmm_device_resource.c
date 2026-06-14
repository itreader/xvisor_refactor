/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_device_resource.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备驱动资源管理
 */

#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_device_driver.h>
#include <vmm_device_resource.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_limits.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>

/**
 * @brief 设备资源节点，将资源绑定到设备并关联释放回调
 */
struct vmm_device_resource_node {
    double_list_t                 entry; /**< 链表入口 */
    vmm_device_resource_release_t release; /**< 释放回调 */
};

/**
 * @brief 设备资源管理结构，维护设备的资源节点链表
 */
struct vmm_device_resource {
    struct vmm_device_resource_node node; /**< 节点 */
    /* -- 3 pointers */
    uint64_t                        data[]; /* guarantee ull alignment */
};

/**
 * @brief 分配设备资源
 * @param release 释放回调函数
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_device_resource *alloc_dr(vmm_device_resource_release_t release, size_t size)
{
    size_t                      tot_size = sizeof(struct vmm_device_resource) + size;
    struct vmm_device_resource *dr;

    dr = vmm_malloc(tot_size);

    if (unlikely(!dr)) {
        return NULL; /**< NULL成员 */
    }

    memset(dr, 0, offsetof(struct vmm_device_resource, data));

    INIT_LIST_HEAD(&dr->node.entry);
    dr->node.release = release;

    return dr;
}

/**
 * @brief 添加设备资源管理跟踪
 * @param dev 设备结构体指针
 * @param node 设备树节点指针
 */
static void add_dr(vmm_device_t *dev, struct vmm_device_resource_node *node)
{
    BUG_ON(!list_empty(&node->entry));
    list_add_tail(&node->entry, &dev->device_resource_head);
}

/**
 * @brief 分配设备资源
 * @param release 释放回调函数
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_device_resource_alloc(vmm_device_resource_release_t release, size_t size)
{
    struct vmm_device_resource *dr;

    dr = alloc_dr(release, size);

    if (unlikely(!dr)) {
        return NULL; /**< NULL成员 */
    }

    return dr->data;
}

/**
 * @brief 遍历设备的所有资源，对每个资源执行回调
 */
void vmm_device_resource_for_each_resource(
    vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data,
    void (*fn)(vmm_device_t *, void *, void *), void *data)
{
    struct vmm_device_resource_node *node;
    struct vmm_device_resource_node *tmp;
    irq_flags_t                      flags;

    if (!fn) {
        return;
    }

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);
    list_for_each_entry_safe_reverse(node, tmp, &dev->device_resource_head, entry)
    {
        struct vmm_device_resource *dr = container_of(node, struct vmm_device_resource, node);

        if (node->release != release) {
            continue;
        }

        if (match && !match(dev, dr->data, match_data)) {
            continue;
        }

        fn(dev, dr->data, data);
    }
    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);
}

/**
 * @brief 释放设备资源
 * @param res 资源结构体指针
 */
void vmm_device_resource_free(void *res)
{
    if (res) {
        struct vmm_device_resource *dr = container_of(res, struct vmm_device_resource, data);

        BUG_ON(!list_empty(&dr->node.entry));
        vmm_free(dr);
    }
}

/**
 * @brief 向设备添加资源
 * @param dev 设备结构体指针
 * @param res 资源结构体指针
 */
void vmm_device_resource_add(vmm_device_t *dev, void *res)
{
    struct vmm_device_resource *dr = container_of(res, struct vmm_device_resource, data);
    irq_flags_t                 flags;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);
    add_dr(dev, &dr->node);
    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);
}

/**
 * @brief 查找设备资源
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_device_resource *find_dr(
    vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    struct vmm_device_resource_node *node;

    list_for_each_entry_reverse(node, &dev->device_resource_head, entry)
    {
        struct vmm_device_resource *dr = container_of(node, struct vmm_device_resource, node); /**< node)成员 */

        if (node->release != release) {
            continue;
        }

        if (match && !match(dev, dr->data, match_data)) {
            continue;
        }

        return dr; /**< dr */
    }

    return NULL;
}

/**
 * @brief 查找设备资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
void *vmm_device_resource_find(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    struct vmm_device_resource *dr;
    irq_flags_t                 flags;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);
    dr = find_dr(dev, release, match, match_data);
    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);

    if (dr) {
        return dr->data;
    }

    return NULL;
}

/**
 * @brief 获取设备关联的资源
 * @param dev 设备结构体指针
 * @param new_res 新资源结构体指针
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
void *vmm_device_resource_get(vmm_device_t *dev, void *new_res, vmm_device_resource_match_t match, void *match_data)
{
    struct vmm_device_resource *new_dr = container_of(new_res, struct vmm_device_resource, data);
    struct vmm_device_resource *dr;
    irq_flags_t                 flags;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);
    dr = find_dr(dev, new_dr->node.release, match, match_data);

    if (!dr) {
        add_dr(dev, &new_dr->node);
        dr     = new_dr;
        new_dr = NULL;
    }

    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);
    vmm_device_resource_free(new_dr);

    return dr->data;
}

/**
 * @brief 从设备中移除资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 目标对象指针，不存在返回NULL
 */
void *vmm_device_resource_remove(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    struct vmm_device_resource *dr;
    irq_flags_t                 flags;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);
    dr = find_dr(dev, release, match, match_data);

    if (dr) {
        list_del_init(&dr->node.entry);
    }

    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);

    if (dr) {
        return dr->data;
    }

    return NULL;
}

/**
 * @brief 销毁设备资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_resource_destroy(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    void *res;

    res = vmm_device_resource_remove(dev, release, match, match_data);

    if (unlikely(!res)) {
        return VMM_ERR_NOENT;
    }

    vmm_device_resource_free(res);

    return VMM_OK;
}

/**
 * @brief 释放设备的指定资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_resource_release(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    void *res;

    res = vmm_device_resource_remove(dev, release, match, match_data);

    if (unlikely(!res)) {
        return VMM_ERR_NOENT;
    }

    (*release)(dev, res);
    vmm_device_resource_free(res);

    return VMM_OK;
}

/**
 * @brief 释放设备资源节点
 * @param dev 设备结构体指针
 * @param first 首元素指针
 * @param end 结束位置或结束地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int release_nodes(vmm_device_t *dev, double_list_t *first, double_list_t *end)
{
    LIST_HEAD(todo);
    irq_flags_t                 flags;
    struct vmm_device_resource *dr = NULL;
    struct vmm_device_resource *tmp = NULL;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);

    list_for_each_entry_safe_reverse(dr, tmp, &todo, node.entry)
    {
        dr->node.release(dev, dr->data);
        vmm_free(dr);
    }

    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);

    return VMM_OK;
}

/**
 * @brief 释放设备的所有资源
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_resource_release_all(vmm_device_t *dev)
{
    /* Looks like an uninitialized device structure */
    if (WARN_ON(dev->device_resource_head.next == NULL)) {
        return VMM_ERR_NODEV;
    }

    return release_nodes(dev, dev->device_resource_head.next, &dev->device_resource_head);
}

/*
 * Managed malloc/free
 */

/**
 * @brief 释放设备管理的内存分配
 * @param dev 设备结构体指针
 * @param res 资源结构体指针
 */
static void devm_malloc_release(vmm_device_t *dev, void *res)
{
    /* noop */
}

/**
 * @brief 检查托管内存分配是否匹配指定地址
 * @param dev 设备结构体指针
 * @param res 资源结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回分配结果，失败返回错误码
 */
static int devm_malloc_match(vmm_device_t *dev, void *res, void *data)
{
    return res == data;
}

/**
 * @brief 设备管理的内存分配
 * @param dev 设备结构体指针
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_malloc(vmm_device_t *dev, size_t size)
{
    struct vmm_device_resource *dr;

    dr = alloc_dr(devm_malloc_release, size);

    if (unlikely(!dr)) {
        return NULL; /**< NULL成员 */
    }

    vmm_device_resource_add(dev, dr->data);

    return dr->data;
}

/**
 * @brief 设备管理的清零内存分配
 * @param dev 设备结构体指针
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_zalloc(vmm_device_t *dev, size_t size)
{
    void *ret = vmm_devm_malloc(dev, size);

    if (ret) {
        memset(ret, 0, size);
    }

    return ret;
}

/**
 * @brief 设备管理的数组内存分配
 * @param dev 设备结构体指针
 * @param n 起始位置编号
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_malloc_array(vmm_device_t *dev, size_t n, size_t size)
{
    if (size != 0 && n > udiv64(SIZE_MAX, size)) {
        return NULL;
    }

    return vmm_devm_malloc(dev, n * size);
}

/**
 * @brief 设备管理的批量内存分配
 * @param dev 设备结构体指针
 * @param n 起始位置编号
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_calloc(vmm_device_t *dev, size_t n, size_t size)
{
    void *ret = vmm_devm_malloc_array(dev, n, size);

    if (ret) {
        memset(ret, 0, n * size);
    }

    return ret;
}

/**
 * @brief 设备管理的字符串复制分配
 * @param dev 设备结构体指针
 * @param s 字符串或数据指针
 * @return 成功返回分配的内存指针，失败返回NULL
 */
char *vmm_devm_strdup(vmm_device_t *dev, const char *s)
{
    size_t size;
    char  *buf;

    if (!s) {
        return NULL;
    }

    size = strlen(s) + 1;
    buf  = vmm_devm_malloc(dev, size);

    if (buf) {
        memcpy(buf, s, size);
    }

    return buf;
}

/**
 * @brief 释放devm
 * @param dev 设备结构体指针
 * @param p 数据指针
 */
void vmm_devm_free(vmm_device_t *dev, void *p)
{
    int rc;

    rc = vmm_device_resource_destroy(dev, devm_malloc_release, devm_malloc_match, p);
    WARN_ON(rc);
}

/*
 * Custom devres actions allow inserting a simple function call
 * into the teardown sequence.
 */

/**
 * @brief 设备资源操作上下文，封装资源添加/移除操作的参数
 */
struct action_device_resource {
    void *data; /**< 数据 */
    void (*action)(void *); /**< 动作 */
};

/**
 * @brief 检查托管设备动作是否匹配指定回调
 * @param dev 设备结构体指针
 * @param res 资源结构体指针
 * @param p 数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int devm_action_match(vmm_device_t *dev, void *res, void *p)
{
    struct action_device_resource *devres = res;
    struct action_device_resource *target = p;

    return devres->action == target->action && devres->data == target->data;
}

/**
 * @brief 释放托管设备动作资源
 * @param dev 设备结构体指针
 * @param res 资源结构体指针
 */
static void devm_action_release(vmm_device_t *dev, void *res)
{
    struct action_device_resource *devres = res;

    devres->action(devres->data);
}

/**
 * @brief 添加托管设备动作到设备资源管理列表
 * @param dev 设备结构体指针
 * @param (*action 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_devm_add_action(vmm_device_t *dev, void (*action)(void *), void *data)
{
    struct action_device_resource *devres;

    devres = vmm_device_resource_alloc(devm_action_release, sizeof(struct action_device_resource));

    if (!devres) {
        return VMM_ERR_NOMEM; /**< VMM_ERR_NOMEM成员 */
    }

    devres->data   = data;
    devres->action = action;

    vmm_device_resource_add(dev, devres);
    return 0;
}

/**
 * @brief 从设备资源管理列表中移除托管设备动作
 * @param dev 设备结构体指针
 * @param (*action 指针参数
 */
void vmm_devm_remove_action(vmm_device_t *dev, void (*action)(void *), void *data)
{
    struct action_device_resource devres = {
        .data   = data, /**< 数据 */
        .action = action, /**< action成员 */
    };

    WARN_ON(vmm_device_resource_destroy(dev, devm_action_release, devm_action_match, &devres));
}
