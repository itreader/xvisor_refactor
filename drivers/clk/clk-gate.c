/*
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Gated clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

/**
 * DOC: basic gatable clock which can gate and ungate it's ouput
 *
 * Traits of this clock:
 * prepare - clock_(un)prepare only ensures parent is (un)prepared
 * enable - clock_enable and clock_disable are functional & control gating
 * rate - inherits rate from parent.  No clock_set_rate support
 * parent - fixed parent.  No clock_set_parent support
 */

/*
 * It works on following logic:
 *
 * For enabling clock, enable = 1
 *  set2dis = 1 -> clear bit    -> set = 0
 *  set2dis = 0 -> set bit  -> set = 1
 *
 * For disabling clock, enable = 0
 *  set2dis = 1 -> set bit  -> set = 1
 *  set2dis = 0 -> clear bit    -> set = 0
 *
 * So, result is always: enable xor set2dis.
 */
static void clock_gate_endisable(struct clock_hw *hw, int enable)
{
    struct clock_gate *gate  = to_clock_gate(hw);
    int                set   = gate->flags & CLK_GATE_SET_TO_DISABLE ? 1 : 0;
    uint64_t           flags = 0;
    uint32_t           reg;

    set ^= enable;

    if (gate->lock) {
        spin_lock_irq_save(gate->lock, flags);
    }

    if (gate->flags & CLK_GATE_HIWORD_MASK) {
        reg = BIT(gate->bit_idx + 16);

        if (set) {
            reg |= BIT(gate->bit_idx);
        }
    } else {
        reg = clock_readl(gate->reg);

        if (set) {
            reg |= BIT(gate->bit_idx);
        } else {
            reg &= ~BIT(gate->bit_idx);
        }
    }

    clock_writel(reg, gate->reg);

    if (gate->lock) {
        spin_unlock_irq_restore(gate->lock, flags);
    }
}

static int clock_gate_enable(struct clock_hw *hw)
{
    clock_gate_endisable(hw, 1);

    return 0;
}

static void clock_gate_disable(struct clock_hw *hw)
{
    clock_gate_endisable(hw, 0);
}

int clock_gate_is_enabled(struct clock_hw *hw)
{
    uint32_t           reg;
    struct clock_gate *gate = to_clock_gate(hw);

    reg                     = clock_readl(gate->reg);

    /* if a set bit disables this clk, flip it before masking */
    if (gate->flags & CLK_GATE_SET_TO_DISABLE) {
        reg ^= BIT(gate->bit_idx);
    }

    reg &= BIT(gate->bit_idx);

    return reg ? 1 : 0;
}

EXPORT_SYMBOL_GPL(clock_gate_is_enabled);

const struct clock_ops clock_gate_ops = {
    .enable     = clock_gate_enable,
    .disable    = clock_gate_disable,
    .is_enabled = clock_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(clock_gate_ops);

/**
 * clock_hw_register_gate - register a gate clock with the clock framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of this clock's parent
 * @flags: framework-specific flags for this clock
 * @reg: register address to control gating of this clock
 * @bit_idx: which bit in the register controls gating of this clock
 * @clock_gate_flags: gate-specific flags for this clock
 * @lock: shared register lock for this clock
 */
struct clock_hw *clock_hw_register_gate(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t bit_idx, uint8_t clock_gate_flags,
    spinlock_t *lock)
{
    struct clock_gate     *gate;
    struct clock_hw       *hw;
    struct clock_init_data init;
    int                    ret;

    if (clock_gate_flags & CLK_GATE_HIWORD_MASK) {
        if (bit_idx > 15) {
            pr_err("gate bit exceeds LOWORD field\n");
            return ERR_PTR(-EINVAL);
        }
    }

    /* allocate the gate */
    gate = kzalloc(sizeof(*gate), GFP_KERNEL);

    if (!gate) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_gate_ops;
    init.flags        = flags | CLK_IS_BASIC;
    init.parent_names = parent_name ? &parent_name : NULL;
    init.num_parents  = parent_name ? 1 : 0;

    /* struct clock_gate assignments */
    gate->reg         = reg;
    gate->bit_idx     = bit_idx;
    gate->flags       = clock_gate_flags;
    gate->lock        = lock;
    gate->hw.init     = &init;

    hw                = &gate->hw;
    ret               = clock_hw_register(dev, hw);

    if (ret) {
        kfree(gate);
        hw = ERR_PTR(ret);
    }

    return hw;
}

EXPORT_SYMBOL_GPL(clock_hw_register_gate);

struct clk *clock_register_gate(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t bit_idx, uint8_t clock_gate_flags,
    spinlock_t *lock)
{
    struct clock_hw *hw;

    hw = clock_hw_register_gate(dev, name, parent_name, flags, reg, bit_idx, clock_gate_flags, lock);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_gate);

void clock_unregister_gate(struct clk *clk)
{
    struct clock_gate *gate;
    struct clock_hw   *hw;

    hw = __clock_get_hw(clk);

    if (!hw) {
        return;
    }

    gate = to_clock_gate(hw);

    clock_unregister(clk);
    kfree(gate);
}

EXPORT_SYMBOL_GPL(clock_unregister_gate);

void clock_hw_unregister_gate(struct clock_hw *hw)
{
    struct clock_gate *gate;

    gate = to_clock_gate(hw);

    clock_hw_unregister(hw);
    kfree(gate);
}

EXPORT_SYMBOL_GPL(clock_hw_unregister_gate);
