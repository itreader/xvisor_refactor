/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 * Author: Elaine <zhangqing@rock-chips.com>
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

#include <dt-bindings/clock/rk3128-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/rockchip/cpu.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define RK3128_GRF_SOC_STATUS0      0x14c
#define RK3128_UART_FRAC_MAX_PRATE  600000000
#define RK3128_I2S_FRAC_MAX_PRATE   600000000
#define RK3128_SPDIF_FRAC_MAX_PRATE 600000000

enum rk3128_plls {
    apll,
    dpll,
    cpll,
    gpll,
};

static struct rockchip_pll_rate_table rk3128_pll_rates[] = {
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

#define RK3128_DIV_CPU_MASK   0x1f
#define RK3128_DIV_CPU_SHIFT  8

#define RK3128_DIV_PERI_MASK  0xf
#define RK3128_DIV_PERI_SHIFT 0
#define RK3128_DIV_ACLK_MASK  0x7
#define RK3128_DIV_ACLK_SHIFT 4
#define RK3128_DIV_HCLK_MASK  0x3
#define RK3128_DIV_HCLK_SHIFT 8
#define RK3128_DIV_PCLK_MASK  0x7
#define RK3128_DIV_PCLK_SHIFT 12

#define RK3128_CLKSEL1(_core_aclock_div, _pclock_dbg_div)                                                                                            \
    {                                                                                                                                                \
        .reg = RK2928_CLKSEL_CON(1),                                                                                                                 \
        .val = HIWORD_UPDATE(_pclock_dbg_div, RK3128_DIV_PERI_MASK, RK3128_DIV_PERI_SHIFT) |                                                         \
               HIWORD_UPDATE(_core_aclock_div, RK3128_DIV_ACLK_MASK, RK3128_DIV_ACLK_SHIFT),                                                         \
    }

#define RK3128_CPUCLK_RATE(_prate, _core_aclock_div, _pclock_dbg_div)                                                                                \
    {                                                                                                                                                \
        .prate = _prate,                                                                                                                             \
        .divs  = {                                                                                                                                   \
            RK3128_CLKSEL1(_core_aclock_div, _pclock_dbg_div),                                                                                      \
        },                                                                                                                                          \
    }

static struct rockchip_cpuclock_rate_table rk3128_cpuclock_rates[] __initdata = {
    RK3128_CPUCLK_RATE(1800000000, 1, 7), RK3128_CPUCLK_RATE(1704000000, 1, 7), RK3128_CPUCLK_RATE(1608000000, 1, 7),
    RK3128_CPUCLK_RATE(1512000000, 1, 7), RK3128_CPUCLK_RATE(1488000000, 1, 5), RK3128_CPUCLK_RATE(1416000000, 1, 5),
    RK3128_CPUCLK_RATE(1392000000, 1, 5), RK3128_CPUCLK_RATE(1296000000, 1, 5), RK3128_CPUCLK_RATE(1200000000, 1, 5),
    RK3128_CPUCLK_RATE(1104000000, 1, 5), RK3128_CPUCLK_RATE(1008000000, 1, 5), RK3128_CPUCLK_RATE(912000000, 1, 5),
    RK3128_CPUCLK_RATE(816000000, 1, 3),  RK3128_CPUCLK_RATE(696000000, 1, 3),  RK3128_CPUCLK_RATE(600000000, 1, 3),
    RK3128_CPUCLK_RATE(408000000, 1, 1),  RK3128_CPUCLK_RATE(312000000, 1, 1),  RK3128_CPUCLK_RATE(216000000, 1, 1),
    RK3128_CPUCLK_RATE(96000000, 1, 1),
};

static const struct rockchip_cpuclock_reg_data rk3128_cpuclock_data = {
    .core_reg       = RK2928_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0x1f,
    .mux_core_alt   = 1,
    .mux_core_main  = 0,
    .mux_core_shift = 7,
    .mux_core_mask  = 0x1,
};

PNAME(mux_pll_p)                                                = {"clock_24m", "xin24m"};

PNAME(mux_ddrphy_p)                                             = {"dpll_ddr", "gpll_div2_ddr"};
PNAME(mux_armclock_p)                                           = {"apll_core", "gpll_div2_core"};
PNAME(mux_usb480m_p)                                            = {"usb480m_phy", "xin24m"};
PNAME(mux_aclock_cpu_src_p)                                     = {"cpll", "gpll", "gpll_div2", "gpll_div3"};

PNAME(mux_pll_src_5plls_p)                                      = {"cpll", "gpll", "gpll_div2", "gpll_div3", "usb480m"};
PNAME(mux_pll_src_4plls_p)                                      = {"cpll", "gpll", "gpll_div2", "usb480m"};
PNAME(mux_pll_src_3plls_p)                                      = {"cpll", "gpll", "gpll_div2"};

PNAME(mux_aclock_peri_src_p)                                    = {"gpll", "cpll", "gpll_div2", "gpll_div3"};
PNAME(mux_mmc_src_p)                                            = {"cpll", "gpll", "gpll_div2", "xin24m"};
PNAME(mux_clock_cif_out_src_p)                                  = {"sclock_cif_src", "xin24m"};
PNAME(mux_sclock_vop_src_p)                                     = {"cpll", "gpll", "gpll_div2", "gpll_div3"};

PNAME(mux_i2s0_p)                                               = {"i2s0_src", "i2s0_frac", "ext_i2s", "xin12m"};
PNAME(mux_i2s1_pre_p)                                           = {"i2s1_src", "i2s1_frac", "ext_i2s", "xin12m"};
PNAME(mux_i2s_out_p)                                            = {"i2s1_pre", "xin12m"};
PNAME(mux_sclock_spdif_p)                                       = {"sclock_spdif_src", "spdif_frac", "xin12m"};

PNAME(mux_uart0_p)                                              = {"uart0_src", "uart0_frac", "xin24m"};
PNAME(mux_uart1_p)                                              = {"uart1_src", "uart1_frac", "xin24m"};
PNAME(mux_uart2_p)                                              = {"uart2_src", "uart2_frac", "xin24m"};

PNAME(mux_sclock_gmac_p)                                        = {"sclock_gmac_src", "gmac_clockin"};
PNAME(mux_sclock_sfc_src_p)                                     = {"cpll", "gpll", "gpll_div2", "xin24m"};

static struct rockchip_pll_clock rk3128_pll_clocks[] __initdata = {
    [apll] = PLL(pll_rk3036, PLL_APLL, "apll", mux_pll_p, 0, RK2928_PLL_CON(0), RK2928_MODE_CON, 0, 1, 0, rk3128_pll_rates),
    [dpll] = PLL(pll_rk3036, PLL_DPLL, "dpll", mux_pll_p, 0, RK2928_PLL_CON(4), RK2928_MODE_CON, 4, 0, 0, NULL),
    [cpll] = PLL(pll_rk3036, PLL_CPLL, "cpll", mux_pll_p, 0, RK2928_PLL_CON(8), RK2928_MODE_CON, 8, 2, 0, rk3128_pll_rates),
    [gpll] = PLL(pll_rk3036, PLL_GPLL, "gpll", mux_pll_p, 0, RK2928_PLL_CON(12), RK2928_MODE_CON, 12, 3, ROCKCHIP_PLL_SYNC_RATE, rk3128_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

static struct rockchip_clock_branch rk3128_i2s0_fracmux __initdata =
    MUX(0, "i2s0_pre", mux_i2s0_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(9), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3128_i2s1_fracmux __initdata =
    MUX(0, "i2s1_pre", mux_i2s1_pre_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(3), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3128_spdif_fracmux __initdata =
    MUX(SCLK_SPDIF, "sclock_spdif", mux_sclock_spdif_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(6), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3128_uart0_fracmux __initdata =
    MUX(SCLK_UART0, "sclock_uart0", mux_uart0_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(13), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3128_uart1_fracmux __initdata =
    MUX(SCLK_UART1, "sclock_uart1", mux_uart1_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(14), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3128_uart2_fracmux __initdata =
    MUX(SCLK_UART2, "sclock_uart2", mux_uart2_p, CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(15), 8, 2, MFLAGS);

static struct rockchip_clock_branch common_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    FACTOR(PLL_GPLL_DIV2, "gpll_div2", "gpll", 0, 1, 2),
    FACTOR(PLL_GPLL_DIV3, "gpll_div3", "gpll", 0, 1, 3),

    DIV(0, "clock_24m", "xin24m", CLK_IGNORE_UNUSED, RK2928_CLKSEL_CON(4), 8, 5, DFLAGS),

    /* PD_DDR */
    GATE(0, "dpll_ddr", "dpll", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 2, GFLAGS),
    GATE(0, "gpll_div2_ddr", "gpll_div2", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 2, GFLAGS),
    COMPOSITE_DDRCLK(SCLK_DDRC, "clock_ddrc", mux_ddrphy_p, 0, RK2928_CLKSEL_CON(26), 8, 2, 0, 2, ROCKCHIP_DDRCLK_SIP_V2),
    FACTOR(0, "clock_ddrphy", "clock_ddrc", 0, 1, 2),

    /* PD_CORE */
    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 6, GFLAGS),
    GATE(0, "gpll_div2_core", "gpll_div2", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(0), 6, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg", "armclk", CLK_IGNORE_UNUSED, RK2928_CLKSEL_CON(1), 0, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK2928_CLKGATE_CON(0), 0, GFLAGS),
    COMPOSITE_NOMUX(
        0, "armcore", "armclk", CLK_IGNORE_UNUSED, RK2928_CLKSEL_CON(1), 4, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK2928_CLKGATE_CON(0), 7, GFLAGS),

    /* PD_MISC */
    MUX(SCLK_USB480M, "usb480m", mux_usb480m_p, CLK_SET_RATE_PARENT, RK2928_MISC_CON, 15, 1, MFLAGS),

    /* PD_CPU */
    COMPOSITE(0, "aclock_cpu_src", mux_aclock_cpu_src_p, 0, RK2928_CLKSEL_CON(0), 13, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(0), 1, GFLAGS),
    GATE(ACLK_CPU, "aclock_cpu", "aclock_cpu_src", 0, RK2928_CLKGATE_CON(0), 3, GFLAGS),
    COMPOSITE_NOMUX(HCLK_CPU, "hclock_cpu", "aclock_cpu_src", 0, RK2928_CLKSEL_CON(1), 8, 2, DFLAGS, RK2928_CLKGATE_CON(0), 4, GFLAGS),
    COMPOSITE_NOMUX(PCLK_CPU, "pclock_cpu", "aclock_cpu_src", 0, RK2928_CLKSEL_CON(1), 12, 2, DFLAGS, RK2928_CLKGATE_CON(0), 5, GFLAGS),
    COMPOSITE_NOMUX(SCLK_CRYPTO, "clock_crypto", "aclock_cpu_src", 0, RK2928_CLKSEL_CON(24), 0, 2, DFLAGS, RK2928_CLKGATE_CON(0), 12, GFLAGS),

    /* PD_VIDEO */
    COMPOSITE(ACLK_VEPU, "aclock_vepu", mux_pll_src_5plls_p, 0, RK2928_CLKSEL_CON(32), 5, 3, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 9, GFLAGS),
    FACTOR(HCLK_VEPU, "hclock_vepu", "aclock_vepu", 0, 1, 4),

    COMPOSITE(
        ACLK_VDPU, "aclock_vdpu", mux_pll_src_5plls_p, 0, RK2928_CLKSEL_CON(32), 13, 3, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(3), 11, GFLAGS),
    FACTOR_GATE(HCLK_VDPU, "hclock_vdpu", "aclock_vdpu", 0, 1, 4, RK2928_CLKGATE_CON(3), 12, GFLAGS),

    COMPOSITE(
        SCLK_HEVC_CORE, "sclock_hevc_core", mux_pll_src_5plls_p, 0, RK2928_CLKSEL_CON(34), 13, 3, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(3), 10,
        GFLAGS),

    /* PD_VIO */
    COMPOSITE(ACLK_VIO0, "aclock_vio0", mux_pll_src_5plls_p, 0, RK2928_CLKSEL_CON(31), 5, 3, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 0, GFLAGS),
    COMPOSITE(ACLK_VIO1, "aclock_vio1", mux_pll_src_5plls_p, 0, RK2928_CLKSEL_CON(31), 13, 3, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(1), 4, GFLAGS),
    FACTOR_GATE(HCLK_VIO, "hclock_vio", "aclock_vio0", 0, 1, 4, RK2928_CLKGATE_CON(0), 11, GFLAGS),

    /* PD_PERI */
    COMPOSITE(0, "aclock_peri_src", mux_aclock_peri_src_p, 0, RK2928_CLKSEL_CON(10), 14, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(2), 0, GFLAGS),

    COMPOSITE_NOMUX(
        PCLK_PERI, "pclock_peri", "aclock_peri_src", 0, RK2928_CLKSEL_CON(10), 12, 2, DFLAGS | CLK_DIVIDER_POWER_OF_TWO, RK2928_CLKGATE_CON(2), 3,
        GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_PERI, "hclock_peri", "aclock_peri_src", 0, RK2928_CLKSEL_CON(10), 8, 2, DFLAGS | CLK_DIVIDER_POWER_OF_TWO, RK2928_CLKGATE_CON(2), 2,
        GFLAGS),
    GATE(ACLK_PERI, "aclock_peri", "aclock_peri_src", 0, RK2928_CLKGATE_CON(2), 1, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, RK2928_CLKGATE_CON(10), 3, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, RK2928_CLKGATE_CON(10), 4, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, RK2928_CLKGATE_CON(10), 5, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, RK2928_CLKGATE_CON(10), 6, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, RK2928_CLKGATE_CON(10), 7, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, RK2928_CLKGATE_CON(10), 8, GFLAGS),

    GATE(SCLK_PVTM_CORE, "clock_pvtm_core", "xin24m", 0, RK2928_CLKGATE_CON(10), 0, GFLAGS),
    GATE(SCLK_PVTM_GPU, "clock_pvtm_gpu", "xin24m", 0, RK2928_CLKGATE_CON(10), 1, GFLAGS),
    GATE(SCLK_PVTM_FUNC, "clock_pvtm_func", "xin24m", 0, RK2928_CLKGATE_CON(10), 2, GFLAGS),
    GATE(SCLK_MIPI_24M, "clock_mipi_24m", "xin24m", 0, RK2928_CLKGATE_CON(2), 15, GFLAGS),

    COMPOSITE(SCLK_SDMMC, "sclock_sdmmc0", mux_mmc_src_p, 0, RK2928_CLKSEL_CON(11), 6, 2, MFLAGS, 0, 6, DFLAGS, RK2928_CLKGATE_CON(2), 11, GFLAGS),

    COMPOSITE(SCLK_SDIO, "sclock_sdio", mux_mmc_src_p, 0, RK2928_CLKSEL_CON(12), 6, 2, MFLAGS, 0, 6, DFLAGS, RK2928_CLKGATE_CON(2), 13, GFLAGS),

    COMPOSITE(SCLK_EMMC, "sclock_emmc", mux_mmc_src_p, 0, RK2928_CLKSEL_CON(12), 14, 2, MFLAGS, 8, 6, DFLAGS, RK2928_CLKGATE_CON(2), 14, GFLAGS),

    DIV(SCLK_PVTM, "clock_pvtm", "clock_pvtm_func", 0, RK2928_CLKSEL_CON(2), 0, 7, DFLAGS),

    /*
     * Clock-Architecture Diagram 2
     */
    COMPOSITE(DCLK_VOP, "dclock_vop", mux_sclock_vop_src_p, 0, RK2928_CLKSEL_CON(27), 0, 2, MFLAGS, 8, 8, DFLAGS, RK2928_CLKGATE_CON(3), 1, GFLAGS),
    COMPOSITE(SCLK_VOP, "sclock_vop", mux_sclock_vop_src_p, 0, RK2928_CLKSEL_CON(28), 0, 2, MFLAGS, 8, 8, DFLAGS, RK2928_CLKGATE_CON(3), 2, GFLAGS),
    COMPOSITE(DCLK_EBC, "dclock_ebc", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(23), 0, 2, MFLAGS, 8, 8, DFLAGS, RK2928_CLKGATE_CON(3), 4, GFLAGS),

    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    COMPOSITE_NODIV(SCLK_CIF_SRC, "sclock_cif_src", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(29), 0, 2, MFLAGS, RK2928_CLKGATE_CON(3), 7, GFLAGS),
    MUX(SCLK_CIF_OUT_SRC, "sclock_cif_out_src", mux_clock_cif_out_src_p, 0, RK2928_CLKSEL_CON(29), 7, 1, MFLAGS),
    DIV(SCLK_CIF_OUT, "sclock_cif_out", "sclock_cif_out_src", 0, RK2928_CLKSEL_CON(29), 2, 5, DFLAGS),

    COMPOSITE(0, "i2s0_src", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(9), 14, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(4), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "i2s0_frac", "i2s0_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(8), 0, RK2928_CLKGATE_CON(4), 5, GFLAGS, &rk3128_i2s0_fracmux,
        RK3128_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S0, "sclock_i2s0", "i2s0_pre", CLK_SET_RATE_PARENT, RK2928_CLKGATE_CON(4), 6, GFLAGS),

    COMPOSITE(0, "i2s1_src", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(3), 14, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(0), 9, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "i2s1_frac", "i2s1_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(7), 0, RK2928_CLKGATE_CON(0), 10, GFLAGS, &rk3128_i2s1_fracmux,
        RK3128_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1, "sclock_i2s1", "i2s1_pre", CLK_SET_RATE_PARENT, RK2928_CLKGATE_CON(0), 14, GFLAGS),
    COMPOSITE_NODIV(SCLK_I2S_OUT, "i2s_out", mux_i2s_out_p, 0, RK2928_CLKSEL_CON(3), 12, 1, MFLAGS, RK2928_CLKGATE_CON(0), 13, GFLAGS),

    COMPOSITE(0, "sclock_spdif_src", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(6), 14, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(2), 10, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "spdif_frac", "sclock_spdif_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(20), 0, RK2928_CLKGATE_CON(2), 12, GFLAGS, &rk3128_spdif_fracmux,
        RK3128_SPDIF_FRAC_MAX_PRATE),

    GATE(0, "jtag", "ext_jtag", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(1), 3, GFLAGS),

    GATE(SCLK_OTGPHY0, "sclock_otgphy0", "xin12m", 0, RK2928_CLKGATE_CON(1), 5, GFLAGS),
    GATE(SCLK_OTGPHY1, "sclock_otgphy1", "xin12m", 0, RK2928_CLKGATE_CON(1), 6, GFLAGS),

    COMPOSITE_NOMUX(SCLK_SARADC, "sclock_saradc", "xin24m", 0, RK2928_CLKSEL_CON(24), 8, 8, DFLAGS, RK2928_CLKGATE_CON(2), 8, GFLAGS),

    COMPOSITE(ACLK_GPU, "aclock_gpu", mux_pll_src_5plls_p, 0, RK2928_CLKSEL_CON(34), 5, 3, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(3), 13, GFLAGS),

    COMPOSITE(SCLK_SPI0, "sclock_spi0", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(25), 8, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(2), 9, GFLAGS),

    /* PD_UART */
    COMPOSITE(0, "uart0_src", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(13), 12, 2, MFLAGS, 0, 7, DFLAGS, RK2928_CLKGATE_CON(1), 8, GFLAGS),
    MUX(0, "uart12_src", mux_pll_src_4plls_p, 0, RK2928_CLKSEL_CON(13), 14, 2, MFLAGS),
    COMPOSITE_NOMUX(0, "uart1_src", "uart12_src", 0, RK2928_CLKSEL_CON(14), 0, 7, DFLAGS, RK2928_CLKGATE_CON(1), 10, GFLAGS),
    COMPOSITE_NOMUX(0, "uart2_src", "uart12_src", 0, RK2928_CLKSEL_CON(15), 0, 7, DFLAGS, RK2928_CLKGATE_CON(1), 13, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "uart0_frac", "uart0_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(17), 0, RK2928_CLKGATE_CON(1), 9, GFLAGS, &rk3128_uart0_fracmux,
        RK3128_UART_FRAC_MAX_PRATE),
    COMPOSITE_FRACMUX(
        0, "uart1_frac", "uart1_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(18), 0, RK2928_CLKGATE_CON(1), 11, GFLAGS, &rk3128_uart1_fracmux,
        RK3128_UART_FRAC_MAX_PRATE),
    COMPOSITE_FRACMUX(
        0, "uart2_frac", "uart2_src", CLK_SET_RATE_PARENT, RK2928_CLKSEL_CON(19), 0, RK2928_CLKGATE_CON(1), 13, GFLAGS, &rk3128_uart2_fracmux,
        RK3128_UART_FRAC_MAX_PRATE),

    COMPOSITE(
        SCLK_MAC_SRC, "sclock_gmac_src", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(5), 6, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(1), 7, GFLAGS),
    MUX(SCLK_MAC, "sclock_gmac", mux_sclock_gmac_p, 0, RK2928_CLKSEL_CON(5), 15, 1, MFLAGS),
    GATE(SCLK_MAC_REFOUT, "sclock_mac_refout", "sclock_gmac", 0, RK2928_CLKGATE_CON(2), 5, GFLAGS),
    GATE(SCLK_MAC_REF, "sclock_mac_ref", "sclock_gmac", 0, RK2928_CLKGATE_CON(2), 4, GFLAGS),
    GATE(SCLK_MAC_RX, "sclock_mac_rx", "sclock_gmac", 0, RK2928_CLKGATE_CON(2), 6, GFLAGS),
    GATE(SCLK_MAC_TX, "sclock_mac_tx", "sclock_gmac", 0, RK2928_CLKGATE_CON(2), 7, GFLAGS),

    COMPOSITE(SCLK_TSP, "sclock_tsp", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(4), 6, 2, MFLAGS, 0, 5, DFLAGS, RK2928_CLKGATE_CON(1), 14, GFLAGS),
    GATE(SCLK_HSADC_TSP, "sclock_hsadc_tsp", "ext_hsadc_tsp", 0, RK2928_CLKGATE_CON(10), 13, GFLAGS),

    COMPOSITE(
        SCLK_NANDC, "sclock_nandc", mux_pll_src_3plls_p, 0, RK2928_CLKSEL_CON(2), 14, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(10), 15, GFLAGS),

    COMPOSITE_NOMUX(PCLK_PMU_PRE, "pclock_pmu_pre", "cpll", 0, RK2928_CLKSEL_CON(29), 8, 6, DFLAGS, RK2928_CLKGATE_CON(1), 0, GFLAGS),

    /*
     * Clock-Architecture Diagram 3
     */

    /* PD_VOP */
    GATE(ACLK_LCDC0, "aclock_lcdc0", "aclock_vio0", 0, RK2928_CLKGATE_CON(6), 0, GFLAGS),
    GATE(ACLK_CIF, "aclock_cif", "aclock_vio0", 0, RK2928_CLKGATE_CON(6), 5, GFLAGS),
    GATE(ACLK_RGA, "aclock_rga", "aclock_vio0", 0, RK2928_CLKGATE_CON(6), 11, GFLAGS),
    GATE(0, "aclock_vio0_niu", "aclock_vio0", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(6), 13, GFLAGS),

    GATE(ACLK_IEP, "aclock_iep", "aclock_vio1", 0, RK2928_CLKGATE_CON(9), 8, GFLAGS),
    GATE(0, "aclock_vio1_niu", "aclock_vio1", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 10, GFLAGS),

    GATE(HCLK_VIO_H2P, "hclock_vio_h2p", "hclock_vio", 0, RK2928_CLKGATE_CON(9), 5, GFLAGS),
    GATE(PCLK_MIPI, "pclock_mipi", "hclock_vio", 0, RK2928_CLKGATE_CON(9), 6, GFLAGS),
    GATE(HCLK_RGA, "hclock_rga", "hclock_vio", 0, RK2928_CLKGATE_CON(6), 10, GFLAGS),
    GATE(HCLK_LCDC0, "hclock_lcdc0", "hclock_vio", 0, RK2928_CLKGATE_CON(6), 1, GFLAGS),
    GATE(HCLK_IEP, "hclock_iep", "hclock_vio", 0, RK2928_CLKGATE_CON(9), 7, GFLAGS),
    GATE(0, "hclock_vio_niu", "hclock_vio", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(6), 12, GFLAGS),
    GATE(HCLK_CIF, "hclock_cif", "hclock_vio", 0, RK2928_CLKGATE_CON(6), 4, GFLAGS),
    GATE(HCLK_EBC, "hclock_ebc", "hclock_vio", 0, RK2928_CLKGATE_CON(9), 9, GFLAGS),

    /* PD_PERI */
    GATE(0, "aclock_peri_axi", "aclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(4), 3, GFLAGS),
    GATE(ACLK_GMAC, "aclock_gmac", "aclock_peri", 0, RK2928_CLKGATE_CON(10), 10, GFLAGS),
    GATE(ACLK_DMAC, "aclock_dmac", "aclock_peri", 0, RK2928_CLKGATE_CON(5), 1, GFLAGS),
    GATE(0, "aclock_peri_niu", "aclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 15, GFLAGS),
    GATE(0, "aclock_cpu_to_peri", "aclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(4), 2, GFLAGS),

    GATE(HCLK_I2S_8CH, "hclock_i2s_8ch", "hclock_peri", 0, RK2928_CLKGATE_CON(7), 4, GFLAGS),
    GATE(0, "hclock_peri_matrix", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(4), 0, GFLAGS),
    GATE(HCLK_I2S_2CH, "hclock_i2s_2ch", "hclock_peri", 0, RK2928_CLKGATE_CON(7), 2, GFLAGS),
    GATE(0, "hclock_usb_peri", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 13, GFLAGS),
    GATE(HCLK_HOST2, "hclock_host2", "hclock_peri", 0, RK2928_CLKGATE_CON(7), 3, GFLAGS),
    GATE(HCLK_OTG, "hclock_otg", "hclock_peri", 0, RK2928_CLKGATE_CON(5), 13, GFLAGS),
    GATE(0, "hclock_peri_ahb", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 14, GFLAGS),
    GATE(HCLK_SPDIF, "hclock_spdif", "hclock_peri", 0, RK2928_CLKGATE_CON(10), 9, GFLAGS),
    GATE(HCLK_TSP, "hclock_tsp", "hclock_peri", 0, RK2928_CLKGATE_CON(10), 12, GFLAGS),
    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_peri", 0, RK2928_CLKGATE_CON(5), 10, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_peri", 0, RK2928_CLKGATE_CON(5), 11, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_peri", 0, RK2928_CLKGATE_CON(7), 0, GFLAGS),
    GATE(0, "hclock_emmc_peri", "hclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(3), 6, GFLAGS),
    GATE(HCLK_NANDC, "hclock_nandc", "hclock_peri", 0, RK2928_CLKGATE_CON(5), 9, GFLAGS),
    GATE(HCLK_USBHOST, "hclock_usbhost", "hclock_peri", 0, RK2928_CLKGATE_CON(10), 14, GFLAGS),
    GATE(HCLK_SFC, "hclock_sfc", "hclock_peri", 0, RK2928_CLKGATE_CON(7), 1, GFLAGS),

    GATE(PCLK_SIM_CARD, "pclock_sim_card", "pclock_peri", 0, RK2928_CLKGATE_CON(9), 12, GFLAGS),
    GATE(PCLK_GMAC, "pclock_gmac", "pclock_peri", 0, RK2928_CLKGATE_CON(10), 11, GFLAGS),
    GATE(0, "pclock_peri_axi", "pclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(4), 1, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "pclock_peri", 0, RK2928_CLKGATE_CON(7), 12, GFLAGS),
    GATE(PCLK_UART0, "pclock_uart0", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 0, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 1, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 2, GFLAGS),
    GATE(PCLK_PWM, "pclock_pwm", "pclock_peri", 0, RK2928_CLKGATE_CON(7), 10, GFLAGS),
    GATE(PCLK_WDT, "pclock_wdt", "pclock_peri", 0, RK2928_CLKGATE_CON(7), 15, GFLAGS),
    GATE(PCLK_I2C0, "pclock_i2c0", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 4, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 5, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 6, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 7, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "pclock_peri", 0, RK2928_CLKGATE_CON(7), 14, GFLAGS),
    GATE(PCLK_EFUSE, "pclock_efuse", "pclock_peri", 0, RK2928_CLKGATE_CON(5), 2, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer", "pclock_peri", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(7), 7, GFLAGS),
    GATE(PCLK_GPIO0, "pclock_gpio0", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 9, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 10, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 11, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_peri", 0, RK2928_CLKGATE_CON(8), 12, GFLAGS),

    /* PD_BUS */
    GATE(0, "aclock_initmem", "aclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(4), 12, GFLAGS),
    GATE(0, "aclock_strc_sys", "aclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(4), 10, GFLAGS),

    GATE(0, "hclock_rom", "hclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(5), 6, GFLAGS),
    GATE(HCLK_CRYPTO, "hclock_crypto", "hclock_cpu", 0, RK2928_CLKGATE_CON(3), 5, GFLAGS),

    GATE(PCLK_ACODEC, "pclock_acodec", "pclock_cpu", 0, RK2928_CLKGATE_CON(5), 14, GFLAGS),
    GATE(0, "pclock_ddrupctl", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(5), 7, GFLAGS),
    GATE(0, "pclock_grf", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(5), 4, GFLAGS),
    GATE(PCLK_MIPIPHY, "pclock_mipiphy", "pclock_cpu", 0, RK2928_CLKGATE_CON(5), 0, GFLAGS),

    GATE(0, "pclock_pmu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 2, GFLAGS),
    GATE(0, "pclock_pmu_niu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(9), 3, GFLAGS),

    /* PD_MMC */
    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "sclock_sdmmc", RK3228_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "sclock_sdmmc", RK3228_SDMMC_CON1, 0),

    MMC(SCLK_SDIO_DRV, "sdio_drv", "sclock_sdio", RK3228_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "sclock_sdio", RK3228_SDIO_CON1, 0),

    MMC(SCLK_EMMC_DRV, "emmc_drv", "sclock_emmc", RK3228_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "sclock_emmc", RK3228_EMMC_CON1, 0),
};

static struct rockchip_clock_branch rk3126_clock_branches[] __initdata = {
    GATE(0, "pclock_stimer", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(3), 15, GFLAGS),
    GATE(0, "pclock_s_efuse", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(3), 14, GFLAGS),
    GATE(0, "pclock_sgrf", "pclock_cpu", CLK_IGNORE_UNUSED, RK2928_CLKGATE_CON(3), 8, GFLAGS),
};

static struct rockchip_clock_branch rk3128_clock_branches[] __initdata = {
    COMPOSITE(SCLK_SFC, "sclock_sfc", mux_sclock_sfc_src_p, 0, RK2928_CLKSEL_CON(11), 14, 2, MFLAGS, 8, 5, DFLAGS, RK2928_CLKGATE_CON(3), 15, GFLAGS),

    GATE(HCLK_GPS, "hclock_gps", "aclock_peri", 0, RK2928_CLKGATE_CON(3), 14, GFLAGS),
    GATE(PCLK_HDMI, "pclock_hdmi", "pclock_cpu", 0, RK2928_CLKGATE_CON(3), 8, GFLAGS),
};

static const char *const rk3128_critical_clocks[] __initconst = {
    "aclock_cpu", "hclock_cpu", "pclock_cpu", "aclock_peri", "hclock_peri", "pclock_peri", "pclock_pmu", "sclock_timer5", "hclock_vio_niu",
};

static void __iomem *rk312x_reg_base;

void rkclock_cpuclock_div_setting(int div)
{
    if (cpu_is_rk312x()) {
        writel_relaxed((0x001f0000 | (div - 1)), rk312x_reg_base + RK2928_CLKSEL_CON(0));
    }
}

static struct rockchip_clock_provider *__init rk3128_common_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return ERR_PTR(-ENOMEM);
    }

    rk312x_reg_base = reg_base;
    ctx             = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(reg_base);
        return ERR_PTR(-ENOMEM);
    }

    rockchip_clock_register_plls(ctx, rk3128_pll_clocks, ARRAY_SIZE(rk3128_pll_clocks), RK3128_GRF_SOC_STATUS0);
    rockchip_clock_register_branches(ctx, common_clock_branches, ARRAY_SIZE(common_clock_branches));

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &rk3128_cpuclock_data, rk3128_cpuclock_rates,
        ARRAY_SIZE(rk3128_cpuclock_rates));

    rockchip_register_softrst(np, 9, reg_base + RK2928_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK2928_GLB_SRST_FST, NULL);

    return ctx;
}

static void __init rk3126_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;

    ctx = rk3128_common_clock_init(np);

    if (IS_ERR(ctx)) {
        return;
    }

    rockchip_clock_register_branches(ctx, rk3126_clock_branches, ARRAY_SIZE(rk3126_clock_branches));
    rockchip_clock_protect_critical(rk3128_critical_clocks, ARRAY_SIZE(rk3128_critical_clocks));

    rockchip_clock_of_add_provider(np, ctx);
}

CLK_OF_DECLARE(rk3126_cru, "rockchip,rk3126-cru", rk3126_clock_init);

static void __init rk3128_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;

    ctx = rk3128_common_clock_init(np);

    if (IS_ERR(ctx)) {
        return;
    }

    rockchip_clock_register_branches(ctx, rk3128_clock_branches, ARRAY_SIZE(rk3128_clock_branches));
    rockchip_clock_protect_critical(rk3128_critical_clocks, ARRAY_SIZE(rk3128_critical_clocks));

    rockchip_clock_of_add_provider(np, ctx);
}

CLK_OF_DECLARE(rk3128_cru, "rockchip,rk3128-cru", rk3128_clock_init);
