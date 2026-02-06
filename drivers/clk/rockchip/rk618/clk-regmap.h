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

#ifndef __CLK_REGMAP_H__
#define __CLK_REGMAP_H__

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define UPDATE(x, h, l)        (((x) << (l)) & GENMASK((h), (l)))
#define HIWORD_UPDATE(v, h, l) (((v) << (l)) | (GENMASK((h), (l)) << 16))

struct clock_regmap_divider {
    struct clock_hw hw;
    struct device  *dev;
    struct regmap  *regmap;
    uint32_t        reg;
    uint8_t         shift;
    uint8_t         width;
};

struct clock_regmap_gate {
    struct clock_hw hw;
    struct device  *dev;
    struct regmap  *regmap;
    uint32_t        reg;
    uint8_t         shift;
};

struct clock_regmap_mux {
    struct clock_hw hw;
    struct device  *dev;
    struct regmap  *regmap;
    uint32_t        reg;
    uint32_t        mask;
    uint8_t         shift;
};

extern const struct clock_ops clock_regmap_mux_ops;
extern const struct clock_ops clock_regmap_divider_ops;
extern const struct clock_ops clock_regmap_gate_ops;

struct clk *devm_clock_regmap_register_pll(
    struct device *dev, const char *name, const char *parent_name, struct regmap *regmap, uint32_t reg, uint64_t flags);

struct clk *devm_clock_regmap_register_mux(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, struct regmap *regmap, uint32_t reg, uint8_t shift,
    uint8_t width, uint64_t flags);

struct clk *devm_clock_regmap_register_divider(
    struct device *dev, const char *name, const char *parent_name, struct regmap *regmap, uint32_t reg, uint8_t shift, uint8_t width, uint64_t flags);

struct clk *devm_clock_regmap_register_gate(
    struct device *dev, const char *name, const char *parent_name, struct regmap *regmap, uint32_t reg, uint8_t shift, uint64_t flags);

struct clk *devm_clock_regmap_register_composite(
    struct device *dev, const char *name, const char *const *parent_names, uint8_t num_parents, struct regmap *regmap, uint32_t mux_reg,
    uint8_t mux_shift, uint8_t mux_width, uint32_t div_reg, uint8_t div_shift, uint8_t div_width, uint32_t gate_reg, uint8_t gate_shift,
    uint64_t flags);

#endif
