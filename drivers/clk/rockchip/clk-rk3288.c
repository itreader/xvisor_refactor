/*
 * Copyright (c) 2014 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
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

#include <asm/psci.h>
#include <dt-bindings/clock/rk3288-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define RK3288_GRF_SOC_CON(x)       (0x244 + x * 4)
#define RK3288_GRF_SOC_STATUS1      0x284
#define RK3288_UART_FRAC_MAX_PRATE  600000000
#define RK3288_I2S_FRAC_MAX_PRATE   600000000
#define RK3288_SPDIF_FRAC_MAX_PRATE 600000000

enum rk3288_plls {
    apll,
    dpll,
    cpll,
    gpll,
    npll,
};

static struct rockchip_pll_rate_table rk3288_pll_rates[] = {
    RK3066_PLL_RATE(2208000000, 1, 92, 1),       RK3066_PLL_RATE(2184000000, 1, 91, 1),
    RK3066_PLL_RATE(2160000000, 1, 90, 1),       RK3066_PLL_RATE(2136000000, 1, 89, 1),
    RK3066_PLL_RATE(2112000000, 1, 88, 1),       RK3066_PLL_RATE(2088000000, 1, 87, 1),
    RK3066_PLL_RATE(2064000000, 1, 86, 1),       RK3066_PLL_RATE(2040000000, 1, 85, 1),
    RK3066_PLL_RATE(2016000000, 1, 84, 1),       RK3066_PLL_RATE(1992000000, 1, 83, 1),
    RK3066_PLL_RATE(1968000000, 1, 82, 1),       RK3066_PLL_RATE(1944000000, 1, 81, 1),
    RK3066_PLL_RATE(1920000000, 1, 80, 1),       RK3066_PLL_RATE(1896000000, 1, 79, 1),
    RK3066_PLL_RATE(1872000000, 1, 78, 1),       RK3066_PLL_RATE(1848000000, 1, 77, 1),
    RK3066_PLL_RATE(1824000000, 1, 76, 1),       RK3066_PLL_RATE(1800000000, 1, 75, 1),
    RK3066_PLL_RATE(1776000000, 1, 74, 1),       RK3066_PLL_RATE(1752000000, 1, 73, 1),
    RK3066_PLL_RATE(1728000000, 1, 72, 1),       RK3066_PLL_RATE(1704000000, 1, 71, 1),
    RK3066_PLL_RATE(1680000000, 1, 70, 1),       RK3066_PLL_RATE(1656000000, 1, 69, 1),
    RK3066_PLL_RATE(1632000000, 1, 68, 1),       RK3066_PLL_RATE(1608000000, 1, 67, 1),
    RK3066_PLL_RATE(1560000000, 1, 65, 1),       RK3066_PLL_RATE(1512000000, 1, 63, 1),
    RK3066_PLL_RATE(1488000000, 1, 62, 1),       RK3066_PLL_RATE(1464000000, 1, 61, 1),
    RK3066_PLL_RATE(1440000000, 1, 60, 1),       RK3066_PLL_RATE(1416000000, 1, 59, 1),
    RK3066_PLL_RATE(1392000000, 1, 58, 1),       RK3066_PLL_RATE(1368000000, 1, 57, 1),
    RK3066_PLL_RATE(1344000000, 1, 56, 1),       RK3066_PLL_RATE(1320000000, 1, 55, 1),
    RK3066_PLL_RATE(1296000000, 1, 54, 1),       RK3066_PLL_RATE(1272000000, 1, 53, 1),
    RK3066_PLL_RATE(1248000000, 1, 52, 1),       RK3066_PLL_RATE(1224000000, 1, 51, 1),
    RK3066_PLL_RATE(1200000000, 1, 50, 1),       RK3066_PLL_RATE(1188000000, 2, 99, 1),
    RK3066_PLL_RATE(1176000000, 1, 49, 1),       RK3066_PLL_RATE(1128000000, 1, 47, 1),
    RK3066_PLL_RATE(1104000000, 1, 46, 1),       RK3066_PLL_RATE(1008000000, 1, 84, 2),
    RK3066_PLL_RATE(912000000, 1, 76, 2),        RK3066_PLL_RATE(891000000, 8, 594, 2),
    RK3066_PLL_RATE(888000000, 1, 74, 2),        RK3066_PLL_RATE(816000000, 1, 68, 2),
    RK3066_PLL_RATE(798000000, 2, 133, 2),       RK3066_PLL_RATE(792000000, 1, 66, 2),
    RK3066_PLL_RATE(768000000, 1, 64, 2),        RK3066_PLL_RATE(742500000, 8, 495, 2),
    RK3066_PLL_RATE(696000000, 1, 58, 2),        RK3066_PLL_RATE(600000000, 1, 50, 2),
    RK3066_PLL_RATE_NB(594000000, 2, 198, 4, 1), RK3066_PLL_RATE(552000000, 1, 46, 2),
    RK3066_PLL_RATE(504000000, 1, 84, 4),        RK3066_PLL_RATE(500000000, 3, 125, 2),
    RK3066_PLL_RATE(456000000, 1, 76, 4),        RK3066_PLL_RATE(408000000, 1, 68, 4),
    RK3066_PLL_RATE(400000000, 3, 100, 2),       RK3066_PLL_RATE(384000000, 2, 128, 4),
    RK3066_PLL_RATE(360000000, 1, 60, 4),        RK3066_PLL_RATE(312000000, 1, 52, 4),
    RK3066_PLL_RATE(300000000, 1, 50, 4),        RK3066_PLL_RATE(297000000, 2, 198, 8),
    RK3066_PLL_RATE(252000000, 1, 84, 8),        RK3066_PLL_RATE(216000000, 1, 72, 8),
    RK3066_PLL_RATE(148500000, 2, 99, 8),        RK3066_PLL_RATE(126000000, 1, 84, 16),
    RK3066_PLL_RATE(48000000, 1, 64, 32),        {/* sentinel */},
};

#define RK3288_DIV_ACLK_CORE_M0_MASK  0xf
#define RK3288_DIV_ACLK_CORE_M0_SHIFT 0
#define RK3288_DIV_ACLK_CORE_MP_MASK  0xf
#define RK3288_DIV_ACLK_CORE_MP_SHIFT 4
#define RK3288_DIV_L2RAM_MASK         0x7
#define RK3288_DIV_L2RAM_SHIFT        0
#define RK3288_DIV_ATCLK_MASK         0x1f
#define RK3288_DIV_ATCLK_SHIFT        4
#define RK3288_DIV_PCLK_DBGPRE_MASK   0x1f
#define RK3288_DIV_PCLK_DBGPRE_SHIFT  9

#define RK3288_CLKSEL0(_core_m0, _core_mp)                                                                                                           \
    {                                                                                                                                                \
        .reg = RK3288_CLKSEL_CON(0),                                                                                                                 \
        .val = HIWORD_UPDATE(_core_m0, RK3288_DIV_ACLK_CORE_M0_MASK, RK3288_DIV_ACLK_CORE_M0_SHIFT) |                                                \
               HIWORD_UPDATE(_core_mp, RK3288_DIV_ACLK_CORE_MP_MASK, RK3288_DIV_ACLK_CORE_MP_SHIFT),                                                 \
    }
#define RK3288_CLKSEL37(_l2ram, _atclk, _pclock_dbg_pre)                                                                                             \
    {                                                                                                                                                \
        .reg = RK3288_CLKSEL_CON(37),                                                                                                                \
        .val = HIWORD_UPDATE(_l2ram, RK3288_DIV_L2RAM_MASK, RK3288_DIV_L2RAM_SHIFT) |                                                                \
               HIWORD_UPDATE(_atclk, RK3288_DIV_ATCLK_MASK, RK3288_DIV_ATCLK_SHIFT) |                                                                \
               HIWORD_UPDATE(_pclock_dbg_pre, RK3288_DIV_PCLK_DBGPRE_MASK, RK3288_DIV_PCLK_DBGPRE_SHIFT),                                            \
    }

#define RK3288_CPUCLK_RATE(_prate, _core_m0, _core_mp, _l2ram, _atclk, _pdbg)                                                                        \
    {                                                                                                                                                \
        .prate = _prate,                                                                                                                             \
        .divs  = {                                                                                                                                   \
            RK3288_CLKSEL0(_core_m0, _core_mp),                                                                                                     \
            RK3288_CLKSEL37(_l2ram, _atclk, _pdbg),                                                                                                 \
        },                                                                                                                                          \
    }

static struct rockchip_cpuclock_rate_table rk3288_cpuclock_rates[] __initdata = {
    RK3288_CPUCLK_RATE(1800000000, 1, 3, 1, 3, 3), RK3288_CPUCLK_RATE(1704000000, 1, 3, 1, 3, 3), RK3288_CPUCLK_RATE(1608000000, 1, 3, 1, 3, 3),
    RK3288_CPUCLK_RATE(1512000000, 1, 3, 1, 3, 3), RK3288_CPUCLK_RATE(1416000000, 1, 3, 1, 3, 3), RK3288_CPUCLK_RATE(1200000000, 1, 3, 1, 3, 3),
    RK3288_CPUCLK_RATE(1008000000, 1, 3, 1, 3, 3), RK3288_CPUCLK_RATE(816000000, 1, 3, 1, 3, 3),  RK3288_CPUCLK_RATE(696000000, 1, 3, 1, 3, 3),
    RK3288_CPUCLK_RATE(600000000, 1, 3, 1, 3, 3),  RK3288_CPUCLK_RATE(408000000, 1, 3, 1, 3, 3),  RK3288_CPUCLK_RATE(312000000, 1, 3, 1, 3, 3),
    RK3288_CPUCLK_RATE(216000000, 1, 3, 1, 3, 3),  RK3288_CPUCLK_RATE(126000000, 1, 3, 1, 3, 3),
};

static const struct rockchip_cpuclock_reg_data rk3288_cpuclock_data = {
    .core_reg       = RK3288_CLKSEL_CON(0),
    .div_core_shift = 8,
    .div_core_mask  = 0x1f,
    .mux_core_alt   = 1,
    .mux_core_main  = 0,
    .mux_core_shift = 15,
    .mux_core_mask  = 0x1,
};

PNAME(mux_pll_p)                                                = {"xin24m", "xin32k"};
PNAME(mux_armclock_p)                                           = {"apll_core", "gpll_core"};
PNAME(mux_ddrphy_p)                                             = {"dpll_ddr", "gpll_ddr"};
PNAME(mux_aclock_cpu_src_p)                                     = {"cpll_aclock_cpu", "gpll_aclock_cpu"};

PNAME(mux_pll_src_cpll_gpll_p)                                  = {"cpll", "gpll"};
PNAME(mux_pll_src_npll_cpll_gpll_p)                             = {"npll", "cpll", "gpll"};
PNAME(mux_pll_src_cpll_gpll_npll_p)                             = {"cpll", "gpll", "npll"};
PNAME(mux_pll_src_cpll_gpll_usb480m_p)                          = {"cpll", "gpll", "usbphy480m_src"};
PNAME(mux_pll_src_cpll_gll_usb_npll_p)                          = {"cpll", "gpll", "usbphy480m_src", "npll"};

PNAME(mux_mmc_src_p)                                            = {"cpll", "gpll", "xin24m", "xin24m"};
PNAME(mux_i2s_pre_p)                                            = {"i2s_src", "i2s_frac", "ext_i2s", "xin12m"};
PNAME(mux_i2s_clockout_p)                                       = {"i2s_pre", "xin12m"};
PNAME(mux_spdif_p)                                              = {"spdif_pre", "spdif_frac", "xin12m"};
PNAME(mux_spdif_8ch_p)                                          = {"spdif_8ch_pre", "spdif_8ch_frac", "xin12m"};
PNAME(mux_uart0_p)                                              = {"uart0_src", "uart0_frac", "xin24m"};
PNAME(mux_uart1_p)                                              = {"uart1_src", "uart1_frac", "xin24m"};
PNAME(mux_uart2_p)                                              = {"uart2_src", "uart2_frac", "xin24m"};
PNAME(mux_uart3_p)                                              = {"uart3_src", "uart3_frac", "xin24m"};
PNAME(mux_uart4_p)                                              = {"uart4_src", "uart4_frac", "xin24m"};
PNAME(mux_vip_out_p)                                            = {"vip_src", "xin24m"};
PNAME(mux_mac_p)                                                = {"mac_pll_src", "ext_gmac"};
PNAME(mux_hsadcout_p)                                           = {"hsadc_src", "ext_hsadc"};
PNAME(mux_edp_24m_p)                                            = {"ext_edp_24m", "xin24m"};
PNAME(mux_tspout_p)                                             = {"cpll", "gpll", "npll", "xin27m"};

PNAME(mux_usbphy480m_p)                                         = {"sclock_otgphy1_480m", "sclock_otgphy2_480m", "sclock_otgphy0_480m"};
PNAME(mux_hsicphy480m_p)                                        = {"cpll", "gpll", "usbphy480m_src"};
PNAME(mux_hsicphy12m_p)                                         = {"hsicphy12m_xin12m", "hsicphy12m_usbphy"};

static struct rockchip_pll_clock rk3288_pll_clocks[] __initdata = {
    [apll] = PLL(pll_rk3066, PLL_APLL, "apll", mux_pll_p, 0, RK3288_PLL_CON(0), RK3288_MODE_CON, 0, 6, 0, rk3288_pll_rates),
    [dpll] = PLL(pll_rk3066, PLL_DPLL, "dpll", mux_pll_p, 0, RK3288_PLL_CON(4), RK3288_MODE_CON, 4, 5, 0, NULL),
    [cpll] = PLL(pll_rk3066, PLL_CPLL, "cpll", mux_pll_p, 0, RK3288_PLL_CON(8), RK3288_MODE_CON, 8, 7, 0, rk3288_pll_rates),
    [gpll] = PLL(pll_rk3066, PLL_GPLL, "gpll", mux_pll_p, 0, RK3288_PLL_CON(12), RK3288_MODE_CON, 12, 8, 0, rk3288_pll_rates),
    [npll] = PLL(pll_rk3066, PLL_NPLL, "npll", mux_pll_p, 0, RK3288_PLL_CON(16), RK3288_MODE_CON, 14, 9, ROCKCHIP_PLL_SYNC_RATE, rk3288_pll_rates),
};

static struct clock_div_table div_hclock_cpu_t[] = {
    {.val = 0, .div = 1},
    {.val = 1, .div = 2},
    {.val = 3, .div = 4},
    {/* sentinel */},
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)
#define IFLAGS ROCKCHIP_INVERTER_HIWORD_MASK

static struct rockchip_clock_branch rk3288_i2s_fracmux __initdata =
    MUX(0, "i2s_pre", mux_i2s_pre_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(4), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_spdif_fracmux __initdata =
    MUX(0, "spdif_mux", mux_spdif_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(5), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_spdif_8ch_fracmux __initdata =
    MUX(0, "spdif_8ch_mux", mux_spdif_8ch_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(40), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_uart0_fracmux __initdata =
    MUX(SCLK_UART0, "sclock_uart0", mux_uart0_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(13), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_uart1_fracmux __initdata =
    MUX(SCLK_UART1, "sclock_uart1", mux_uart1_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(14), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_uart2_fracmux __initdata =
    MUX(SCLK_UART2, "sclock_uart2", mux_uart2_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(15), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_uart3_fracmux __initdata =
    MUX(SCLK_UART3, "sclock_uart3", mux_uart3_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(16), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_uart4_fracmux __initdata =
    MUX(SCLK_UART4, "sclock_uart4", mux_uart4_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(3), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3288_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 1, GFLAGS),
    GATE(0, "gpll_core", "gpll", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 2, GFLAGS),

    COMPOSITE_NOMUX(
        0, "armcore0", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(36), 0, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 0, GFLAGS),
    COMPOSITE_NOMUX(
        0, "armcore1", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(36), 4, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 1, GFLAGS),
    COMPOSITE_NOMUX(
        0, "armcore2", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(36), 8, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 2, GFLAGS),
    COMPOSITE_NOMUX(
        0, "armcore3", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(36), 12, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 3, GFLAGS),
    COMPOSITE_NOMUX(
        0, "l2ram", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(37), 0, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 4, GFLAGS),
    COMPOSITE_NOMUX(
        0, "aclock_core_m0", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(0), 0, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 5,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "aclock_core_mp", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(0), 4, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 6,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "atclk", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(37), 4, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 7, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg_pre", "armclk", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(37), 9, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3288_CLKGATE_CON(12), 8,
        GFLAGS),
    GATE(0, "pclock_dbg", "pclock_dbg_pre", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(12), 9, GFLAGS),
    GATE(0, "cs_dbg", "pclock_dbg_pre", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(12), 10, GFLAGS),
    GATE(0, "pclock_core_niu", "pclock_dbg_pre", 0, RK3288_CLKGATE_CON(12), 11, GFLAGS),

    GATE(0, "dpll_ddr", "dpll", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 8, GFLAGS),
    GATE(0, "gpll_ddr", "gpll", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 9, GFLAGS),
    COMPOSITE_DDRCLK(SCLK_DDRCLK, "sclock_ddrc", mux_ddrphy_p, 0, RK3288_CLKSEL_CON(26), 2, 1, 0, 0, ROCKCHIP_DDRCLK_SIP_V2),
    COMPOSITE_NOGATE(0, "ddrphy", mux_ddrphy_p, CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(26), 2, 1, MFLAGS, 0, 2, DFLAGS | CLK_DIVIDER_POWER_OF_TWO),

    GATE(0, "gpll_aclock_cpu", "gpll", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 10, GFLAGS),
    GATE(0, "cpll_aclock_cpu", "cpll", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 11, GFLAGS),
    COMPOSITE_NOGATE(0, "aclock_cpu_src", mux_aclock_cpu_src_p, CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(1), 15, 1, MFLAGS, 3, 5, DFLAGS),
    DIV(0, "aclock_cpu_pre", "aclock_cpu_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(1), 0, 3, DFLAGS),
    GATE(ACLK_CPU, "aclock_cpu", "aclock_cpu_pre", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 3, GFLAGS),
    COMPOSITE_NOMUX(
        PCLK_CPU, "pclock_cpu", "aclock_cpu_pre", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(1), 12, 3, DFLAGS, RK3288_CLKGATE_CON(0), 5, GFLAGS),
    COMPOSITE_NOMUX_DIVTBL(
        HCLK_CPU, "hclock_cpu", "aclock_cpu_pre", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(1), 8, 2, DFLAGS, div_hclock_cpu_t, RK3288_CLKGATE_CON(0), 4,
        GFLAGS),
    GATE(0, "c2c_host", "aclock_cpu_src", 0, RK3288_CLKGATE_CON(13), 8, GFLAGS),
    COMPOSITE_NOMUX(SCLK_CRYPTO, "crypto", "aclock_cpu_pre", 0, RK3288_CLKSEL_CON(26), 6, 2, DFLAGS, RK3288_CLKGATE_CON(5), 4, GFLAGS),
    GATE(0, "aclock_bus_2pmu", "aclock_cpu_pre", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(0), 7, GFLAGS),

    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    COMPOSITE(
        SCLK_I2S_SRC, "i2s_src", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(4), 15, 1, MFLAGS, 0, 7, DFLAGS, RK3288_CLKGATE_CON(4), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "i2s_frac", "i2s_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(8), 0, RK3288_CLKGATE_CON(4), 2, GFLAGS, &rk3288_i2s_fracmux,
        RK3288_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(SCLK_I2S0_OUT, "i2s0_clockout", mux_i2s_clockout_p, 0, RK3288_CLKSEL_CON(4), 12, 1, MFLAGS, RK3288_CLKGATE_CON(4), 0, GFLAGS),
    GATE(SCLK_I2S0, "sclock_i2s0", "i2s_pre", CLK_SET_RATE_PARENT, RK3288_CLKGATE_CON(4), 3, GFLAGS),

    MUX(0, "spdif_src", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(5), 15, 1, MFLAGS),
    COMPOSITE_NOMUX(0, "spdif_pre", "spdif_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(5), 0, 7, DFLAGS, RK3288_CLKGATE_CON(4), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "spdif_frac", "spdif_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(9), 0, RK3288_CLKGATE_CON(4), 5, GFLAGS, &rk3288_spdif_fracmux,
        RK3288_SPDIF_FRAC_MAX_PRATE),
    GATE(SCLK_SPDIF, "sclock_spdif", "spdif_mux", CLK_SET_RATE_PARENT, RK3288_CLKGATE_CON(4), 6, GFLAGS),
    COMPOSITE_NOMUX(0, "spdif_8ch_pre", "spdif_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(40), 0, 7, DFLAGS, RK3288_CLKGATE_CON(4), 7, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "spdif_8ch_frac", "spdif_8ch_pre", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(41), 0, RK3288_CLKGATE_CON(4), 8, GFLAGS,
        &rk3288_spdif_8ch_fracmux, RK3288_SPDIF_FRAC_MAX_PRATE),
    GATE(SCLK_SPDIF8CH, "sclock_spdif_8ch", "spdif_8ch_mux", CLK_SET_RATE_PARENT, RK3288_CLKGATE_CON(4), 9, GFLAGS),

    GATE(0, "sclock_acc_efuse", "xin24m", 0, RK3288_CLKGATE_CON(0), 12, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, RK3288_CLKGATE_CON(1), 0, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, RK3288_CLKGATE_CON(1), 1, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, RK3288_CLKGATE_CON(1), 2, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, RK3288_CLKGATE_CON(1), 3, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, RK3288_CLKGATE_CON(1), 4, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, RK3288_CLKGATE_CON(1), 5, GFLAGS),

    /*
     * Clock-Architecture Diagram 2
     */

    COMPOSITE(
        0, "aclock_vepu", mux_pll_src_cpll_gpll_usb480m_p, 0, RK3288_CLKSEL_CON(32), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3288_CLKGATE_CON(3), 9, GFLAGS),
    COMPOSITE(
        0, "aclock_vdpu", mux_pll_src_cpll_gpll_usb480m_p, 0, RK3288_CLKSEL_CON(32), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3288_CLKGATE_CON(3), 11, GFLAGS),
    /*
     * We use aclock_vdpu by default GRF_SOC_CON0[7] setting in system,
     * so we ignore the mux and make clocks nodes as following,
     */
    GATE(ACLK_VCODEC, "aclock_vcodec", "aclock_vdpu", 0, RK3288_CLKGATE_CON(9), 0, GFLAGS),

    FACTOR_GATE(0, "hclock_vcodec_pre", "aclock_vdpu", 0, 1, 4, RK3288_CLKGATE_CON(3), 10, GFLAGS),

    GATE(HCLK_VCODEC, "hclock_vcodec", "hclock_vcodec_pre", 0, RK3288_CLKGATE_CON(9), 1, GFLAGS),

    COMPOSITE(
        ACLK_VIO0, "aclock_vio0", mux_pll_src_cpll_gpll_usb480m_p, CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(31), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3288_CLKGATE_CON(3), 0, GFLAGS),
    COMPOSITE(
        ACLK_VIO1, "aclock_vio1", mux_pll_src_cpll_gpll_usb480m_p, CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(31), 14, 2, MFLAGS, 8, 5, DFLAGS,
        RK3288_CLKGATE_CON(3), 2, GFLAGS),

    COMPOSITE(
        0, "aclock_rga_pre", mux_pll_src_cpll_gpll_usb480m_p, 0, RK3288_CLKSEL_CON(30), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3288_CLKGATE_CON(3), 5, GFLAGS),
    COMPOSITE(
        SCLK_RGA, "sclock_rga", mux_pll_src_cpll_gpll_usb480m_p, 0, RK3288_CLKSEL_CON(30), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3288_CLKGATE_CON(3), 4,
        GFLAGS),

    COMPOSITE(
        DCLK_VOP0, "dclock_vop0", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(27), 0, 2, MFLAGS, 8, 8, DFLAGS, RK3288_CLKGATE_CON(3), 1,
        GFLAGS),
    COMPOSITE(
        DCLK_VOP1, "dclock_vop1", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(29), 6, 2, MFLAGS, 8, 8, DFLAGS, RK3288_CLKGATE_CON(3), 3,
        GFLAGS),

    COMPOSITE_NODIV(SCLK_EDP_24M, "sclock_edp_24m", mux_edp_24m_p, 0, RK3288_CLKSEL_CON(28), 15, 1, MFLAGS, RK3288_CLKGATE_CON(3), 12, GFLAGS),
    COMPOSITE(
        SCLK_EDP, "sclock_edp", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(28), 6, 2, MFLAGS, 0, 6, DFLAGS, RK3288_CLKGATE_CON(3), 13,
        GFLAGS),

    COMPOSITE(
        SCLK_ISP, "sclock_isp", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(6), 6, 2, MFLAGS, 0, 6, DFLAGS, RK3288_CLKGATE_CON(3), 14, GFLAGS),
    COMPOSITE(
        SCLK_ISP_JPE, "sclock_isp_jpe", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(6), 14, 2, MFLAGS, 8, 6, DFLAGS, RK3288_CLKGATE_CON(3), 15,
        GFLAGS),

    GATE(SCLK_HDMI_HDCP, "sclock_hdmi_hdcp", "xin24m", 0, RK3288_CLKGATE_CON(5), 12, GFLAGS),
    GATE(SCLK_HDMI_CEC, "sclock_hdmi_cec", "xin32k", 0, RK3288_CLKGATE_CON(5), 11, GFLAGS),

    COMPOSITE(
        ACLK_HEVC, "aclock_hevc", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(39), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3288_CLKGATE_CON(13), 13,
        GFLAGS),
    DIV(HCLK_HEVC, "hclock_hevc", "aclock_hevc", 0, RK3288_CLKSEL_CON(40), 12, 2, DFLAGS),

    COMPOSITE(
        SCLK_HEVC_CABAC, "sclock_hevc_cabac", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(42), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3288_CLKGATE_CON(13), 14, GFLAGS),
    COMPOSITE(
        SCLK_HEVC_CORE, "sclock_hevc_core", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(42), 14, 2, MFLAGS, 8, 5, DFLAGS,
        RK3288_CLKGATE_CON(13), 15, GFLAGS),

    COMPOSITE_NODIV(0, "vip_src", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(26), 8, 1, MFLAGS, RK3288_CLKGATE_CON(3), 7, GFLAGS),
    COMPOSITE_NOGATE(SCLK_VIP_OUT, "sclock_vip_out", mux_vip_out_p, 0, RK3288_CLKSEL_CON(26), 15, 1, MFLAGS, 9, 5, DFLAGS),

    DIV(PCLK_PD_ALIVE, "pclock_pd_alive", "gpll", 0, RK3288_CLKSEL_CON(33), 8, 5, DFLAGS),
    COMPOSITE_NOMUX(PCLK_PD_PMU, "pclock_pd_pmu", "gpll", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(33), 0, 5, DFLAGS, RK3288_CLKGATE_CON(5), 8, GFLAGS),

    COMPOSITE(
        SCLK_GPU, "sclock_gpu", mux_pll_src_cpll_gll_usb_npll_p, 0, RK3288_CLKSEL_CON(34), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3288_CLKGATE_CON(5), 7,
        GFLAGS),

    COMPOSITE(
        0, "aclock_peri_src", mux_pll_src_cpll_gpll_p, CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(10), 15, 1, MFLAGS, 0, 5, DFLAGS, RK3288_CLKGATE_CON(2),
        0, GFLAGS),
    COMPOSITE_NOMUX(
        PCLK_PERI, "pclock_peri", "aclock_peri_src", 0, RK3288_CLKSEL_CON(10), 12, 2, DFLAGS | CLK_DIVIDER_POWER_OF_TWO, RK3288_CLKGATE_CON(2), 3,
        GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_PERI, "hclock_peri", "aclock_peri_src", CLK_IGNORE_UNUSED, RK3288_CLKSEL_CON(10), 8, 2, DFLAGS | CLK_DIVIDER_POWER_OF_TWO,
        RK3288_CLKGATE_CON(2), 2, GFLAGS),
    GATE(ACLK_PERI, "aclock_peri", "aclock_peri_src", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(2), 1, GFLAGS),

    /*
     * Clock-Architecture Diagram 3
     */

    COMPOSITE(
        SCLK_SPI0, "sclock_spi0", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(25), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3288_CLKGATE_CON(2), 9, GFLAGS),
    COMPOSITE(
        SCLK_SPI1, "sclock_spi1", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(25), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3288_CLKGATE_CON(2), 10, GFLAGS),
    COMPOSITE(
        SCLK_SPI2, "sclock_spi2", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(39), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3288_CLKGATE_CON(2), 11, GFLAGS),

    COMPOSITE(SCLK_SDMMC, "sclock_sdmmc", mux_mmc_src_p, 0, RK3288_CLKSEL_CON(11), 6, 2, MFLAGS, 0, 6, DFLAGS, RK3288_CLKGATE_CON(13), 0, GFLAGS),
    COMPOSITE(SCLK_SDIO0, "sclock_sdio0", mux_mmc_src_p, 0, RK3288_CLKSEL_CON(12), 6, 2, MFLAGS, 0, 6, DFLAGS, RK3288_CLKGATE_CON(13), 1, GFLAGS),
    COMPOSITE(SCLK_SDIO1, "sclock_sdio1", mux_mmc_src_p, 0, RK3288_CLKSEL_CON(34), 14, 2, MFLAGS, 8, 6, DFLAGS, RK3288_CLKGATE_CON(13), 2, GFLAGS),
    COMPOSITE(SCLK_EMMC, "sclock_emmc", mux_mmc_src_p, 0, RK3288_CLKSEL_CON(12), 14, 2, MFLAGS, 8, 6, DFLAGS, RK3288_CLKGATE_CON(13), 3, GFLAGS),

    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "sclock_sdmmc", RK3288_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "sclock_sdmmc", RK3288_SDMMC_CON1, 0),

    MMC(SCLK_SDIO0_DRV, "sdio0_drv", "sclock_sdio0", RK3288_SDIO0_CON0, 1),
    MMC(SCLK_SDIO0_SAMPLE, "sdio0_sample", "sclock_sdio0", RK3288_SDIO0_CON1, 0),

    MMC(SCLK_SDIO1_DRV, "sdio1_drv", "sclock_sdio1", RK3288_SDIO1_CON0, 1),
    MMC(SCLK_SDIO1_SAMPLE, "sdio1_sample", "sclock_sdio1", RK3288_SDIO1_CON1, 0),

    MMC(SCLK_EMMC_DRV, "emmc_drv", "sclock_emmc", RK3288_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "sclock_emmc", RK3288_EMMC_CON1, 0),

    COMPOSITE(SCLK_TSPOUT, "sclock_tspout", mux_tspout_p, 0, RK3288_CLKSEL_CON(35), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3288_CLKGATE_CON(4), 11, GFLAGS),
    COMPOSITE(
        SCLK_TSP, "sclock_tsp", mux_pll_src_cpll_gpll_npll_p, 0, RK3288_CLKSEL_CON(35), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3288_CLKGATE_CON(4), 10,
        GFLAGS),

    GATE(SCLK_OTGPHY0, "sclock_otgphy0", "xin24m", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(13), 4, GFLAGS),
    GATE(SCLK_OTGPHY1, "sclock_otgphy1", "xin24m", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(13), 5, GFLAGS),
    GATE(SCLK_OTGPHY2, "sclock_otgphy2", "xin24m", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(13), 6, GFLAGS),
    GATE(SCLK_OTG_ADP, "sclock_otg_adp", "xin32k", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(13), 7, GFLAGS),

    COMPOSITE_NOMUX(SCLK_TSADC, "sclock_tsadc", "xin32k", 0, RK3288_CLKSEL_CON(2), 0, 6, DFLAGS, RK3288_CLKGATE_CON(2), 7, GFLAGS),

    COMPOSITE_NOMUX(SCLK_SARADC, "sclock_saradc", "xin24m", 0, RK3288_CLKSEL_CON(24), 8, 8, DFLAGS, RK3288_CLKGATE_CON(2), 8, GFLAGS),

    GATE(SCLK_PS2C, "sclock_ps2c", "xin24m", 0, RK3288_CLKGATE_CON(5), 13, GFLAGS),

    COMPOSITE(
        SCLK_NANDC0, "sclock_nandc0", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(38), 7, 1, MFLAGS, 0, 5, DFLAGS, RK3288_CLKGATE_CON(5), 5,
        GFLAGS),
    COMPOSITE(
        SCLK_NANDC1, "sclock_nandc1", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(38), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3288_CLKGATE_CON(5), 6,
        GFLAGS),

    COMPOSITE(
        0, "uart0_src", mux_pll_src_cpll_gll_usb_npll_p, 0, RK3288_CLKSEL_CON(13), 13, 2, MFLAGS, 0, 7, DFLAGS, RK3288_CLKGATE_CON(1), 8, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart0_frac", "uart0_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(17), 0, RK3288_CLKGATE_CON(1), 9, GFLAGS, &rk3288_uart0_fracmux,
        RK3288_UART_FRAC_MAX_PRATE),
    MUX(0, "uart_src", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(13), 15, 1, MFLAGS),
    COMPOSITE_NOMUX(0, "uart1_src", "uart_src", 0, RK3288_CLKSEL_CON(14), 0, 7, DFLAGS, RK3288_CLKGATE_CON(1), 10, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart1_frac", "uart1_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(18), 0, RK3288_CLKGATE_CON(1), 11, GFLAGS, &rk3288_uart1_fracmux,
        RK3288_UART_FRAC_MAX_PRATE),
    COMPOSITE_NOMUX(0, "uart2_src", "uart_src", 0, RK3288_CLKSEL_CON(15), 0, 7, DFLAGS, RK3288_CLKGATE_CON(1), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart2_frac", "uart2_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(19), 0, RK3288_CLKGATE_CON(1), 13, GFLAGS, &rk3288_uart2_fracmux,
        RK3288_UART_FRAC_MAX_PRATE),
    COMPOSITE_NOMUX(0, "uart3_src", "uart_src", 0, RK3288_CLKSEL_CON(16), 0, 7, DFLAGS, RK3288_CLKGATE_CON(1), 14, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart3_frac", "uart3_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(20), 0, RK3288_CLKGATE_CON(1), 15, GFLAGS, &rk3288_uart3_fracmux,
        RK3288_UART_FRAC_MAX_PRATE),
    COMPOSITE_NOMUX(0, "uart4_src", "uart_src", 0, RK3288_CLKSEL_CON(3), 0, 7, DFLAGS, RK3288_CLKGATE_CON(2), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart4_frac", "uart4_src", CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(7), 0, RK3288_CLKGATE_CON(2), 13, GFLAGS, &rk3288_uart4_fracmux,
        RK3288_UART_FRAC_MAX_PRATE),

    COMPOSITE(
        SCLK_MAC_PLL, "mac_pll_src", mux_pll_src_npll_cpll_gpll_p, 0, RK3288_CLKSEL_CON(21), 0, 2, MFLAGS, 8, 5, DFLAGS, RK3288_CLKGATE_CON(2), 5,
        GFLAGS),
    MUX(SCLK_MAC, "mac_clock", mux_mac_p, CLK_SET_RATE_PARENT, RK3288_CLKSEL_CON(21), 4, 1, MFLAGS),
    GATE(SCLK_MACREF_OUT, "sclock_macref_out", "mac_clock", 0, RK3288_CLKGATE_CON(5), 3, GFLAGS),
    GATE(SCLK_MACREF, "sclock_macref", "mac_clock", 0, RK3288_CLKGATE_CON(5), 2, GFLAGS),
    GATE(SCLK_MAC_RX, "sclock_mac_rx", "mac_clock", 0, RK3288_CLKGATE_CON(5), 0, GFLAGS),
    GATE(SCLK_MAC_TX, "sclock_mac_tx", "mac_clock", 0, RK3288_CLKGATE_CON(5), 1, GFLAGS),

    COMPOSITE(0, "hsadc_src", mux_pll_src_cpll_gpll_p, 0, RK3288_CLKSEL_CON(22), 0, 1, MFLAGS, 8, 8, DFLAGS, RK3288_CLKGATE_CON(2), 6, GFLAGS),
    MUX(0, "sclock_hsadc_out", mux_hsadcout_p, 0, RK3288_CLKSEL_CON(22), 4, 1, MFLAGS),
    INVERTER(SCLK_HSADC, "sclock_hsadc", "sclock_hsadc_out", RK3288_CLKSEL_CON(22), 7, IFLAGS),

    GATE(0, "jtag", "ext_jtag", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(4), 14, GFLAGS),

    COMPOSITE_NODIV(
        SCLK_USBPHY480M_SRC, "usbphy480m_src", mux_usbphy480m_p, 0, RK3288_CLKSEL_CON(13), 11, 2, MFLAGS, RK3288_CLKGATE_CON(5), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_HSICPHY480M, "sclock_hsicphy480m", mux_hsicphy480m_p, 0, RK3288_CLKSEL_CON(29), 0, 2, MFLAGS, RK3288_CLKGATE_CON(3), 6, GFLAGS),
    GATE(0, "hsicphy12m_xin12m", "xin12m", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(13), 9, GFLAGS),
    DIV(0, "hsicphy12m_usbphy", "sclock_hsicphy480m", 0, RK3288_CLKSEL_CON(11), 8, 6, DFLAGS),
    MUX(SCLK_HSICPHY12M, "sclock_hsicphy12m", mux_hsicphy12m_p, 0, RK3288_CLKSEL_CON(22), 4, 1, MFLAGS),

    /*
     * Clock-Architecture Diagram 4
     */

    /* aclock_cpu gates */
    GATE(0, "sclock_intmem0", "aclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(10), 5, GFLAGS),
    GATE(0, "sclock_intmem1", "aclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(10), 6, GFLAGS),
    GATE(0, "sclock_intmem2", "aclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(10), 7, GFLAGS),
    GATE(ACLK_DMAC1, "aclock_dmac1", "aclock_cpu", 0, RK3288_CLKGATE_CON(10), 12, GFLAGS),
    GATE(0, "aclock_strc_sys", "aclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(10), 13, GFLAGS),
    GATE(0, "aclock_intmem", "aclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(10), 4, GFLAGS),
    GATE(ACLK_CRYPTO, "aclock_crypto", "aclock_cpu", 0, RK3288_CLKGATE_CON(11), 6, GFLAGS),
    GATE(0, "aclock_ccp", "aclock_cpu", 0, RK3288_CLKGATE_CON(11), 8, GFLAGS),

    /* hclock_cpu gates */
    GATE(HCLK_CRYPTO, "hclock_crypto", "hclock_cpu", 0, RK3288_CLKGATE_CON(11), 7, GFLAGS),
    GATE(HCLK_I2S0, "hclock_i2s0", "hclock_cpu", 0, RK3288_CLKGATE_CON(10), 8, GFLAGS),
    GATE(HCLK_ROM, "hclock_rom", "hclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(10), 9, GFLAGS),
    GATE(HCLK_SPDIF, "hclock_spdif", "hclock_cpu", 0, RK3288_CLKGATE_CON(10), 10, GFLAGS),
    GATE(HCLK_SPDIF8CH, "hclock_spdif_8ch", "hclock_cpu", 0, RK3288_CLKGATE_CON(10), 11, GFLAGS),

    /* pclock_cpu gates */
    GATE(PCLK_PWM, "pclock_pwm", "pclock_cpu", 0, RK3288_CLKGATE_CON(10), 0, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer", "pclock_cpu", 0, RK3288_CLKGATE_CON(10), 1, GFLAGS),
    GATE(PCLK_I2C0, "pclock_i2c0", "pclock_cpu", 0, RK3288_CLKGATE_CON(10), 2, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "pclock_cpu", 0, RK3288_CLKGATE_CON(10), 3, GFLAGS),
    GATE(PCLK_DDRUPCTL0, "pclock_ddrupctl0", "pclock_cpu", 0, RK3288_CLKGATE_CON(10), 14, GFLAGS),
    GATE(PCLK_PUBL0, "pclock_publ0", "pclock_cpu", 0, RK3288_CLKGATE_CON(10), 15, GFLAGS),
    GATE(PCLK_DDRUPCTL1, "pclock_ddrupctl1", "pclock_cpu", 0, RK3288_CLKGATE_CON(11), 0, GFLAGS),
    GATE(PCLK_PUBL1, "pclock_publ1", "pclock_cpu", 0, RK3288_CLKGATE_CON(11), 1, GFLAGS),
    GATE(PCLK_EFUSE1024, "pclock_efuse_1024", "pclock_cpu", 0, RK3288_CLKGATE_CON(11), 2, GFLAGS),
    GATE(PCLK_TZPC, "pclock_tzpc", "pclock_cpu", 0, RK3288_CLKGATE_CON(11), 3, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_cpu", 0, RK3288_CLKGATE_CON(11), 9, GFLAGS),
    GATE(PCLK_EFUSE256, "pclock_efuse_256", "pclock_cpu", 0, RK3288_CLKGATE_CON(11), 10, GFLAGS),
    GATE(PCLK_RKPWM, "pclock_rkpwm", "pclock_cpu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(11), 11, GFLAGS),

    /* ddrctrl [DDR Controller PHY clock] gates */
    GATE(0, "nclock_ddrupctl0", "ddrphy", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(11), 4, GFLAGS),
    GATE(0, "nclock_ddrupctl1", "ddrphy", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(11), 5, GFLAGS),

    /* ddrphy gates */
    GATE(0, "sclock_ddrphy0", "ddrphy", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(4), 12, GFLAGS),
    GATE(0, "sclock_ddrphy1", "ddrphy", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(4), 13, GFLAGS),

    /* aclock_peri gates */
    GATE(0, "aclock_peri_axi_matrix", "aclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(6), 2, GFLAGS),
    GATE(ACLK_DMAC2, "aclock_dmac2", "aclock_peri", 0, RK3288_CLKGATE_CON(6), 3, GFLAGS),
    GATE(0, "aclock_peri_niu", "aclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 11, GFLAGS),
    GATE(ACLK_MMU, "aclock_mmu", "aclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(8), 12, GFLAGS),
    GATE(ACLK_GMAC, "aclock_gmac", "aclock_peri", 0, RK3288_CLKGATE_CON(8), 0, GFLAGS),
    GATE(HCLK_GPS, "hclock_gps", "aclock_peri", 0, RK3288_CLKGATE_CON(8), 2, GFLAGS),

    /* hclock_peri gates */
    GATE(0, "hclock_peri_matrix", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(6), 0, GFLAGS),
    GATE(HCLK_OTG0, "hclock_otg0", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 4, GFLAGS),
    GATE(HCLK_USBHOST0, "hclock_host0", "hclock_peri", 0, RK3288_CLKGATE_CON(7), 6, GFLAGS),
    GATE(HCLK_USBHOST1, "hclock_host1", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 7, GFLAGS),
    GATE(HCLK_HSIC, "hclock_hsic", "hclock_peri", 0, RK3288_CLKGATE_CON(7), 8, GFLAGS),
    GATE(HCLK_USB_PERI, "hclock_usb_peri", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 9, GFLAGS),
    GATE(0, "hclock_peri_ahb_arbi", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 10, GFLAGS),
    GATE(0, "hclock_emem", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 12, GFLAGS),
    GATE(0, "hclock_mem", "hclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(7), 13, GFLAGS),
    GATE(HCLK_NANDC0, "hclock_nandc0", "hclock_peri", 0, RK3288_CLKGATE_CON(7), 14, GFLAGS),
    GATE(HCLK_NANDC1, "hclock_nandc1", "hclock_peri", 0, RK3288_CLKGATE_CON(7), 15, GFLAGS),
    GATE(HCLK_TSP, "hclock_tsp", "hclock_peri", 0, RK3288_CLKGATE_CON(8), 8, GFLAGS),
    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_peri", 0, RK3288_CLKGATE_CON(8), 3, GFLAGS),
    GATE(HCLK_SDIO0, "hclock_sdio0", "hclock_peri", 0, RK3288_CLKGATE_CON(8), 4, GFLAGS),
    GATE(HCLK_SDIO1, "hclock_sdio1", "hclock_peri", 0, RK3288_CLKGATE_CON(8), 5, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_peri", 0, RK3288_CLKGATE_CON(8), 6, GFLAGS),
    GATE(HCLK_HSADC, "hclock_hsadc", "hclock_peri", 0, RK3288_CLKGATE_CON(8), 7, GFLAGS),
    GATE(0, "pmu_hclock_otg0", "hclock_peri", 0, RK3288_CLKGATE_CON(7), 5, GFLAGS),

    /* pclock_peri gates */
    GATE(0, "pclock_peri_matrix", "pclock_peri", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(6), 1, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 4, GFLAGS),
    GATE(PCLK_SPI1, "pclock_spi1", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 5, GFLAGS),
    GATE(PCLK_SPI2, "pclock_spi2", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 6, GFLAGS),
    GATE(PCLK_PS2C, "pclock_ps2c", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 7, GFLAGS),
    GATE(PCLK_UART0, "pclock_uart0", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 8, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 9, GFLAGS),
    GATE(PCLK_I2C4, "pclock_i2c4", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 15, GFLAGS),
    GATE(PCLK_UART3, "pclock_uart3", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 11, GFLAGS),
    GATE(PCLK_UART4, "pclock_uart4", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 12, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 13, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "pclock_peri", 0, RK3288_CLKGATE_CON(6), 14, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "pclock_peri", 0, RK3288_CLKGATE_CON(7), 1, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "pclock_peri", 0, RK3288_CLKGATE_CON(7), 2, GFLAGS),
    GATE(PCLK_SIM, "pclock_sim", "pclock_peri", 0, RK3288_CLKGATE_CON(7), 3, GFLAGS),
    GATE(PCLK_I2C5, "pclock_i2c5", "pclock_peri", 0, RK3288_CLKGATE_CON(7), 0, GFLAGS),
    GATE(PCLK_GMAC, "pclock_gmac", "pclock_peri", 0, RK3288_CLKGATE_CON(8), 1, GFLAGS),

    GATE(SCLK_LCDC_PWM0, "sclock_lcdc_pwm0", "xin24m", 0, RK3288_CLKGATE_CON(13), 10, GFLAGS),
    GATE(SCLK_LCDC_PWM1, "sclock_lcdc_pwm1", "xin24m", 0, RK3288_CLKGATE_CON(13), 11, GFLAGS),
    GATE(SCLK_PVTM_CORE, "sclock_pvtm_core", "xin24m", 0, RK3288_CLKGATE_CON(5), 9, GFLAGS),
    GATE(SCLK_PVTM_GPU, "sclock_pvtm_gpu", "xin24m", 0, RK3288_CLKGATE_CON(5), 10, GFLAGS),
    GATE(SCLK_MIPIDSI_24M, "sclock_mipidsi_24m", "xin24m", 0, RK3288_CLKGATE_CON(5), 15, GFLAGS),

    /* sclock_gpu gates */
    GATE(ACLK_GPU, "aclock_gpu", "sclock_gpu", 0, RK3288_CLKGATE_CON(18), 0, GFLAGS),

    /* pclock_pd_alive gates */
    GATE(PCLK_GPIO8, "pclock_gpio8", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 8, GFLAGS),
    GATE(PCLK_GPIO7, "pclock_gpio7", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 7, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 1, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 2, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 3, GFLAGS),
    GATE(PCLK_GPIO4, "pclock_gpio4", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 4, GFLAGS),
    GATE(PCLK_GPIO5, "pclock_gpio5", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 5, GFLAGS),
    GATE(PCLK_GPIO6, "pclock_gpio6", "pclock_pd_alive", 0, RK3288_CLKGATE_CON(14), 6, GFLAGS),
    GATE(PCLK_GRF, "pclock_grf", "pclock_pd_alive", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(14), 11, GFLAGS),
    GATE(0, "pclock_alive_niu", "pclock_pd_alive", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(14), 12, GFLAGS),

    /* pclock_pd_pmu gates */
    GATE(PCLK_PMU, "pclock_pmu", "pclock_pd_pmu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(17), 0, GFLAGS),
    GATE(0, "pclock_intmem1", "pclock_pd_pmu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(17), 1, GFLAGS),
    GATE(0, "pclock_pmu_niu", "pclock_pd_pmu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(17), 2, GFLAGS),
    GATE(PCLK_SGRF, "pclock_sgrf", "pclock_pd_pmu", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(17), 3, GFLAGS),
    GATE(PCLK_GPIO0, "pclock_gpio0", "pclock_pd_pmu", 0, RK3288_CLKGATE_CON(17), 4, GFLAGS),

    /* hclock_vio gates */
    GATE(HCLK_RGA, "hclock_rga", "hclock_vio", 0, RK3288_CLKGATE_CON(15), 1, GFLAGS),
    GATE(HCLK_VOP0, "hclock_vop0", "hclock_vio", 0, RK3288_CLKGATE_CON(15), 6, GFLAGS),
    GATE(HCLK_VOP1, "hclock_vop1", "hclock_vio", 0, RK3288_CLKGATE_CON(15), 8, GFLAGS),
    GATE(HCLK_VIO_AHB_ARBI, "hclock_vio_ahb_arbi", "hclock_vio", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(15), 9, GFLAGS),
    GATE(HCLK_VIO_NIU, "hclock_vio_niu", "hclock_vio", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(15), 10, GFLAGS),
    GATE(HCLK_VIP, "hclock_vip", "hclock_vio", 0, RK3288_CLKGATE_CON(15), 15, GFLAGS),
    GATE(HCLK_IEP, "hclock_iep", "hclock_vio", 0, RK3288_CLKGATE_CON(15), 3, GFLAGS),
    GATE(HCLK_ISP, "hclock_isp", "hclock_vio", 0, RK3288_CLKGATE_CON(16), 1, GFLAGS),
    GATE(HCLK_VIO2_H2P, "hclock_vio2_h2p", "hclock_vio", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(16), 10, GFLAGS),
    GATE(PCLK_MIPI_DSI0, "pclock_mipi_dsi0", "hclock_vio", 0, RK3288_CLKGATE_CON(16), 4, GFLAGS),
    GATE(PCLK_MIPI_DSI1, "pclock_mipi_dsi1", "hclock_vio", 0, RK3288_CLKGATE_CON(16), 5, GFLAGS),
    GATE(PCLK_MIPI_CSI, "pclock_mipi_csi", "hclock_vio", 0, RK3288_CLKGATE_CON(16), 6, GFLAGS),
    GATE(PCLK_LVDS_PHY, "pclock_lvds_phy", "hclock_vio", 0, RK3288_CLKGATE_CON(16), 7, GFLAGS),
    GATE(PCLK_EDP_CTRL, "pclock_edp_ctrl", "hclock_vio", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(16), 8, GFLAGS),
    GATE(PCLK_HDMI_CTRL, "pclock_hdmi_ctrl", "hclock_vio", 0, RK3288_CLKGATE_CON(16), 9, GFLAGS),
    GATE(PCLK_VIO2_H2P, "pclock_vio2_h2p", "hclock_vio", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(16), 11, GFLAGS),

    /* aclock_vio0 gates */
    GATE(ACLK_VOP0, "aclock_vop0", "aclock_vio0", 0, RK3288_CLKGATE_CON(15), 5, GFLAGS),
    GATE(ACLK_IEP, "aclock_iep", "aclock_vio0", 0, RK3288_CLKGATE_CON(15), 2, GFLAGS),
    GATE(ACLK_VIO0_NIU, "aclock_vio0_niu", "aclock_vio0", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(15), 11, GFLAGS),
    GATE(ACLK_VIP, "aclock_vip", "aclock_vio0", 0, RK3288_CLKGATE_CON(15), 14, GFLAGS),

    /* aclock_vio1 gates */
    GATE(ACLK_VOP1, "aclock_vop1", "aclock_vio1", 0, RK3288_CLKGATE_CON(15), 7, GFLAGS),
    GATE(ACLK_ISP, "aclock_isp", "aclock_vio1", 0, RK3288_CLKGATE_CON(16), 2, GFLAGS),
    GATE(ACLK_VIO1_NIU, "aclock_vio1_niu", "aclock_vio1", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(15), 12, GFLAGS),

    /* aclock_rga_pre gates */
    GATE(ACLK_RGA, "aclock_rga", "aclock_rga_pre", 0, RK3288_CLKGATE_CON(15), 0, GFLAGS),
    GATE(ACLK_RGA_NIU, "aclock_rga_niu", "aclock_rga_pre", CLK_IGNORE_UNUSED, RK3288_CLKGATE_CON(15), 13, GFLAGS),

    /*
     * Other ungrouped clocks.
     */

    GATE(PCLK_VIP_IN, "pclock_vip_in", "ext_vip", 0, RK3288_CLKGATE_CON(16), 0, GFLAGS),
    INVERTER(PCLK_VIP, "pclock_vip", "pclock_vip_in", RK3288_CLKSEL_CON(29), 4, IFLAGS),
    GATE(PCLK_ISP_IN, "pclock_isp_in", "ext_isp", 0, RK3288_CLKGATE_CON(16), 3, GFLAGS),
    INVERTER(0, "pclock_isp", "pclock_isp_in", RK3288_CLKSEL_CON(29), 3, IFLAGS),

    GATE(SCLK_HSADC0_TSP, "clock_hsadc0_tsp", "ext_hsadc0_tsp", 0, RK3288_CLKGATE_CON(8), 9, GFLAGS),
    GATE(SCLK_HSADC1_TSP, "clock_hsadc1_tsp", "ext_hsadc0_tsp", 0, RK3288_CLKGATE_CON(8), 10, GFLAGS),
    GATE(SCLK_27M_TSP, "clock_27m_tsp", "ext_27m_tsp", 0, RK3288_CLKGATE_CON(8), 11, GFLAGS),
};

static const char *const rk3288_critical_clocks[] __initconst = {
    "aclock_cpu",       "aclock_peri",    "aclock_peri_niu",  "aclock_vio0_niu", "aclock_vio1_niu", "aclock_rga_niu",
    "hclock_peri",      "hclock_vio_niu", "pclock_alive_niu", "pclock_pd_pmu",   "pclock_pmu_niu",  "pclock_core_niu",
    "pclock_ddrupctl0", "pclock_publ0",   "pclock_ddrupctl1", "pclock_publ1",    "pmu_hclock_otg0", "aclock_dmac1",
};

static void __iomem *rk3288_cru_base;

/*
 * Some CRU registers will be reset in maskrom when the system
 * wakes up from fastboot.
 * So save them before suspend, restore them after resume.
 */
static const int rk3288_saved_cru_reg_ids[] = {
    RK3288_MODE_CON, RK3288_CLKSEL_CON(0), RK3288_CLKSEL_CON(1), RK3288_CLKSEL_CON(10), RK3288_CLKSEL_CON(33), RK3288_CLKSEL_CON(37),
};

static uint32_t rk3288_saved_cru_regs[ARRAY_SIZE(rk3288_saved_cru_reg_ids)];

static int rk3288_clock_suspend(void)
{
    int i, reg_id;

    for (i = 0; i < ARRAY_SIZE(rk3288_saved_cru_reg_ids); i++) {
        reg_id                   = rk3288_saved_cru_reg_ids[i];

        rk3288_saved_cru_regs[i] = readl_relaxed(rk3288_cru_base + reg_id);
    }

    /*
     * Switch PLLs other than DPLL (for SDRAM) to slow mode to
     * avoid crashes on resume. The Mask ROM on the system will
     * put APLL, CPLL, and GPLL into slow mode at resume time
     * anyway (which is why we restore them), but we might not
     * even make it to the Mask ROM if this isn't done at suspend
     * time.
     *
     * NOTE: only APLL truly matters here, but we'll do them all.
     */

    writel_relaxed(0xf3030000, rk3288_cru_base + RK3288_MODE_CON);

    return 0;
}

static void rk3288_clock_resume(void)
{
    int i, reg_id;

    for (i = ARRAY_SIZE(rk3288_saved_cru_reg_ids) - 1; i >= 0; i--) {
        reg_id = rk3288_saved_cru_reg_ids[i];

        writel_relaxed(rk3288_saved_cru_regs[i] | 0xffff0000, rk3288_cru_base + reg_id);
    }
}

static void rk3288_clock_shutdown(void)
{
    writel_relaxed(0xf3030000, rk3288_cru_base + RK3288_MODE_CON);
}

static struct syscore_ops rk3288_clock_syscore_ops = {
    .suspend = rk3288_clock_suspend,
    .resume  = rk3288_clock_resume,
};

static void __init rk3288_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    struct clk                     *clk;

    rk3288_cru_base = of_iomap(np, 0);

    if (!rk3288_cru_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    ctx = rockchip_clock_init(np, rk3288_cru_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(rk3288_cru_base);
        return;
    }

    /* Watchdog pclk is controlled by RK3288_SGRF_SOC_CON0[1]. */
    clk = clock_register_fixed_factor(NULL, "pclock_wdt", "pclock_pd_alive", 0, 1, 1);

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock pclock_wdt: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, PCLK_WDT);
    }

    rockchip_clock_register_plls(ctx, rk3288_pll_clocks, ARRAY_SIZE(rk3288_pll_clocks), RK3288_GRF_SOC_STATUS1);
    rockchip_clock_register_branches(ctx, rk3288_clock_branches, ARRAY_SIZE(rk3288_clock_branches));

    if (of_machine_is_compatible("rockchip,rk3288w")) {
        clk = clock_register_divider(NULL, "hclock_vio", "aclock_vio1", 0, ctx->reg_base + RK3288_CLKSEL_CON(28), 8, 5, DFLAGS, &ctx->lock);
    } else {
        clk = clock_register_divider(NULL, "hclock_vio", "aclock_vio0", 0, ctx->reg_base + RK3288_CLKSEL_CON(28), 8, 5, DFLAGS, &ctx->lock);
    }

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock hclock_vio: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, HCLK_VIO);
    }

    rockchip_clock_protect_critical(rk3288_critical_clocks, ARRAY_SIZE(rk3288_critical_clocks));

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &rk3288_cpuclock_data, rk3288_cpuclock_rates,
        ARRAY_SIZE(rk3288_cpuclock_rates));

    rockchip_register_softrst(np, 12, rk3288_cru_base + RK3288_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK3288_GLB_SRST_FST, rk3288_clock_shutdown);

    if (!psci_smp_available()) {
        register_syscore_ops(&rk3288_clock_syscore_ops);
    }

    rockchip_clock_of_add_provider(np, ctx);
}

CLK_OF_DECLARE(rk3288_cru, "rockchip,rk3288-cru", rk3288_clock_init);
