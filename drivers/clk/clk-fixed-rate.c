/*
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Fixed rate clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/*
 * DOC: basic fixed-rate clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clock_(un)prepare only ensures parents are prepared
 * enable - clock_enable only ensures parents are enabled
 * rate - rate is always a fixed value.  No clock_set_rate support
 * parent - fixed parent.  No clock_set_parent support
 */

static uint64_t clock_fixed_rate_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    return to_clock_fixed_rate(hw)->fixed_rate;
}

static uint64_t clock_fixed_rate_recalc_accuracy(struct clock_hw *hw, uint64_t parent_accuracy)
{
    return to_clock_fixed_rate(hw)->fixed_accuracy;
}

const struct clock_ops clock_fixed_rate_ops = {
    .recalc_rate     = clock_fixed_rate_recalc_rate,
    .recalc_accuracy = clock_fixed_rate_recalc_accuracy,
};
EXPORT_SYMBOL_GPL(clock_fixed_rate_ops);

/**
 * clock_hw_register_fixed_rate_with_accuracy - register fixed-rate clock with
 * the clock framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @fixed_rate: non-adjustable clock rate
 * @fixed_accuracy: non-adjustable clock rate
 */
struct clock_hw *clock_hw_register_fixed_rate_with_accuracy(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate, uint64_t fixed_accuracy)
{
    struct clock_fixed_rate *fixed;
    struct clock_hw         *hw;
    struct clock_init_data   init;
    int                      ret;

    /* allocate fixed-rate clock */
    fixed = kzalloc(sizeof(*fixed), GFP_KERNEL);

    if (!fixed) {
        return ERR_PTR(-ENOMEM);
    }

    init.name             = name;
    init.ops              = &clock_fixed_rate_ops;
    init.flags            = flags | CLK_IS_BASIC;
    init.parent_names     = (parent_name ? &parent_name : NULL);
    init.num_parents      = (parent_name ? 1 : 0);

    /* struct clock_fixed_rate assignments */
    fixed->fixed_rate     = fixed_rate;
    fixed->fixed_accuracy = fixed_accuracy;
    fixed->hw.init        = &init;

    /* register the clock */
    hw                    = &fixed->hw;
    ret                   = clock_hw_register(dev, hw);

    if (ret) {
        kfree(fixed);
        hw = ERR_PTR(ret);
    }

    return hw;
}

EXPORT_SYMBOL_GPL(clock_hw_register_fixed_rate_with_accuracy);

struct clk *clock_register_fixed_rate_with_accuracy(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate, uint64_t fixed_accuracy)
{
    struct clock_hw *hw;

    hw = clock_hw_register_fixed_rate_with_accuracy(dev, name, parent_name, flags, fixed_rate, fixed_accuracy);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_fixed_rate_with_accuracy);

/**
 * clock_hw_register_fixed_rate - register fixed-rate clock with the clock
 * framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @fixed_rate: non-adjustable clock rate
 */
struct clock_hw *clock_hw_register_fixed_rate(struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate)
{
    return clock_hw_register_fixed_rate_with_accuracy(dev, name, parent_name, flags, fixed_rate, 0);
}

EXPORT_SYMBOL_GPL(clock_hw_register_fixed_rate);

struct clk *clock_register_fixed_rate(struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate)
{
    return clock_register_fixed_rate_with_accuracy(dev, name, parent_name, flags, fixed_rate, 0);
}

EXPORT_SYMBOL_GPL(clock_register_fixed_rate);

void clock_unregister_fixed_rate(struct clk *clk)
{
    struct clock_hw *hw;

    hw = __clock_get_hw(clk);

    if (!hw) {
        return;
    }

    clock_unregister(clk);
    kfree(to_clock_fixed_rate(hw));
}

EXPORT_SYMBOL_GPL(clock_unregister_fixed_rate);

void clock_hw_unregister_fixed_rate(struct clock_hw *hw)
{
    struct clock_fixed_rate *fixed;

    fixed = to_clock_fixed_rate(hw);

    clock_hw_unregister(hw);
    kfree(fixed);
}

EXPORT_SYMBOL_GPL(clock_hw_unregister_fixed_rate);

#ifdef CONFIG_OF
/**
 * of_fixed_clock_setup() - Setup function for simple fixed rate clock
 */
void of_fixed_clock_setup(struct device_node *node)
{
    struct clk *clk;
    const char *clock_name = node->name;
    uint32_t    rate;
    uint32_t    accuracy = 0;

    if (of_property_read_u32(node, "clock-frequency", &rate)) {
        return;
    }

    of_property_read_u32(node, "clock-accuracy", &accuracy);

    of_property_read_string(node, "clock-output-names", &clock_name);

    clk = clock_register_fixed_rate_with_accuracy(NULL, clock_name, NULL, CLK_IS_ROOT, rate, accuracy);

    if (!IS_ERR(clk)) {
        of_clock_add_provider(node, of_clock_src_simple_get, clk);
    }
}

EXPORT_SYMBOL_GPL(of_fixed_clock_setup);
CLK_OF_DECLARE(fixed_clock, "fixed-clock", of_fixed_clock_setup);
#endif
