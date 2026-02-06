/*
 * Copyright (C) 2011 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Standard functionality for the common clock API.
 */
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/*
 * DOC: basic fixed multiplier and divider clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clock_prepare only ensures that parents are prepared
 * enable - clock_enable only ensures that parents are enabled
 * rate - rate is fixed.  clk->rate = parent->rate / div * mult
 * parent - fixed parent.  No clock_set_parent support
 */

static uint64_t clock_factor_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_fixed_factor *fix = to_clock_fixed_factor(hw);
    uint64_t                   rate;

    rate = (uint64_t)parent_rate * fix->mult;
#if 0
    do_div(rate, fix->div);
#else
    rate = udiv64(rate, fix->div);
#endif
    return (uint64_t)rate;
}

static long clock_factor_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct clock_fixed_factor *fix = to_clock_fixed_factor(hw);

    if (clock_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
        uint64_t best_parent;

#if 0
        best_parent = (rate / fix->mult) * fix->div;
#else
        best_parent = udiv64(rate, fix->mult) * fix->div;
#endif
        *prate = clock_hw_round_rate(clock_hw_get_parent(hw), best_parent);
    }

#if 0
    return (*prate / fix->div) * fix->mult;
#else
    return udiv64(*prate, fix->div) * fix->mult;
#endif
}

static int clock_factor_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    /*
     * We must report success but we can do so unconditionally because
     * clock_factor_round_rate returns values that ensure this call is a
     * nop.
     */

    return 0;
}

const struct clock_ops clock_fixed_factor_ops = {
    .round_rate  = clock_factor_round_rate,
    .set_rate    = clock_factor_set_rate,
    .recalc_rate = clock_factor_recalc_rate,
};
EXPORT_SYMBOL_GPL(clock_fixed_factor_ops);

struct clock_hw *clock_hw_register_fixed_factor(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint32_t mult, uint32_t div)
{
    struct clock_fixed_factor *fix;
    struct clock_init_data     init;
    struct clock_hw           *hw;
    int                        ret;

    fix = kmalloc(sizeof(*fix), GFP_KERNEL);

    if (!fix) {
        return ERR_PTR(-ENOMEM);
    }

    /* struct clock_fixed_factor assignments */
    fix->mult         = mult;
    fix->div          = div;
    fix->hw.init      = &init;

    init.name         = name;
    init.ops          = &clock_fixed_factor_ops;
    init.flags        = flags | CLK_IS_BASIC;
    init.parent_names = &parent_name;
    init.num_parents  = 1;

    hw                = &fix->hw;
    ret               = clock_hw_register(dev, hw);

    if (ret) {
        kfree(fix);
        hw = ERR_PTR(ret);
    }

    return hw;
}

EXPORT_SYMBOL_GPL(clock_hw_register_fixed_factor);

struct clk *clock_register_fixed_factor(struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint32_t mult, uint32_t div)
{
    struct clock_hw *hw;

    hw = clock_hw_register_fixed_factor(dev, name, parent_name, flags, mult, div);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_fixed_factor);

void clock_unregister_fixed_factor(struct clk *clk)
{
    struct clock_hw *hw;

    hw = __clock_get_hw(clk);

    if (!hw) {
        return;
    }

    clock_unregister(clk);
    kfree(to_clock_fixed_factor(hw));
}

EXPORT_SYMBOL_GPL(clock_unregister_fixed_factor);

void clock_hw_unregister_fixed_factor(struct clock_hw *hw)
{
    struct clock_fixed_factor *fix;

    fix = to_clock_fixed_factor(hw);

    clock_hw_unregister(hw);
    kfree(fix);
}

EXPORT_SYMBOL_GPL(clock_hw_unregister_fixed_factor);

#ifdef CONFIG_OF
/**
 * of_fixed_factor_clock_setup() - Setup function for simple fixed factor clock
 */
void __init of_fixed_factor_clock_setup(struct device_node *node)
{
    struct clk *clk;
    const char *clock_name = node->name;
    const char *parent_name;
    uint32_t    div, mult;

    if (of_property_read_u32(node, "clock-div", &div)) {
        pr_err("%s Fixed factor clock <%s> must have a clock-div property\n", __func__, node->name);
        return;
    }

    if (of_property_read_u32(node, "clock-mult", &mult)) {
        pr_err("%s Fixed factor clock <%s> must have a clock-mult property\n", __func__, node->name);
        return;
    }

    of_property_read_string(node, "clock-output-names", &clock_name);
    parent_name = of_clock_get_parent_name(node, 0);

    clk         = clock_register_fixed_factor(NULL, clock_name, parent_name, 0, mult, div);

    if (!IS_ERR(clk)) {
        of_clock_add_provider(node, of_clock_src_simple_get, clk);
    }
}

EXPORT_SYMBOL_GPL(of_fixed_factor_clock_setup);
CLK_OF_DECLARE(fixed_factor_clock, "fixed-factor-clock", of_fixed_factor_clock_setup);
#endif
