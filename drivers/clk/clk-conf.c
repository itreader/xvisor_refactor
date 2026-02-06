/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWid
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski for Xvisor port.
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @file clk-conf.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Device tree clock configuration helper.
 */

#include <drv/clk-provider.h>
#include <drv/clk.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

static int __set_clock_parents(vmm_device_tree_node_t *node, bool clock_supplier)
{
    struct vmm_device_tree_phandle_args clkspec;
    int                                 index, rc, num_parents;
    struct clk                         *clk, *pclk;
    const char                         *list_name  = "assigned-clock-parents";
    const char                         *cells_name = "#clock-cells";

    num_parents                                    = vmm_device_tree_count_phandle_with_args(node, list_name, cells_name);

    if (num_parents == VMM_EINVALID) {
        vmm_printf(
            "clk: invalid value of clock-parents property at "
            "%s\n",
            node->name);
    }

    for (index = 0; index < num_parents; index++) {
        rc = vmm_device_tree_parse_phandle_with_args(node, list_name, cells_name, index, &clkspec);

        if (rc < 0) {
            /* skip empty (null) phandles */
            if (rc == VMM_ENOENT) {
                continue;
            } else {
                return rc;
            }
        }

        if (clkspec.np == node && !clock_supplier) {
            return 0;
        }

        pclk = of_clock_get_from_provider(&clkspec);

        if (VMM_IS_ERR_VALUE(pclk)) {
            vmm_printf(
                "clk: couldn't get parent clock %d for "
                "%s\n",
                index, node->name);
            return VMM_EFAIL;
        }

        rc = vmm_device_tree_parse_phandle_with_args(node, "assigned-clocks", cells_name, index, &clkspec);

        if (rc < 0) {
            goto err;
        }

        if (clkspec.np == node && !clock_supplier) {
            rc = 0;
            goto err;
        }

        clk = of_clock_get_from_provider(&clkspec);

        if (VMM_IS_ERR_VALUE(clk)) {
            vmm_printf(
                "clk: couldn't get parent clock %d for "
                "%s\n",
                index, node->name);
            rc = VMM_EFAIL;
            goto err;
        }

        rc = clock_set_parent(clk, pclk);

        if (rc < 0) {
            vmm_printf("clk: failed to reparent %s to %s: %d\n", __clock_get_name(clk), __clock_get_name(pclk), rc);
        }

        clock_put(clk);
        clock_put(pclk);
    }

    return 0;
err:
    clock_put(pclk);
    return rc;
}

static int __set_clock_rates(vmm_device_tree_node_t *node, bool clock_supplier)
{
    struct vmm_device_tree_phandle_args clkspec;
    int                                 rate_idx = 0;
    int                                 rc, index = 0;
    struct clk                         *clk;
    uint32_t                            rate;

    while (1) {
        rc = vmm_device_tree_read_u32_atindex(node, "assigned-clock-rates", &rate, rate_idx);

        if (VMM_OK != rc) {
            break;
        }

        ++rate_idx;

        if (!rate) {
            continue;
        }

        rc = vmm_device_tree_parse_phandle_with_args(node, "assigned-clocks", "#clock-cells", index, &clkspec);

        if (rc < 0) {
            /* skip empty (null) phandles */
            if (rc == VMM_ENOENT) {
                continue;
            } else {
                return rc;
            }
        }

        if (clkspec.np == node && !clock_supplier) {
            return 0;
        }

        clk = of_clock_get_from_provider(&clkspec);

        if (VMM_IS_ERR_VALUE(clk)) {
            vmm_printf("clk: couldn't get clock %d for %s\n", index, node->name);
            return VMM_PTR_ERR(clk);
        }

        rc = clock_set_rate(clk, rate);

        if (rc < 0) {
            vmm_printf("clk: couldn't set %s clock rate: %d\n", __clock_get_name(clk), rc);
        }

        clock_put(clk);
        index++;
    }

    return 0;
}

/**
 * of_clock_set_defaults() - parse and set assigned clocks configuration
 * @node: device node to apply clock settings for
 * @clock_supplier: true if clocks supplied by @node should also be considered
 *
 * This function parses 'assigned-{clocks/clock-parents/clock-rates}' properties
 * and sets any specified clock parents and rates. The @clock_supplier argument
 * should be set to true if @node may be also a clock supplier of any clock
 * listed in its 'assigned-clocks' or 'assigned-clock-parents' properties.
 * If @clock_supplier is false the function exits returnning 0 as soon as it
 * determines the @node is also a supplier of any of the clocks.
 */
int of_clock_set_defaults(vmm_device_tree_node_t *node, bool clock_supplier)
{
    int rc;

    if (!node) {
        return 0;
    }

    rc = __set_clock_parents(node, clock_supplier);

    if (rc < 0) {
        return rc;
    }

    return __set_clock_rates(node, clock_supplier);
}

VMM_EXPORT_SYMBOL_GPL(of_clock_set_defaults);
