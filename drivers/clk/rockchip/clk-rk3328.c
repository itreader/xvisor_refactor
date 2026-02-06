/*
 * Copyright (c) 2016 Rockchip Electronics Co. Ltd.
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

#include <dt-bindings/clock/rk3328-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define RK3328_GRF_SOC_CON4         0x410
#define RK3328_GRF_SOC_STATUS0      0x480
#define RK3328_GRF_MAC_CON1         0x904
#define RK3328_GRF_MAC_CON2         0x908
#define RK3328_I2S_FRAC_MAX_PRATE   600000000
#define RK3328_UART_FRAC_MAX_PRATE  600000000
#define RK3328_SPDIF_FRAC_MAX_PRATE 600000000

enum rk3328_plls {
    apll,
    dpll,
    cpll,
    gpll,
    npll,
};

static struct rockchip_pll_rate_table rk3328_pll_rates[] = {
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

static struct rockchip_pll_rate_table rk3328_pll_frac_rates[] = {
    /* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
    RK3036_PLL_RATE(1016064000, 3, 127, 1, 1, 0, 134217),
    /* vco = 1016064000 */
    RK3036_PLL_RATE(983040000, 24, 983, 1, 1, 0, 671088),
    /* vco = 983040000 */
    RK3036_PLL_RATE(491520000, 24, 983, 2, 1, 0, 671088),
    /* vco = 983040000 */
    RK3036_PLL_RATE(61440000, 6, 215, 7, 2, 0, 671088),
    /* vco = 860156000 */
    RK3036_PLL_RATE(56448000, 12, 451, 4, 4, 0, 9797894),
    /* vco = 903168000 */
    RK3036_PLL_RATE(40960000, 12, 409, 4, 5, 0, 10066329),
    /* vco = 819200000 */
    {/* sentinel */},
};

#define RK3328_DIV_ACLKM_MASK     0x7
#define RK3328_DIV_ACLKM_SHIFT    4
#define RK3328_DIV_PCLK_DBG_MASK  0xf
#define RK3328_DIV_PCLK_DBG_SHIFT 0

#define RK3328_CLKSEL1(_aclock_core, _pclock_dbg)                                               \
    {                                                                                           \
        .reg = RK3328_CLKSEL_CON(1),                                                            \
        .val = HIWORD_UPDATE(_aclock_core, RK3328_DIV_ACLKM_MASK, RK3328_DIV_ACLKM_SHIFT) |     \
               HIWORD_UPDATE(_pclock_dbg, RK3328_DIV_PCLK_DBG_MASK, RK3328_DIV_PCLK_DBG_SHIFT), \
    }

#define RK3328_CPUCLK_RATE(_prate, _aclock_core, _pclock_dbg) \
    {                                                         \
        .prate = _prate,                                      \
        .divs  = {                                            \
            RK3328_CLKSEL1(_aclock_core, _pclock_dbg),       \
        },                                                   \
    }

static struct rockchip_cpuclock_rate_table rk3328_cpuclock_rates[] __initdata = {
    RK3328_CPUCLK_RATE(1800000000, 1, 7), RK3328_CPUCLK_RATE(1704000000, 1, 7), RK3328_CPUCLK_RATE(1608000000, 1, 7),
    RK3328_CPUCLK_RATE(1512000000, 1, 7), RK3328_CPUCLK_RATE(1488000000, 1, 5), RK3328_CPUCLK_RATE(1416000000, 1, 5),
    RK3328_CPUCLK_RATE(1392000000, 1, 5), RK3328_CPUCLK_RATE(1296000000, 1, 5), RK3328_CPUCLK_RATE(1200000000, 1, 5),
    RK3328_CPUCLK_RATE(1104000000, 1, 5), RK3328_CPUCLK_RATE(1008000000, 1, 5), RK3328_CPUCLK_RATE(912000000, 1, 5),
    RK3328_CPUCLK_RATE(816000000, 1, 3),  RK3328_CPUCLK_RATE(696000000, 1, 3),  RK3328_CPUCLK_RATE(600000000, 1, 3),
    RK3328_CPUCLK_RATE(408000000, 1, 1),  RK3328_CPUCLK_RATE(312000000, 1, 1),  RK3328_CPUCLK_RATE(216000000, 1, 1),
    RK3328_CPUCLK_RATE(96000000, 1, 1),
};

static const struct rockchip_cpuclock_reg_data rk3328_cpuclock_data = {
    .core_reg       = RK3328_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0x1f,
    .mux_core_alt   = 1,
    .mux_core_main  = 3,
    .mux_core_shift = 6,
    .mux_core_mask  = 0x3,
};

PNAME(mux_pll_p)                                                = {"xin24m"};
PNAME(mux_hdmiphy_gpll_p)                                       = {"hdmiphy", "gpll"};
PNAME(mux_2plls_p)                                              = {"cpll", "gpll"};
PNAME(mux_gpll_cpll_p)                                          = {"gpll", "cpll"};
PNAME(mux_cpll_gpll_apll_p)                                     = {"cpll", "gpll", "apll"};
PNAME(mux_2plls_xin24m_p)                                       = {"cpll", "gpll", "xin24m"};
PNAME(mux_2plls_hdmiphy_p)                                      = {"cpll", "gpll", "dummy_hdmiphy"};
PNAME(mux_4plls_p)                                              = {"cpll", "gpll", "dummy_hdmiphy", "usb480m"};
PNAME(mux_2plls_u480m_p)                                        = {"cpll", "gpll", "usb480m"};
PNAME(mux_2plls_24m_u480m_p)                                    = {"cpll", "gpll", "xin24m", "usb480m"};

PNAME(mux_ddrphy_p)                                             = {"dpll", "apll", "cpll"};
PNAME(mux_armclock_p)                                           = {"apll_core", "gpll_core", "dpll_core", "npll_core"};
PNAME(mux_hdmiphy_p)                                            = {"hdmi_phy", "xin24m"};
PNAME(mux_usb480m_p)                                            = {"usb480m_phy", "xin24m"};

PNAME(mux_i2s0_p)                                               = {"clock_i2s0_div", "clock_i2s0_frac", "xin12m", "xin12m"};
PNAME(mux_i2s1_p)                                               = {"clock_i2s1_div", "clock_i2s1_frac", "clkin_i2s1", "xin12m"};
PNAME(mux_i2s2_p)                                               = {"clock_i2s2_div", "clock_i2s2_frac", "clkin_i2s2", "xin12m"};
PNAME(mux_i2s1out_p)                                            = {"clock_i2s1", "xin12m"};
PNAME(mux_i2s2out_p)                                            = {"clock_i2s2", "xin12m"};
PNAME(mux_spdif_p)                                              = {"clock_spdif_div", "clock_spdif_frac", "xin12m", "xin12m"};
PNAME(mux_uart0_p)                                              = {"clock_uart0_div", "clock_uart0_frac", "xin24m"};
PNAME(mux_uart1_p)                                              = {"clock_uart1_div", "clock_uart1_frac", "xin24m"};
PNAME(mux_uart2_p)                                              = {"clock_uart2_div", "clock_uart2_frac", "xin24m"};

PNAME(mux_sclock_cif_p)                                         = {"clock_cif_src", "xin24m"};
PNAME(mux_dclock_lcdc_p)                                        = {"hdmiphy", "dclock_lcdc_src"};
PNAME(mux_aclock_peri_pre_p)                                    = {"cpll_peri", "gpll_peri", "hdmiphy_peri"};
PNAME(mux_ref_usb3otg_src_p)                                    = {"xin24m", "clock_usb3otg_ref"};
PNAME(mux_xin24m_32k_p)                                         = {"xin24m", "clock_rtc32k"};
PNAME(mux_mac2io_src_p)                                         = {"clock_mac2io_src", "gmac_clockin"};
PNAME(mux_mac2phy_src_p)                                        = {"clock_mac2phy_src", "phy_50m_out"};
PNAME(mux_mac2io_ext_p)                                         = {"clock_mac2io", "gmac_clockin"};
PNAME(mux_i2s_plls_p)                                           = {"cpll", "dummy_gpll"};

static struct rockchip_pll_clock rk3328_pll_clocks[] __initdata = {
    [apll] = PLL(pll_rk3328, PLL_APLL, "apll", mux_pll_p, 0, RK3328_PLL_CON(0), RK3328_MODE_CON, 0, 4, 0, rk3328_pll_frac_rates),
    [dpll] = PLL(pll_rk3328, PLL_DPLL, "dpll", mux_pll_p, 0, RK3328_PLL_CON(8), RK3328_MODE_CON, 4, 3, 0, NULL),
    [cpll] = PLL(pll_rk3328, PLL_CPLL, "cpll", mux_pll_p, 0, RK3328_PLL_CON(16), RK3328_MODE_CON, 8, 2, 0, rk3328_pll_rates),
    [gpll] = PLL(pll_rk3328, PLL_GPLL, "gpll", mux_pll_p, 0, RK3328_PLL_CON(24), RK3328_MODE_CON, 12, 1, 0, rk3328_pll_frac_rates),
    [npll] = PLL(pll_rk3328, PLL_NPLL, "npll", mux_pll_p, 0, RK3328_PLL_CON(40), RK3328_MODE_CON, 1, 0, 0, rk3328_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

static struct rockchip_clock_branch rk3328_i2s0_fracmux __initdata =
    MUX(0, "i2s0_pre", mux_i2s0_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(6), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_i2s1_fracmux __initdata =
    MUX(0, "i2s1_pre", mux_i2s1_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(8), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_i2s2_fracmux __initdata =
    MUX(0, "i2s2_pre", mux_i2s2_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(10), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_spdif_fracmux __initdata =
    MUX(SCLK_SPDIF, "sclock_spdif", mux_spdif_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(12), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_uart0_fracmux __initdata =
    MUX(SCLK_UART0, "sclock_uart0", mux_uart0_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(14), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_uart1_fracmux __initdata =
    MUX(SCLK_UART1, "sclock_uart1", mux_uart1_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(16), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_uart2_fracmux __initdata =
    MUX(SCLK_UART2, "sclock_uart2", mux_uart2_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(18), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3328_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    DIV(0, "clock_24m", "xin24m", CLK_IGNORE_UNUSED, RK3328_CLKSEL_CON(2), 8, 5, DFLAGS),
    COMPOSITE(
        SCLK_RTC32K, "clock_rtc32k", mux_2plls_xin24m_p, 0, RK3328_CLKSEL_CON(38), 14, 2, MFLAGS, 0, 14, DFLAGS, RK3328_CLKGATE_CON(0), 11, GFLAGS),

    /* PD_MISC */
    MUX(HDMIPHY, "hdmiphy", mux_hdmiphy_p, CLK_SET_RATE_PARENT, RK3328_MISC_CON, 13, 1, MFLAGS),
    MUX(USB480M, "usb480m", mux_usb480m_p, CLK_SET_RATE_PARENT, RK3328_MISC_CON, 15, 1, MFLAGS),

    /*
     * Clock-Architecture Diagram 2
     */

    /* PD_CORE */
    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "gpll_core", "gpll", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(0), 2, GFLAGS),
    GATE(0, "dpll_core", "dpll", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(0), 1, GFLAGS),
    GATE(0, "npll_core", "npll", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(0), 12, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg", "armclk", CLK_IGNORE_UNUSED, RK3328_CLKSEL_CON(1), 0, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3328_CLKGATE_CON(7), 0, GFLAGS),
    COMPOSITE_NOMUX(
        0, "aclock_core", "armclk", CLK_IGNORE_UNUSED, RK3328_CLKSEL_CON(1), 4, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3328_CLKGATE_CON(7), 1, GFLAGS),
    GATE(0, "aclock_core_niu", "aclock_core", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(13), 0, GFLAGS),
    GATE(0, "aclock_gic400", "aclock_core", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(13), 1, GFLAGS),

    GATE(0, "clock_jtag", "jtag_clockin", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(7), 2, GFLAGS),

    /* PD_GPU */
    COMPOSITE(0, "aclock_gpu_pre", mux_4plls_p, 0, RK3328_CLKSEL_CON(44), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(6), 6, GFLAGS),
    GATE(ACLK_GPU, "aclock_gpu", "aclock_gpu_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(14), 0, GFLAGS),
    GATE(0, "aclock_gpu_niu", "aclock_gpu_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(14), 1, GFLAGS),

    /* PD_DDR */
    COMPOSITE_DDRCLK(SCLK_DDRCLK, "sclock_ddrc", mux_ddrphy_p, 0, RK3328_CLKSEL_CON(3), 8, 2, 0, 3, ROCKCHIP_DDRCLK_SIP_V2),

    GATE(0, "clock_ddrmsch", "sclock_ddrc", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 6, GFLAGS),
    GATE(0, "clock_ddrupctl", "sclock_ddrc", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 5, GFLAGS),
    GATE(0, "aclock_ddrupctl", "sclock_ddrc", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 4, GFLAGS),
    GATE(0, "clock_ddrmon", "xin24m", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(0), 6, GFLAGS),

    COMPOSITE(PCLK_DDR, "pclock_ddr", mux_2plls_hdmiphy_p, 0, RK3328_CLKSEL_CON(4), 13, 2, MFLAGS, 8, 3, DFLAGS, RK3328_CLKGATE_CON(7), 4, GFLAGS),
    GATE(0, "pclock_ddrupctl", "pclock_ddr", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 1, GFLAGS),
    GATE(0, "pclock_ddr_msch", "pclock_ddr", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 2, GFLAGS),
    GATE(0, "pclock_ddr_mon", "pclock_ddr", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 3, GFLAGS),
    GATE(0, "pclock_ddrstdby", "pclock_ddr", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 7, GFLAGS),
    GATE(0, "pclock_ddr_grf", "pclock_ddr", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(18), 9, GFLAGS),

    /*
     * Clock-Architecture Diagram 3
     */

    /* PD_BUS */
    COMPOSITE(
        ACLK_BUS_PRE, "aclock_bus_pre", mux_2plls_hdmiphy_p, 0, RK3328_CLKSEL_CON(0), 13, 2, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(8), 0, GFLAGS),
    COMPOSITE_NOMUX(HCLK_BUS_PRE, "hclock_bus_pre", "aclock_bus_pre", 0, RK3328_CLKSEL_CON(1), 8, 2, DFLAGS, RK3328_CLKGATE_CON(8), 1, GFLAGS),
    COMPOSITE_NOMUX(PCLK_BUS_PRE, "pclock_bus_pre", "aclock_bus_pre", 0, RK3328_CLKSEL_CON(1), 12, 3, DFLAGS, RK3328_CLKGATE_CON(8), 2, GFLAGS),
    GATE(0, "pclock_bus", "pclock_bus_pre", 0, RK3328_CLKGATE_CON(8), 3, GFLAGS),
    GATE(0, "pclock_phy_pre", "pclock_bus_pre", 0, RK3328_CLKGATE_CON(8), 4, GFLAGS),

    COMPOSITE(SCLK_TSP, "clock_tsp", mux_2plls_p, 0, RK3328_CLKSEL_CON(21), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(2), 5, GFLAGS),
    GATE(0, "clock_hsadc_tsp", "ext_gpio3a2", 0, RK3328_CLKGATE_CON(17), 13, GFLAGS),

    /* PD_I2S */
    COMPOSITE(0, "clock_i2s0_div", mux_i2s_plls_p, 0, RK3328_CLKSEL_CON(6), 15, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(1), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_frac", "clock_i2s0_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(7), 0, RK3328_CLKGATE_CON(1), 2, GFLAGS, &rk3328_i2s0_fracmux,
        RK3328_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S0, "clock_i2s0", "i2s0_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(1), 3, GFLAGS),

    COMPOSITE(0, "clock_i2s1_div", mux_i2s_plls_p, 0, RK3328_CLKSEL_CON(8), 15, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(1), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_frac", "clock_i2s1_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(9), 0, RK3328_CLKGATE_CON(1), 5, GFLAGS, &rk3328_i2s1_fracmux,
        RK3328_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1, "clock_i2s1", "i2s1_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(1), 6, GFLAGS),
    COMPOSITE_NODIV(SCLK_I2S1_OUT, "i2s1_out", mux_i2s1out_p, 0, RK3328_CLKSEL_CON(8), 12, 1, MFLAGS, RK3328_CLKGATE_CON(1), 7, GFLAGS),

    COMPOSITE(0, "clock_i2s2_div", mux_i2s_plls_p, 0, RK3328_CLKSEL_CON(10), 15, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(1), 8, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s2_frac", "clock_i2s2_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(11), 0, RK3328_CLKGATE_CON(1), 9, GFLAGS, &rk3328_i2s2_fracmux,
        RK3328_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S2, "clock_i2s2", "i2s2_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(1), 10, GFLAGS),
    COMPOSITE_NODIV(SCLK_I2S2_OUT, "i2s2_out", mux_i2s2out_p, 0, RK3328_CLKSEL_CON(10), 12, 1, MFLAGS, RK3328_CLKGATE_CON(1), 11, GFLAGS),

    COMPOSITE(0, "clock_spdif_div", mux_2plls_p, 0, RK3328_CLKSEL_CON(12), 15, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(1), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_spdif_frac", "clock_spdif_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(13), 0, RK3328_CLKGATE_CON(1), 13, GFLAGS,
        &rk3328_spdif_fracmux, RK3328_SPDIF_FRAC_MAX_PRATE),

    /* PD_UART */
    COMPOSITE(0, "clock_uart0_div", mux_2plls_u480m_p, 0, RK3328_CLKSEL_CON(14), 12, 2, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(1), 14, GFLAGS),
    COMPOSITE(0, "clock_uart1_div", mux_2plls_u480m_p, 0, RK3328_CLKSEL_CON(16), 12, 2, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(2), 0, GFLAGS),
    COMPOSITE(0, "clock_uart2_div", mux_2plls_u480m_p, 0, RK3328_CLKSEL_CON(18), 12, 2, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(2), 2, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart0_frac", "clock_uart0_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(15), 0, RK3328_CLKGATE_CON(1), 15, GFLAGS,
        &rk3328_uart0_fracmux, RK3328_UART_FRAC_MAX_PRATE),
    COMPOSITE_FRACMUX(
        0, "clock_uart1_frac", "clock_uart1_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(17), 0, RK3328_CLKGATE_CON(2), 1, GFLAGS,
        &rk3328_uart1_fracmux, RK3328_UART_FRAC_MAX_PRATE),
    COMPOSITE_FRACMUX(
        0, "clock_uart2_frac", "clock_uart2_div", CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(19), 0, RK3328_CLKGATE_CON(2), 3, GFLAGS,
        &rk3328_uart2_fracmux, RK3328_UART_FRAC_MAX_PRATE),

    /*
     * Clock-Architecture Diagram 4
     */

    COMPOSITE(SCLK_I2C0, "clock_i2c0", mux_2plls_p, 0, RK3328_CLKSEL_CON(34), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(2), 9, GFLAGS),
    COMPOSITE(SCLK_I2C1, "clock_i2c1", mux_2plls_p, 0, RK3328_CLKSEL_CON(34), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3328_CLKGATE_CON(2), 10, GFLAGS),
    COMPOSITE(SCLK_I2C2, "clock_i2c2", mux_2plls_p, 0, RK3328_CLKSEL_CON(35), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(2), 11, GFLAGS),
    COMPOSITE(SCLK_I2C3, "clock_i2c3", mux_2plls_p, 0, RK3328_CLKSEL_CON(35), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3328_CLKGATE_CON(2), 12, GFLAGS),
    COMPOSITE(SCLK_CRYPTO, "clock_crypto", mux_2plls_p, 0, RK3328_CLKSEL_CON(20), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(2), 4, GFLAGS),
    COMPOSITE_NOMUX(SCLK_TSADC, "clock_tsadc", "clock_24m", 0, RK3328_CLKSEL_CON(22), 0, 10, DFLAGS, RK3328_CLKGATE_CON(2), 6, GFLAGS),
    COMPOSITE_NOMUX(SCLK_SARADC, "clock_saradc", "clock_24m", 0, RK3328_CLKSEL_CON(23), 0, 10, DFLAGS, RK3328_CLKGATE_CON(2), 14, GFLAGS),
    COMPOSITE(SCLK_SPI, "clock_spi", mux_2plls_p, 0, RK3328_CLKSEL_CON(24), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(2), 7, GFLAGS),
    COMPOSITE(SCLK_PWM, "clock_pwm", mux_2plls_p, 0, RK3328_CLKSEL_CON(24), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3328_CLKGATE_CON(2), 8, GFLAGS),
    COMPOSITE(SCLK_OTP, "clock_otp", mux_2plls_xin24m_p, 0, RK3328_CLKSEL_CON(4), 6, 2, MFLAGS, 0, 6, DFLAGS, RK3328_CLKGATE_CON(3), 8, GFLAGS),
    COMPOSITE(SCLK_EFUSE, "clock_efuse", mux_2plls_xin24m_p, 0, RK3328_CLKSEL_CON(5), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(2), 13, GFLAGS),
    COMPOSITE(
        SCLK_PDM, "clock_pdm", mux_cpll_gpll_apll_p, CLK_SET_RATE_NO_REPARENT | CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(20), 14, 2, MFLAGS, 8, 5,
        DFLAGS, RK3328_CLKGATE_CON(2), 15, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, RK3328_CLKGATE_CON(8), 5, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, RK3328_CLKGATE_CON(8), 6, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, RK3328_CLKGATE_CON(8), 7, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, RK3328_CLKGATE_CON(8), 8, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, RK3328_CLKGATE_CON(8), 9, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, RK3328_CLKGATE_CON(8), 10, GFLAGS),

    COMPOSITE(SCLK_WIFI, "clock_wifi", mux_2plls_u480m_p, 0, RK3328_CLKSEL_CON(52), 6, 2, MFLAGS, 0, 6, DFLAGS, RK3328_CLKGATE_CON(0), 10, GFLAGS),

    /*
     * Clock-Architecture Diagram 5
     */

    /* PD_VIDEO */
    COMPOSITE(
        ACLK_RKVDEC_PRE, "aclock_rkvdec_pre", mux_4plls_p, 0, RK3328_CLKSEL_CON(48), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(6), 0, GFLAGS),
    FACTOR_GATE(HCLK_RKVDEC_PRE, "hclock_rkvdec_pre", "aclock_rkvdec_pre", 0, 1, 4, RK3328_CLKGATE_CON(11), 0, GFLAGS),
    GATE(ACLK_RKVDEC, "aclock_rkvdec", "aclock_rkvdec_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(24), 0, GFLAGS),
    GATE(HCLK_RKVDEC, "hclock_rkvdec", "hclock_rkvdec_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(24), 1, GFLAGS),
    GATE(0, "aclock_rkvdec_niu", "aclock_rkvdec_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(24), 2, GFLAGS),
    GATE(0, "hclock_rkvdec_niu", "hclock_rkvdec_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(24), 3, GFLAGS),

    COMPOSITE(
        SCLK_VDEC_CABAC, "sclock_vdec_cabac", mux_4plls_p, 0, RK3328_CLKSEL_CON(48), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(6), 1, GFLAGS),

    COMPOSITE(
        SCLK_VDEC_CORE, "sclock_vdec_core", mux_4plls_p, 0, RK3328_CLKSEL_CON(49), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(6), 2, GFLAGS),

    COMPOSITE(ACLK_VPU_PRE, "aclock_vpu_pre", mux_4plls_p, 0, RK3328_CLKSEL_CON(50), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(6), 5, GFLAGS),
    FACTOR_GATE(HCLK_VPU_PRE, "hclock_vpu_pre", "aclock_vpu_pre", 0, 1, 4, RK3328_CLKGATE_CON(11), 8, GFLAGS),
    GATE(ACLK_VPU, "aclock_vpu", "aclock_vpu_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(23), 0, GFLAGS),
    GATE(HCLK_VPU, "hclock_vpu", "hclock_vpu_pre", CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(23), 1, GFLAGS),
    GATE(0, "aclock_vpu_niu", "aclock_vpu_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(23), 2, GFLAGS),
    GATE(0, "hclock_vpu_niu", "hclock_vpu_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(23), 3, GFLAGS),

    COMPOSITE(ACLK_RKVENC, "aclock_rkvenc", mux_4plls_p, 0, RK3328_CLKSEL_CON(51), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(6), 3, GFLAGS),
    FACTOR_GATE(HCLK_RKVENC, "hclock_rkvenc", "aclock_rkvenc", 0, 1, 4, RK3328_CLKGATE_CON(11), 4, GFLAGS),
    GATE(0, "aclock_rkvenc_niu", "aclock_rkvenc", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(25), 0, GFLAGS),
    GATE(0, "hclock_rkvenc_niu", "hclock_rkvenc", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(25), 1, GFLAGS),
    GATE(ACLK_H265, "aclock_h265", "aclock_rkvenc", 0, RK3328_CLKGATE_CON(25), 2, GFLAGS),
    GATE(PCLK_H265, "pclock_h265", "hclock_rkvenc", 0, RK3328_CLKGATE_CON(25), 3, GFLAGS),
    GATE(ACLK_H264, "aclock_h264", "aclock_rkvenc", 0, RK3328_CLKGATE_CON(25), 4, GFLAGS),
    GATE(HCLK_H264, "hclock_h264", "hclock_rkvenc", 0, RK3328_CLKGATE_CON(25), 5, GFLAGS),
    GATE(ACLK_AXISRAM, "aclock_axisram", "aclock_rkvenc", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(25), 6, GFLAGS),

    COMPOSITE(
        SCLK_VENC_CORE, "sclock_venc_core", mux_4plls_p, 0, RK3328_CLKSEL_CON(51), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(6), 4, GFLAGS),

    COMPOSITE(SCLK_VENC_DSP, "sclock_venc_dsp", mux_4plls_p, 0, RK3328_CLKSEL_CON(52), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(6), 7, GFLAGS),

    /*
     * Clock-Architecture Diagram 6
     */

    /* PD_VIO */
    COMPOSITE(ACLK_VIO_PRE, "aclock_vio_pre", mux_4plls_p, 0, RK3328_CLKSEL_CON(37), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(5), 2, GFLAGS),
    DIV(HCLK_VIO_PRE, "hclock_vio_pre", "aclock_vio_pre", 0, RK3328_CLKSEL_CON(37), 8, 5, DFLAGS),

    COMPOSITE(ACLK_RGA_PRE, "aclock_rga_pre", mux_4plls_p, 0, RK3328_CLKSEL_CON(36), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(5), 0, GFLAGS),
    COMPOSITE(SCLK_RGA, "clock_rga", mux_4plls_p, 0, RK3328_CLKSEL_CON(36), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(5), 1, GFLAGS),
    COMPOSITE(ACLK_VOP_PRE, "aclock_vop_pre", mux_4plls_p, 0, RK3328_CLKSEL_CON(39), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(5), 5, GFLAGS),
    GATE(SCLK_HDMI_SFC, "clock_hdmi_sfc", "xin24m", 0, RK3328_CLKGATE_CON(5), 4, GFLAGS),

    COMPOSITE_NODIV(0, "clock_cif_src", mux_hdmiphy_gpll_p, 0, RK3328_CLKSEL_CON(42), 7, 1, MFLAGS, RK3328_CLKGATE_CON(5), 3, GFLAGS),
    COMPOSITE_NOGATE(SCLK_CIF_OUT, "clock_cif_out", mux_sclock_cif_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(42), 5, 1, MFLAGS, 0, 5, DFLAGS),

    COMPOSITE(
        DCLK_LCDC_SRC, "dclock_lcdc_src", mux_gpll_cpll_p, 0, RK3328_CLKSEL_CON(40), 0, 1, MFLAGS, 8, 8, DFLAGS, RK3328_CLKGATE_CON(5), 6, GFLAGS),
    DIV(DCLK_HDMIPHY, "dclock_hdmiphy", "dclock_lcdc_src", 0, RK3328_CLKSEL_CON(40), 3, 3, DFLAGS),
    MUX(DCLK_LCDC, "dclock_lcdc", mux_dclock_lcdc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3328_CLKSEL_CON(40), 1, 1, MFLAGS),

    /*
     * Clock-Architecture Diagram 7
     */

    /* PD_PERI */
    GATE(0, "gpll_peri", "gpll", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(4), 0, GFLAGS),
    GATE(0, "cpll_peri", "cpll", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(4), 1, GFLAGS),
    GATE(0, "hdmiphy_peri", "hdmiphy", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(4), 2, GFLAGS),
    COMPOSITE_NOGATE(ACLK_PERI_PRE, "aclock_peri_pre", mux_aclock_peri_pre_p, 0, RK3328_CLKSEL_CON(28), 6, 2, MFLAGS, 0, 5, DFLAGS),
    COMPOSITE_NOMUX(
        PCLK_PERI, "pclock_peri", "aclock_peri_pre", CLK_IGNORE_UNUSED, RK3328_CLKSEL_CON(29), 0, 2, DFLAGS, RK3328_CLKGATE_CON(10), 2, GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_PERI, "hclock_peri", "aclock_peri_pre", CLK_IGNORE_UNUSED, RK3328_CLKSEL_CON(29), 4, 3, DFLAGS, RK3328_CLKGATE_CON(10), 1, GFLAGS),
    GATE(ACLK_PERI, "aclock_peri", "aclock_peri_pre", CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT, RK3328_CLKGATE_CON(10), 0, GFLAGS),

    COMPOSITE(
        SCLK_SDMMC, "clock_sdmmc", mux_2plls_24m_u480m_p, 0, RK3328_CLKSEL_CON(30), 8, 2, MFLAGS, 0, 8, DFLAGS, RK3328_CLKGATE_CON(4), 3, GFLAGS),

    COMPOSITE(SCLK_SDIO, "clock_sdio", mux_2plls_24m_u480m_p, 0, RK3328_CLKSEL_CON(31), 8, 2, MFLAGS, 0, 8, DFLAGS, RK3328_CLKGATE_CON(4), 4, GFLAGS),

    COMPOSITE(SCLK_EMMC, "clock_emmc", mux_2plls_24m_u480m_p, 0, RK3328_CLKSEL_CON(32), 8, 2, MFLAGS, 0, 8, DFLAGS, RK3328_CLKGATE_CON(4), 5, GFLAGS),

    COMPOSITE(
        SCLK_SDMMC_EXT, "clock_sdmmc_ext", mux_2plls_24m_u480m_p, 0, RK3328_CLKSEL_CON(43), 8, 2, MFLAGS, 0, 8, DFLAGS, RK3328_CLKGATE_CON(4), 10,
        GFLAGS),

    COMPOSITE(
        SCLK_REF_USB3OTG_SRC, "clock_ref_usb3otg_src", mux_2plls_p, 0, RK3328_CLKSEL_CON(45), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3328_CLKGATE_CON(4), 9,
        GFLAGS),

    MUX(SCLK_REF_USB3OTG, "clock_ref_usb3otg", mux_ref_usb3otg_src_p, CLK_SET_RATE_PARENT, RK3328_CLKSEL_CON(45), 8, 1, MFLAGS),

    GATE(SCLK_USB3OTG_REF, "clock_usb3otg_ref", "xin24m", 0, RK3328_CLKGATE_CON(4), 7, GFLAGS),

    COMPOSITE(
        SCLK_USB3OTG_SUSPEND, "clock_usb3otg_suspend", mux_xin24m_32k_p, 0, RK3328_CLKSEL_CON(33), 15, 1, MFLAGS, 0, 10, DFLAGS,
        RK3328_CLKGATE_CON(4), 8, GFLAGS),

    /*
     * Clock-Architecture Diagram 8
     */

    /* PD_GMAC */
    COMPOSITE(ACLK_GMAC, "aclock_gmac", mux_2plls_hdmiphy_p, 0, RK3328_CLKSEL_CON(25), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(3), 2, GFLAGS),
    COMPOSITE_NOMUX(PCLK_GMAC, "pclock_gmac", "aclock_gmac", 0, RK3328_CLKSEL_CON(25), 8, 3, DFLAGS, RK3328_CLKGATE_CON(9), 0, GFLAGS),

    COMPOSITE(
        SCLK_MAC2IO_SRC, "clock_mac2io_src", mux_2plls_p, 0, RK3328_CLKSEL_CON(27), 7, 1, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(3), 1, GFLAGS),
    GATE(SCLK_MAC2IO_REF, "clock_mac2io_ref", "clock_mac2io", 0, RK3328_CLKGATE_CON(9), 7, GFLAGS),
    GATE(SCLK_MAC2IO_RX, "clock_mac2io_rx", "clock_mac2io", 0, RK3328_CLKGATE_CON(9), 4, GFLAGS),
    GATE(SCLK_MAC2IO_TX, "clock_mac2io_tx", "clock_mac2io", 0, RK3328_CLKGATE_CON(9), 5, GFLAGS),
    GATE(SCLK_MAC2IO_REFOUT, "clock_mac2io_refout", "clock_mac2io", 0, RK3328_CLKGATE_CON(9), 6, GFLAGS),
    COMPOSITE(
        SCLK_MAC2IO_OUT, "clock_mac2io_out", mux_2plls_p, 0, RK3328_CLKSEL_CON(27), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3328_CLKGATE_CON(3), 5, GFLAGS),
    MUXGRF(SCLK_MAC2IO, "clock_mac2io", mux_mac2io_src_p, CLK_SET_RATE_NO_REPARENT, RK3328_GRF_MAC_CON1, 10, 1, MFLAGS),
    MUXGRF(SCLK_MAC2IO_EXT, "clock_mac2io_ext", mux_mac2io_ext_p, CLK_SET_RATE_NO_REPARENT, RK3328_GRF_SOC_CON4, 14, 1, MFLAGS),

    COMPOSITE(
        SCLK_MAC2PHY_SRC, "clock_mac2phy_src", mux_2plls_p, 0, RK3328_CLKSEL_CON(26), 7, 1, MFLAGS, 0, 5, DFLAGS, RK3328_CLKGATE_CON(3), 0, GFLAGS),
    GATE(SCLK_MAC2PHY_REF, "clock_mac2phy_ref", "clock_mac2phy", 0, RK3328_CLKGATE_CON(9), 3, GFLAGS),
    GATE(SCLK_MAC2PHY_RXTX, "clock_mac2phy_rxtx", "clock_mac2phy", 0, RK3328_CLKGATE_CON(9), 1, GFLAGS),
    COMPOSITE_NOMUX(SCLK_MAC2PHY_OUT, "clock_mac2phy_out", "clock_mac2phy", 0, RK3328_CLKSEL_CON(26), 8, 2, DFLAGS, RK3328_CLKGATE_CON(9), 2, GFLAGS),
    MUXGRF(SCLK_MAC2PHY, "clock_mac2phy", mux_mac2phy_src_p, CLK_SET_RATE_NO_REPARENT, RK3328_GRF_MAC_CON2, 10, 1, MFLAGS),

    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    /*
     * Clock-Architecture Diagram 9
     */

    /* PD_VOP */
    GATE(ACLK_RGA, "aclock_rga", "aclock_rga_pre", 0, RK3328_CLKGATE_CON(21), 10, GFLAGS),
    GATE(0, "aclock_rga_niu", "aclock_rga_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(22), 3, GFLAGS),
    GATE(ACLK_VOP, "aclock_vop", "aclock_vop_pre", 0, RK3328_CLKGATE_CON(21), 2, GFLAGS),
    GATE(0, "aclock_vop_niu", "aclock_vop_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(21), 4, GFLAGS),

    GATE(ACLK_IEP, "aclock_iep", "aclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 6, GFLAGS),
    GATE(ACLK_CIF, "aclock_cif", "aclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 8, GFLAGS),
    GATE(ACLK_HDCP, "aclock_hdcp", "aclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 15, GFLAGS),
    GATE(0, "aclock_vio_niu", "aclock_vio_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(22), 2, GFLAGS),

    GATE(HCLK_VOP, "hclock_vop", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 3, GFLAGS),
    GATE(0, "hclock_vop_niu", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(21), 5, GFLAGS),
    GATE(HCLK_IEP, "hclock_iep", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 7, GFLAGS),
    GATE(HCLK_CIF, "hclock_cif", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 9, GFLAGS),
    GATE(HCLK_RGA, "hclock_rga", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(21), 11, GFLAGS),
    GATE(0, "hclock_ahb1tom", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(21), 12, GFLAGS),
    GATE(0, "pclock_vio_h2p", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(21), 13, GFLAGS),
    GATE(0, "hclock_vio_h2p", "hclock_vio_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(21), 14, GFLAGS),
    GATE(HCLK_HDCP, "hclock_hdcp", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(22), 0, GFLAGS),
    GATE(HCLK_VIO, "hclock_vio", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(22), 1, GFLAGS),
    GATE(PCLK_HDMI, "pclock_hdmi", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(22), 4, GFLAGS),
    GATE(PCLK_HDCP, "pclock_hdcp", "hclock_vio_pre", 0, RK3328_CLKGATE_CON(22), 5, GFLAGS),

    /* PD_PERI */
    GATE(0, "aclock_peri_noc", "aclock_peri", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(19), 11, GFLAGS),
    GATE(ACLK_USB3OTG, "aclock_usb3otg", "aclock_peri", 0, RK3328_CLKGATE_CON(19), 14, GFLAGS),

    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 0, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 1, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 2, GFLAGS),
    GATE(HCLK_SDMMC_EXT, "hclock_sdmmc_ext", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 15, GFLAGS),
    GATE(HCLK_HOST0, "hclock_host0", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 6, GFLAGS),
    GATE(HCLK_HOST0_ARB, "hclock_host0_arb", "hclock_peri", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(19), 7, GFLAGS),
    GATE(HCLK_OTG, "hclock_otg", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 8, GFLAGS),
    GATE(HCLK_OTG_PMU, "hclock_otg_pmu", "hclock_peri", 0, RK3328_CLKGATE_CON(19), 9, GFLAGS),
    GATE(0, "hclock_peri_niu", "hclock_peri", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(19), 12, GFLAGS),
    GATE(0, "pclock_peri_niu", "hclock_peri", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(19), 13, GFLAGS),

    /* PD_GMAC */
    GATE(ACLK_MAC2PHY, "aclock_mac2phy", "aclock_gmac", 0, RK3328_CLKGATE_CON(26), 0, GFLAGS),
    GATE(ACLK_MAC2IO, "aclock_mac2io", "aclock_gmac", 0, RK3328_CLKGATE_CON(26), 2, GFLAGS),
    GATE(0, "aclock_gmac_niu", "aclock_gmac", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(26), 4, GFLAGS),
    GATE(PCLK_MAC2PHY, "pclock_mac2phy", "pclock_gmac", 0, RK3328_CLKGATE_CON(26), 1, GFLAGS),
    GATE(PCLK_MAC2IO, "pclock_mac2io", "pclock_gmac", 0, RK3328_CLKGATE_CON(26), 3, GFLAGS),
    GATE(0, "pclock_gmac_niu", "pclock_gmac", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(26), 5, GFLAGS),

    /* PD_BUS */
    GATE(0, "aclock_bus_niu", "aclock_bus_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 12, GFLAGS),
    GATE(ACLK_DCF, "aclock_dcf", "aclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 11, GFLAGS),
    GATE(ACLK_TSP, "aclock_tsp", "aclock_bus_pre", 0, RK3328_CLKGATE_CON(17), 12, GFLAGS),
    GATE(0, "aclock_intmem", "aclock_bus_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 0, GFLAGS),
    GATE(ACLK_DMAC, "aclock_dmac_bus", "aclock_bus_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 1, GFLAGS),

    GATE(0, "hclock_rom", "hclock_bus_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 2, GFLAGS),
    GATE(HCLK_I2S0_8CH, "hclock_i2s0_8ch", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 3, GFLAGS),
    GATE(HCLK_I2S1_8CH, "hclock_i2s1_8ch", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 4, GFLAGS),
    GATE(HCLK_I2S2_2CH, "hclock_i2s2_2ch", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 5, GFLAGS),
    GATE(HCLK_SPDIF_8CH, "hclock_spdif_8ch", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 6, GFLAGS),
    GATE(HCLK_TSP, "hclock_tsp", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(17), 11, GFLAGS),
    GATE(HCLK_CRYPTO_MST, "hclock_crypto_mst", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 7, GFLAGS),
    GATE(HCLK_CRYPTO_SLV, "hclock_crypto_slv", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(15), 8, GFLAGS),
    GATE(0, "hclock_bus_niu", "hclock_bus_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 13, GFLAGS),
    GATE(HCLK_PDM, "hclock_pdm", "hclock_bus_pre", 0, RK3328_CLKGATE_CON(28), 0, GFLAGS),

    GATE(0, "pclock_bus_niu", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 14, GFLAGS),
    GATE(0, "pclock_efuse", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 9, GFLAGS),
    GATE(0, "pclock_otp", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(28), 4, GFLAGS),
    GATE(PCLK_I2C0, "pclock_i2c0", "pclock_bus", 0, RK3328_CLKGATE_CON(15), 10, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 0, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 1, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 2, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer0", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 3, GFLAGS),
    GATE(0, "pclock_stimer", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 4, GFLAGS),
    GATE(PCLK_SPI, "pclock_spi", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 5, GFLAGS),
    GATE(PCLK_PWM, "pclock_rk_pwm", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 6, GFLAGS),
    GATE(PCLK_GPIO0, "pclock_gpio0", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 7, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 8, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 9, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 10, GFLAGS),
    GATE(PCLK_UART0, "pclock_uart0", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 11, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 12, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 13, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 14, GFLAGS),
    GATE(PCLK_DCF, "pclock_dcf", "pclock_bus", 0, RK3328_CLKGATE_CON(16), 15, GFLAGS),
    GATE(PCLK_GRF, "pclock_grf", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 0, GFLAGS),
    GATE(0, "pclock_cru", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 4, GFLAGS),
    GATE(PCLK_ACODEC, "pclock_acodec", "pclock_bus", 0, RK3328_CLKGATE_CON(17), 5, GFLAGS),
    GATE(0, "pclock_sgrf", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 6, GFLAGS),
    GATE(0, "pclock_sim", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 10, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "pclock_bus", 0, RK3328_CLKGATE_CON(17), 15, GFLAGS),
    GATE(0, "pclock_pmu", "pclock_bus", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(28), 3, GFLAGS),

    GATE(PCLK_USB3PHY_OTG, "pclock_usb3phy_otg", "pclock_phy_pre", 0, RK3328_CLKGATE_CON(28), 1, GFLAGS),
    GATE(PCLK_USB3PHY_PIPE, "pclock_usb3phy_pipe", "pclock_phy_pre", 0, RK3328_CLKGATE_CON(28), 2, GFLAGS),
    GATE(PCLK_USB3_GRF, "pclock_usb3_grf", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 2, GFLAGS),
    GATE(PCLK_USB2_GRF, "pclock_usb2_grf", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 14, GFLAGS),
    GATE(0, "pclock_ddrphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 13, GFLAGS),
    GATE(0, "pclock_acodecphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 5, GFLAGS),
    GATE(PCLK_HDMIPHY, "pclock_hdmiphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 7, GFLAGS),
    GATE(0, "pclock_vdacphy", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(17), 8, GFLAGS),
    GATE(0, "pclock_phy_niu", "pclock_phy_pre", CLK_IGNORE_UNUSED, RK3328_CLKGATE_CON(15), 15, GFLAGS),

    /* PD_MMC */
    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "clock_sdmmc", RK3328_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "clock_sdmmc", RK3328_SDMMC_CON1, 1),

    MMC(SCLK_SDIO_DRV, "sdio_drv", "clock_sdio", RK3328_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "clock_sdio", RK3328_SDIO_CON1, 1),

    MMC(SCLK_EMMC_DRV, "emmc_drv", "clock_emmc", RK3328_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "clock_emmc", RK3328_EMMC_CON1, 1),

    MMC(SCLK_SDMMC_EXT_DRV, "sdmmc_ext_drv", "clock_sdmmc_ext", RK3328_SDMMC_EXT_CON0, 1),
    MMC(SCLK_SDMMC_EXT_SAMPLE, "sdmmc_ext_sample", "clock_sdmmc_ext", RK3328_SDMMC_EXT_CON1, 1),
};

static const char *const rk3328_critical_clocks[] __initconst = {
    "aclock_bus",      "pclock_bus",     "hclock_bus",     "aclock_peri",    "hclock_peri",    "pclock_peri",    "pclock_dbg",    "aclock_core_niu",
    "aclock_gic400",   "aclock_intmem",  "hclock_rom",     "pclock_grf",     "pclock_cru",     "pclock_sgrf",    "pclock_timer0", "clock_timer0",
    "pclock_ddr_msch", "pclock_ddr_mon", "pclock_ddr_grf", "clock_ddrupctl", "clock_ddrmsch",  "hclock_ahb1tom", "clock_jtag",    "pclock_ddrphy",
    "pclock_pmu",      "hclock_otg_pmu", "aclock_rga_niu", "pclock_vio_h2p", "hclock_vio_h2p",
};

static void __iomem *rk3328_cru_base;

void rk3328_dump_cru(void)
{
    if (rk3328_cru_base) {
        pr_warn("CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4, rk3328_cru_base, 0x400, false);
    }
}

EXPORT_SYMBOL_GPL(rk3328_dump_cru);

static int rk3328_clock_panic(struct notifier_block *this, uint64_t ev, void *ptr)
{
    rk3328_dump_cru();
    return NOTIFY_DONE;
}

static struct notifier_block rk3328_clock_panic_block = {
    .notifier_call = rk3328_clock_panic,
};

static void __init rk3328_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    rk3328_cru_base = reg_base;

    ctx             = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(reg_base);
        return;
    }

    rockchip_clock_register_plls(ctx, rk3328_pll_clocks, ARRAY_SIZE(rk3328_pll_clocks), RK3328_GRF_SOC_STATUS0);
    rockchip_clock_register_branches(ctx, rk3328_clock_branches, ARRAY_SIZE(rk3328_clock_branches));
    rockchip_clock_protect_critical(rk3328_critical_clocks, ARRAY_SIZE(rk3328_critical_clocks));

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &rk3328_cpuclock_data, rk3328_cpuclock_rates,
        ARRAY_SIZE(rk3328_cpuclock_rates));

    rockchip_register_softrst(np, 12, reg_base + RK3328_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK3328_GLB_SRST_FST, NULL);

    rockchip_clock_of_add_provider(np, ctx);

    atomic_notifier_chain_register(&panic_notifier_list, &rk3328_clock_panic_block);
}

CLK_OF_DECLARE(rk3328_cru, "rockchip,rk3328-cru", rk3328_clock_init);
