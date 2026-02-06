/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-pfd.c
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
 * @file clk-pfd.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX "pfd" clock function helpers
 */

#include <imx-common.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include "clk.h"

/**
 * struct clock_pfd - IMX PFD clock
 * @clock_hw:   clock source
 * @reg:    PFD register address
 * @idx:    the index of PFD encoded in the register
 *
 * PFD clock found on i.MX6 series.  Each register for PFD has 4 clock_pfd
 * data encoded, and member idx is used to specify the one.  And each
 * register has SET, CLR and TOG registers at offset 0x4 0x8 and 0xc.
 */
struct clock_pfd {
    struct clock_hw hw;
    void __iomem   *reg;
    uint8_t         idx;
};

#define to_clock_pfd(_hw) container_of(_hw, struct clock_pfd, hw)

#define SET               0x4
#define CLR               0x8
#define OTG               0xc

static int clock_pfd_enable(struct clock_hw *hw)
{
    struct clock_pfd *pfd = to_clock_pfd(hw);

    writel_relaxed(1 << ((pfd->idx + 1) * 8 - 1), pfd->reg + CLR);

    return 0;
}

static void clock_pfd_disable(struct clock_hw *hw)
{
    struct clock_pfd *pfd = to_clock_pfd(hw);

    writel_relaxed(1 << ((pfd->idx + 1) * 8 - 1), pfd->reg + SET);
}

static uint64_t clock_pfd_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_pfd *pfd  = to_clock_pfd(hw);
    uint64_t          tmp  = parent_rate;
    uint8_t           frac = (readl_relaxed(pfd->reg) >> (pfd->idx * 8)) & 0x3f;

    tmp *= 18;
    do_div(tmp, frac);

    return tmp;
}

static long clock_pfd_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    uint64_t tmp = *prate;
    uint8_t  frac;

    tmp = tmp * 18 + rate / 2;
    do_div(tmp, rate);
    frac = tmp;

    if (frac < 12) {
        frac = 12;
    } else if (frac > 35) {
        frac = 35;
    }

    tmp = *prate;
    tmp *= 18;
    do_div(tmp, frac);

    return tmp;
}

static int clock_pfd_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_pfd *pfd = to_clock_pfd(hw);
    uint64_t          tmp = parent_rate;
    uint8_t           frac;

    tmp = tmp * 18 + rate / 2;
    do_div(tmp, rate);
    frac = tmp;

    if (frac < 12) {
        frac = 12;
    } else if (frac > 35) {
        frac = 35;
    }

    writel_relaxed(0x3f << (pfd->idx * 8), pfd->reg + CLR);
    writel_relaxed(frac << (pfd->idx * 8), pfd->reg + SET);

    return 0;
}

static const struct clock_ops clock_pfd_ops = {
    .enable      = clock_pfd_enable,
    .disable     = clock_pfd_disable,
    .recalc_rate = clock_pfd_recalc_rate,
    .round_rate  = clock_pfd_round_rate,
    .set_rate    = clock_pfd_set_rate,
};

struct clk *imx_clock_pfd(const char *name, const char *parent_name, void __iomem *reg, uint8_t idx)
{
    struct clock_pfd      *pfd;
    struct clk            *clk;
    struct clock_init_data init;

    pfd = kzalloc(sizeof(*pfd), GFP_KERNEL);

    if (!pfd) {
        return ERR_PTR(-ENOMEM);
    }

    pfd->reg          = reg;
    pfd->idx          = idx;

    init.name         = name;
    init.ops          = &clock_pfd_ops;
    init.flags        = 0;
    init.parent_names = &parent_name;
    init.num_parents  = 1;

    pfd->hw.init      = &init;

    clk               = clock_register(NULL, &pfd->hw);

    if (IS_ERR(clk)) {
        kfree(pfd);
    }

    return clk;
}
