/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "clk-regmap.h"

#define PLLCON_OFFSET(x)               (x * 4)

#define PLL_BYPASS(x)                  HIWORD_UPDATE(x, 15, 15)
#define PLL_BYPASS_MASK                BIT(15)
#define PLL_BYPASS_SHIFT               15
#define PLL_POSTDIV1(x)                HIWORD_UPDATE(x, 14, 12)
#define PLL_POSTDIV1_MASK              GENMASK(14, 12)
#define PLL_POSTDIV1_SHIFT             12
#define PLL_FBDIV(x)                   HIWORD_UPDATE(x, 11, 0)
#define PLL_FBDIV_MASK                 GENMASK(11, 0)
#define PLL_FBDIV_SHIFT                0

#define PLL_LOCK                       BIT(15)
#define PLL_POWER_DOWN                 HIWORD_UPDATE(1, 10, 10)
#define PLL_POWER_UP                   HIWORD_UPDATE(0, 10, 10)
#define PLL_DSMPD_MASK                 BIT(9)
#define PLL_DSMPD_SHIFT                9
#define PLL_DSMPD(x)                   HIWORD_UPDATE(x, 9, 9)
#define PLL_POSTDIV2(x)                HIWORD_UPDATE(x, 8, 6)
#define PLL_POSTDIV2_MASK              GENMASK(8, 6)
#define PLL_POSTDIV2_SHIFT             6
#define PLL_REFDIV(x)                  HIWORD_UPDATE(x, 5, 0)
#define PLL_REFDIV_MASK                GENMASK(5, 0)
#define PLL_REFDIV_SHIFT               0

#define PLL_FOUT_4PHASE_CLK_POWER_DOWN BIT(27)
#define PLL_FOUT_VCO_CLK_POWER_DOWN    BIT(26)
#define PLL_FOUT_POST_DIV_POWER_DOWN   BIT(25)
#define PLL_DAC_POWER_DOWN             BIT(24)
#define PLL_FRAC(x)                    UPDATE(x, 23, 0)
#define PLL_FRAC_MASK                  GENMASK(23, 0)
#define PLL_FRAC_SHIFT                 0

#define MIN_FREF_RATE                  10000000UL
#define MAX_FREF_RATE                  800000000UL
#define MIN_FREFDIV_RATE               1000000UL
#define MAX_FREFDIV_RATE               40000000UL
#define MIN_FVCO_RATE                  400000000UL
#define MAX_FVCO_RATE                  1600000000UL
#define MIN_FOUTPOSTDIV_RATE           8000000UL
#define MAX_FOUTPOSTDIV_RATE           1600000000UL

struct clock_regmap_pll {
    struct clock_hw hw;
    struct device  *dev;
    struct regmap  *regmap;
    uint32_t        reg;
};

#define to_clock_regmap_pll(_hw) container_of(_hw, struct clock_regmap_pll, hw)

static uint64_t clock_regmap_pll_recalc_rate(struct clock_hw *hw, uint64_t prate)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);
    uint32_t                 postdiv1, fbdiv, dsmpd, postdiv2, refdiv, frac, bypass;
    uint32_t                 con0, con1, con2;
    uint64_t                 foutvco, foutpostdiv;

    regmap_read(pll->regmap, pll->reg + PLLCON_OFFSET(0), &con0);
    regmap_read(pll->regmap, pll->reg + PLLCON_OFFSET(1), &con1);
    regmap_read(pll->regmap, pll->reg + PLLCON_OFFSET(2), &con2);

    bypass   = (con0 & PLL_BYPASS_MASK) >> PLL_BYPASS_SHIFT;
    postdiv1 = (con0 & PLL_POSTDIV1_MASK) >> PLL_POSTDIV1_SHIFT;
    fbdiv    = (con0 & PLL_FBDIV_MASK) >> PLL_FBDIV_SHIFT;
    dsmpd    = (con1 & PLL_DSMPD_MASK) >> PLL_DSMPD_SHIFT;
    postdiv2 = (con1 & PLL_POSTDIV2_MASK) >> PLL_POSTDIV2_SHIFT;
    refdiv   = (con1 & PLL_REFDIV_MASK) >> PLL_REFDIV_SHIFT;
    frac     = (con2 & PLL_FRAC_MASK) >> PLL_FRAC_SHIFT;

    if (bypass) {
        return prate;
    }

    foutvco = prate * fbdiv;
    do_div(foutvco, refdiv);

    if (!dsmpd) {
        uint64_t frac_rate = prate * frac;

        do_div(frac_rate, refdiv);
        foutvco += frac_rate >> 24;
    }

    foutpostdiv = foutvco;
    do_div(foutpostdiv, postdiv1);
    do_div(foutpostdiv, postdiv2);

    return foutpostdiv;
}

static long clock_pll_round_rate(
    uint64_t fin, uint64_t fout, uint8_t *refdiv, uint16_t *fbdiv, uint8_t *postdiv1, uint8_t *postdiv2, uint32_t *frac, uint8_t *dsmpd,
    uint8_t *bypass)
{
    uint8_t  min_refdiv, max_refdiv, postdiv;
    uint8_t  _dsmpd = 1, _postdiv1 = 0, _postdiv2 = 0, _refdiv = 0;
    uint16_t _fbdiv = 0;
    uint32_t _frac  = 0;
    uint64_t foutvco, foutpostdiv;

    /*
     * FREF : 10MHz ~ 800MHz
     * FREFDIV : 1MHz ~ 40MHz
     * FOUTVCO : 400MHz ~ 1.6GHz
     * FOUTPOSTDIV : 8MHz ~ 1.6GHz
     */
    if (fin < MIN_FREF_RATE || fin > MAX_FREF_RATE) {
        return -EINVAL;
    }

    if (fout < MIN_FOUTPOSTDIV_RATE || fout > MAX_FOUTPOSTDIV_RATE) {
        return -EINVAL;
    }

    if (fin == fout) {
        if (bypass) {
            *bypass = true;
        }

        return fin;
    }

    min_refdiv = DIV_ROUND_UP(fin, MAX_FREFDIV_RATE);
    max_refdiv = fin / MIN_FREFDIV_RATE;

    if (max_refdiv > 64) {
        max_refdiv = 64;
    }

    if (fout < MIN_FVCO_RATE) {
        postdiv = DIV_ROUND_UP_ULL(MIN_FVCO_RATE, fout);

        for (_postdiv2 = 1; _postdiv2 < 8; _postdiv2++) {
            if (postdiv % _postdiv2) {
                continue;
            }

            _postdiv1 = postdiv / _postdiv2;

            if (_postdiv1 > 0 && _postdiv1 < 8) {
                break;
            }
        }

        fout *= _postdiv1 * _postdiv2;
    } else {
        _postdiv1 = 1;
        _postdiv2 = 1;
    }

    for (_refdiv = min_refdiv; _refdiv <= max_refdiv; _refdiv++) {
        uint64_t tmp, frac_rate;

        if (fin % _refdiv) {
            continue;
        }

        tmp = (uint64_t)fout * _refdiv;
        do_div(tmp, fin);
        _fbdiv = tmp;

        if (_fbdiv < 10 || _fbdiv > 1600) {
            continue;
        }

        tmp = (uint64_t)_fbdiv * fin;
        do_div(tmp, _refdiv);

        if (fout < MIN_FVCO_RATE || fout > MAX_FVCO_RATE) {
            continue;
        }

        frac_rate = fout - tmp;

        if (frac_rate) {
            tmp = (uint64_t)frac_rate * _refdiv;
            tmp <<= 24;
            do_div(tmp, fin);
            _frac  = tmp;
            _dsmpd = 0;
        }

        break;
    }

    /*
     * If DSMPD = 1 (DSM is disabled, "integer mode")
     * FOUTVCO = FREF / REFDIV * FBDIV
     * FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
     *
     * If DSMPD = 0 (DSM is enabled, "fractional mode")
     * FOUTVCO = FREF / REFDIV * (FBDIV + FRAC / 2^24)
     * FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
     */
    foutvco = fin * _fbdiv;
    do_div(foutvco, _refdiv);

    if (!_dsmpd) {
        uint64_t frac_rate = fin * _frac;

        do_div(frac_rate, _refdiv);
        foutvco += frac_rate >> 24;
    }

    foutpostdiv = foutvco;
    do_div(foutpostdiv, _postdiv1);
    do_div(foutpostdiv, _postdiv2);

    if (refdiv) {
        *refdiv = _refdiv;
    }

    if (fbdiv) {
        *fbdiv = _fbdiv;
    }

    if (postdiv1) {
        *postdiv1 = _postdiv1;
    }

    if (postdiv2) {
        *postdiv2 = _postdiv2;
    }

    if (frac) {
        *frac = _frac;
    }

    if (dsmpd) {
        *dsmpd = _dsmpd;
    }

    if (bypass) {
        *bypass = false;
    }

    return (uint64_t)foutpostdiv;
}

static long clock_regmap_pll_round_rate(struct clock_hw *hw, uint64_t drate, uint64_t *prate)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);
    long                     rate;

    rate = clock_pll_round_rate(*prate, drate, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    dev_dbg(pll->dev, "%s: prate=%ld, drate=%ld, rate=%ld\n", clock_hw_get_name(hw), *prate, drate, rate);

    return rate;
}

static int clock_regmap_pll_set_rate(struct clock_hw *hw, uint64_t drate, uint64_t prate)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);
    uint8_t                  refdiv, postdiv1, postdiv2, dsmpd, bypass;
    uint16_t                 fbdiv;
    uint32_t                 frac;
    long                     rate;

    rate = clock_pll_round_rate(prate, drate, &refdiv, &fbdiv, &postdiv1, &postdiv2, &frac, &dsmpd, &bypass);

    if (rate < 0) {
        return rate;
    }

    dev_dbg(pll->dev, "%s: rate=%ld, bypass=%d\n", clock_hw_get_name(hw), drate, bypass);

    if (bypass) {
        regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(0), PLL_BYPASS(1));
    } else {
        regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(0), PLL_BYPASS(0) | PLL_POSTDIV1(postdiv1) | PLL_FBDIV(fbdiv));
        regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(1), PLL_DSMPD(dsmpd) | PLL_POSTDIV2(postdiv2) | PLL_REFDIV(refdiv));
        regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(2), PLL_FRAC(frac));

        dev_dbg(pll->dev, "refdiv=%d, fbdiv=%d, frac=%d\n", refdiv, fbdiv, frac);
        dev_dbg(pll->dev, "postdiv1=%d, postdiv2=%d\n", postdiv1, postdiv2);
    }

    return 0;
}

static int clock_regmap_pll_prepare(struct clock_hw *hw)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);
    uint32_t                 v;
    int                      ret;

    regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(1), PLL_POWER_UP);

    ret = regmap_read_poll_timeout(pll->regmap, pll->reg + PLLCON_OFFSET(1), v, v & PLL_LOCK, 50, 50000);

    if (ret) {
        dev_err(pll->dev, "%s is not lock\n", clock_hw_get_name(hw));
    }

    return 0;
}

static void clock_regmap_pll_unprepare(struct clock_hw *hw)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);

    regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(1), PLL_POWER_DOWN);
}

static int clock_regmap_pll_is_prepared(struct clock_hw *hw)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);
    uint32_t                 con1;

    regmap_read(pll->regmap, pll->reg + PLLCON_OFFSET(1), &con1);

    return !(con1 & PLL_POWER_DOWN);
}

static void clock_regmap_pll_init(struct clock_hw *hw)
{
    struct clock_regmap_pll *pll = to_clock_regmap_pll(hw);

    regmap_write(pll->regmap, pll->reg + PLLCON_OFFSET(0), PLL_BYPASS(1));
}

static const struct clock_ops clock_regmap_pll_ops = {
    .recalc_rate = clock_regmap_pll_recalc_rate,
    .round_rate  = clock_regmap_pll_round_rate,
    .set_rate    = clock_regmap_pll_set_rate,
    .prepare     = clock_regmap_pll_prepare,
    .unprepare   = clock_regmap_pll_unprepare,
    .is_prepared = clock_regmap_pll_is_prepared,
    .init        = clock_regmap_pll_init,
};

struct clk *devm_clock_regmap_register_pll(
    struct device *dev, const char *name, const char *parent_name, struct regmap *regmap, uint32_t reg, uint64_t flags)
{
    struct clock_regmap_pll *pll;
    struct clock_init_data   init;

    pll = devm_kzalloc(dev, sizeof(*pll), GFP_KERNEL);

    if (!pll) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_regmap_pll_ops;
    init.flags        = flags;
    init.parent_names = (parent_name ? &parent_name : NULL);
    init.num_parents  = (parent_name ? 1 : 0);

    pll->dev          = dev;
    pll->regmap       = regmap;
    pll->reg          = reg;
    pll->hw.init      = &init;

    return devm_clock_register(dev, &pll->hw);
}

EXPORT_SYMBOL_GPL(devm_clock_regmap_register_pll);
