/*
 * Copyright (C) 2015 Jean-Christophe Dubois.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 4.0.0 arch/arm/mach-imx/clk-pllv1.c
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file clk-pllv1.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Freescale i.MX PLLv1 function helpers
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "clk.h"
#if 0
#include "common.h"
#include "hardware.h"
#else
#include "imx-hardware.h"
#endif

/**
 * pll v1
 *
 * @clock_hw    clock source
 * @parent  the parent clock name
 * @base    base address of pll registers
 *
 * PLL clock version 1, found on i.MX1/21/25/27/31/35
 */

#define MFN_BITS (10)
#define MFN_SIGN (BIT(MFN_BITS - 1))
#define MFN_MASK (MFN_SIGN - 1)

struct clock_pllv1 {
    struct clock_hw hw;
    void __iomem   *base;
};

#define to_clock_pllv1(clk) (container_of(clk, struct clock_pllv1, clk))

static inline bool mfn_is_negative(uint32_t mfn)
{
    return !cpu_is_mx1() && !cpu_is_mx21() && (mfn & MFN_SIGN);
}

static uint64_t clock_pllv1_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_pllv1 *pll = to_clock_pllv1(hw);
    long long           ll;
    int                 mfn_abs;
    uint32_t            mfi, mfn, mfd, pd;
    uint32_t            reg;
    uint64_t            rate;

    reg     = readl(pll->base);

    /*
     * Get the resulting clock rate from a PLL register value and the input
     * frequency. PLLs with this register layout can be found on i.MX1,
     * i.MX21, i.MX27 and i,MX31
     *
     *                  mfi + mfn / (mfd + 1)
     *  f = 2 * f_ref * --------------------
     *                        pd + 1
     */

    mfi     = (reg >> 10) & 0xf;
    mfn     = reg & 0x3ff;
    mfd     = (reg >> 16) & 0x3ff;
    pd      = (reg >> 26) & 0xf;

    mfi     = mfi <= 5 ? 5 : mfi;

    mfn_abs = mfn;

    /*
     * On all i.MXs except i.MX1 and i.MX21 mfn is a 10bit
     * 2's complements number.
     * On i.MX27 the bit 9 is the sign bit.
     */
    if (mfn_is_negative(mfn)) {
        if (cpu_is_mx27()) {
            mfn_abs = mfn & MFN_MASK;
        } else {
            mfn_abs = BIT(MFN_BITS) - mfn;
        }
    }

    rate = parent_rate * 2;
#if 0
    rate /= pd + 1;
#else
    rate = udiv32(rate, pd + 1);
#endif

    ll = (uint64_t)rate * mfn_abs;

#if 0
    do_div(ll, mfd + 1);
#else
    ll = udiv64(ll, mfd + 1);
#endif

    if (mfn_is_negative(mfn)) {
        ll = -ll;
    }

    ll = (rate * mfi) + ll;

    return ll;
}

static struct clock_ops clock_pllv1_ops = {
    .recalc_rate = clock_pllv1_recalc_rate,
};

struct clk *imx_clock_pllv1(const char *name, const char *parent, void __iomem *base)
{
    struct clock_pllv1    *pll;
    struct clk            *clk;
    struct clock_init_data init;

    pll = kmalloc(sizeof(*pll), GFP_KERNEL);

    if (!pll) {
        return ERR_PTR(-ENOMEM);
    }

    pll->base         = base;

    init.name         = name;
    init.ops          = &clock_pllv1_ops;
    init.flags        = 0;
    init.parent_names = &parent;
    init.num_parents  = 1;

    pll->hw.init      = &init;

    clk               = clock_register(NULL, &pll->hw);

    if (IS_ERR(clk)) {
        kfree(pll);
    }

    return clk;
}
