/*
 * Copyright (C) 2011 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 * Copyright (C) 2011 Richard Zhao, Linaro <richard.zhao@linaro.org>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Simple multiplexer clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>

/*
 * DOC: basic adjustable multiplexer clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clock_prepare only ensures that parents are prepared
 * enable - clock_enable only ensures that parents are enabled
 * rate - rate is only affected by parent switching.  No clock_set_rate support
 * parent - parent is adjustable through clock_set_parent
 */

static uint8_t clock_mux_get_parent(struct clock_hw *hw)
{
    struct clock_mux *mux         = to_clock_mux(hw);
    int               num_parents = clock_hw_get_num_parents(hw);
    uint32_t          val;

    /*
     * FIXME need a mux-specific flag to determine if val is bitwise or numeric
     * e.g. sys_clockin_ck's clksel field is 3 bits wide, but ranges from 0x1
     * to 0x7 (index starts at one)
     * OTOH, pmd_trace_clock_mux_ck uses a separate bit for each clock, so
     * val = 0x4 really means "bit 2, index starts at bit 0"
     */
    val = clock_readl(mux->reg) >> mux->shift;
    val &= mux->mask;

    if (mux->table) {
        int i;

        for (i = 0; i < num_parents; i++) {
            if (mux->table[i] == val) {
                return i;
            }
        }

        return -EINVAL;
    }

    if (val && (mux->flags & CLK_MUX_INDEX_BIT)) {
        val = ffs(val) - 1;
    }

    if (val && (mux->flags & CLK_MUX_INDEX_ONE)) {
        val--;
    }

    if (val >= num_parents) {
        return -EINVAL;
    }

    return val;
}

static int clock_mux_set_parent(struct clock_hw *hw, uint8_t index)
{
    struct clock_mux *mux = to_clock_mux(hw);
    uint32_t          val;
    uint64_t          flags = 0;

    if (mux->table) {
        index = mux->table[index];
    } else {
        if (mux->flags & CLK_MUX_INDEX_BIT) {
            index = 1 << index;
        }

        if (mux->flags & CLK_MUX_INDEX_ONE) {
            index++;
        }
    }

    if (mux->lock) {
        spin_lock_irq_save(mux->lock, flags);
    }

    if (mux->flags & CLK_MUX_HIWORD_MASK) {
        val = mux->mask << (mux->shift + 16);
    } else {
        val = clock_readl(mux->reg);
        val &= ~(mux->mask << mux->shift);
    }

    val |= index << mux->shift;
    clock_writel(val, mux->reg);

    if (mux->lock) {
        spin_unlock_irq_restore(mux->lock, flags);
    }

    return 0;
}

const struct clock_ops clock_mux_ops = {
    .get_parent     = clock_mux_get_parent,
    .set_parent     = clock_mux_set_parent,
    .determine_rate = __clock_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(clock_mux_ops);

const struct clock_ops clock_mux_ro_ops = {
    .get_parent = clock_mux_get_parent,
};
EXPORT_SYMBOL_GPL(clock_mux_ro_ops);

struct clock_hw *clock_hw_register_mux_table(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void __iomem *reg, uint8_t shift,
    uint32_t mask, uint8_t clock_mux_flags, uint32_t *table, spinlock_t *lock)
{
    struct clock_mux      *mux;
    struct clock_hw       *hw;
    struct clock_init_data init;
    uint8_t                width = 0;
    int                    ret;

    if (clock_mux_flags & CLK_MUX_HIWORD_MASK) {
        width = fls(mask) - ffs(mask) + 1;

        if (width + shift > 16) {
            pr_err("mux value exceeds LOWORD field\n");
            return ERR_PTR(-EINVAL);
        }
    }

    /* allocate the mux */
    mux = kzalloc(sizeof(*mux), GFP_KERNEL);

    if (!mux) {
        return ERR_PTR(-ENOMEM);
    }

    init.name = name;

    if (clock_mux_flags & CLK_MUX_READ_ONLY) {
        init.ops = &clock_mux_ro_ops;
    } else {
        init.ops = &clock_mux_ops;
    }

    init.flags        = flags | CLK_IS_BASIC;
    init.parent_names = parent_names;
    init.num_parents  = num_parents;

    /* struct clock_mux assignments */
    mux->reg          = reg;
    mux->shift        = shift;
    mux->mask         = mask;
    mux->flags        = clock_mux_flags;
    mux->lock         = lock;
    mux->table        = table;
    mux->hw.init      = &init;

    hw                = &mux->hw;
    ret               = clock_hw_register(dev, hw);

    if (ret) {
        kfree(mux);
        hw = ERR_PTR(ret);
    }

    return hw;
}

EXPORT_SYMBOL_GPL(clock_hw_register_mux_table);

struct clk *clock_register_mux_table(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void __iomem *reg, uint8_t shift,
    uint32_t mask, uint8_t clock_mux_flags, uint32_t *table, spinlock_t *lock)
{
    struct clock_hw *hw;

    hw = clock_hw_register_mux_table(dev, name, parent_names, num_parents, flags, reg, shift, mask, clock_mux_flags, table, lock);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_mux_table);

struct clk *clock_register_mux(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void __iomem *reg, uint8_t shift,
    uint8_t width, uint8_t clock_mux_flags, spinlock_t *lock)
{
    uint32_t mask = BIT(width) - 1;

    return clock_register_mux_table(dev, name, parent_names, num_parents, flags, reg, shift, mask, clock_mux_flags, NULL, lock);
}

EXPORT_SYMBOL_GPL(clock_register_mux);

struct clock_hw *clock_hw_register_mux(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void __iomem *reg, uint8_t shift,
    uint8_t width, uint8_t clock_mux_flags, spinlock_t *lock)
{
    uint32_t mask = BIT(width) - 1;

    return clock_hw_register_mux_table(dev, name, parent_names, num_parents, flags, reg, shift, mask, clock_mux_flags, NULL, lock);
}

EXPORT_SYMBOL_GPL(clock_hw_register_mux);

void clock_unregister_mux(struct clk *clk)
{
    struct clock_mux *mux;
    struct clock_hw  *hw;

    hw = __clock_get_hw(clk);

    if (!hw) {
        return;
    }

    mux = to_clock_mux(hw);

    clock_unregister(clk);
    kfree(mux);
}

EXPORT_SYMBOL_GPL(clock_unregister_mux);

void clock_hw_unregister_mux(struct clock_hw *hw)
{
    struct clock_mux *mux;

    mux = to_clock_mux(hw);

    clock_hw_unregister(hw);
    kfree(mux);
}

EXPORT_SYMBOL_GPL(clock_hw_unregister_mux);
