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

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/mfd/rk618.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-regmap.h"

#define RK618_CRU_CLKSEL0   0x0058
#define RK618_CRU_CLKSEL1   0x005c
#define RK618_CRU_CLKSEL2   0x0060
#define RK618_CRU_CLKSEL3   0x0064
#define RK618_CRU_PLL0_CON0 0x0068
#define RK618_CRU_PLL0_CON1 0x006c
#define RK618_CRU_PLL0_CON2 0x0070
#define RK618_CRU_PLL1_CON0 0x0074
#define RK618_CRU_PLL1_CON1 0x0078
#define RK618_CRU_PLL1_CON2 0x007c

struct clock_pll_data {
    uint32_t    id;
    const char *name;
    const char *parent_name;
    uint32_t    reg;
    uint64_t    flags;
};

#define PLL(_id, _name, _parent_name, _reg, _flags)                                                                                                  \
    {                                                                                                                                                \
        .id = _id, .name = _name, .parent_name = _parent_name, .reg = _reg, .flags = _flags,                                                         \
    }

struct clock_mux_data {
    uint32_t           id;
    const char        *name;
    const char *const *parent_names;
    uint8_t            num_parents;
    uint32_t           reg;
    uint8_t            shift;
    uint8_t            width;
    uint64_t           flags;
};

#define MUX(_id, _name, _parent_names, _reg, _shift, _width, _flags)                                                                                 \
    {                                                                                                                                                \
        .id = _id, .name = _name, .parent_names = _parent_names, .num_parents = ARRAY_SIZE(_parent_names), .reg = _reg, .shift = _shift,             \
        .width = _width, .flags = _flags,                                                                                                            \
    }

struct clock_gate_data {
    uint32_t    id;
    const char *name;
    const char *parent_name;
    uint32_t    reg;
    uint8_t     shift;
    uint64_t    flags;
};

#define GATE(_id, _name, _parent_name, _reg, _shift, _flags)                                                                                         \
    {                                                                                                                                                \
        .id = _id, .name = _name, .parent_name = _parent_name, .reg = _reg, .shift = _shift, .flags = _flags,                                        \
    }

struct clock_divider_data {
    uint32_t    id;
    const char *name;
    const char *parent_name;
    uint32_t    reg;
    uint8_t     shift;
    uint8_t     width;
    uint64_t    flags;
};

#define DIV(_id, _name, _parent_name, _reg, _shift, _width, _flags)                                                                                  \
    {                                                                                                                                                \
        .id = _id, .name = _name, .parent_name = _parent_name, .reg = _reg, .shift = _shift, .width = _width, .flags = _flags,                       \
    }

struct clock_composite_data {
    uint32_t           id;
    const char        *name;
    const char *const *parent_names;
    uint8_t            num_parents;
    uint32_t           mux_reg;
    uint8_t            mux_shift;
    uint8_t            mux_width;
    uint32_t           div_reg;
    uint8_t            div_shift;
    uint8_t            div_width;
    uint32_t           gate_reg;
    uint8_t            gate_shift;
    uint64_t           flags;
};

#define COMPOSITE(_id, _name, _parent_names, _mux_reg, _mux_shift, _mux_width, _div_reg, _div_shift, _div_width, _gate_reg, _gate_shift, _flags)     \
    {                                                                                                                                                \
        .id = _id, .name = _name, .parent_names = _parent_names, .num_parents = ARRAY_SIZE(_parent_names), .mux_reg = _mux_reg,                      \
        .mux_shift = _mux_shift, .mux_width = _mux_width, .div_reg = _div_reg, .div_shift = _div_shift, .div_width = _div_width,                     \
        .gate_reg = _gate_reg, .gate_shift = _gate_shift, .flags = _flags,                                                                           \
    }

#define COMPOSITE_NODIV(_id, _name, _parent_names, _mux_reg, _mux_shift, _mux_width, _gate_reg, _gate_shift, _flags)                                 \
    {                                                                                                                                                \
        .id = _id, .name = _name, .parent_names = _parent_names, .num_parents = ARRAY_SIZE(_parent_names), .mux_reg = _mux_reg,                      \
        .mux_shift = _mux_shift, .mux_width = _mux_width, .gate_reg = _gate_reg, .gate_shift = _gate_shift, .flags = _flags,                         \
    }

enum {
    LCDC0_CLK = 1,
    LCDC1_CLK,
    VIF_PLLIN_CLK,
    SCALER_PLLIN_CLK,
    VIF_PLL_CLK,
    SCALER_PLL_CLK,
    VIF0_CLK,
    VIF1_CLK,
    SCALER_IN_CLK,
    SCALER_CLK,
    DITHER_CLK,
    HDMI_CLK,
    MIPI_CLK,
    LVDS_CLK,
    LVTTL_CLK,
    RGB_CLK,
    VIF0_PRE_CLK,
    VIF1_PRE_CLK,
    CODEC_CLK,
    NR_CLKS,
};

struct rk618_cru {
    struct device *dev;
    struct rk618  *parent;
    struct regmap *regmap;

    struct clock_onecell_data clock_data;
};

static char clkin_name[16]       = "dummy";
static char lcdc0_dclkp_name[16] = "dummy";
static char lcdc1_dclkp_name[16] = "dummy";

#define PNAME(x) static const char *const x[]

PNAME(mux_pll_in_p)  = {"lcdc0_clock", "lcdc1_clock", clkin_name};
PNAME(mux_pll_src_p) = {
    "vif_pll_clock",
    "scaler_pll_clock",
};
PNAME(mux_scaler_in_src_p)                            = {"vif0_clock", "vif1_clock"};
PNAME(mux_hdmi_src_p)                                 = {"vif1_clock", "scaler_clock", "vif0_clock"};
PNAME(mux_dither_src_p)                               = {"vif0_clock", "scaler_clock"};
PNAME(mux_vif0_src_p)                                 = {"vif0_pre_clock", lcdc0_dclkp_name};
PNAME(mux_vif1_src_p)                                 = {"vif1_pre_clock", lcdc1_dclkp_name};
PNAME(mux_codec_src_p)                                = {"vif_pll_clock", "scaler_pll_clock", clkin_name};

/* Two PLL, one for dual datarate input logic, the other for scaler */
static const struct clock_pll_data rk618_clock_plls[] = {
    PLL(VIF_PLL_CLK, "vif_pll_clock", "vif_pllin_clock", RK618_CRU_PLL0_CON0, 0),
    PLL(SCALER_PLL_CLK, "scaler_pll_clock", "scaler_pllin_clock", RK618_CRU_PLL1_CON0, 0),
};

static const struct clock_mux_data rk618_clock_muxes[] = {
    MUX(VIF_PLLIN_CLK, "vif_pllin_clock", mux_pll_in_p, RK618_CRU_CLKSEL0, 6, 2, 0),
    MUX(SCALER_PLLIN_CLK, "scaler_pllin_clock", mux_pll_in_p, RK618_CRU_CLKSEL0, 8, 2, 0),
    MUX(SCALER_IN_CLK, "scaler_in_clock", mux_scaler_in_src_p, RK618_CRU_CLKSEL3, 15, 1, 0),
    MUX(DITHER_CLK, "dither_clock", mux_dither_src_p, RK618_CRU_CLKSEL3, 14, 1, 0),
    MUX(VIF0_CLK, "vif0_clock", mux_vif0_src_p, RK618_CRU_CLKSEL3, 1, 1, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT),
    MUX(VIF1_CLK, "vif1_clock", mux_vif1_src_p, RK618_CRU_CLKSEL3, 7, 1, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT),
};

static const struct clock_divider_data rk618_clock_dividers[] = {
    DIV(LCDC0_CLK, "lcdc0_clock", lcdc0_dclkp_name, RK618_CRU_CLKSEL0, 0, 3, 0),
    DIV(LCDC1_CLK, "lcdc1_clock", lcdc1_dclkp_name, RK618_CRU_CLKSEL0, 3, 3, 0),
};

static const struct clock_gate_data rk618_clock_gates[] = {
    GATE(MIPI_CLK, "mipi_clock", "dither_clock", RK618_CRU_CLKSEL1, 10, 0),
    GATE(LVDS_CLK, "lvds_clock", "dither_clock", RK618_CRU_CLKSEL1, 9, CLK_IGNORE_UNUSED),
    GATE(LVTTL_CLK, "lvttl_clock", "dither_clock", RK618_CRU_CLKSEL1, 12, 0),
    GATE(RGB_CLK, "rgb_clock", "dither_clock", RK618_CRU_CLKSEL1, 11, 0),
};

static const struct clock_composite_data rk618_clock_composites[] = {
    COMPOSITE(
        SCALER_CLK, "scaler_clock", mux_pll_src_p, RK618_CRU_CLKSEL1, 3, 1, RK618_CRU_CLKSEL1, 5, 3, RK618_CRU_CLKSEL1, 4,
        CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT),
    COMPOSITE_NODIV(HDMI_CLK, "hdmi_clock", mux_hdmi_src_p, RK618_CRU_CLKSEL3, 12, 2, RK618_CRU_CLKSEL1, 8, 0),
    COMPOSITE(
        VIF0_PRE_CLK, "vif0_pre_clock", mux_pll_src_p, RK618_CRU_CLKSEL3, 0, 1, RK618_CRU_CLKSEL3, 3, 3, RK618_CRU_CLKSEL3, 2,
        CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT),
    COMPOSITE(
        VIF1_PRE_CLK, "vif1_pre_clock", mux_pll_src_p, RK618_CRU_CLKSEL3, 6, 1, RK618_CRU_CLKSEL3, 9, 3, RK618_CRU_CLKSEL3, 8,
        CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT),
    COMPOSITE_NODIV(CODEC_CLK, "codec_clock", mux_codec_src_p, RK618_CRU_CLKSEL1, 0, 2, RK618_CRU_CLKSEL1, 2, 0),
};

static void rk618_clock_add_lookup(struct rk618_cru *cru, struct clk *clk, uint32_t id)
{
    if (cru->clock_data.clks && id) {
        cru->clock_data.clks[id] = clk;
    }
}

static void rk618_clock_register_muxes(struct rk618_cru *cru)
{
    struct clk *clk;
    int         i;

    for (i = 0; i < ARRAY_SIZE(rk618_clock_muxes); i++) {
        const struct clock_mux_data *data = &rk618_clock_muxes[i];

        clk                               = devm_clock_regmap_register_mux(
            cru->dev, data->name, data->parent_names, data->num_parents, cru->regmap, data->reg, data->shift, data->width, data->flags);

        if (IS_ERR(clk)) {
            dev_err(cru->dev, "failed to register clock %s\n", data->name);
            continue;
        }

        rk618_clock_add_lookup(cru, clk, data->id);
    }
}

static void rk618_clock_register_dividers(struct rk618_cru *cru)
{
    struct clk *clk;
    int         i;

    for (i = 0; i < ARRAY_SIZE(rk618_clock_dividers); i++) {
        const struct clock_divider_data *data = &rk618_clock_dividers[i];

        clk                                   = devm_clock_regmap_register_divider(
            cru->dev, data->name, data->parent_name, cru->regmap, data->reg, data->shift, data->width, data->flags);

        if (IS_ERR(clk)) {
            dev_err(cru->dev, "failed to register clock %s\n", data->name);
            continue;
        }

        rk618_clock_add_lookup(cru, clk, data->id);
    }
}

static void rk618_clock_register_gates(struct rk618_cru *cru)
{
    struct clk *clk;
    int         i;

    for (i = 0; i < ARRAY_SIZE(rk618_clock_gates); i++) {
        const struct clock_gate_data *data = &rk618_clock_gates[i];

        clk = devm_clock_regmap_register_gate(cru->dev, data->name, data->parent_name, cru->regmap, data->reg, data->shift, data->flags);

        if (IS_ERR(clk)) {
            dev_err(cru->dev, "failed to register clock %s\n", data->name);
            continue;
        }

        rk618_clock_add_lookup(cru, clk, data->id);
    }
}

static void rk618_clock_register_composites(struct rk618_cru *cru)
{
    struct clk *clk;
    int         i;

    for (i = 0; i < ARRAY_SIZE(rk618_clock_composites); i++) {
        const struct clock_composite_data *data = &rk618_clock_composites[i];

        clk                                     = devm_clock_regmap_register_composite(
            cru->dev, data->name, data->parent_names, data->num_parents, cru->regmap, data->mux_reg, data->mux_shift, data->mux_width, data->div_reg,
            data->div_shift, data->div_width, data->gate_reg, data->gate_shift, data->flags);

        if (IS_ERR(clk)) {
            dev_err(cru->dev, "failed to register clock %s\n", data->name);
            continue;
        }

        rk618_clock_add_lookup(cru, clk, data->id);
    }
}

static void rk618_clock_register_plls(struct rk618_cru *cru)
{
    struct clk *clk;
    int         i;

    for (i = 0; i < ARRAY_SIZE(rk618_clock_plls); i++) {
        const struct clock_pll_data *data = &rk618_clock_plls[i];

        clk = devm_clock_regmap_register_pll(cru->dev, data->name, data->parent_name, cru->regmap, data->reg, data->flags);

        if (IS_ERR(clk)) {
            dev_err(cru->dev, "failed to register clock %s\n", data->name);
            continue;
        }

        rk618_clock_add_lookup(cru, clk, data->id);
    }
}

static int rk618_cru_probe(struct platform_device *pdev)
{
    struct rk618     *rk618 = dev_get_drvdata(pdev->dev.parent);
    struct device    *dev   = &pdev->dev;
    struct rk618_cru *cru;
    struct clk      **clock_table;
    const char       *parent_name;
    struct clk       *clk;
    int               ret, i;

    if (!of_device_is_available(dev->of_node)) {
        return -ENODEV;
    }

    cru = devm_kzalloc(dev, sizeof(*cru), GFP_KERNEL);

    if (!cru) {
        return -ENOMEM;
    }

    clock_table = devm_kcalloc(dev, NR_CLKS, sizeof(struct clk *), GFP_KERNEL);

    if (!clock_table) {
        return -ENOMEM;
    }

    for (i = 0; i < NR_CLKS; i++) {
        clock_table[i] = ERR_PTR(-ENOENT);
    }

    cru->dev                  = dev;
    cru->parent               = rk618;
    cru->regmap               = rk618->regmap;
    cru->clock_data.clks      = clock_table;
    cru->clock_data.clock_num = NR_CLKS;
    platform_set_drvdata(pdev, cru);

    clk = devm_clock_get(dev, "clkin");

    if (IS_ERR(clk)) {
        ret = PTR_ERR(clk);
        dev_err(dev, "failed to get clkin: %d\n", ret);
        return ret;
    }

    strlcpy(clkin_name, __clock_get_name(clk), sizeof(clkin_name));

    clk = devm_clock_get(dev, "lcdc0_dclkp");

    if (IS_ERR(clk)) {
        if (PTR_ERR(clk) != -ENOENT) {
            ret = PTR_ERR(clk);
            dev_err(dev, "failed to get lcdc0_dclkp: %d\n", ret);
            return ret;
        }

        clk = NULL;
    }

    parent_name = __clock_get_name(clk);

    if (parent_name) {
        strlcpy(lcdc0_dclkp_name, parent_name, sizeof(lcdc0_dclkp_name));
    }

    clk = devm_clock_get(dev, "lcdc1_dclkp");

    if (IS_ERR(clk)) {
        if (PTR_ERR(clk) != -ENOENT) {
            ret = PTR_ERR(clk);
            dev_err(dev, "failed to get lcdc1_dclkp: %d\n", ret);
            return ret;
        }

        clk = NULL;
    }

    parent_name = __clock_get_name(clk);

    if (parent_name) {
        strlcpy(lcdc1_dclkp_name, parent_name, sizeof(lcdc1_dclkp_name));
    }

    rk618_clock_register_plls(cru);
    rk618_clock_register_muxes(cru);
    rk618_clock_register_dividers(cru);
    rk618_clock_register_gates(cru);
    rk618_clock_register_composites(cru);

    return of_clock_add_provider(dev->of_node, of_clock_src_onecell_get, &cru->clock_data);
}

static int rk618_cru_remove(struct platform_device *pdev)
{
    of_clock_del_provider(pdev->dev.of_node);

    return 0;
}

static const struct of_device_id rk618_cru_of_match[] = {
    {
     .compatible = "rockchip,rk618-cru",
     },
    {},
};
MODULE_DEVICE_TABLE(of, rk618_cru_of_match);

static struct platform_driver rk618_cru_driver = {
    .driver =
        {
                 .name           = "rk618-cru",
                 .of_match_table = of_match_ptr(rk618_cru_of_match),
                 },
    .probe  = rk618_cru_probe,
    .remove = rk618_cru_remove,
};
module_platform_driver(rk618_cru_driver);

MODULE_AUTHOR("Wyon Bi <bivvy.bi@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip rk618 CRU driver");
MODULE_LICENSE("GPL v2");
