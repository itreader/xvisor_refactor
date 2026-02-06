/*
 * Copyright (c) 2015 Rockchip Electronics Co. Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 *         Jeffy Chen <jeffy.chen@rock-chips.com>
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

#include <dt-bindings/clock/rk3228-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define RK3228_GRF_SOC_STATUS0      0x480

#define RK3228_UART_FRAC_MAX_PRATE  600000000
#define RK3228_SPDIF_FRAC_MAX_PRATE 600000000
#define RK3228_I2S_FRAC_MAX_PRATE   600000000

enum rk3228_plls {
    apll,
    dpll,
    cpll,
    gpll,
};

static struct rockchip_pll_rate_table rk3228_pll_rates[] = {
    /* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
    RK3036_PLL_RATE(1608000000, 1, 67, 1, 1, 1, 0),
    RK3036_PLL_RATE(1584000000, 1, 66, 1, 1, 1, 0),
    RK3036_PLL_RATE(1560000000, 1, 65, 1, 1, 1, 0),
    RK3036_PLL_RATE(1536000000, 1, 64, 1, 1, 1, 0),
    RK3036_PLL_RATE(1512000000, 1, 63, 1, 1, 1, 0),
    RK3036_PLL_RATE(1488000000, 1, 62, 1, 1, 1, 0),
    RK3036_PLL_RATE(1464000000, 1, 61, 1, 1, 1, 0),
    RK3036_PLL_RATE(1440000000, 1, 60, 1, 1, 1, 0),
    RK3036_PLL_RATE(1416000000, 1, 59, 1, 1, 1, 0),
    RK3036_PLL_RATE(1392000000, 1, 58, 1, 1, 1, 0),
    RK3036_PLL_RATE(1368000000, 1, 57, 1, 1, 1, 0),
    RK3036_PLL_RATE(1344000000, 1, 56, 1, 1, 1, 0),
    RK3036_PLL_RATE(1320000000, 1, 55, 1, 1, 1, 0),
    RK3036_PLL_RATE(1296000000, 1, 54, 1, 1, 1, 0),
    RK3036_PLL_RATE(1272000000, 1, 53, 1, 1, 1, 0),
    RK3036_PLL_RATE(1248000000, 1, 52, 1, 1, 1, 0),
    RK3036_PLL_RATE(1200000000, 1, 50, 1, 1, 1, 0),
    RK3036_PLL_RATE(1188000000, 2, 99, 1, 1, 1, 0),
    RK3036_PLL_RATE(1104000000, 1, 46, 1, 1, 1, 0),
    RK3036_PLL_RATE(1100000000, 12, 550, 1, 1, 1, 0),
    RK3036_PLL_RATE(1008000000, 1, 84, 2, 1, 1, 0),
    RK3036_PLL_RATE(1000000000, 6, 500, 2, 1, 1, 0),
    RK3036_PLL_RATE(984000000, 1, 82, 2, 1, 1, 0),
    RK3036_PLL_RATE(960000000, 1, 80, 2, 1, 1, 0),
    RK3036_PLL_RATE(936000000, 1, 78, 2, 1, 1, 0),
    RK3036_PLL_RATE(912000000, 1, 76, 2, 1, 1, 0),
    RK3036_PLL_RATE(900000000, 4, 300, 2, 1, 1, 0),
    RK3036_PLL_RATE(888000000, 1, 74, 2, 1, 1, 0),
    RK3036_PLL_RATE(864000000, 1, 72, 2, 1, 1, 0),
    RK3036_PLL_RATE(840000000, 1, 70, 2, 1, 1, 0),
    RK3036_PLL_RATE(816000000, 1, 68, 2, 1, 1, 0),
    RK3036_PLL_RATE(800000000, 6, 400, 2, 1, 1, 0),
    RK3036_PLL_RATE(700000000, 6, 350, 2, 1, 1, 0),
    RK3036_PLL_RATE(696000000, 1, 58, 2, 1, 1, 0),
    RK3036_PLL_RATE(600000000, 1, 75, 3, 1, 1, 0),
    RK3036_PLL_RATE(594000000, 2, 99, 2, 1, 1, 0),
    RK3036_PLL_RATE(504000000, 1, 63, 3, 1, 1, 0),
    RK3036_PLL_RATE(500000000, 6, 250, 2, 1, 1, 0),
    RK3036_PLL_RATE(408000000, 1, 68, 2, 2, 1, 0),
    RK3036_PLL_RATE(312000000, 1, 52, 2, 2, 1, 0),
    RK3036_PLL_RATE(216000000, 1, 72, 4, 2, 1, 0),
    RK3036_PLL_RATE(96000000, 1, 64, 4, 4, 1, 0),
    {/* sentinel */},
};

#define RK3228_DIV_CPU_MASK   0x1f
#define RK3228_DIV_CPU_SHIFT  8

#define RK3228_DIV_PERI_MASK  0xf
#define RK3228_DIV_PERI_SHIFT 0
#define RK3228_DIV_ACLK_MASK  0x7
#define RK3228_DIV_ACLK_SHIFT 4
#define RK3228_DIV_HCLK_MASK  0x3
#define RK3228_DIV_HCLK_SHIFT 8
#define RK3228_DIV_PCLK_MASK  0x7
#define RK3228_DIV_PCLK_SHIFT 12

#define RK3228_CLKSEL1(_core_aclock_div, _core_peri_div)                                     \
    {                                                                                        \
        .reg = RK2928_CLKSEL_CON(1),                                                         \
        .val = HIWORD_UPDATE(_core_peri_div, RK3228_DIV_PERI_MASK, RK3228_DIV_PERI_SHIFT) |  \
               HIWORD_UPDATE(_core_aclock_div, RK3228_DIV_ACLK_MASK, RK3228_DIV_ACLK_SHIFT), \
    }

#define RK3228_CPUCLK_RATE(_prate, _core_aclock_div, _core_peri_div) \
    {                                                                \
        .prate = _prate,                                             \
        .divs  = {                                                   \
            RK3228_CLKSEL1(_core_aclock_div, _core_peri_div),       \
        },                                                          \
    }

static struct rockchip_cpuclock_rate_table rk3228_cpuclock_rates[] __initdata = {
    RK3228_CPUCLK_RATE(1800000000, 1, 7), RK3228_CPUCLK_RATE(1704000000, 1, 7), RK3228_CPUCLK_RATE(1608000000, 1, 7),
    RK3228_CPUCLK_RATE(1512000000, 1, 7), RK3228_CPUCLK_RATE(1488000000, 1, 5), RK3228_CPUCLK_RATE(1464000000, 1, 5),
    RK3228_CPUCLK_RATE(1416000000, 1, 5), RK3228_CPUCLK_RATE(1392000000, 1, 5), RK3228_CPUCLK_RATE(1296000000, 1, 5),
    RK3228_CPUCLK_RATE(1200000000, 1, 5), RK3228_CPUCLK_RATE(1104000000, 1, 5), RK3228_CPUCLK_RATE(1008000000, 1, 5),
    RK3228_CPUCLK_RATE(912000000, 1, 5),  RK3228_CPUCLK_RATE(816000000, 1, 3),  RK3228_CPUCLK_RATE(696000000, 1, 3),
    RK3228_CPUCLK_RATE(600000000, 1, 3),  RK3228_CPUCLK_RATE(408000000, 1, 1),  RK3228_CPUCLK_RATE(312000000, 1, 1),
    RK3228_CPUCLK_RATE(216000000, 1, 1),  RK3228_CPUCLK_RATE(96000000, 1, 1),
};

static const struct rockchip_cpuclock_reg_data rk3228_cpuclock_data = {
    .core_reg       = RK2928_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0x1f,
    .mux_core_alt   = 1,
    .mux_core_main  = 0,
    .mux_core_shift = 6,
    .mux_core_mask  = 0x1,
};

PNAME(mux_pll_p)           = {"clock_24m", "xin24m"};

PNAME(mux_ddrphy_p)        = {"dpll", "gpll", "apll"};
PNAME(mux_armclock_p)      = {"apll_core", "gpll_core", "dpll_core"};
PNAME(mux_usb480m_phy_p)   = {"usb480m_phy0", "usb480m_phy1"};
PNAME(mux_usb480m_p)       = {"usb480m_phy", "xin24m"};
PNAME(mux_hdmiphy_p)       = {"hdmiphy_phy", "xin24m"};

PNAME(mux_pll_src_4plls_p) = {
    "cpll", "gpll",
    "hdmiphy"
    "usb480m"};
PNAME(mux_pll_src_3plls_p)                                      = {"cpll", "gpll", "hdmiphy"};
PNAME(mux_pll_src_2plls_p)                                      = {"cpll", "gpll"};
PNAME(mux_sclock_hdmi_cec_p)                                    = {"cpll", "gpll", "xin24m"};
PNAME(mux_mmc_src_p)                                            = {"cpll", "gpll", "xin24m", "usb480m"};
PNAME(mux_pll_src_cpll_gpll_usb480m_p)                          = {"cpll", "gpll", "usb480m"};

PNAME(mux_sclock_rga_p)                                         = {"gpll", "cpll", "sclock_rga_src"};

PNAME(mux_sclock_vop_src_p)                                     = {"gpll", "cpll"};
PNAME(mux_dclock_vop_p)                                         = {"hdmiphy", "sclock_vop_pre"};

PNAME(mux_i2s0_p)                                               = {"i2s0_src", "i2s0_frac", "ext_i2s", "xin12m"};
PNAME(mux_i2s1_pre_p)                                           = {"i2s1_src", "i2s1_frac", "ext_i2s", "xin12m"};
PNAME(mux_i2s_out_p)                                            = {"i2s1_pre", "xin12m"};
PNAME(mux_i2s2_p)                                               = {"i2s2_src", "i2s2_frac", "xin12m"};
PNAME(mux_sclock_spdif_p)                                       = {"sclock_spdif_src", "spdif_frac", "xin12m"};

PNAME(mux_uart0_p)                                              = {"uart0_src", "uart0_frac", "xin24m"};
PNAME(mux_uart1_p)                                              = {"uart1_src", "uart1_frac", "xin24m"};
PNAME(mux_uart2_p)                                              = {"uart2_src", "uart2_frac", "xin24m"};

PNAME(mux_sclock_mac_extclock_p)                                = {"ext_gmac", "phy_50m_out"};
PNAME(mux_sclock_gmac_pre_p)                                    = {"sclock_gmac_src", "sclock_mac_extclk"};
PNAME(mux_sclock_macphy_p)                                      = {"sclock_gmac_src", "ext_gmac"};

static struct rockchip_pll_clock rk3228_pll_clocks[] __initdata = {
    [apll] = PLL(pll_rk3036, PLL_APLL, "apll", mux_pll_p, 0, RK2928_PLL_CON(0), RK2928_MODE_CON, 0, 7, 0, rk3228_pll_rates),
    [dpll] = PLL(pll_rk3036, PLL_DPLL, "dpll", mux_pll_p, 0, RK2928_PLL_CON(3), RK2928_MODE_CON, 4, 6, 0, NULL),
    [cpll] = PLL(pll_rk3036, PLL_CPLL, "cpll", mux_pll_p, 0, RK2928_PLL_CON(6), RK2928_MODE_CON, 8, 8, 0, NULL),
    [gpll] = PLL(pll_rk3036, PLL_GPLL, "gpll", mux_pll_p, 0, RK2928_PLL_CON(9), RK2928_MODE_CON, 12, 9, 0, rk3228_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

static struct rockchip_clock_branch rk3228_i2s0_fracmux __initdata =
    MUX(0, "i2s0_pre", mux_i2s0_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(9), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_i2s1_fracmux __initdata =
    MUX(0, "i2s1_pre", mux_i2s1_pre_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(3), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_i2s2_fracmux __initdata =
    MUX(0, "i2s2_pre", mux_i2s2_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(16), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_spdif_fracmux __initdata =
    MUX(SCLK_SPDIF, "sclock_spdif", mux_sclock_spdif_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(6), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_uart0_fracmux __initdata =
    MUX(SCLK_UART0, "sclock_uart0", mux_uart0_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(13), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_uart1_fracmux __initdata =
    MUX(SCLK_UART1, "sclock_uart1", mux_uart1_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(14), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_uart2_fracmux __initdata =
    MUX(SCLK_UART2, "sclock_uart2", mux_uart2_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(15), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3228_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    DIV(0, "clock_24m", "xin24m", CLK_IGNORE_UNUSED, RK2928_CLKSEL_CON(4), 8, 5, DFLAGS),

    /* PD_DDR */
    COMPOSITE_DDRCLK(SCLK_DDRC, "clock_ddrc", mux_ddrphy_p, 0, RK2928_CLKSEL_CON(26), 8, 2, 0, 2, ROCKCHIP_DDRCLK_SIP_V2),
    FACTOR(0, "clock_ddrphy", "clock_ddrc", 0, 1, 4),
    GATE(0, "ddrphy4x", "clock_ddrc", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(7), 1, GFLAGS),
    GATE(0, "ddrc", "ddrphy_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(8), 5, GFLAGS),
    FACTOR_GATE(0, "ddrphy", "ddrphy4x", CLK_IGNORE_UNUSED, 1, 4, RK2928_CLKGATE_CON(7), 0, GFLAGS),

    /* PD_CORE */
    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 6, GFLAGS),
    GATE(0, "gpll_core", "gpll", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 6, GFLAGS),
    GATE(0, "dpll_core", "dpll", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 6, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg", "armclk", CLK_IGNORE_UNUSED, RK2928_CLKSEL_CON(1), 0, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK2928_CLKGATE_CON(4), 1, GFLAGS),
    COMPOSITE_NOMUX(
        0, "armcore", "armclk", CLK_IGNORE_UNUSED, RK2928_CLKSEL_CON(1), 4, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK2928_CLKGATE_CON(4), 0, GFLAGS),

    /* PD_MISC */
    MUX(HDMIPHY, "hdmiphy", mux_hdmiphy_p, CLK_SET_RATE_PARENT, RK2928_MISC_CON, 13, 1, MFLAGS),
    MUX(0, "usb480m_phy", mux_usb480m_phy_p, CLK_SET_RATE_PARENT, RK2928_MISC_CON, 14, 1, MFLAGS),
    MUX(0, "usb480m", mux_usb480m_p, CLK_SET_RATE_PARENT, RK2928_MISC_CON, 15, 1, MFLAGS),

    /* PD_BUS */
    COMPOSITE(0, "aclock_cpu_src", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(0), 13, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(0), 1, GFLAGS),
    GATE(ACLK_CPU, "aclock_cpu", "aclock_cpu_src", 0, RK2928_CLKGATE_CON(6), 0, GFLAGS),
    COMPOSITE_NOMUX(HCLK_CPU, "hclock_cpu", "aclock_cpu_src", 0, RK2928_CLKSEL_CON(1), 8, 2, DFLAGS, RK2928_CLKGATE_CON(6), 1, GFLAGS),
    COMPOSITE_NOMUX(0, "pclock_bus_src", "aclock_cpu_src", 0, RK2928_CLKSEL_CON(1), 12, 3, DFLAGS, RK2928_CLKGATE_CON(6), 2, GFLAGS),
    GATE(PCLK_CPU, "pclock_cpu", "pclock_bus_src", 0, RK2928_CLKGATE_CON(6), 3, GFLAGS),
    GATE(0, "pclock_phy_pre", "pclock_bus_src", 0, RK2928_CLKGATE_CON(6), 4, GFLAGS),
    GATE(0, "pclock_ddr_pre", "pclock_bus_src", 0, RK2928_CLKGATE_CON(6), 13, GFLAGS),

    /* PD_VIDEO */
    COMPOSITE(
        ACLK_VPU_PRE, "aclock_vpu_pre", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(32), 5, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 11, GFLAGS),
    FACTOR_GATE(HCLK_VPU_PRE, "hclock_vpu_pre", "aclock_vpu_pre", 0, 1, 4, RK2928_CLKGATE_CON(4), 4, GFLAGS),

    COMPOSITE(
        ACLK_RKVDEC_PRE, "aclock_rkvdec_pre", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(28), 6, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 2,
        GFLAGS),
    FACTOR_GATE(HCLK_RKVDEC_PRE, "hclock_rkvdec_pre", "aclock_rkvdec_pre", 0, 1, 4, RK2928_CLKGATE_CON(4), 5, GFLAGS),

    COMPOSITE(
        SCLK_VDEC_CABAC, "sclock_vdec_cabac", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(28), 14, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(3), 3,
        GFLAGS),

    COMPOSITE(
        SCLK_VDEC_CORE, "sclock_vdec_core", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(34), 13, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(3), 4,
        GFLAGS),

    /* PD_VIO */
    COMPOSITE(
        ACLK_IEP_PRE, "aclock_iep_pre", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(31), 5, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 0, GFLAGS),
    DIV(HCLK_VIO_PRE, "hclock_vio_pre", "aclock_iep_pre", 0, RK2928_CLKSEL_CON(2), 0, 5, DFLAGS),

    COMPOSITE(
        ACLK_HDCP_PRE, "aclock_hdcp_pre", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(31), 13, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(1), 4,
        GFLAGS),

    MUX(0, "sclock_rga_src", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(33), 13, 2, MFLAGS),
    COMPOSITE_NOMUX(ACLK_RGA_PRE, "aclock_rga_pre", "sclock_rga_src", 0, RK2928_CLKSEL_CON(33), 8, 5, DFLAGS, RK2928_CLKGATE_CON(1), 2, GFLAGS),
    COMPOSITE(SCLK_RGA, "sclock_rga", mux_sclock_rga_p, 0, RK2928_CLKSEL_CON(22), 5, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 6, GFLAGS),

    COMPOSITE(
        ACLK_VOP_PRE, "aclock_vop_pre", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(33), 5, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(1), 1, GFLAGS),

    COMPOSITE(SCLK_HDCP, "sclock_hdcp", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(23), 14, 2, MFLAGS, 8, 6, DFLAGS, RK2928_CLKGATE_CON(3), 5, GFLAGS),

    GATE(SCLK_HDMI_HDCP, "sclock_hdmi_hdcp", "xin24m", 0, RK2928_CLKGATE_CON(3), 7, GFLAGS),

    COMPOSITE(
        SCLK_HDMI_CEC, "sclock_hdmi_cec", mux_sclock_hdmi_cec_p, 0, RK2928_CLKSEL_CON(21), 14, 2, MFLAGS, 0, 14, DFLAGS, RK2928_CLKGATE_CON(3), 8,
        GFLAGS),

    /* PD_PERI */
    COMPOSITE(0, "aclock_peri_src", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(10), 10, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(2), 0, GFLAGS),
    COMPOSITE_NOMUX(PCLK_PERI, "pclock_peri", "aclock_peri_src", 0, RK2928_CLKSEL_CON(10), 12, 3, DFLAGS, RK2928_CLKGATE_CON(5), 2, GFLAGS),
    COMPOSITE_NOMUX(HCLK_PERI, "hclock_peri", "aclock_peri_src", 0, RK2928_CLKSEL_CON(10), 8, 2, DFLAGS, RK2928_CLKGATE_CON(5), 1, GFLAGS),
    GATE(ACLK_PERI, "aclock_peri", "aclock_peri_src", 0, RK2928_CLKGATE_CON(5), 0, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, RK2928_CLKGATE_CON(6), 5, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, RK2928_CLKGATE_CON(6), 6, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, RK2928_CLKGATE_CON(6), 7, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, RK2928_CLKGATE_CON(6), 8, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, RK2928_CLKGATE_CON(6), 9, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, RK2928_CLKGATE_CON(6), 10, GFLAGS),

    COMPOSITE(
        SCLK_CRYPTO, "sclock_crypto", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(24), 5, 1, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(2), 7, GFLAGS),

    COMPOSITE(SCLK_TSP, "sclock_tsp", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(22), 15, 1, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(2), 6, GFLAGS),

    GATE(SCLK_HSADC, "sclock_hsadc", "ext_hsadc", 0, RK2928_CLKGATE_CON(10), 12, GFLAGS),

    COMPOSITE(
        SCLK_WIFI, "sclock_wifi", mux_pll_src_cpll_gpll_usb480m_p, 0, RK2928_CLKSEL_CON(23), 5, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(2), 15,
        GFLAGS),

    COMPOSITE(SCLK_SDMMC, "sclock_sdmmc", mux_mmc_src_p, 0, RK2928_CLKSEL_CON(11), 8, 2, MFLAGS, 0, 8, DFLAGS, RK2928_CLKGATE_CON(2), 11, GFLAGS),

    COMPOSITE_NODIV(SCLK_SDIO_SRC, "sclock_sdio_src", mux_mmc_src_p, 0, RK2928_CLKSEL_CON(11), 10, 2, MFLAGS, RK2928_CLKGATE_CON(2), 13, GFLAGS),
    DIV(SCLK_SDIO, "sclock_sdio", "sclock_sdio_src", 0, RK2928_CLKSEL_CON(12), 0, 8, DFLAGS),

    COMPOSITE_NODIV(0, "sclock_emmc_src", mux_mmc_src_p, 0, RK2928_CLKSEL_CON(11), 12, 2, MFLAGS, RK2928_CLKGATE_CON(2), 14, GFLAGS),
    DIV(SCLK_EMMC, "sclock_emmc", "sclock_emmc_src", 0, RK2928_CLKSEL_CON(12), 8, 8, DFLAGS),

    /*
     * Clock-Architecture Diagram 2
     */

    COMPOSITE_NODIV(0, "sclock_vop_src", mux_sclock_vop_src_p, 0, RK2928_CLKSEL_CON(27), 0, 1, MFLAGS, RK2928_CLKGATE_CON(3), 1, GFLAGS),
    DIV(DCLK_HDMI_PHY, "dclock_hdmiphy", "sclock_vop_src", 0, RK2928_CLKSEL_CON(29), 0, 3, DFLAGS),
    DIV(0, "sclock_vop_pre", "sclock_vop_src", 0, RK2928_CLKSEL_CON(27), 8, 8, DFLAGS),
    MUX(DCLK_VOP, "dclock_vop", mux_dclock_vop_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK2928_CLKSEL_CON(27), 1, 1, MFLAGS),

    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    COMPOSITE(0, "i2s0_src", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(9), 15, 1, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(0), 3, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "i2s0_frac", "i2s0_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(8), 0, RK2928_CLKGATE_CON(0), 4, GFLAGS, &rk3228_i2s0_fracmux,
        RK3228_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S0, "sclock_i2s0", "i2s0_pre", CLK_SET_RATE_PARENT, RK2928_CLKGATE_CON(0), 5, GFLAGS),

    COMPOSITE(0, "i2s1_src", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(3), 15, 1, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(0), 10, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "i2s1_frac", "i2s1_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(7), 0, RK2928_CLKGATE_CON(0), 11, GFLAGS, &rk3228_i2s1_fracmux,
        RK3228_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1, "sclock_i2s1", "i2s1_pre", CLK_SET_RATE_PARENT, RK2928_CLKGATE_CON(0), 14, GFLAGS),
    COMPOSITE_NODIV(SCLK_I2S_OUT, "i2s_out", mux_i2s_out_p, 0, RK2928_CLKSEL_CON(3), 12, 1, MFLAGS, RK2928_CLKGATE_CON(0), 13, GFLAGS),

    COMPOSITE(0, "i2s2_src", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(16), 15, 1, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(0), 7, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "i2s2_frac", "i2s2_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(30), 0, RK2928_CLKGATE_CON(0), 8, GFLAGS, &rk3228_i2s2_fracmux,
        RK3228_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S2, "sclock_i2s2", "i2s2_pre", CLK_SET_RATE_PARENT, RK2928_CLKGATE_CON(0), 9, GFLAGS),

    COMPOSITE(0, "sclock_spdif_src", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(6), 15, 1, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(2), 10, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "spdif_frac", "sclock_spdif_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(20), 0, RK2928_CLKGATE_CON(2), 12, GFLAGS, &rk3228_spdif_fracmux,
        RK3228_SPDIF_FRAC_MAX_PRATE),

    GATE(0, "jtag", "ext_jtag", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(1), 3, GFLAGS),

    GATE(SCLK_OTGPHY0, "sclock_otgphy0", "xin24m", 0, RK2928_CLKGATE_CON(1), 5, GFLAGS),
    GATE(SCLK_OTGPHY1, "sclock_otgphy1", "xin24m", 0, RK2928_CLKGATE_CON(1), 6, GFLAGS),

    COMPOSITE_NOMUX(SCLK_TSADC, "sclock_tsadc", "xin24m", 0, RK2928_CLKSEL_CON(24), 6, 10, DFLAGS, RK2928_CLKGATE_CON(2), 8, GFLAGS),

    COMPOSITE(0, "aclock_gpu_pre", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(34), 5, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 13, GFLAGS),

    COMPOSITE(SCLK_SPI0, "sclock_spi0", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(25), 8, 1, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(2), 9, GFLAGS),

    /* PD_UART */
    COMPOSITE(
        0, "uart0_src", mux_pll_src_cpll_gpll_usb480m_p, 0, RK2928_CLKSEL_CON(13), 12, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(1), 8, GFLAGS),
    COMPOSITE(
        0, "uart1_src", mux_pll_src_cpll_gpll_usb480m_p, 0, RK2928_CLKSEL_CON(14), 12, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(1), 10, GFLAGS),
    COMPOSITE(
        0, "uart2_src", mux_pll_src_cpll_gpll_usb480m_p, 0, RK2928_CLKSEL_CON(15), 12, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(1), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart0_frac", "uart0_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(17), 0, RK2928_CLKGATE_CON(1), 9, GFLAGS, &rk3228_uart0_fracmux,
        RK3228_UART_FRAC_MAX_PRATE),
    COMPOSITE_FRACMUX(
        0, "uart1_frac", "uart1_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(18), 0, RK2928_CLKGATE_CON(1), 11, GFLAGS, &rk3228_uart1_fracmux,
        RK3228_UART_FRAC_MAX_PRATE),
    COMPOSITE_FRACMUX(
        0, "uart2_frac", "uart2_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(19), 0, RK2928_CLKGATE_CON(1), 13, GFLAGS, &rk3228_uart2_fracmux,
        RK3228_UART_FRAC_MAX_PRATE),

    COMPOSITE(
        SCLK_NANDC, "sclock_nandc", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(2), 14, 1, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(1), 0, GFLAGS),

    COMPOSITE(
        SCLK_MAC_SRC, "sclock_gmac_src", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(5), 7, 1, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(1), 7, GFLAGS),
    MUX(SCLK_MAC_EXTCLK, "sclock_mac_extclk", mux_sclock_mac_extclock_p, 0, RK2928_CLKSEL_CON(29), 10, 1, MFLAGS),
    MUX(SCLK_MAC, "sclock_gmac_pre", mux_sclock_gmac_pre_p, 0, RK2928_CLKSEL_CON(5), 5, 1, MFLAGS),
    GATE(SCLK_MAC_REFOUT, "sclock_mac_refout", "sclock_gmac_pre", 0, RK2928_CLKGATE_CON(5), 4, GFLAGS),
    GATE(SCLK_MAC_REF, "sclock_mac_ref", "sclock_gmac_pre", 0, RK2928_CLKGATE_CON(5), 3, GFLAGS),
    GATE(SCLK_MAC_RX, "sclock_mac_rx", "sclock_gmac_pre", 0, RK2928_CLKGATE_CON(5), 5, GFLAGS),
    GATE(SCLK_MAC_TX, "sclock_mac_tx", "sclock_gmac_pre", 0, RK2928_CLKGATE_CON(5), 6, GFLAGS),
    COMPOSITE(
        SCLK_MAC_PHY, "sclock_macphy", mux_sclock_macphy_p, 0, RK2928_CLKSEL_CON(29), 12, 1, MFLAGS, 8, 2, DFLAGS, RK2928_CLKGATE_CON(5), 7, GFLAGS),
    COMPOSITE(
        SCLK_MAC_OUT, "sclock_gmac_out", mux_pll_src_2plls_p, 0, RK2928_CLKSEL_CON(5), 15, 1, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(2), 2, GFLAGS),

    /*
     * Clock-Architecture Diagram 3
     */

    /* PD_VOP */
    GATE(ACLK_RGA, "aclock_rga", "aclock_rga_pre", 0, RK2928_CLKGATE_CON(13), 0, GFLAGS),
    GATE(0, "aclock_rga_noc", "aclock_rga_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(13), 11, GFLAGS),
    GATE(ACLK_IEP, "aclock_iep", "aclock_iep_pre", 0, RK2928_CLKGATE_CON(13), 2, GFLAGS),
    GATE(0, "aclock_iep_noc", "aclock_iep_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(13), 9, GFLAGS),

    GATE(ACLK_VOP, "aclock_vop", "aclock_vop_pre", 0, RK2928_CLKGATE_CON(13), 5, GFLAGS),
    GATE(0, "aclock_vop_noc", "aclock_vop_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(13), 12, GFLAGS),

    GATE(ACLK_HDCP, "aclock_hdcp", "aclock_hdcp_pre", 0, RK2928_CLKGATE_CON(14), 10, GFLAGS),
    GATE(0, "aclock_hdcp_noc", "aclock_hdcp_pre", 0, RK2928_CLKGATE_CON(13), 10, GFLAGS),

    GATE(HCLK_RGA, "hclock_rga", "hclock_vio_pre", 0, RK2928_CLKGATE_CON(13), 1, GFLAGS),
    GATE(HCLK_IEP, "hclock_iep", "hclock_vio_pre", 0, RK2928_CLKGATE_CON(13), 3, GFLAGS),
    GATE(HCLK_VOP, "hclock_vop", "hclock_vio_pre", 0, RK2928_CLKGATE_CON(13), 6, GFLAGS),
    GATE(0, "hclock_vio_ahb_arbi", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(13), 7, GFLAGS),
    GATE(0, "hclock_vio_noc", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(13), 8, GFLAGS),
    GATE(0, "hclock_vop_noc", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(13), 13, GFLAGS),
    GATE(HCLK_VIO_H2P, "hclock_vio_h2p", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(14), 7, GFLAGS),
    GATE(HCLK_HDCP_MMU, "hclock_hdcp_mmu", "hclock_vio_pre", 0, RK2928_CLKGATE_CON(14), 12, GFLAGS),
    GATE(PCLK_HDMI_CTRL, "pclock_hdmi_ctrl", "hclock_vio_pre", 0, RK2928_CLKGATE_CON(14), 6, GFLAGS),
    GATE(PCLK_VIO_H2P, "pclock_vio_h2p", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(14), 8, GFLAGS),
    GATE(PCLK_HDCP, "pclock_hdcp", "hclock_vio_pre", 0, RK2928_CLKGATE_CON(14), 11, GFLAGS),

    /* PD_PERI */
    GATE(0, "aclock_peri_noc", "aclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(12), 0, GFLAGS),
    GATE(ACLK_GMAC, "aclock_gmac", "aclock_peri", 0, RK2928_CLKGATE_CON(11), 4, GFLAGS),

    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 0, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 1, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 2, GFLAGS),
    GATE(HCLK_NANDC, "hclock_nandc", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 3, GFLAGS),
    GATE(HCLK_HOST0, "hclock_host0", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 6, GFLAGS),
    GATE(0, "hclock_host0_arb", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(11), 7, GFLAGS),
    GATE(HCLK_HOST1, "hclock_host1", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 8, GFLAGS),
    GATE(0, "hclock_host1_arb", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(11), 9, GFLAGS),
    GATE(HCLK_HOST2, "hclock_host2", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 10, GFLAGS),
    GATE(HCLK_OTG, "hclock_otg", "hclock_peri", 0, RK2928_CLKGATE_CON(11), 12, GFLAGS),
    GATE(0, "hclock_otg_pmu", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(11), 13, GFLAGS),
    GATE(0, "hclock_host2_arb", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(11), 14, GFLAGS),
    GATE(0, "hclock_peri_noc", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(12), 1, GFLAGS),

    GATE(PCLK_GMAC, "pclock_gmac", "pclock_peri", 0, RK2928_CLKGATE_CON(11), 5, GFLAGS),
    GATE(0, "pclock_peri_noc", "pclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(12), 2, GFLAGS),

    /* PD_GPU */
    GATE(ACLK_GPU, "aclock_gpu", "aclock_gpu_pre", 0, RK2928_CLKGATE_CON(7), 14, GFLAGS),
    GATE(0, "aclock_gpu_noc", "aclock_gpu_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(7), 15, GFLAGS),

    /* PD_BUS */
    GATE(0, "sclock_initmem_mbist", "aclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(8), 1, GFLAGS),
    GATE(0, "aclock_initmem", "aclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(8), 0, GFLAGS),
    GATE(ACLK_DMAC, "aclock_dmac_bus", "aclock_cpu", 0, RK2928_CLKGATE_CON(8), 2, GFLAGS),
    GATE(0, "aclock_bus_noc", "aclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 1, GFLAGS),

    GATE(0, "hclock_rom", "hclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(8), 3, GFLAGS),
    GATE(HCLK_I2S0_8CH, "hclock_i2s0_8ch", "hclock_cpu", 0, RK2928_CLKGATE_CON(8), 7, GFLAGS),
    GATE(HCLK_I2S1_8CH, "hclock_i2s1_8ch", "hclock_cpu", 0, RK2928_CLKGATE_CON(8), 8, GFLAGS),
    GATE(HCLK_I2S2_2CH, "hclock_i2s2_2ch", "hclock_cpu", 0, RK2928_CLKGATE_CON(8), 9, GFLAGS),
    GATE(HCLK_SPDIF_8CH, "hclock_spdif_8ch", "hclock_cpu", 0, RK2928_CLKGATE_CON(8), 10, GFLAGS),
    GATE(HCLK_TSP, "hclock_tsp", "hclock_cpu", 0, RK2928_CLKGATE_CON(10), 11, GFLAGS),
    GATE(HCLK_M_CRYPTO, "hclock_crypto_mst", "hclock_cpu", 0, RK2928_CLKGATE_CON(8), 11, GFLAGS),
    GATE(HCLK_S_CRYPTO, "hclock_crypto_slv", "hclock_cpu", 0, RK2928_CLKGATE_CON(8), 12, GFLAGS),

    GATE(0, "pclock_ddrupctl", "pclock_ddr_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(8), 4, GFLAGS),
    GATE(0, "pclock_ddrmon", "pclock_ddr_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(8), 6, GFLAGS),
    GATE(0, "pclock_msch_noc", "pclock_ddr_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 2, GFLAGS),

    GATE(PCLK_EFUSE_1024, "pclock_efuse_1024", "pclock_cpu", 0, RK2928_CLKGATE_CON(8), 13, GFLAGS),
    GATE(PCLK_EFUSE_256, "pclock_efuse_256", "pclock_cpu", 0, RK2928_CLKGATE_CON(8), 14, GFLAGS),
    GATE(PCLK_I2C0, "pclock_i2c0", "pclock_cpu", 0, RK2928_CLKGATE_CON(8), 15, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 0, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 1, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 2, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer0", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 4, GFLAGS),
    GATE(0, "pclock_stimer", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 5, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 6, GFLAGS),
    GATE(PCLK_PWM, "pclock_rk_pwm", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 7, GFLAGS),
    GATE(PCLK_GPIO0, "pclock_gpio0", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 8, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 9, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 10, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 11, GFLAGS),
    GATE(PCLK_UART0, "pclock_uart0", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 12, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 13, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 14, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "pclock_cpu", 0, RK2928_CLKGATE_CON(9), 15, GFLAGS),
    GATE(PCLK_GRF, "pclock_grf", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 0, GFLAGS),
    GATE(0, "pclock_cru", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 1, GFLAGS),
    GATE(0, "pclock_sgrf", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 2, GFLAGS),
    GATE(0, "pclock_sim", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 3, GFLAGS),

    GATE(0, "pclock_ddrphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 3, GFLAGS),
    GATE(PCLK_ACODECPHY, "pclock_acodecphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 5, GFLAGS),
    GATE(PCLK_HDMI_PHY, "pclock_hdmiphy", "pclock_phy_pre", 0, RK2928_CLKGATE_CON(10), 7, GFLAGS),
    GATE(0, "pclock_vdacphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 8, GFLAGS),
    GATE(0, "pclock_phy_noc", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(10), 9, GFLAGS),

    GATE(ACLK_VPU, "aclock_vpu", "aclock_vpu_pre", 0, RK2928_CLKGATE_CON(15), 0, GFLAGS),
    GATE(0, "aclock_vpu_noc", "aclock_vpu_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(15), 4, GFLAGS),
    GATE(ACLK_RKVDEC, "aclock_rkvdec", "aclock_rkvdec_pre", 0, RK2928_CLKGATE_CON(15), 2, GFLAGS),
    GATE(0, "aclock_rkvdec_noc", "aclock_rkvdec_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(15), 6, GFLAGS),
    GATE(HCLK_VPU, "hclock_vpu", "hclock_vpu_pre", 0, RK2928_CLKGATE_CON(15), 1, GFLAGS),
    GATE(0, "hclock_vpu_noc", "hclock_vpu_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(15), 5, GFLAGS),
    GATE(HCLK_RKVDEC, "hclock_rkvdec", "hclock_rkvdec_pre", 0, RK2928_CLKGATE_CON(15), 3, GFLAGS),
    GATE(0, "hclock_rkvdec_noc", "hclock_rkvdec_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(15), 7, GFLAGS),

    /* PD_MMC */
    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "sclock_sdmmc", RK3228_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "sclock_sdmmc", RK3228_SDMMC_CON1, 1),

    MMC(SCLK_SDIO_DRV, "sdio_drv", "sclock_sdio", RK3228_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "sclock_sdio", RK3228_SDIO_CON1, 1),

    MMC(SCLK_EMMC_DRV, "emmc_drv", "sclock_emmc", RK3228_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "sclock_emmc", RK3228_EMMC_CON1, 1),
};

static const char *const rk3228_critical_clocks[] __initconst = {
    "aclock_cpu",       "pclock_cpu",        "hclock_cpu",      "aclock_peri",         "hclock_peri",          "pclock_peri",      "aclock_rga_noc",
    "aclock_iep_noc",   "aclock_vop_noc",    "aclock_hdcp_noc", "hclock_vio_ahb_arbi", "hclock_vio_noc",       "hclock_vop_noc",   "hclock_host0_arb",
    "hclock_host1_arb", "hclock_host2_arb",  "hclock_otg_pmu",  "aclock_gpu_noc",      "sclock_initmem_mbist", "aclock_initmem",   "hclock_rom",
    "pclock_ddrupctl",  "pclock_ddrmon",     "pclock_msch_noc", "pclock_stimer",       "pclock_ddrphy",        "pclock_acodecphy", "pclock_phy_noc",
    "aclock_vpu_noc",   "aclock_rkvdec_noc", "aclock_rkvdec",   "hclock_vpu_noc",      "hclock_rkvdec_noc",    "hclock_rkvdec",
};

static void __init rk3228_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    ctx = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(reg_base);
        return;
    }

    rockchip_clock_register_plls(ctx, rk3228_pll_clocks, ARRAY_SIZE(rk3228_pll_clocks), RK3228_GRF_SOC_STATUS0);
    rockchip_clock_register_branches(ctx, rk3228_clock_branches, ARRAY_SIZE(rk3228_clock_branches));
    rockchip_clock_protect_critical(rk3228_critical_clocks, ARRAY_SIZE(rk3228_critical_clocks));

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &rk3228_cpuclock_data, rk3228_cpuclock_rates,
        ARRAY_SIZE(rk3228_cpuclock_rates));

    rockchip_register_softrst(np, 9, reg_base + RK2928_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK3228_GLB_SRST_FST, NULL);

    rockchip_clock_of_add_provider(np, ctx);
}

CLK_OF_DECLARE(rk3228_cru, "rockchip,rk3228-cru", rk3228_clock_init);
