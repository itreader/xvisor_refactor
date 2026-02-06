/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-fixup-div.c
 *
 * Copyright (C) 2013 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file clk-fixup-div.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX "fixup" clock function helpers
 */

#include <imx-common.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include "clk.h"

#define to_clock_div(_hw) container_of(_hw, struct clock_divider, hw)
#define div_mask(d)       ((1 << (d->width)) - 1)

/**
 * struct clock_fixup_div - imx integer fixup divider clock
 * @divider: the parent class
 * @ops: pointer to clock_ops of parent class
 * @fixup: a hook to fixup the write value
 *
 * The imx fixup divider clock is a subclass of basic clock_divider
 * with an addtional fixup hook.
 */
struct clock_fixup_div {
    struct clock_divider    divider;
    const struct clock_ops *ops;
    void (*fixup)(uint32_t *val);
};

static inline struct clock_fixup_div *to_clock_fixup_div(struct clock_hw *hw)
{
    struct clock_divider *divider = to_clock_div(hw);

    return container_of(divider, struct clock_fixup_div, divider);
}

static uint64_t clock_fixup_div_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_fixup_div *fixup_div = to_clock_fixup_div(hw);

    return fixup_div->ops->recalc_rate(&fixup_div->divider.hw, parent_rate);
}

static long clock_fixup_div_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct clock_fixup_div *fixup_div = to_clock_fixup_div(hw);

    return fixup_div->ops->round_rate(&fixup_div->divider.hw, rate, prate);
}

static int clock_fixup_div_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_fixup_div *fixup_div = to_clock_fixup_div(hw);
    struct clock_divider   *div       = to_clock_div(hw);
    uint32_t                divider, value;
    uint64_t                flags = 0;
    uint32_t                val;

    divider = do_div(parent_rate, rate);

    /* Zero based divider */
    value   = divider - 1;

    if (value > div_mask(div)) {
        value = div_mask(div);
    }

    spin_lock_irq_save(div->lock, flags);

    val = readl(div->reg);
    val &= ~(div_mask(div) << div->shift);
    val |= value << div->shift;
    fixup_div->fixup(&val);
    writel(val, div->reg);

    spin_unlock_irq_restore(div->lock, flags);

    return 0;
}

static const struct clock_ops clock_fixup_div_ops = {
    .recalc_rate = clock_fixup_div_recalc_rate,
    .round_rate  = clock_fixup_div_round_rate,
    .set_rate    = clock_fixup_div_set_rate,
};

struct clk *imx_clock_fixup_divider(
    const char *name, const char *parent, void __iomem *reg, uint8_t shift, uint8_t width, void (*fixup)(uint32_t *val))
{
    struct clock_fixup_div *fixup_div;
    struct clk             *clk;
    struct clock_init_data  init;

    if (!fixup) {
        return ERR_PTR(-EINVAL);
    }

    fixup_div = kzalloc(sizeof(*fixup_div), GFP_KERNEL);

    if (!fixup_div) {
        return ERR_PTR(-ENOMEM);
    }

    init.name                  = name;
    init.ops                   = &clock_fixup_div_ops;
    init.flags                 = CLK_SET_RATE_PARENT;
    init.parent_names          = parent ? &parent : NULL;
    init.num_parents           = parent ? 1 : 0;

    fixup_div->divider.reg     = reg;
    fixup_div->divider.shift   = shift;
    fixup_div->divider.width   = width;
    fixup_div->divider.lock    = &imx_ccm_lock;
    fixup_div->divider.hw.init = &init;
    fixup_div->ops             = &clock_divider_ops;
    fixup_div->fixup           = fixup;

    clk                        = clock_register(NULL, &fixup_div->divider.hw);

    if (IS_ERR(clk)) {
        kfree(fixup_div);
    }

    return clk;
}
