/*
 * Copyright (C) 2011 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 * Copyright (C) 2011 Richard Zhao, Linaro <richard.zhao@linaro.org>
 * Copyright (C) 2011-2012 Mike Turquette, Linaro Ltd <mturquette@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable divider clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

/*
 * DOC: basic adjustable divider clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clock_prepare only ensures that parents are prepared
 * enable - clock_enable only ensures that parents are enabled
 * rate - rate is adjustable.  clk->rate = ceiling(parent->rate / divisor)
 * parent - fixed parent.  No clock_set_parent support
 */

#define div_mask(width) ((1 << (width)) - 1)

static uint32_t _get_table_maxdiv(const struct clock_div_table *table, uint8_t width)
{
    uint32_t                      maxdiv = 0, mask = div_mask(width);
    const struct clock_div_table *clkt;

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->div > maxdiv && clkt->val <= mask) {
            maxdiv = clkt->div;
        }
    }

    return maxdiv;
}

static uint32_t _get_table_mindiv(const struct clock_div_table *table)
{
    uint32_t                      mindiv = UINT_MAX;
    const struct clock_div_table *clkt;

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->div < mindiv) {
            mindiv = clkt->div;
        }
    }

    return mindiv;
}

static uint32_t _get_maxdiv(const struct clock_div_table *table, uint8_t width, uint64_t flags)
{
    if (flags & CLK_DIVIDER_ONE_BASED) {
        return div_mask(width);
    }

    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        return 1 << div_mask(width);
    }

    if (table) {
        return _get_table_maxdiv(table, width);
    }

    return div_mask(width) + 1;
}

static uint32_t _get_table_div(const struct clock_div_table *table, uint32_t val)
{
    const struct clock_div_table *clkt;

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->val == val) {
            return clkt->div;
        }
    }

    return 0;
}

static uint32_t _get_div(const struct clock_div_table *table, uint32_t val, uint64_t flags, uint8_t width)
{
    if (flags & CLK_DIVIDER_ONE_BASED) {
        return val;
    }

    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        return 1 << val;
    }

    if (flags & CLK_DIVIDER_MAX_AT_ZERO) {
        return val ? val : div_mask(width) + 1;
    }

    if (table) {
        return _get_table_div(table, val);
    }

    return val + 1;
}

static uint32_t _get_table_val(const struct clock_div_table *table, uint32_t div)
{
    const struct clock_div_table *clkt;

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->div == div) {
            return clkt->val;
        }
    }

    return 0;
}

static uint32_t _get_val(const struct clock_div_table *table, uint32_t div, uint64_t flags, uint8_t width)
{
    if (flags & CLK_DIVIDER_ONE_BASED) {
        return div;
    }

    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        return __ffs(div);
    }

    if (flags & CLK_DIVIDER_MAX_AT_ZERO) {
        return (div == div_mask(width) + 1) ? 0 : div;
    }

    if (table) {
        return _get_table_val(table, div);
    }

    return div - 1;
}

uint64_t divider_recalc_rate(struct clock_hw *hw, uint64_t parent_rate, uint32_t val, const struct clock_div_table *table, uint64_t flags)
{
    struct clock_divider *divider = to_clock_divider(hw);
    uint32_t              div;

    div = _get_div(table, val, flags, divider->width);

    if (!div) {
        WARN(!(flags & CLK_DIVIDER_ALLOW_ZERO), "%s: Zero divisor and CLK_DIVIDER_ALLOW_ZERO not set\n", clock_hw_get_name(hw));
        return parent_rate;
    }

    return DIV_ROUND_UP_ULL((uint64_t)parent_rate, div);
}

EXPORT_SYMBOL_GPL(divider_recalc_rate);

static uint64_t clock_divider_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_divider *divider = to_clock_divider(hw);
    uint32_t              val;

    val = clock_readl(divider->reg) >> divider->shift;
    val &= div_mask(divider->width);

    return divider_recalc_rate(hw, parent_rate, val, divider->table, divider->flags);
}

static bool _is_valid_table_div(const struct clock_div_table *table, uint32_t div)
{
    const struct clock_div_table *clkt;

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->div == div) {
            return true;
        }
    }

    return false;
}

static bool _is_valid_div(const struct clock_div_table *table, uint32_t div, uint64_t flags)
{
    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        return is_power_of_2(div);
    }

    if (table) {
        return _is_valid_table_div(table, div);
    }

    return true;
}

static int _round_up_table(const struct clock_div_table *table, int div)
{
    const struct clock_div_table *clkt;
    int                           up = INT_MAX;

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->div == div) {
            return clkt->div;
        } else if (clkt->div < div) {
            continue;
        }

        if ((clkt->div - div) < (up - div)) {
            up = clkt->div;
        }
    }

    return up;
}

static int _round_down_table(const struct clock_div_table *table, int div)
{
    const struct clock_div_table *clkt;
    int                           down = _get_table_mindiv(table);

    for (clkt = table; clkt->div; clkt++) {
        if (clkt->div == div) {
            return clkt->div;
        } else if (clkt->div > div) {
            continue;
        }

        if ((div - clkt->div) < (div - down)) {
            down = clkt->div;
        }
    }

    return down;
}

static int _div_round_up(const struct clock_div_table *table, uint64_t parent_rate, uint64_t rate, uint64_t flags)
{
    int div = DIV_ROUND_UP_ULL((uint64_t)parent_rate, rate);

    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        div = __roundup_pow_of_two(div);
    }

    if (table) {
        div = _round_up_table(table, div);
    }

    return div;
}

static int _div_round_closest(const struct clock_div_table *table, uint64_t parent_rate, uint64_t rate, uint64_t flags)
{
    int      up, down;
    uint64_t up_rate, down_rate;

    up   = DIV_ROUND_UP_ULL((uint64_t)parent_rate, rate);
    down = udiv64(parent_rate, rate);

    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        up   = __roundup_pow_of_two(up);
        down = __rounddown_pow_of_two(down);
    } else if (table) {
        up   = _round_up_table(table, up);
        down = _round_down_table(table, down);
    }

    up_rate   = DIV_ROUND_UP_ULL((uint64_t)parent_rate, up);
    down_rate = DIV_ROUND_UP_ULL((uint64_t)parent_rate, down);

    return (rate - up_rate) <= (down_rate - rate) ? up : down;
}

static int _div_round(const struct clock_div_table *table, uint64_t parent_rate, uint64_t rate, uint64_t flags)
{
    if (flags & CLK_DIVIDER_ROUND_CLOSEST) {
        return _div_round_closest(table, parent_rate, rate, flags);
    }

    return _div_round_up(table, parent_rate, rate, flags);
}

static bool _is_best_div(uint64_t rate, uint64_t now, uint64_t best, uint64_t flags)
{
    if (flags & CLK_DIVIDER_ROUND_CLOSEST) {
        return abs(rate - now) < abs(rate - best);
    }

    return now <= rate && now > best;
}

static int _next_div(const struct clock_div_table *table, int div, uint64_t flags)
{
    div++;

    if (flags & CLK_DIVIDER_POWER_OF_TWO) {
        return __roundup_pow_of_two(div);
    }

    if (table) {
        return _round_up_table(table, div);
    }

    return div;
}

static int clock_divider_bestdiv(
    struct clock_hw *hw, struct clock_hw *parent, uint64_t rate, uint64_t *best_parent_rate, const struct clock_div_table *table, uint8_t width,
    uint64_t flags)
{
    int      i, bestdiv        = 0;
    uint64_t parent_rate, best = 0, now, maxdiv;
    uint64_t parent_rate_saved = *best_parent_rate;

    if (!rate) {
        rate = 1;
    }

    maxdiv = _get_maxdiv(table, width, flags);

    if (!(clock_hw_get_flags(hw) & CLK_SET_RATE_PARENT)) {
        parent_rate = *best_parent_rate;
        bestdiv     = _div_round(table, parent_rate, rate, flags);
        bestdiv     = bestdiv == 0 ? 1 : bestdiv;
        bestdiv     = bestdiv > maxdiv ? maxdiv : bestdiv;
        return bestdiv;
    }

    /*
     * The maximum divider we can use without overflowing
     * uint64_t in rate * i below
     */
    maxdiv = min(udiv64(ULONG_MAX, rate), (uint64_t)maxdiv);

    for (i = _next_div(table, 0, flags); i <= maxdiv; i = _next_div(table, i, flags)) {
        if (rate * i == parent_rate_saved) {
            /*
             * It's the most ideal case if the requested rate can be
             * divided from parent clock without needing to change
             * parent rate, so return the divider immediately.
             */
            *best_parent_rate = parent_rate_saved;
            return i;
        }

        parent_rate = clock_hw_round_rate(parent, rate * i);
        now         = DIV_ROUND_UP_ULL((uint64_t)parent_rate, i);

        if (_is_best_div(rate, now, best, flags)) {
            bestdiv           = i;
            best              = now;
            *best_parent_rate = parent_rate;
        }
    }

    if (!bestdiv) {
        bestdiv           = _get_maxdiv(table, width, flags);
        *best_parent_rate = clock_hw_round_rate(parent, 1);
    }

    return bestdiv;
}

long divider_round_rate_parent(
    struct clock_hw *hw, struct clock_hw *parent, uint64_t rate, uint64_t *prate, const struct clock_div_table *table, uint8_t width, uint64_t flags)
{
    int div;

    div = clock_divider_bestdiv(hw, parent, rate, prate, table, width, flags);

    return DIV_ROUND_UP_ULL((uint64_t)*prate, div);
}

EXPORT_SYMBOL_GPL(divider_round_rate_parent);

static long clock_divider_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct clock_divider *divider = to_clock_divider(hw);
    int                   bestdiv;

    /* if read only, just return current value */
    if (divider->flags & CLK_DIVIDER_READ_ONLY) {
        bestdiv = clock_readl(divider->reg) >> divider->shift;
        bestdiv &= div_mask(divider->width);
        bestdiv = _get_div(divider->table, bestdiv, divider->flags, divider->width);
        return DIV_ROUND_UP_ULL((uint64_t)*prate, bestdiv);
    }

    return divider_round_rate(hw, rate, prate, divider->table, divider->width, divider->flags);
}

int divider_get_val(uint64_t rate, uint64_t parent_rate, const struct clock_div_table *table, uint8_t width, uint64_t flags)
{
    uint32_t div, value;

    div = DIV_ROUND_UP_ULL((uint64_t)parent_rate, rate);

    if (!_is_valid_div(table, div, flags)) {
        return -EINVAL;
    }

    value = _get_val(table, div, flags, width);

    return min_t(uint32_t, value, div_mask(width));
}

EXPORT_SYMBOL_GPL(divider_get_val);

static int clock_divider_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_divider *divider = to_clock_divider(hw);
    int                   value;
    uint64_t              flags = 0;
    uint32_t              val;

    value = divider_get_val(rate, parent_rate, divider->table, divider->width, divider->flags);

    if (value < 0) {
        return value;
    }

    if (divider->lock) {
        spin_lock_irq_save(divider->lock, flags);
    }

    if (divider->flags & CLK_DIVIDER_HIWORD_MASK) {
        val = div_mask(divider->width) << (divider->shift + 16);
    } else {
        val = clock_readl(divider->reg);
        val &= ~(div_mask(divider->width) << divider->shift);
    }

    val |= (uint32_t)value << divider->shift;
    clock_writel(val, divider->reg);

    if (divider->lock) {
        spin_unlock_irq_restore(divider->lock, flags);
    }

    return 0;
}

const struct clock_ops clock_divider_ops = {
    .recalc_rate = clock_divider_recalc_rate,
    .round_rate  = clock_divider_round_rate,
    .set_rate    = clock_divider_set_rate,
};
EXPORT_SYMBOL_GPL(clock_divider_ops);

const struct clock_ops clock_divider_ro_ops = {
    .recalc_rate = clock_divider_recalc_rate,
    .round_rate  = clock_divider_round_rate,
};
EXPORT_SYMBOL_GPL(clock_divider_ro_ops);

static struct clock_hw *_register_divider(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, const struct clock_div_table *table, spinlock_t *lock)
{
    struct clock_divider  *div;
    struct clock_hw       *hw;
    struct clock_init_data init;
    int                    ret;

    if (clock_divider_flags & CLK_DIVIDER_HIWORD_MASK) {
        if (width + shift > 16) {
            pr_warn("divider value exceeds LOWORD field\n");
            return ERR_PTR(-EINVAL);
        }
    }

    /* allocate the divider */
    div = kzalloc(sizeof(*div), GFP_KERNEL);

    if (!div) {
        return ERR_PTR(-ENOMEM);
    }

    init.name = name;

    if (clock_divider_flags & CLK_DIVIDER_READ_ONLY) {
        init.ops = &clock_divider_ro_ops;
    } else {
        init.ops = &clock_divider_ops;
    }

    init.flags        = flags | CLK_IS_BASIC;
    init.parent_names = (parent_name ? &parent_name : NULL);
    init.num_parents  = (parent_name ? 1 : 0);

    /* struct clock_divider assignments */
    div->reg          = reg;
    div->shift        = shift;
    div->width        = width;
    div->flags        = clock_divider_flags;
    div->lock         = lock;
    div->hw.init      = &init;
    div->table        = table;

    /* register the clock */
    hw                = &div->hw;
    ret               = clock_hw_register(dev, hw);

    if (ret) {
        kfree(div);
        hw = ERR_PTR(ret);
    }

    return hw;
}

/**
 * clock_register_divider - register a divider clock with the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust divider
 * @shift: number of bits to shift the bitfield
 * @width: width of the bitfield
 * @clock_divider_flags: divider-specific flags for this clock
 * @lock: shared register lock for this clock
 */
struct clk *clock_register_divider(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, spinlock_t *lock)
{
    struct clock_hw *hw;

    hw = _register_divider(dev, name, parent_name, flags, reg, shift, width, clock_divider_flags, NULL, lock);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_divider);

/**
 * clock_hw_register_divider - register a divider clock with the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust divider
 * @shift: number of bits to shift the bitfield
 * @width: width of the bitfield
 * @clock_divider_flags: divider-specific flags for this clock
 * @lock: shared register lock for this clock
 */
struct clock_hw *clock_hw_register_divider(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, spinlock_t *lock)
{
    return _register_divider(dev, name, parent_name, flags, reg, shift, width, clock_divider_flags, NULL, lock);
}

EXPORT_SYMBOL_GPL(clock_hw_register_divider);

/**
 * clock_register_divider_table - register a table based divider clock with
 * the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust divider
 * @shift: number of bits to shift the bitfield
 * @width: width of the bitfield
 * @clock_divider_flags: divider-specific flags for this clock
 * @table: array of divider/value pairs ending with a div set to 0
 * @lock: shared register lock for this clock
 */
struct clk *clock_register_divider_table(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, const struct clock_div_table *table, spinlock_t *lock)
{
    struct clock_hw *hw;

    hw = _register_divider(dev, name, parent_name, flags, reg, shift, width, clock_divider_flags, table, lock);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_divider_table);

/**
 * clock_hw_register_divider_table - register a table based divider clock with
 * the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust divider
 * @shift: number of bits to shift the bitfield
 * @width: width of the bitfield
 * @clock_divider_flags: divider-specific flags for this clock
 * @table: array of divider/value pairs ending with a div set to 0
 * @lock: shared register lock for this clock
 */
struct clock_hw *clock_hw_register_divider_table(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, const struct clock_div_table *table, spinlock_t *lock)
{
    return _register_divider(dev, name, parent_name, flags, reg, shift, width, clock_divider_flags, table, lock);
}

EXPORT_SYMBOL_GPL(clock_hw_register_divider_table);

void clock_unregister_divider(struct clk *clk)
{
    struct clock_divider *div;
    struct clock_hw      *hw;

    hw = __clock_get_hw(clk);

    if (!hw) {
        return;
    }

    div = to_clock_divider(hw);

    clock_unregister(clk);
    kfree(div);
}

EXPORT_SYMBOL_GPL(clock_unregister_divider);

/**
 * clock_hw_unregister_divider - unregister a clk divider
 * @hw: hardware-specific clock data to unregister
 */
void clock_hw_unregister_divider(struct clock_hw *hw)
{
    struct clock_divider *div;

    div = to_clock_divider(hw);

    clock_hw_unregister(hw);
    kfree(div);
}

EXPORT_SYMBOL_GPL(clock_hw_unregister_divider);
