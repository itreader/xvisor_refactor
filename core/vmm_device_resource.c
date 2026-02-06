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
 * @brief Device driver resource management
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

struct vmm_device_resource_node {
    double_list_t                 entry;
    vmm_device_resource_release_t release;
};

struct vmm_device_resource {
    struct vmm_device_resource_node node;
    /* -- 3 pointers */
    uint64_t                        data[]; /* guarantee ull alignment */
};

static struct vmm_device_resource *alloc_dr(vmm_device_resource_release_t release, size_t size)
{
    size_t                      tot_size = sizeof(struct vmm_device_resource) + size;
    struct vmm_device_resource *dr;

    dr = vmm_malloc(tot_size);

    if (unlikely(!dr)) {
        return NULL;
    }

    memset(dr, 0, offsetof(struct vmm_device_resource, data));

    INIT_LIST_HEAD(&dr->node.entry);
    dr->node.release = release;

    return dr;
}

static void add_dr(vmm_device_t *dev, struct vmm_device_resource_node *node)
{
    BUG_ON(!list_empty(&node->entry));
    list_add_tail(&node->entry, &dev->device_resource_head);
}

void *vmm_device_resource_alloc(vmm_device_resource_release_t release, size_t size)
{
    struct vmm_device_resource *dr;

    dr = alloc_dr(release, size);

    if (unlikely(!dr)) {
        return NULL;
    }

    return dr->data;
}

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

void vmm_device_resource_free(void *res)
{
    if (res) {
        struct vmm_device_resource *dr = container_of(res, struct vmm_device_resource, data);

        BUG_ON(!list_empty(&dr->node.entry));
        vmm_free(dr);
    }
}

void vmm_device_resource_add(vmm_device_t *dev, void *res)
{
    struct vmm_device_resource *dr = container_of(res, struct vmm_device_resource, data);
    irq_flags_t                 flags;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);
    add_dr(dev, &dr->node);
    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);
}

static struct vmm_device_resource *find_dr(
    vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    struct vmm_device_resource_node *node;

    list_for_each_entry_reverse(node, &dev->device_resource_head, entry)
    {
        struct vmm_device_resource *dr = container_of(node, struct vmm_device_resource, node);

        if (node->release != release) {
            continue;
        }

        if (match && !match(dev, dr->data, match_data)) {
            continue;
        }

        return dr;
    }

    return NULL;
}

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

int vmm_device_resource_destroy(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    void *res;

    res = vmm_device_resource_remove(dev, release, match, match_data);

    if (unlikely(!res)) {
        return VMM_ENOENT;
    }

    vmm_device_resource_free(res);

    return VMM_OK;
}

int vmm_device_resource_release(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data)
{
    void *res;

    res = vmm_device_resource_remove(dev, release, match, match_data);

    if (unlikely(!res)) {
        return VMM_ENOENT;
    }

    (*release)(dev, res);
    vmm_device_resource_free(res);

    return VMM_OK;
}

static int release_nodes(vmm_device_t *dev, double_list_t *first, double_list_t *end)
{
    LIST_HEAD(todo);
    irq_flags_t                 flags;
    struct vmm_device_resource *dr, *tmp;

    vmm_spin_lock_irq_save(&dev->device_resource_lock, flags);

    list_for_each_entry_safe_reverse(dr, tmp, &todo, node.entry)
    {
        dr->node.release(dev, dr->data);
        vmm_free(dr);
    }

    vmm_spin_unlock_irq_restore(&dev->device_resource_lock, flags);

    return VMM_OK;
}

int vmm_device_resource_release_all(vmm_device_t *dev)
{
    /* Looks like an uninitialized device structure */
    if (WARN_ON(dev->device_resource_head.next == NULL)) {
        return VMM_ENODEV;
    }

    return release_nodes(dev, dev->device_resource_head.next, &dev->device_resource_head);
}

/*
 * Managed malloc/free
 */

static void devm_malloc_release(vmm_device_t *dev, void *res)
{
    /* noop */
}

static int devm_malloc_match(vmm_device_t *dev, void *res, void *data)
{
    return res == data;
}

void *vmm_devm_malloc(vmm_device_t *dev, size_t size)
{
    struct vmm_device_resource *dr;

    dr = alloc_dr(devm_malloc_release, size);

    if (unlikely(!dr)) {
        return NULL;
    }

    vmm_device_resource_add(dev, dr->data);

    return dr->data;
}

void *vmm_devm_zalloc(vmm_device_t *dev, size_t size)
{
    void *ret = vmm_devm_malloc(dev, size);

    if (ret) {
        memset(ret, 0, size);
    }

    return ret;
}

void *vmm_devm_malloc_array(vmm_device_t *dev, size_t n, size_t size)
{
    if (size != 0 && n > udiv64(SIZE_MAX, size)) {
        return NULL;
    }

    return vmm_devm_malloc(dev, n * size);
}

void *vmm_devm_calloc(vmm_device_t *dev, size_t n, size_t size)
{
    void *ret = vmm_devm_malloc_array(dev, n, size);

    if (ret) {
        memset(ret, 0, n * size);
    }

    return ret;
}

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

struct action_device_resource {
    void *data;
    void (*action)(void *);
};

static int devm_action_match(vmm_device_t *dev, void *res, void *p)
{
    struct action_device_resource *devres = res;
    struct action_device_resource *target = p;

    return devres->action == target->action && devres->data == target->data;
}

static void devm_action_release(vmm_device_t *dev, void *res)
{
    struct action_device_resource *devres = res;

    devres->action(devres->data);
}

int vmm_devm_add_action(vmm_device_t *dev, void (*action)(void *), void *data)
{
    struct action_device_resource *devres;

    devres = vmm_device_resource_alloc(devm_action_release, sizeof(struct action_device_resource));

    if (!devres) {
        return VMM_ENOMEM;
    }

    devres->data   = data;
    devres->action = action;

    vmm_device_resource_add(dev, devres);
    return 0;
}

void vmm_devm_remove_action(vmm_device_t *dev, void (*action)(void *), void *data)
{
    struct action_device_resource devres = {
        .data   = data,
        .action = action,
    };

    WARN_ON(vmm_device_resource_destroy(dev, devm_action_release, devm_action_match, &devres));
}
