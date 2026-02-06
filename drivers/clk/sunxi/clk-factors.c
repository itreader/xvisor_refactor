/*
 * Copyright (C) 2013 Emilio López <emilio@elopez.com.ar>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable factor-based clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/delay.h>

#include "clk-factors.h"

/*
 * DOC: basic adjustable factor-based clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clock_prepare only ensures that parents are prepared
 * enable - clock_enable only ensures that parents are enabled
 * rate - rate is adjustable.
 *        clk->rate = (parent->rate * N * (K + 1) >> P) / (M + 1)
 * parent - fixed parent.  No clock_set_parent support
 */

#define to_clock_factors(_hw)          container_of(_hw, struct clock_factors, hw)

#define SETMASK(len, pos)              (((1U << (len)) - 1) << (pos))
#define CLRMASK(len, pos)              (~(SETMASK(len, pos)))
#define FACTOR_GET(bit, len, reg)      (((reg) & SETMASK(len, bit)) >> (bit))

#define FACTOR_SET(bit, len, reg, val) (((reg) & CLRMASK(len, bit)) | (val << (bit)))

static uint64_t clock_factors_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    uint8_t                      n = 1, k = 0, p = 0, m = 0;
    uint32_t                     reg;
    uint64_t                     rate;
    struct clock_factors        *factors = to_clock_factors(hw);
    struct clock_factors_config *config  = factors->config;

    /* Fetch the register value */
    reg                                  = readl(factors->reg);

    /* Get each individual factor if applicable */
    if (config->nwidth != SUNXI_FACTORS_NOT_APPLICABLE) {
        n = FACTOR_GET(config->nshift, config->nwidth, reg);
    }

    if (config->kwidth != SUNXI_FACTORS_NOT_APPLICABLE) {
        k = FACTOR_GET(config->kshift, config->kwidth, reg);
    }

    if (config->mwidth != SUNXI_FACTORS_NOT_APPLICABLE) {
        m = FACTOR_GET(config->mshift, config->mwidth, reg);
    }

    if (config->pwidth != SUNXI_FACTORS_NOT_APPLICABLE) {
        p = FACTOR_GET(config->pshift, config->pwidth, reg);
    }

    /* Calculate the rate */
#if 0
    rate = (parent_rate * n * (k + 1) >> p) / (m + 1);
#else
    rate = udiv64((parent_rate * n * (k + 1) >> p), (m + 1));
#endif

    return rate;
}

static long clock_factors_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *parent_rate)
{
    struct clock_factors *factors = to_clock_factors(hw);
    factors->get_factors((uint32_t *)&rate, (uint32_t)*parent_rate, NULL, NULL, NULL, NULL);

    return rate;
}

static int clock_factors_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    uint8_t                      n = 0, k = 0, m = 0, p = 0;
    uint32_t                     reg;
    struct clock_factors        *factors = to_clock_factors(hw);
    struct clock_factors_config *config  = factors->config;
    uint64_t                     flags   = 0;

    factors->get_factors((uint32_t *)&rate, (uint32_t)parent_rate, &n, &k, &m, &p);

    if (factors->lock) {
        spin_lock_irq_save(factors->lock, flags);
    }

    /* Fetch the register value */
    reg = readl(factors->reg);

    /* Set up the new factors - macros do not do anything if width is 0 */
    reg = FACTOR_SET(config->nshift, config->nwidth, reg, n);
    reg = FACTOR_SET(config->kshift, config->kwidth, reg, k);
    reg = FACTOR_SET(config->mshift, config->mwidth, reg, m);
    reg = FACTOR_SET(config->pshift, config->pwidth, reg, p);

    /* Apply them now */
    writel(reg, factors->reg);

    /* delay 500us so pll stabilizes */
    __delay((rate >> 20) * 500 / 2);

    if (factors->lock) {
        spin_unlock_irq_restore(factors->lock, flags);
    }

    return 0;
}

const struct clock_ops clock_factors_ops = {
    .recalc_rate = clock_factors_recalc_rate,
    .round_rate  = clock_factors_round_rate,
    .set_rate    = clock_factors_set_rate,
};
