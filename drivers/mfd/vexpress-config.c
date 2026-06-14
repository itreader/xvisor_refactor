/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vexpres-config.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Versatile Express config bridge implementation
 *
 * Adapted from linux/drivers/mfd/vexpres-config.c
 *
 * Copyright (C) 2012 ARM Limited
 *
 * The original source is licensed under GPL.
 */

#include <arch_barrier.h>
#include <libs/bitmap.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <vmm_completion.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#include <linux/vexpress.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define VEXPRESS_CONFIG_MAX_BRIDGES 2

struct vexpress_config_bridge {
    vmm_device_tree_node_t             *node;
    struct vexpress_config_bridge_info *info;
    list_head_t                         transactions;
    vmm_spinlock_t                      transactions_lock;
} vexpress_config_bridges[VEXPRESS_CONFIG_MAX_BRIDGES];

static DECLARE_BITMAP(vexpress_config_bridges_map, array_size(vexpress_config_bridges));
static DEFINE_SPINLOCK(vexpress_config_bridges_lock);

struct vexpress_config_bridge *vexpress_config_bridge_register(vmm_device_tree_node_t *node, struct vexpress_config_bridge_info *info)
{
    struct vexpress_config_bridge *bridge;
    int                            i;

    DPRINTF("Registering bridge '%s'\n", info->name);

    vmm_spin_lock(&vexpress_config_bridges_lock);
    i = find_first_zero_bit(vexpress_config_bridges_map, array_size(vexpress_config_bridges));

    if (i >= array_size(vexpress_config_bridges)) {
        vmm_printf("Can't register more bridges!\n");
        vmm_spin_unlock(&vexpress_config_bridges_lock);
        return NULL;
    }

    __set_bit(i, vexpress_config_bridges_map);
    bridge = &vexpress_config_bridges[i];

    vmm_device_tree_ref_node(node);
    bridge->node = node;
    bridge->info = info;
    INIT_LIST_HEAD(&bridge->transactions);
    INIT_SPIN_LOCK(&bridge->transactions_lock);

    vmm_spin_unlock(&vexpress_config_bridges_lock);

    return bridge;
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_bridge_register);

void vexpress_config_bridge_unregister(struct vexpress_config_bridge *bridge)
{
    struct vexpress_config_bridge __bridge = *bridge;
    int i;

    vmm_spin_lock(&vexpress_config_bridges_lock);

    for (i = 0; i < array_size(vexpress_config_bridges); i++) {
        if (&vexpress_config_bridges[i] == bridge) {
            __clear_bit(i, vexpress_config_bridges_map);
        }
    }

    vmm_spin_unlock(&vexpress_config_bridges_lock);

    WARN_ON(!list_empty(&__bridge.transactions));

    while (!list_empty(&__bridge.transactions)) {
        arch_smp_mb(); /* FIXME: cpu_relax(); */
    }

    vmm_device_tree_dref_node(__bridge.node);
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_bridge_unregister);

struct vexpress_config_func {
    struct vexpress_config_bridge *bridge;
    void                          *func;
};

struct vexpress_config_func *__vexpress_config_func_get(vmm_device_t *dev, vmm_device_tree_node_t *node)
{
    vmm_device_tree_node_t      *bridge_node;
    struct vexpress_config_func *func;
    int                          i;

    if (WARN_ON(dev && node && dev->of_node != node)) {
        return NULL;
    }

    if (dev && !node) {
        node = dev->of_node;
    }

    func = vmm_zalloc(sizeof(*func));

    if (!func) {
        return NULL;
    }

    bridge_node = node;

    while (bridge_node) {
        uint32_t prop;

        if (vmm_device_tree_read_u32(bridge_node, "arm,vexpress,config-bridge", &prop) == VMM_OK) {
            bridge_node = vmm_device_tree_find_node_by_phandle(prop);
            vmm_device_tree_dref_node(bridge_node);
            break;
        }

        bridge_node = bridge_node->parent;
    }

    vmm_spin_lock(&vexpress_config_bridges_lock);

    for (i = 0; i < array_size(vexpress_config_bridges); i++) {
        struct vexpress_config_bridge *bridge = &vexpress_config_bridges[i];

        if (test_bit(i, vexpress_config_bridges_map) && bridge->node == bridge_node) {
            func->bridge = bridge;
            func->func   = bridge->info->func_get(dev, node);
            break;
        }
    }

    vmm_spin_unlock(&vexpress_config_bridges_lock);

    if (!func->func) {
        vmm_free(func);
        return NULL;
    }

    return func;
}

VMM_ERR_XPORT_SYMBOL(__vexpress_config_func_get);

void vexpress_config_func_put(struct vexpress_config_func *func)
{
    func->bridge->info->func_put(func->func);
    vmm_free(func);
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_func_put);

struct vexpress_config_trans {
    struct vexpress_config_func *func;
    int                          offset;
    bool                         write;
    uint32_t                    *data;
    int                          status;
    vmm_completion_t             completion;
    list_head_t                  list;
};

static void vexpress_config_dump_trans(const char *what, struct vexpress_config_trans *trans)
{
    DPRINTF(
        "%s %s trans %p func 0x%p offset %d data 0x%x status %d\n", what, trans->write ? "write" : "read", trans, trans->func->func, trans->offset,
        trans->data ? *trans->data : 0, trans->status);
}

static int vexpress_config_schedule(struct vexpress_config_trans *trans)
{
    int                            status;
    struct vexpress_config_bridge *bridge = trans->func->bridge;
    irq_flags_t                    flags;

    INIT_COMPLETION(&trans->completion);
    trans->status = VMM_ERR_FAULT;

    vmm_spin_lock_irq_save(&bridge->transactions_lock, flags);

    if (list_empty(&bridge->transactions)) {
        vexpress_config_dump_trans("Executing", trans);
        status = bridge->info->func_exec(trans->func->func, trans->offset, trans->write, trans->data);
    } else {
        vexpress_config_dump_trans("Queuing", trans);
        status = VEXPRESS_CONFIG_STATUS_WAIT;
    }

    switch (status) {
        case VEXPRESS_CONFIG_STATUS_DONE:
            vexpress_config_dump_trans("Finished", trans);
            trans->status = status;
            break;

        case VEXPRESS_CONFIG_STATUS_WAIT:
            list_add_tail(&trans->list, &bridge->transactions);
            break;
    }

    vmm_spin_unlock_irq_restore(&bridge->transactions_lock, flags);

    return status;
}

void vexpress_config_complete(struct vexpress_config_bridge *bridge, int status)
{
    struct vexpress_config_trans *trans;
    irq_flags_t                   flags;
    const char                   *message = "Completed";

    vmm_spin_lock_irq_save(&bridge->transactions_lock, flags);

    trans         = list_first_entry(&bridge->transactions, struct vexpress_config_trans, list);
    trans->status = status;

    do {
        vexpress_config_dump_trans(message, trans);
        list_del(&trans->list);
        vmm_completion_complete(&trans->completion);

        if (list_empty(&bridge->transactions)) {
            break;
        }

        trans = list_first_entry(&bridge->transactions, struct vexpress_config_trans, list);
        vexpress_config_dump_trans("Executing pending", trans);
        trans->status = bridge->info->func_exec(trans->func->func, trans->offset, trans->write, trans->data);
        message       = "Finished pending";
    } while (trans->status == VEXPRESS_CONFIG_STATUS_DONE);

    vmm_spin_unlock_irq_restore(&bridge->transactions_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_complete);

int vexpress_config_wait(struct vexpress_config_trans *trans)
{
    vmm_completion_wait(&trans->completion);

    return trans->status;
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_wait);

int vexpress_config_read(struct vexpress_config_func *func, int offset, uint32_t *data)
{
    struct vexpress_config_trans trans = {
        .func   = func,
        .offset = offset,
        .write  = false,
        .data   = data,
        .status = 0,
    };
    int status = vexpress_config_schedule(&trans);

    if (status == VEXPRESS_CONFIG_STATUS_WAIT) {
        status = vexpress_config_wait(&trans);
    }

    return status;
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_read);

int vexpress_config_write(struct vexpress_config_func *func, int offset, uint32_t data)
{
    struct vexpress_config_trans trans = {
        .func   = func,
        .offset = offset,
        .write  = true,
        .data   = &data,
        .status = 0,
    };
    int status = vexpress_config_schedule(&trans);

    if (status == VEXPRESS_CONFIG_STATUS_WAIT) {
        status = vexpress_config_wait(&trans);
    }

    return status;
}

VMM_ERR_XPORT_SYMBOL(vexpress_config_write);
