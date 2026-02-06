/*
 * Copyright (C) 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable fractional divider clock implementation.
 * Output rate = (m / n) * parent_rate.
 * Uses rational best approximation algorithm.
 */

#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/rational.h>
#include <linux/slab.h>

static uint64_t clock_fd_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_fractional_divider *fd    = to_clock_fd(hw);
    uint64_t                         flags = 0;
    uint64_t                         m, n;
    uint32_t                         val;
    uint64_t                         ret;

    if (fd->lock) {
        spin_lock_irq_save(fd->lock, flags);
    }

#if 0
    else
        __acquire(fd->lock);

#endif

    val = clock_readl(fd->reg);

    if (fd->lock) {
        spin_unlock_irq_restore(fd->lock, flags);
    }

#if 0
    else
        __release(fd->lock);

#endif

    m = (val & fd->mmask) >> fd->mshift;
    n = (val & fd->nmask) >> fd->nshift;

    if (!n || !m) {
        return parent_rate;
    }

    ret = (uint64_t)parent_rate * m;
    ret = udiv64(ret, n);

    return ret;
}

static void clock_fd_general_approximation(struct clock_hw *hw, uint64_t rate, uint64_t *parent_rate, uint64_t *m, uint64_t *n)
{
    struct clock_fractional_divider *fd = to_clock_fd(hw);
    uint64_t                         scale;

    /*
     * Get rate closer to *parent_rate to guarantee there is no overflow
     * for m and n. In the result it will be the nearest rate left shifted
     * by (scale - fd->nwidth) bits.
     */
    scale = fls_long((uint64_t)udiv64(*parent_rate, rate) - 1);

    if (scale > fd->nwidth) {
        rate <<= scale - fd->nwidth;
    }

    rational_best_approximation(rate, *parent_rate, GENMASK(fd->mwidth - 1, 0), GENMASK(fd->nwidth - 1, 0), m, n);
}

static long clock_fd_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *parent_rate)
{
    struct clock_fractional_divider *fd = to_clock_fd(hw);
    uint64_t                         m, n;
    uint64_t                         ret;

    if (!rate || rate >= *parent_rate) {
        return *parent_rate;
    }

    if (fd->approximation) {
        fd->approximation(hw, rate, parent_rate, &m, &n);
    } else {
        clock_fd_general_approximation(hw, rate, parent_rate, &m, &n);
    }

    ret = (uint64_t)*parent_rate * m;
    ret = udiv64(ret, n);

    return ret;
}

static int clock_fd_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_fractional_divider *fd    = to_clock_fd(hw);
    uint64_t                         flags = 0;
    uint64_t                         m, n;
    uint32_t                         val;

    rational_best_approximation(rate, parent_rate, GENMASK(fd->mwidth - 1, 0), GENMASK(fd->nwidth - 1, 0), &m, &n);

    if (fd->lock) {
        spin_lock_irq_save(fd->lock, flags);
    }

#if 0
    else
        __acquire(fd->lock);

#endif

    val = clock_readl(fd->reg);
    val &= ~(fd->mmask | fd->nmask);
    val |= (m << fd->mshift) | (n << fd->nshift);
    clock_writel(val, fd->reg);

    if (fd->lock) {
        spin_unlock_irq_restore(fd->lock, flags);
    }

#if 0
    else
        __release(fd->lock);

#endif

    return 0;
}

const struct clock_ops clock_fractional_divider_ops = {
    .recalc_rate = clock_fd_recalc_rate,
    .round_rate  = clock_fd_round_rate,
    .set_rate    = clock_fd_set_rate,
};
EXPORT_SYMBOL_GPL(clock_fractional_divider_ops);

struct clock_hw *clock_hw_register_fractional_divider(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t mshift, uint8_t mwidth, uint8_t nshift,
    uint8_t nwidth, uint8_t clock_divider_flags, spinlock_t *lock)
{
    struct clock_fractional_divider *fd;
    struct clock_init_data           init;
    struct clock_hw                 *hw;
    int                              ret;

    fd = kzalloc(sizeof(*fd), GFP_KERNEL);

    if (!fd) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_fractional_divider_ops;
    init.flags        = flags | CLK_IS_BASIC;
    init.parent_names = parent_name ? &parent_name : NULL;
    init.num_parents  = parent_name ? 1 : 0;

    fd->reg           = reg;
    fd->mshift        = mshift;
    fd->mwidth        = mwidth;
    fd->mmask         = GENMASK(mwidth - 1, 0) << mshift;
    fd->nshift        = nshift;
    fd->nwidth        = nwidth;
    fd->nmask         = GENMASK(nwidth - 1, 0) << nshift;
    fd->flags         = clock_divider_flags;
    fd->lock          = lock;
    fd->hw.init       = &init;

    hw                = &fd->hw;
    ret               = clock_hw_register(dev, hw);

    if (ret) {
        kfree(fd);
        hw = ERR_PTR(ret);
    }

    return hw;
}

EXPORT_SYMBOL_GPL(clock_hw_register_fractional_divider);

struct clk *clock_register_fractional_divider(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t mshift, uint8_t mwidth, uint8_t nshift,
    uint8_t nwidth, uint8_t clock_divider_flags, spinlock_t *lock)
{
    struct clock_hw *hw;

    hw = clock_hw_register_fractional_divider(dev, name, parent_name, flags, reg, mshift, mwidth, nshift, nwidth, clock_divider_flags, lock);

    if (IS_ERR(hw)) {
        return ERR_CAST(hw);
    }

    return hw->clk;
}

EXPORT_SYMBOL_GPL(clock_register_fractional_divider);

void clock_hw_unregister_fractional_divider(struct clock_hw *hw)
{
    struct clock_fractional_divider *fd;

    fd = to_clock_fd(hw);

    clock_hw_unregister(hw);
    kfree(fd);
}
