/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vmm_platform.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Platform bus implementation
 */

#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_msi.h>
#include <vmm_platform.h>

int __weak vmm_platform_pinctrl_bind(vmm_device_t *dev)
{
    /* Nothing to do here. */
    /* The pinctrl framework will provide actual implementation */
    return VMM_OK;
}

int __weak vmm_platform_pinctrl_init(vmm_device_t *dev)
{
    /* Nothing to do here. */
    /* The pinctrl framework will provide actual implementation */
    return VMM_OK;
}

static vmm_msi_domain_t *platform_get_msi_domain(vmm_device_t *dev, vmm_device_tree_node_t *np)
{
    int                                 index = 0;
    vmm_msi_domain_t                   *d     = NULL;
    vmm_device_tree_node_t             *msi_np;
    struct vmm_device_tree_phandle_args args;

    if (!dev) {
        return NULL;
    }

    /* Check for a single msi-parent property */
    msi_np = vmm_device_tree_parse_phandle(np, "msi-parent", 0);

    if (msi_np) {
        if (!vmm_device_tree_getattr(msi_np, "#msi-cells")) {
            d = vmm_msi_find_domain(msi_np, VMM_MSI_DOMAIN_PLATFORM);
        }

        vmm_device_tree_dref_node(msi_np);
        return d;
    }

    /* Check for the complex msi-parent version */
    while (!vmm_device_tree_parse_phandle_with_args(np, "msi-parent", "#msi-cells", index, &args)) {
        d = vmm_msi_find_domain(args.np, VMM_MSI_DOMAIN_PLATFORM);
        vmm_device_tree_dref_node(args.np);

        if (d) {
            return d;
        }

        index++;
    }

    return NULL;
}

static void platform_msi_configure(vmm_device_t *dev)
{
    vmm_device_driver_set_msi_domain(dev, platform_get_msi_domain(dev, dev->of_node));
}

static int platform_bus_match(vmm_device_t *dev, vmm_driver_t *drv)
{
    const struct vmm_device_tree_nodeid *match;

    if (!dev || !dev->of_node || !drv || !drv->match_table) {
        return 0;
    }

    if (!vmm_device_tree_is_available(dev->of_node)) {
        return 0;
    }

    if (dev->parent && (dev->of_node == dev->parent->of_node)) {
        return 0;
    }

    match = vmm_device_tree_match_node(drv->match_table, dev->of_node);

    if (!match) {
        return 0;
    }

    return 1;
}

static int platform_bus_probe(vmm_device_t *dev)
{
    int           rc;
    vmm_driver_t *drv;

    if (!dev || !dev->of_node || !dev->driver) {
        return VMM_EFAIL;
    }

    drv = dev->driver;

    if (!drv->match_table) {
        return VMM_EFAIL;
    }

    rc = vmm_platform_pinctrl_bind(dev);

    if (rc == VMM_EPROBE_DEFER) {
        return rc;
    }

    rc = drv->probe(dev);

    vmm_platform_pinctrl_init(dev);

    return rc;
}

static int platform_bus_remove(vmm_device_t *dev)
{
    vmm_driver_t *drv;

    if (!dev || !dev->of_node || !dev->driver) {
        return VMM_EFAIL;
    }

    drv = dev->driver;

    return drv->remove(dev);
}

static void platform_device_release(vmm_device_t *dev)
{
    vmm_device_tree_dref_node(dev->of_node);
    dev->of_node = NULL;
    vmm_free(dev);
}

enum platform_probe_order {
    PLATFORM_PROBE_ORDER_IRQCHIP = 0,
    PLATFORM_PROBE_ORDER_MISC,
    PLATFORM_PROBE_ORDER_MAX
};

static enum platform_probe_order platform_get_probe_order(vmm_device_tree_node_t *node)
{
    enum platform_probe_order ret = PLATFORM_PROBE_ORDER_MISC;

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_INTERRUPT_CNTRL_ATTR_NAME)) {
        ret = PLATFORM_PROBE_ORDER_IRQCHIP;
    }

    return ret;
}

static int platform_probe(vmm_device_tree_node_t *node, vmm_device_t *parent)
{
    int                     rc, order;
    vmm_device_t           *dev;
    vmm_device_tree_node_t *child;

    if (!node) {
        return VMM_EFAIL;
    }

    dev = vmm_zalloc(sizeof(vmm_device_t));

    if (!dev) {
        return VMM_ENOMEM;
    }

    vmm_device_driver_initialize_device(dev);

    if (strlcpy(dev->name, node->name, sizeof(dev->name)) >= sizeof(dev->name)) {
        vmm_free(dev);
        return VMM_EOVERFLOW;
    }

    vmm_device_tree_ref_node(node);
    dev->of_node = node;
    dev->parent  = parent;
    dev->bus     = &platform_bus;
    dev->release = platform_device_release;
    dev->private = NULL;

    platform_msi_configure(dev);

    rc = vmm_device_driver_register_device(dev);

    if (rc) {
        vmm_free(dev);
        return rc;
    }

    for (order = 0; order < PLATFORM_PROBE_ORDER_MAX; order++) {
        vmm_device_tree_for_each_child(child, node)
        {
            if (platform_get_probe_order(child) != order) {
                continue;
            }

            platform_probe(child, dev);
        }
    }

    return VMM_OK;
}

vmm_bus_t platform_bus = {
    .name   = "platform",
    .match  = platform_bus_match,
    .probe  = platform_bus_probe,
    .remove = platform_bus_remove,
};

const struct vmm_device_tree_nodeid *vmm_platform_match_nodeid(vmm_device_t *dev)
{
    if (!dev || !dev->of_node || !dev->driver || !dev->driver->match_table || (dev->bus != &platform_bus)) {
        return NULL;
    }

    return vmm_device_tree_match_node(dev->driver->match_table, dev->of_node);
}

vmm_device_t *vmm_platform_find_device_by_node(vmm_device_tree_node_t *np)
{
    return vmm_device_driver_bus_find_device_by_node(&platform_bus, NULL, np);
}

int vmm_platform_probe(vmm_device_tree_node_t *node)
{
    return platform_probe(node, NULL);
}
