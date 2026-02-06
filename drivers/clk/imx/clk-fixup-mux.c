/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk-fixup-mux.c
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
 * @file clk-fixup-mux.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX "fixup" clock function helpers
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include "clk.h"

#define to_clock_mux(_hw) container_of(_hw, struct clock_mux, hw)

/**
 * struct clock_fixup_mux - imx integer fixup multiplexer clock
 * @mux: the parent class
 * @ops: pointer to clock_ops of parent class
 * @fixup: a hook to fixup the write value
 *
 * The imx fixup multiplexer clock is a subclass of basic clock_mux
 * with an addtional fixup hook.
 */
struct clock_fixup_mux {
    struct clock_mux        mux;
    const struct clock_ops *ops;
    void (*fixup)(uint32_t *val);
};

static inline struct clock_fixup_mux *to_clock_fixup_mux(struct clock_hw *hw)
{
    struct clock_mux *mux = to_clock_mux(hw);

    return container_of(mux, struct clock_fixup_mux, mux);
}

static uint8_t clock_fixup_mux_get_parent(struct clock_hw *hw)
{
    struct clock_fixup_mux *fixup_mux = to_clock_fixup_mux(hw);

    return fixup_mux->ops->get_parent(&fixup_mux->mux.hw);
}

static int clock_fixup_mux_set_parent(struct clock_hw *hw, uint8_t index)
{
    struct clock_fixup_mux *fixup_mux = to_clock_fixup_mux(hw);
    struct clock_mux       *mux       = to_clock_mux(hw);
    uint64_t                flags     = 0;
    uint32_t                val;

    spin_lock_irq_save(mux->lock, flags);

    val = readl(mux->reg);
    val &= ~(mux->mask << mux->shift);
    val |= index << mux->shift;
    fixup_mux->fixup(&val);
    writel(val, mux->reg);

    spin_unlock_irq_restore(mux->lock, flags);

    return 0;
}

static const struct clock_ops clock_fixup_mux_ops = {
    .get_parent = clock_fixup_mux_get_parent,
    .set_parent = clock_fixup_mux_set_parent,
};

struct clk *imx_clock_fixup_mux(
    const char *name, void __iomem *reg, uint8_t shift, uint8_t width, const char **parents, int num_parents, void (*fixup)(uint32_t *val))
{
    struct clock_fixup_mux *fixup_mux;
    struct clk             *clk;
    struct clock_init_data  init;

    if (!fixup) {
        return ERR_PTR(-EINVAL);
    }

    fixup_mux = kzalloc(sizeof(*fixup_mux), GFP_KERNEL);

    if (!fixup_mux) {
        return ERR_PTR(-ENOMEM);
    }

    init.name              = name;
    init.ops               = &clock_fixup_mux_ops;
    init.parent_names      = parents;
    init.num_parents       = num_parents;
    init.flags             = 0;

    fixup_mux->mux.reg     = reg;
    fixup_mux->mux.shift   = shift;
    fixup_mux->mux.mask    = BIT(width) - 1;
    fixup_mux->mux.lock    = &imx_ccm_lock;
    fixup_mux->mux.hw.init = &init;
    fixup_mux->ops         = &clock_mux_ops;
    fixup_mux->fixup       = fixup;

    clk                    = clock_register(NULL, &fixup_mux->mux.hw);

    if (IS_ERR(clk)) {
        kfree(fixup_mux);
    }

    return clk;
}
