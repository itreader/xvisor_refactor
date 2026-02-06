/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-gate2.c
 *
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Gated clock implementation
 *
 * @file clk-fixup-div.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX gate clock function helpers
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "clk.h"

/**
 * DOC: basic gatable clock which can gate and ungate it's ouput
 *
 * Traits of this clock:
 * prepare - clock_(un)prepare only ensures parent is (un)prepared
 * enable - clock_enable and clock_disable are functional & control gating
 * rate - inherits rate from parent.  No clock_set_rate support
 * parent - fixed parent.  No clock_set_parent support
 */

struct clock_gate2 {
    struct clock_hw hw;
    void __iomem   *reg;
    uint8_t         bit_idx;
    uint8_t         flags;
    spinlock_t     *lock;
    uint32_t       *share_count;
};

#define to_clock_gate2(_hw) container_of(_hw, struct clock_gate2, hw)

static int clock_gate2_enable(struct clock_hw *hw)
{
    struct clock_gate2 *gate = to_clock_gate2(hw);
    uint32_t            reg;
    uint64_t            flags = 0;

    spin_lock_irq_save(gate->lock, flags);

    if (gate->share_count && (*gate->share_count)++ > 0) {
        goto out;
    }

    reg = readl(gate->reg);
    reg |= 3 << gate->bit_idx;
    writel(reg, gate->reg);

out:
    spin_unlock_irq_restore(gate->lock, flags);

    return 0;
}

static void clock_gate2_disable(struct clock_hw *hw)
{
    struct clock_gate2 *gate = to_clock_gate2(hw);
    uint32_t            reg;
    uint64_t            flags = 0;

    spin_lock_irq_save(gate->lock, flags);

    if (gate->share_count && --(*gate->share_count) > 0) {
        goto out;
    }

    reg = readl(gate->reg);
    reg &= ~(3 << gate->bit_idx);
    writel(reg, gate->reg);

out:
    spin_unlock_irq_restore(gate->lock, flags);
}

static int clock_gate2_is_enabled(struct clock_hw *hw)
{
    uint32_t            reg;
    struct clock_gate2 *gate = to_clock_gate2(hw);

    reg                      = readl(gate->reg);

    if (((reg >> gate->bit_idx) & 1) == 1) {
        return 1;
    }

    return 0;
}

static struct clock_ops clock_gate2_ops = {
    .enable     = clock_gate2_enable,
    .disable    = clock_gate2_disable,
    .is_enabled = clock_gate2_is_enabled,
};

struct clk *clock_register_gate2(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t bit_idx, uint8_t clock_gate2_flags,
    spinlock_t *lock, uint32_t *share_count)
{
    struct clock_gate2    *gate;
    struct clk            *clk;
    struct clock_init_data init;

    gate = kzalloc(sizeof(struct clock_gate2), GFP_KERNEL);

    if (!gate) {
        return ERR_PTR(-ENOMEM);
    }

    /* struct clock_gate2 assignments */
    gate->reg         = reg;
    gate->bit_idx     = bit_idx;
    gate->flags       = clock_gate2_flags;
    gate->lock        = lock;
    gate->share_count = share_count;

    init.name         = name;
    init.ops          = &clock_gate2_ops;
    init.flags        = flags;
    init.parent_names = parent_name ? &parent_name : NULL;
    init.num_parents  = parent_name ? 1 : 0;

    gate->hw.init     = &init;

    clk               = clock_register(dev, &gate->hw);

    if (IS_ERR(clk)) {
        kfree(gate);
    }

    return clk;
}
