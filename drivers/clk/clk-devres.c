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
 * @file clk-devres.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic clocking devres APIs
 *
 * Adapted from linux/drivers/clk/clk-devres.c
 *
 * The original source is licensed under GPL.
 */

#include <drv/clk.h>
#include <vmm_device_driver.h>
#include <vmm_device_resource.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

static void devm_clock_release(vmm_device_t *dev, void *res)
{
    clock_put(*(struct clk **)res);
}

struct clk *devm_clock_get(vmm_device_t *dev, const char *id)
{
    struct clk **ptr, *clk;

    ptr = vmm_device_resource_alloc(devm_clock_release, sizeof(*ptr));

    if (!ptr) {
        return VMM_ERR_PTR(VMM_ENOMEM);
    }

    clk = clock_get(dev, id);

    if (!VMM_IS_ERR(clk)) {
        *ptr = clk;
        vmm_device_resource_add(dev, ptr);
    } else {
        vmm_device_resource_free(ptr);
    }

    return clk;
}

VMM_EXPORT_SYMBOL(devm_clock_get);

struct clock_bulk_device_resource {
    struct clock_bulk_data *clks;
    int                     num_clocks;
};

static void devm_clock_bulk_release(vmm_device_t *dev, void *res)
{
    struct clock_bulk_device_resource *devres = res;

    clock_bulk_put(devres->num_clocks, devres->clks);
}

int devm_clock_bulk_get(vmm_device_t *dev, int num_clocks, struct clock_bulk_data *clks)
{
    struct clock_bulk_device_resource *devres;
    int                                ret;

    devres = vmm_device_resource_alloc(devm_clock_bulk_release, sizeof(*devres));

    if (!devres) {
        return VMM_ENOMEM;
    }

    ret = clock_bulk_get(dev, num_clocks, clks);

    if (!ret) {
        devres->clks       = clks;
        devres->num_clocks = num_clocks;
        vmm_device_resource_add(dev, devres);
    } else {
        vmm_device_resource_free(devres);
    }

    return ret;
}

VMM_EXPORT_SYMBOL(devm_clock_bulk_get);

static int devm_clock_match(vmm_device_t *dev, void *res, void *data)
{
    struct clk **c = res;

    if (!c || !*c) {
        WARN_ON(!c || !*c);
        return 0;
    }

    return *c == data;
}

void devm_clock_put(vmm_device_t *dev, struct clk *clk)
{
    int ret;

    ret = vmm_device_resource_release(dev, devm_clock_release, devm_clock_match, clk);

    WARN_ON(ret);
}

VMM_EXPORT_SYMBOL(devm_clock_put);

struct clk *devm_get_clock_from_child(vmm_device_t *dev, vmm_device_tree_node_t *np, const char *con_id)
{
    struct clk **ptr, *clk;

    ptr = vmm_device_resource_alloc(devm_clock_release, sizeof(*ptr));

    if (!ptr) {
        return VMM_ERR_PTR(VMM_ENOMEM);
    }

    clk = of_clock_get_by_name(np, con_id);

    if (!VMM_IS_ERR(clk)) {
        *ptr = clk;
        vmm_device_resource_add(dev, ptr);
    } else {
        vmm_device_resource_free(ptr);
    }

    return clk;
}

VMM_EXPORT_SYMBOL(devm_get_clock_from_child);
