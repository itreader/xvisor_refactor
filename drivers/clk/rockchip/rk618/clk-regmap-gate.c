/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *
 * Base on code in drivers/clk/clk-gate.c.
 * See clk-gate.c for further copyright information.
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

#define to_clock_regmap_gate(_hw) container_of(_hw, struct clock_regmap_gate, hw)

static int clock_regmap_gate_prepare(struct clock_hw *hw)
{
    struct clock_regmap_gate *gate = to_clock_regmap_gate(hw);

    return regmap_write(gate->regmap, gate->reg, 0 | BIT(gate->shift + 16));
}

static void clock_regmap_gate_unprepare(struct clock_hw *hw)
{
    struct clock_regmap_gate *gate = to_clock_regmap_gate(hw);

    regmap_write(gate->regmap, gate->reg, BIT(gate->shift) | BIT(gate->shift + 16));
}

static int clock_regmap_gate_is_prepared(struct clock_hw *hw)
{
    struct clock_regmap_gate *gate = to_clock_regmap_gate(hw);
    uint32_t                  val;

    regmap_read(gate->regmap, gate->reg, &val);

    return !(val & BIT(gate->shift));
}

const struct clock_ops clock_regmap_gate_ops = {
    .prepare     = clock_regmap_gate_prepare,
    .unprepare   = clock_regmap_gate_unprepare,
    .is_prepared = clock_regmap_gate_is_prepared,
};
EXPORT_SYMBOL_GPL(clock_regmap_gate_ops);

struct clk *devm_clock_regmap_register_gate(
    struct device *dev, const char *name, const char *parent_name, struct regmap *regmap, uint32_t reg, uint8_t shift, uint64_t flags)
{
    struct clock_regmap_gate *gate;
    struct clock_init_data    init;

    gate = devm_kzalloc(dev, sizeof(*gate), GFP_KERNEL);

    if (!gate) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_regmap_gate_ops;
    init.flags        = flags;
    init.parent_names = (parent_name ? &parent_name : NULL);
    init.num_parents  = (parent_name ? 1 : 0);

    gate->dev         = dev;
    gate->regmap      = regmap;
    gate->reg         = reg;
    gate->shift       = shift;
    gate->hw.init     = &init;

    return devm_clock_register(dev, &gate->hw);
}

EXPORT_SYMBOL_GPL(devm_clock_regmap_register_gate);
