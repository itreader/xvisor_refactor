/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-pllv3.c
 *
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file clk-pllv3.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX PLLv3 function helpers
 */

#include <imx-common.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include "clk.h"

#define PLL_NUM_OFFSET   0x10
#define PLL_DENOM_OFFSET 0x20

#define BM_PLL_POWER     (0x1 << 12)
#define BM_PLL_ENABLE    (0x1 << 13)
#define BM_PLL_BYPASS    (0x1 << 16)
#define BM_PLL_LOCK      (0x1 << 31)

/**
 * struct clock_pllv3 - IMX PLL clock version 3
 * @clock_hw:    clock source
 * @base:    base address of PLL registers
 * @powerup_set: set POWER bit to power up the PLL
 * @div_mask:    mask of divider bits
 *
 * IMX PLL clock version 3, found on i.MX6 series.  Divider for pllv3
 * is actually a multiplier, and always sits at bit 0.
 */
struct clock_pllv3 {
    struct clock_hw hw;
    void __iomem   *base;
    bool            powerup_set;
    uint32_t        div_mask;
};

#define to_clock_pllv3(_hw) container_of(_hw, struct clock_pllv3, hw)

static int clock_pllv3_wait_lock(struct clock_pllv3 *pll)
{
    uint64_t timeout = jiffies + msecs_to_jiffies(10);
    uint32_t val     = readl_relaxed(pll->base) & BM_PLL_POWER;

    /* No need to wait for lock when pll is not powered up */
    if ((pll->powerup_set && !val) || (!pll->powerup_set && val)) {
        return 0;
    }

    /* Wait for PLL to lock */
    do {
        if (readl_relaxed(pll->base) & BM_PLL_LOCK) {
            break;
        }

        if (time_after(jiffies, timeout)) {
            break;
        }

        /* usleep_range(50, 500); */
        udelay(250);
    } while (1);

    return readl_relaxed(pll->base) & BM_PLL_LOCK ? 0 : -ETIMEDOUT;
}

static int clock_pllv3_prepare(struct clock_hw *hw)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            val;
    int                 ret;

    val = readl_relaxed(pll->base);

    if (pll->powerup_set) {
        val |= BM_PLL_POWER;
    } else {
        val &= ~BM_PLL_POWER;
    }

    writel_relaxed(val, pll->base);

    ret = clock_pllv3_wait_lock(pll);

    if (ret) {
        return ret;
    }

    val = readl_relaxed(pll->base);
    val &= ~BM_PLL_BYPASS;
    writel_relaxed(val, pll->base);

    return 0;
}

static void clock_pllv3_unprepare(struct clock_hw *hw)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            val;

    val = readl_relaxed(pll->base);
    val |= BM_PLL_BYPASS;

    if (pll->powerup_set) {
        val &= ~BM_PLL_POWER;
    } else {
        val |= BM_PLL_POWER;
    }

    writel_relaxed(val, pll->base);
}

static int clock_pllv3_enable(struct clock_hw *hw)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            val;

    val = readl_relaxed(pll->base);
    val |= BM_PLL_ENABLE;
    writel_relaxed(val, pll->base);

    return 0;
}

static void clock_pllv3_disable(struct clock_hw *hw)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            val;

    val = readl_relaxed(pll->base);
    val &= ~BM_PLL_ENABLE;
    writel_relaxed(val, pll->base);
}

static uint64_t clock_pllv3_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            div = readl_relaxed(pll->base) & pll->div_mask;

    return (div == 1) ? parent_rate * 22 : parent_rate * 20;
}

static long clock_pllv3_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    uint64_t parent_rate = *prate;

    return (rate >= parent_rate * 22) ? parent_rate * 22 : parent_rate * 20;
}

static int clock_pllv3_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            val, div;

    if (rate == parent_rate * 22) {
        div = 1;
    } else if (rate == parent_rate * 20) {
        div = 0;
    } else {
        return -EINVAL;
    }

    val = readl_relaxed(pll->base);
    val &= ~pll->div_mask;
    val |= div;
    writel_relaxed(val, pll->base);

    return clock_pllv3_wait_lock(pll);
}

static const struct clock_ops clock_pllv3_ops = {
    .prepare     = clock_pllv3_prepare,
    .unprepare   = clock_pllv3_unprepare,
    .enable      = clock_pllv3_enable,
    .disable     = clock_pllv3_disable,
    .recalc_rate = clock_pllv3_recalc_rate,
    .round_rate  = clock_pllv3_round_rate,
    .set_rate    = clock_pllv3_set_rate,
};

static uint64_t clock_pllv3_sys_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            div = readl_relaxed(pll->base) & pll->div_mask;

    return do_div(parent_rate * div, 2);
}

static long clock_pllv3_sys_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    uint64_t parent_rate = *prate;
    uint64_t min_rate    = do_div(parent_rate * 54, 2);
    uint64_t max_rate    = do_div(parent_rate * 108, 2);
    uint32_t div;

    if (rate > max_rate) {
        rate = max_rate;
    } else if (rate < min_rate) {
        rate = min_rate;
    }

    div = do_div(rate * 2, parent_rate);

    return do_div(parent_rate * div, 2);
}

static int clock_pllv3_sys_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_pllv3 *pll      = to_clock_pllv3(hw);
    uint64_t            min_rate = parent_rate * 54 / 2;
    uint64_t            max_rate = parent_rate * 108 / 2;
    uint32_t            val, div;

    if (rate < min_rate || rate > max_rate) {
        return -EINVAL;
    }

    div = do_div(rate * 2, parent_rate);
    val = readl_relaxed(pll->base);
    val &= ~pll->div_mask;
    val |= div;
    writel_relaxed(val, pll->base);

    return clock_pllv3_wait_lock(pll);
}

static const struct clock_ops clock_pllv3_sys_ops = {
    .prepare     = clock_pllv3_prepare,
    .unprepare   = clock_pllv3_unprepare,
    .enable      = clock_pllv3_enable,
    .disable     = clock_pllv3_disable,
    .recalc_rate = clock_pllv3_sys_recalc_rate,
    .round_rate  = clock_pllv3_sys_round_rate,
    .set_rate    = clock_pllv3_sys_set_rate,
};

static uint64_t clock_pllv3_av_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_pllv3 *pll = to_clock_pllv3(hw);
    uint32_t            mfn = readl_relaxed(pll->base + PLL_NUM_OFFSET);
    uint32_t            mfd = readl_relaxed(pll->base + PLL_DENOM_OFFSET);
    uint32_t            div = readl_relaxed(pll->base) & pll->div_mask;

    return parent_rate * div + (do_div(parent_rate, mfd) * mfn);
}

static long clock_pllv3_av_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    uint64_t parent_rate = *prate;
    uint64_t min_rate    = parent_rate * 27;
    uint64_t max_rate    = parent_rate * 54;
    uint32_t div;
    uint32_t mfn, mfd = 1000000;
    int64_t  temp64;

    if (rate > max_rate) {
        rate = max_rate;
    } else if (rate < min_rate) {
        rate = min_rate;
    }

    div    = do_div(rate, parent_rate);
    temp64 = (uint64_t)(rate - div * parent_rate);
    temp64 *= mfd;
    temp64 = do_div(temp64, parent_rate);
    mfn    = temp64;

    return parent_rate * div + do_div(parent_rate, mfd) * mfn;
}

static int clock_pllv3_av_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_pllv3 *pll      = to_clock_pllv3(hw);
    uint64_t            min_rate = parent_rate * 27;
    uint64_t            max_rate = parent_rate * 54;
    uint32_t            val, div;
    uint32_t            mfn, mfd = 1000000;
    int64_t             temp64;

    if (rate < min_rate || rate > max_rate) {
        return -EINVAL;
    }

    div    = do_div(rate, parent_rate);
    temp64 = (uint64_t)(rate - div * parent_rate);
    temp64 *= mfd;
    temp64 = do_div(temp64, parent_rate);
    mfn    = temp64;

    val    = readl_relaxed(pll->base);
    val &= ~pll->div_mask;
    val |= div;
    writel_relaxed(val, pll->base);
    writel_relaxed(mfn, pll->base + PLL_NUM_OFFSET);
    writel_relaxed(mfd, pll->base + PLL_DENOM_OFFSET);

    return clock_pllv3_wait_lock(pll);
}

static const struct clock_ops clock_pllv3_av_ops = {
    .prepare     = clock_pllv3_prepare,
    .unprepare   = clock_pllv3_unprepare,
    .enable      = clock_pllv3_enable,
    .disable     = clock_pllv3_disable,
    .recalc_rate = clock_pllv3_av_recalc_rate,
    .round_rate  = clock_pllv3_av_round_rate,
    .set_rate    = clock_pllv3_av_set_rate,
};

static uint64_t clock_pllv3_enet_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    return 500000000;
}

static const struct clock_ops clock_pllv3_enet_ops = {
    .prepare     = clock_pllv3_prepare,
    .unprepare   = clock_pllv3_unprepare,
    .enable      = clock_pllv3_enable,
    .disable     = clock_pllv3_disable,
    .recalc_rate = clock_pllv3_enet_recalc_rate,
};

struct clk *imx_clock_pllv3(enum imx_pllv3_type type, const char *name, const char *parent_name, void __iomem *base, uint32_t div_mask)
{
    struct clock_pllv3     *pll;
    const struct clock_ops *ops;
    struct clk             *clk;
    struct clock_init_data  init;

    pll = kzalloc(sizeof(*pll), GFP_KERNEL);

    if (!pll) {
        return ERR_PTR(-ENOMEM);
    }

    switch (type) {
        case IMX_PLLV3_SYS:
            ops = &clock_pllv3_sys_ops;
            break;

        case IMX_PLLV3_USB:
            ops              = &clock_pllv3_ops;
            pll->powerup_set = true;
            break;

        case IMX_PLLV3_AV:
            ops = &clock_pllv3_av_ops;
            break;

        case IMX_PLLV3_ENET:
            ops = &clock_pllv3_enet_ops;
            break;

        default:
            ops = &clock_pllv3_ops;
    }

    pll->base         = base;
    pll->div_mask     = div_mask;

    init.name         = name;
    init.ops          = ops;
    init.flags        = 0;
    init.parent_names = &parent_name;
    init.num_parents  = 1;

    pll->hw.init      = &init;

    clk               = clock_register(NULL, &pll->hw);

    if (IS_ERR(clk)) {
        kfree(pll);
    }

    return clk;
}
