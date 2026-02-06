/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-busy.c
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
 * @file clk-busy.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX "busy" clock function helpers
 */

#include <imx-common.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include "clk.h"

static int clock_busy_wait(void __iomem *reg, uint8_t shift)
{
    uint64_t timeout = jiffies + msecs_to_jiffies(10);

    while (readl_relaxed(reg) & (1 << shift)) {
        if (time_after(jiffies, timeout)) {
            return -ETIMEDOUT;
        }
    }

    return 0;
}

struct clock_busy_divider {
    struct clock_divider    div;
    const struct clock_ops *div_ops;
    void __iomem           *reg;
    uint8_t                 shift;
};

static inline struct clock_busy_divider *to_clock_busy_divider(struct clock_hw *hw)
{
    struct clock_divider *div = container_of(hw, struct clock_divider, hw);

    return container_of(div, struct clock_busy_divider, div);
}

static uint64_t clock_busy_divider_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_busy_divider *busy = to_clock_busy_divider(hw);

    return busy->div_ops->recalc_rate(&busy->div.hw, parent_rate);
}

static long clock_busy_divider_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct clock_busy_divider *busy = to_clock_busy_divider(hw);

    return busy->div_ops->round_rate(&busy->div.hw, rate, prate);
}

static int clock_busy_divider_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_busy_divider *busy = to_clock_busy_divider(hw);
    int                        ret;

    ret = busy->div_ops->set_rate(&busy->div.hw, rate, parent_rate);

    if (!ret) {
        ret = clock_busy_wait(busy->reg, busy->shift);
    }

    return ret;
}

static struct clock_ops clock_busy_divider_ops = {
    .recalc_rate = clock_busy_divider_recalc_rate,
    .round_rate  = clock_busy_divider_round_rate,
    .set_rate    = clock_busy_divider_set_rate,
};

struct clk *imx_clock_busy_divider(
    const char *name, const char *parent_name, void __iomem *reg, uint8_t shift, uint8_t width, void __iomem *busy_reg, uint8_t busy_shift)
{
    struct clock_busy_divider *busy;
    struct clk                *clk;
    struct clock_init_data     init;

    busy = kzalloc(sizeof(*busy), GFP_KERNEL);

    if (!busy) {
        return ERR_PTR(-ENOMEM);
    }

    busy->reg         = busy_reg;
    busy->shift       = busy_shift;

    busy->div.reg     = reg;
    busy->div.shift   = shift;
    busy->div.width   = width;
    busy->div.lock    = &imx_ccm_lock;
    busy->div_ops     = &clock_divider_ops;

    init.name         = name;
    init.ops          = &clock_busy_divider_ops;
    init.flags        = CLK_SET_RATE_PARENT;
    init.parent_names = &parent_name;
    init.num_parents  = 1;

    busy->div.hw.init = &init;

    clk               = clock_register(NULL, &busy->div.hw);

    if (IS_ERR(clk)) {
        kfree(busy);
    }

    return clk;
}

struct clock_busy_mux {
    struct clock_mux        mux;
    const struct clock_ops *mux_ops;
    void __iomem           *reg;
    uint8_t                 shift;
};

static inline struct clock_busy_mux *to_clock_busy_mux(struct clock_hw *hw)
{
    struct clock_mux *mux = container_of(hw, struct clock_mux, hw);

    return container_of(mux, struct clock_busy_mux, mux);
}

static uint8_t clock_busy_mux_get_parent(struct clock_hw *hw)
{
    struct clock_busy_mux *busy = to_clock_busy_mux(hw);

    return busy->mux_ops->get_parent(&busy->mux.hw);
}

static int clock_busy_mux_set_parent(struct clock_hw *hw, uint8_t index)
{
    struct clock_busy_mux *busy = to_clock_busy_mux(hw);
    int                    ret;

    ret = busy->mux_ops->set_parent(&busy->mux.hw, index);

    if (!ret) {
        ret = clock_busy_wait(busy->reg, busy->shift);
    }

    return ret;
}

static struct clock_ops clock_busy_mux_ops = {
    .get_parent = clock_busy_mux_get_parent,
    .set_parent = clock_busy_mux_set_parent,
};

struct clk *imx_clock_busy_mux(
    const char *name, void __iomem *reg, uint8_t shift, uint8_t width, void __iomem *busy_reg, uint8_t busy_shift, const char **parent_names,
    int num_parents)
{
    struct clock_busy_mux *busy;
    struct clk            *clk;
    struct clock_init_data init;

    busy = kzalloc(sizeof(*busy), GFP_KERNEL);

    if (!busy) {
        return ERR_PTR(-ENOMEM);
    }

    busy->reg         = busy_reg;
    busy->shift       = busy_shift;

    busy->mux.reg     = reg;
    busy->mux.shift   = shift;
    busy->mux.mask    = BIT(width) - 1;
    busy->mux.lock    = &imx_ccm_lock;
    busy->mux_ops     = &clock_mux_ops;

    init.name         = name;
    init.ops          = &clock_busy_mux_ops;
    init.flags        = 0;
    init.parent_names = parent_names;
    init.num_parents  = num_parents;

    busy->mux.hw.init = &init;

    clk               = clock_register(NULL, &busy->mux.hw);

    if (IS_ERR(clk)) {
        kfree(busy);
    }

    return clk;
}
