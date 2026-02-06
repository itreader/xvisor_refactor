/*
 * Copyright (c) 2013 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_address.h>

static DEFINE_SPINLOCK(clock_lock);

/*
 * Gate clocks
 */

static void __init rk2928_gate_clock_init(struct device_node *node)
{
    struct clock_onecell_data *clock_data;
    const char                *clock_parent;
    const char                *clock_name;
    void __iomem              *reg;
    void __iomem              *reg_idx;
    int                        flags;
    int                        qty;
    int                        reg_bit;
    int                        clkflags = CLK_SET_RATE_PARENT;
    int                        i;

    qty = of_property_count_strings(node, "clock-output-names");

    if (qty < 0) {
        pr_err("%s: error in clock-output-names %d\n", __func__, qty);
        return;
    }

    if (qty == 0) {
        pr_info("%s: nothing to do\n", __func__);
        return;
    }

    reg        = of_iomap(node, 0);

    clock_data = kzalloc(sizeof(struct clock_onecell_data), GFP_KERNEL);

    if (!clock_data) {
        return;
    }

    clock_data->clks = kzalloc(qty * sizeof(struct clk *), GFP_KERNEL);

    if (!clock_data->clks) {
        kfree(clock_data);
        return;
    }

    flags = CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE;

    for (i = 0; i < qty; i++) {
        of_property_read_string_index(node, "clock-output-names", i, &clock_name);

        /* ignore empty slots */
        if (!strcmp("reserved", clock_name)) {
            continue;
        }

        clock_parent = of_clock_get_parent_name(node, i);

        /* keep all gates untouched for now */
        clkflags |= CLK_IGNORE_UNUSED;

        reg_idx             = reg + (4 * (i / 16));
        reg_bit             = (i % 16);

        clock_data->clks[i] = clock_register_gate(NULL, clock_name, clock_parent, clkflags, reg_idx, reg_bit, flags, &clock_lock);
        WARN_ON(IS_ERR(clock_data->clks[i]));
    }

    clock_data->clock_num = qty;

    of_clock_add_provider(node, of_clock_src_onecell_get, clock_data);
}

CLK_OF_DECLARE(rk2928_gate, "rockchip,rk2928-gate-clk", rk2928_gate_clock_init);
