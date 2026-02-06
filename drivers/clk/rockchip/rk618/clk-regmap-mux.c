/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *
 * Base on code in drivers/clk/clk-mux.c.
 * See clk-mux.c for further copyright information.
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

#define to_clock_regmap_mux(_hw) container_of(_hw, struct clock_regmap_mux, hw)

static uint8_t clock_regmap_mux_get_parent(struct clock_hw *hw)
{
    struct clock_regmap_mux *mux = to_clock_regmap_mux(hw);
    uint8_t                  index;
    uint32_t                 val;

    regmap_read(mux->regmap, mux->reg, &val);

    index = val >> mux->shift;
    index &= mux->mask;

    return index;
}

static int clock_regmap_mux_set_parent(struct clock_hw *hw, uint8_t index)
{
    struct clock_regmap_mux *mux = to_clock_regmap_mux(hw);

    return regmap_write(mux->regmap, mux->reg, (index << mux->shift) | (mux->mask << (mux->shift + 16)));
}

const struct clock_ops clock_regmap_mux_ops = {
    .set_parent     = clock_regmap_mux_set_parent,
    .get_parent     = clock_regmap_mux_get_parent,
    .determine_rate = __clock_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(clock_regmap_mux_ops);

struct clk *devm_clock_regmap_register_mux(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, struct regmap *regmap, uint32_t reg, uint8_t shift,
    uint8_t width, uint64_t flags)
{
    struct clock_regmap_mux *mux;
    struct clock_init_data   init;

    mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);

    if (!mux) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_regmap_mux_ops;
    init.flags        = flags;
    init.parent_names = parent_names;
    init.num_parents  = num_parents;

    mux->dev          = dev;
    mux->regmap       = regmap;
    mux->reg          = reg;
    mux->shift        = shift;
    mux->mask         = BIT(width) - 1;
    mux->hw.init      = &init;

    return devm_clock_register(dev, &mux->hw);
}

EXPORT_SYMBOL_GPL(devm_clock_regmap_register_mux);
