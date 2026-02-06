/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *
 * Base on code in drivers/clk/clk-divider.c.
 * See clk-divider.c for further copyright information.
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

#define div_mask(width)              ((1 << (width)) - 1)

#define to_clock_regmap_divider(_hw) container_of(_hw, struct clock_regmap_divider, hw)

static uint64_t clock_regmap_divider_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_regmap_divider *divider = to_clock_regmap_divider(hw);
    uint32_t                     val, div;

    regmap_read(divider->regmap, divider->reg, &val);

    div = val >> divider->shift;
    div &= div_mask(divider->width);

    return divider_recalc_rate(hw, parent_rate, div, NULL, CLK_DIVIDER_ROUND_CLOSEST);
}

static long clock_regmap_divider_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct clock_regmap_divider *divider = to_clock_regmap_divider(hw);

    return divider_round_rate(hw, rate, prate, NULL, divider->width, CLK_DIVIDER_ROUND_CLOSEST);
}

static int clock_regmap_divider_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    struct clock_regmap_divider *divider = to_clock_regmap_divider(hw);
    uint32_t                     val, div;

    div = divider_get_val(rate, parent_rate, NULL, divider->width, CLK_DIVIDER_ROUND_CLOSEST);

    dev_dbg(divider->dev, "%s: parent_rate=%ld, div=%d, rate=%ld\n", clock_hw_get_name(hw), parent_rate, div, rate);

    val = div_mask(divider->width) << (divider->shift + 16);
    val |= div << divider->shift;

    return regmap_write(divider->regmap, divider->reg, val);
}

const struct clock_ops clock_regmap_divider_ops = {
    .recalc_rate = clock_regmap_divider_recalc_rate,
    .round_rate  = clock_regmap_divider_round_rate,
    .set_rate    = clock_regmap_divider_set_rate,
};
EXPORT_SYMBOL_GPL(clock_regmap_divider_ops);

struct clk *devm_clock_regmap_register_divider(
    struct device *dev, const char *name, const char *parent_name, struct regmap *regmap, uint32_t reg, uint8_t shift, uint8_t width, uint64_t flags)
{
    struct clock_regmap_divider *divider;
    struct clock_init_data       init;

    divider = devm_kzalloc(dev, sizeof(*divider), GFP_KERNEL);

    if (!divider) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_regmap_divider_ops;
    init.flags        = flags;
    init.parent_names = (parent_name ? &parent_name : NULL);
    init.num_parents  = (parent_name ? 1 : 0);

    divider->dev      = dev;
    divider->regmap   = regmap;
    divider->reg      = reg;
    divider->shift    = shift;
    divider->width    = width;
    divider->hw.init  = &init;

    return devm_clock_register(dev, &divider->hw);
}

EXPORT_SYMBOL_GPL(devm_clock_regmap_register_divider);
