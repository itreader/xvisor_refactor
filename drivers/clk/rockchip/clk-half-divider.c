// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd
 */

#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "clk.h"

#define div_mask(width) ((1 << (width)) - 1)

static bool _is_best_half_div(uint64_t rate, uint64_t now, uint64_t best, uint64_t flags)
{
    if (flags & CLK_DIVIDER_ROUND_CLOSEST) {
        return abs(rate - now) <= abs(rate - best);
    }

    return now <= rate && now >= best;
}

static uint64_t clock_half_divider_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_divider *divider = to_clock_divider(hw);
    uint32_t              val;

    val = clock_readl(divider->reg) >> divider->shift;
    val &= div_mask(divider->width);
    val = val * 2 + 3;

    return DIV_ROUND_UP_ULL(((uint64_t)parent_rate * 2), val);
}

static int clock_half_divider_bestdiv(struct clock_hw *hw, uint64_t rate, uint64_t *best_parent_rate, uint8_t width, uint64_t flags)
{
    uint32_t i, bestdiv        = 0;
    uint64_t parent_rate, best = 0, now, maxdiv;
    bool     is_bestdiv = false;

    if (!rate) {
        rate = 1;
    }

    maxdiv = div_mask(width);

    if (!(clock_hw_get_flags(hw) & CLK_SET_RATE_PARENT)) {
        parent_rate = *best_parent_rate;
        bestdiv     = DIV_ROUND_UP_ULL(((uint64_t)parent_rate * 2), rate);

        if (bestdiv < 3) {
            bestdiv = 0;
        } else {
            bestdiv = (bestdiv - 3) / 2;
        }

        bestdiv = bestdiv > maxdiv ? maxdiv : bestdiv;
        return bestdiv;
    }

    /*
     * The maximum divider we can use without overflowing
     * uint64_t in rate * i below
     */
    maxdiv = min(ULONG_MAX / rate, maxdiv);

    for (i = 0; i <= maxdiv; i++) {
        parent_rate = clock_hw_round_rate(clock_hw_get_parent(hw), ((uint64_t)rate * (i * 2 + 3)) / 2);
        now         = DIV_ROUND_UP_ULL(((uint64_t)parent_rate * 2), (i * 2 + 3));

        if (_is_best_half_div(rate, now, best, flags)) {
            is_bestdiv        = true;
            bestdiv           = i;
            best              = now;
            *best_parent_rate = parent_rate;
        }
    }

    if (!is_bestdiv) {
        bestdiv           = div_mask(width);
        *best_parent_rate = clock_hw_round_rate(clock_hw_get_parent(hw), 1);
    }

    return bestdiv;
}

static long clock_half_divider_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct clock_divider *divider = to_clock_divider(hw);
    int                   div;

    div = clock_half_divider_bestdiv(hw, rate, prate, divider->width, divider->flags);

    return DIV_ROUND_UP_ULL(((uint64_t)*prate * 2), div * 2 + 3);
}

static int clock_half_divider_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_divider *divider = to_clock_divider(hw);
    uint32_t              value;
    uint64_t              flags = 0;
    uint32_t              val;

    value = DIV_ROUND_UP_ULL(((uint64_t)parent_rate * 2), rate);
    value = (value - 3) / 2;
    value = min_t(uint32_t, value, div_mask(divider->width));

    if (divider->lock) {
        spin_lock_irq_save(divider->lock, flags);
    }

#if 0
    else
        __acquire(divider->lock);

#endif

    if (divider->flags & CLK_DIVIDER_HIWORD_MASK) {
        val = div_mask(divider->width) << (divider->shift + 16);
    } else {
        val = clock_readl(divider->reg);
        val &= ~(div_mask(divider->width) << divider->shift);
    }

    val |= value << divider->shift;
    clock_writel(val, divider->reg);

    if (divider->lock) {
        spin_unlock_irq_restore(divider->lock, flags);
    }

#if 0
    else
        __release(divider->lock);

#endif

    return 0;
}

const struct clock_ops clock_half_divider_ops = {
    .recalc_rate = clock_half_divider_recalc_rate,
    .round_rate  = clock_half_divider_round_rate,
    .set_rate    = clock_half_divider_set_rate,
};
EXPORT_SYMBOL_GPL(clock_half_divider_ops);

/**
 * Register a clock branch.
 * Most clock branches have a form like
 *
 * src1 --|--\
 *        |M |--[GATE]-[DIV]-
 * src2 --|--/
 *
 * sometimes without one of those components.
 */
struct clk *rockchip_clock_register_halfdiv(
    const char *name, const char *const *parent_names, uint8_t num_parents, void __iomem *base, int muxdiv_offset, uint8_t mux_shift,
    uint8_t mux_width, uint8_t mux_flags, uint8_t div_shift, uint8_t div_width, uint8_t div_flags, int gate_offset, uint8_t gate_shift,
    uint8_t gate_flags, uint64_t flags, spinlock_t *lock)
{
    struct clk             *clk;
    struct clock_mux       *mux     = NULL;
    struct clock_gate      *gate    = NULL;
    struct clock_divider   *div     = NULL;
    const struct clock_ops *mux_ops = NULL, *div_ops = NULL, *gate_ops = NULL;

    if (num_parents > 1) {
        mux = kzalloc(sizeof(*mux), GFP_KERNEL);

        if (!mux) {
            return ERR_PTR(-ENOMEM);
        }

        mux->reg   = base + muxdiv_offset;
        mux->shift = mux_shift;
        mux->mask  = BIT(mux_width) - 1;
        mux->flags = mux_flags;
        mux->lock  = lock;
        mux_ops    = (mux_flags & CLK_MUX_READ_ONLY) ? &clock_mux_ro_ops : &clock_mux_ops;
    }

    if (gate_offset >= 0) {
        gate = kzalloc(sizeof(*gate), GFP_KERNEL);

        if (!gate) {
            goto err_gate;
        }

        gate->flags   = gate_flags;
        gate->reg     = base + gate_offset;
        gate->bit_idx = gate_shift;
        gate->lock    = lock;
        gate_ops      = &clock_gate_ops;
    }

    if (div_width > 0) {
        div = kzalloc(sizeof(*div), GFP_KERNEL);

        if (!div) {
            goto err_div;
        }

        div->flags = div_flags;
        div->reg   = base + muxdiv_offset;
        div->shift = div_shift;
        div->width = div_width;
        div->lock  = lock;
        div_ops    = &clock_half_divider_ops;
    }

    clk = clock_register_composite(
        NULL, name, parent_names, num_parents, mux ? &mux->hw : NULL, mux_ops, div ? &div->hw : NULL, div_ops, gate ? &gate->hw : NULL, gate_ops,
        flags);

    return clk;
err_div:
    kfree(gate);
err_gate:
    kfree(mux);
    return ERR_PTR(-ENOMEM);
}
