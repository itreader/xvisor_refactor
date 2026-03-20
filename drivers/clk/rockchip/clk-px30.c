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

#include <dt-bindings/clock/px30-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define PX30_GRF_SOC_STATUS0 0x480
#define PX30_FRAC_MAX_PRATE  600000000

enum px30_plls {
    apll,
    dpll,
    cpll,
    npll,
    apll_b_h,
    apll_b_l,
};

enum px30_pmu_plls {
    gpll,
};

static struct rockchip_pll_rate_table px30_pll_rates[] = {
    /* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
    RK3036_PLL_RATE(1608000000, 1, 67, 1, 1, 1, 0), RK3036_PLL_RATE(1584000000, 1, 66, 1, 1, 1, 0),
    RK3036_PLL_RATE(1560000000, 1, 65, 1, 1, 1, 0), RK3036_PLL_RATE(1536000000, 1, 64, 1, 1, 1, 0),
    RK3036_PLL_RATE(1512000000, 1, 63, 1, 1, 1, 0), RK3036_PLL_RATE(1488000000, 1, 62, 1, 1, 1, 0),
    RK3036_PLL_RATE(1464000000, 1, 61, 1, 1, 1, 0), RK3036_PLL_RATE(1440000000, 1, 60, 1, 1, 1, 0),
    RK3036_PLL_RATE(1416000000, 1, 59, 1, 1, 1, 0), RK3036_PLL_RATE(1392000000, 1, 58, 1, 1, 1, 0),
    RK3036_PLL_RATE(1368000000, 1, 57, 1, 1, 1, 0), RK3036_PLL_RATE(1344000000, 1, 56, 1, 1, 1, 0),
    RK3036_PLL_RATE(1320000000, 1, 55, 1, 1, 1, 0), RK3036_PLL_RATE(1296000000, 1, 54, 1, 1, 1, 0),
    RK3036_PLL_RATE(1272000000, 1, 53, 1, 1, 1, 0), RK3036_PLL_RATE(1248000000, 1, 52, 1, 1, 1, 0),
    RK3036_PLL_RATE(1200000000, 1, 50, 1, 1, 1, 0), RK3036_PLL_RATE(1188000000, 2, 99, 1, 1, 1, 0),
    RK3036_PLL_RATE(1104000000, 1, 46, 1, 1, 1, 0), RK3036_PLL_RATE(1100000000, 12, 550, 1, 1, 1, 0),
    RK3036_PLL_RATE(1008000000, 1, 84, 2, 1, 1, 0), RK3036_PLL_RATE(1000000000, 6, 500, 2, 1, 1, 0),
    RK3036_PLL_RATE(984000000, 1, 82, 2, 1, 1, 0),  RK3036_PLL_RATE(960000000, 1, 80, 2, 1, 1, 0),
    RK3036_PLL_RATE(936000000, 1, 78, 2, 1, 1, 0),  RK3036_PLL_RATE(912000000, 1, 76, 2, 1, 1, 0),
    RK3036_PLL_RATE(900000000, 4, 300, 2, 1, 1, 0), RK3036_PLL_RATE(888000000, 1, 74, 2, 1, 1, 0),
    RK3036_PLL_RATE(864000000, 1, 72, 2, 1, 1, 0),  RK3036_PLL_RATE(840000000, 1, 70, 2, 1, 1, 0),
    RK3036_PLL_RATE(816000000, 1, 68, 2, 1, 1, 0),  RK3036_PLL_RATE(800000000, 6, 400, 2, 1, 1, 0),
    RK3036_PLL_RATE(700000000, 6, 350, 2, 1, 1, 0), RK3036_PLL_RATE(696000000, 1, 58, 2, 1, 1, 0),
    RK3036_PLL_RATE(624000000, 1, 52, 2, 1, 1, 0),  RK3036_PLL_RATE(600000000, 1, 75, 3, 1, 1, 0),
    RK3036_PLL_RATE(594000000, 2, 99, 2, 1, 1, 0),  RK3036_PLL_RATE(504000000, 1, 63, 3, 1, 1, 0),
    RK3036_PLL_RATE(500000000, 6, 250, 2, 1, 1, 0), RK3036_PLL_RATE(408000000, 1, 68, 2, 2, 1, 0),
    RK3036_PLL_RATE(312000000, 1, 52, 2, 2, 1, 0),  RK3036_PLL_RATE(216000000, 1, 72, 4, 2, 1, 0),
    RK3036_PLL_RATE(96000000, 1, 64, 4, 4, 1, 0),   {/* sentinel */},
};

#define PX30_DIV_ACLKM_MASK     0x7
#define PX30_DIV_ACLKM_SHIFT    12
#define PX30_DIV_PCLK_DBG_MASK  0xf
#define PX30_DIV_PCLK_DBG_SHIFT 8

#define PX30_CLKSEL0(_aclock_core, _pclock_dbg)                                                                                                      \
    {                                                                                                                                                \
        .reg = PX30_CLKSEL_CON(0),                                                                                                                   \
        .val = HIWORD_UPDATE(_aclock_core, PX30_DIV_ACLKM_MASK, PX30_DIV_ACLKM_SHIFT) |                                                              \
               HIWORD_UPDATE(_pclock_dbg, PX30_DIV_PCLK_DBG_MASK, PX30_DIV_PCLK_DBG_SHIFT),                                                          \
    }

#define PX30_CPUCLK_RATE(_prate, _aclock_core, _pclock_dbg)                                                                                          \
    {                                                                                                                                                \
        .prate = _prate,                                                                                                                             \
        .divs  = {                                                                                                                                   \
            PX30_CLKSEL0(_aclock_core, _pclock_dbg),                                                                                                \
        },                                                                                                                                          \
    }

static struct rockchip_cpuclock_rate_table px30_cpuclock_rates[] __initdata = {
    PX30_CPUCLK_RATE(1608000000, 1, 7), PX30_CPUCLK_RATE(1584000000, 1, 7), PX30_CPUCLK_RATE(1560000000, 1, 7), PX30_CPUCLK_RATE(1536000000, 1, 7),
    PX30_CPUCLK_RATE(1512000000, 1, 7), PX30_CPUCLK_RATE(1488000000, 1, 5), PX30_CPUCLK_RATE(1464000000, 1, 5), PX30_CPUCLK_RATE(1440000000, 1, 5),
    PX30_CPUCLK_RATE(1416000000, 1, 5), PX30_CPUCLK_RATE(1392000000, 1, 5), PX30_CPUCLK_RATE(1368000000, 1, 5), PX30_CPUCLK_RATE(1344000000, 1, 5),
    PX30_CPUCLK_RATE(1320000000, 1, 5), PX30_CPUCLK_RATE(1296000000, 1, 5), PX30_CPUCLK_RATE(1272000000, 1, 5), PX30_CPUCLK_RATE(1248000000, 1, 5),
    PX30_CPUCLK_RATE(1224000000, 1, 5), PX30_CPUCLK_RATE(1200000000, 1, 5), PX30_CPUCLK_RATE(1104000000, 1, 5), PX30_CPUCLK_RATE(1008000000, 1, 5),
    PX30_CPUCLK_RATE(912000000, 1, 5),  PX30_CPUCLK_RATE(816000000, 1, 3),  PX30_CPUCLK_RATE(696000000, 1, 3),  PX30_CPUCLK_RATE(600000000, 1, 3),
    PX30_CPUCLK_RATE(408000000, 1, 1),  PX30_CPUCLK_RATE(312000000, 1, 1),  PX30_CPUCLK_RATE(216000000, 1, 1),  PX30_CPUCLK_RATE(96000000, 1, 1),
};

static const struct rockchip_cpuclock_reg_data px30_cpuclock_data = {
    .core_reg       = PX30_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0xf,
    .mux_core_alt   = 1,
    .mux_core_main  = 0,
    .mux_core_shift = 7,
    .mux_core_mask  = 0x1,
    .pll_name       = "pll_apll",
};

PNAME(mux_pll_p)                          = {"xin24m"};
PNAME(mux_usb480m_p)                      = {"xin24m", "usb480m_phy", "clock_rtc32k_pmu"};
PNAME(mux_armclock_p)                     = {"apll_core", "gpll_core"};
PNAME(mux_ddrphy_p)                       = {"dpll_ddr", "gpll_ddr"};
PNAME(mux_ddrstdby_p)                     = {"clock_ddrphy1x", "clock_stdby_2wrap"};
PNAME(mux_gpll_dmycpll_usb480m_npll_p)    = {"gpll", "dummy_cpll", "usb480m", "npll"};
PNAME(mux_gpll_dmycpll_usb480m_dmynpll_p) = {"gpll", "dummy_cpll", "usb480m", "dummy_npll"};
PNAME(mux_cpll_npll_p)                    = {"cpll", "npll"};
PNAME(mux_npll_cpll_p)                    = {"npll", "cpll"};
PNAME(mux_gpll_cpll_p)                    = {"gpll", "dummy_cpll"};
PNAME(mux_gpll_npll_p)                    = {"gpll", "dummy_npll"};
PNAME(mux_gpll_xin24m_p)                  = {"gpll", "xin24m"};
PNAME(mux_gpll_cpll_npll_p)               = {"gpll", "dummy_cpll", "dummy_npll"};
PNAME(mux_gpll_cpll_npll_xin24m_p)        = {"gpll", "dummy_cpll", "dummy_npll", "xin24m"};
PNAME(mux_gpll_xin24m_npll_p)             = {"gpll", "xin24m", "dummy_npll"};
PNAME(mux_pdm_p)                          = {"clock_pdm_src", "clock_pdm_frac"};
PNAME(mux_i2s0_tx_p)                      = {"clock_i2s0_tx_src", "clock_i2s0_tx_frac", "mclock_i2s0_tx_in", "xin12m"};
PNAME(mux_i2s0_rx_p)                      = {"clock_i2s0_rx_src", "clock_i2s0_rx_frac", "mclock_i2s0_rx_in", "xin12m"};
PNAME(mux_i2s1_p)                         = {"clock_i2s1_src", "clock_i2s1_frac", "i2s1_clockin", "xin12m"};
PNAME(mux_i2s2_p)                         = {"clock_i2s2_src", "clock_i2s2_frac", "i2s2_clockin", "xin12m"};
PNAME(mux_i2s0_tx_out_p)                  = {"clock_i2s0_tx", "xin12m", "clock_i2s0_rx"};
PNAME(mux_i2s0_rx_out_p)                  = {"clock_i2s0_rx", "xin12m", "clock_i2s0_tx"};
PNAME(mux_i2s1_out_p)                     = {"clock_i2s1", "xin12m"};
PNAME(mux_i2s2_out_p)                     = {"clock_i2s2", "xin12m"};
PNAME(mux_i2s0_tx_rx_p)                   = {"clock_i2s0_tx_mux", "clock_i2s0_rx_mux"};
PNAME(mux_i2s0_rx_tx_p)                   = {"clock_i2s0_rx_mux", "clock_i2s0_tx_mux"};
PNAME(mux_uart_src_p)                     = {"gpll", "xin24m", "usb480m", "dummy_npll"};
PNAME(mux_uart1_p)                        = {"clock_uart1_src", "clock_uart1_np5", "clock_uart1_frac"};
PNAME(mux_uart2_p)                        = {"clock_uart2_src", "clock_uart2_np5", "clock_uart2_frac"};
PNAME(mux_uart3_p)                        = {"clock_uart3_src", "clock_uart3_np5", "clock_uart3_frac"};
PNAME(mux_uart4_p)                        = {"clock_uart4_src", "clock_uart4_np5", "clock_uart4_frac"};
PNAME(mux_uart5_p)                        = {"clock_uart5_src", "clock_uart5_np5", "clock_uart5_frac"};
PNAME(mux_cif_out_p)                      = {"xin24m", "dummy_cpll", "dummy_npll", "usb480m"};
PNAME(mux_dclock_vopb_p)                  = {"dclock_vopb_src", "dclock_vopb_frac", "xin24m"};
PNAME(mux_dclock_vopl_p)                  = {"dclock_vopl_src", "dclock_vopl_frac", "xin24m"};
PNAME(mux_nandc_p)                        = {"clock_nandc_div", "clock_nandc_div50"};
PNAME(mux_sdio_p)                         = {"clock_sdio_div", "clock_sdio_div50"};
PNAME(mux_emmc_p)                         = {"clock_emmc_div", "clock_emmc_div50"};
PNAME(mux_sdmmc_p)                        = {"clock_sdmmc_div", "clock_sdmmc_div50"};
PNAME(mux_gmac_p)                         = {"clock_gmac_src", "gmac_clockin"};
PNAME(mux_gmac_rmii_sel_p)                = {"clock_gmac_rx_tx_div20", "clock_gmac_rx_tx_div2"};
PNAME(mux_rtc32k_pmu_p)                   = {
    "xin32k",
    "pmu_pvtm_32k",
    "clock_rtc32k_frac",
};
PNAME(mux_wifi_pmu_p)                                         = {"xin24m", "clock_wifi_pmu_src"};
PNAME(mux_uart0_pmu_p)                                        = {"clock_uart0_pmu_src", "clock_uart0_np5", "clock_uart0_frac"};
PNAME(mux_usbphy_ref_p)                                       = {"xin24m", "clock_ref24m_pmu"};
PNAME(mux_mipidsiphy_ref_p)                                   = {"xin24m", "clock_ref24m_pmu"};

static struct rockchip_pll_clock px30_pll_clocks[] __initdata = {
    [apll] = PLL(pll_rk3328, PLL_APLL, "apll", mux_pll_p, 0, PX30_PLL_CON(0), PX30_MODE_CON, 0, 0, 0, px30_pll_rates),
    [dpll] = PLL(pll_rk3328, PLL_DPLL, "dpll", mux_pll_p, 0, PX30_PLL_CON(8), PX30_MODE_CON, 4, 1, 0, NULL),
    [cpll] = PLL(pll_rk3328, PLL_CPLL, "cpll", mux_pll_p, 0, PX30_PLL_CON(16), PX30_MODE_CON, 2, 2, 0, px30_pll_rates),
    [npll] = PLL(pll_rk3328, PLL_NPLL, "npll", mux_pll_p, 0, PX30_PLL_CON(24), PX30_MODE_CON, 6, 4, 0, px30_pll_rates),
};

static struct rockchip_pll_clock px30_pmu_pll_clocks[] __initdata = {
    [gpll] = PLL(pll_rk3328, PLL_GPLL, "gpll", mux_pll_p, 0, PX30_PMU_PLL_CON(0), PX30_PMU_MODE, 0, 3, 0, px30_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

static struct rockchip_clock_branch px30_pdm_fracmux __initdata =
    MUX(0, "clock_pdm_mux", mux_pdm_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(26), 15, 1, MFLAGS);

static struct rockchip_clock_branch px30_i2s0_tx_fracmux __initdata =
    MUX(SCLK_I2S0_TX_MUX, "clock_i2s0_tx_mux", mux_i2s0_tx_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(28), 10, 2, MFLAGS);

static struct rockchip_clock_branch px30_i2s0_rx_fracmux __initdata =
    MUX(SCLK_I2S0_RX_MUX, "clock_i2s0_rx_mux", mux_i2s0_rx_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(58), 10, 2, MFLAGS);

static struct rockchip_clock_branch px30_i2s1_fracmux __initdata =
    MUX(0, "clock_i2s1_mux", mux_i2s1_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(30), 10, 2, MFLAGS);

static struct rockchip_clock_branch px30_i2s2_fracmux __initdata =
    MUX(0, "clock_i2s2_mux", mux_i2s2_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(32), 10, 2, MFLAGS);

static struct rockchip_clock_branch px30_uart1_fracmux __initdata =
    MUX(0, "clock_uart1_mux", mux_uart1_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(35), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_uart2_fracmux __initdata =
    MUX(0, "clock_uart2_mux", mux_uart2_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(38), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_uart3_fracmux __initdata =
    MUX(0, "clock_uart3_mux", mux_uart3_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(41), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_uart4_fracmux __initdata =
    MUX(0, "clock_uart4_mux", mux_uart4_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(44), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_uart5_fracmux __initdata =
    MUX(0, "clock_uart5_mux", mux_uart5_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(47), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_dclock_vopb_fracmux __initdata =
    MUX(0, "dclock_vopb_mux", mux_dclock_vopb_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(5), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_dclock_vopl_fracmux __initdata =
    MUX(0, "dclock_vopl_mux", mux_dclock_vopl_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(8), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_rtc32k_pmu_fracmux __initdata =
    MUX(SCLK_RTC32K_PMU, "clock_rtc32k_pmu", mux_rtc32k_pmu_p, CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(0), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_uart0_pmu_fracmux __initdata =
    MUX(0, "clock_uart0_pmu_mux", mux_uart0_pmu_p, CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(4), 14, 2, MFLAGS);

static struct rockchip_clock_branch px30_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    MUX(USB480M, "usb480m", mux_usb480m_p, CLK_SET_RATE_PARENT, PX30_MODE_CON, 8, 2, MFLAGS),
    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    /*
     * Clock-Architecture Diagram 3
     */

    /* PD_CORE */
    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "gpll_core", "gpll", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 0, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg", "armclk", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(0), 8, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, PX30_CLKGATE_CON(0), 2, GFLAGS),
    COMPOSITE_NOMUX(
        0, "aclock_core", "armclk", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(0), 12, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, PX30_CLKGATE_CON(0), 1, GFLAGS),
    GATE(0, "aclock_core_niu", "aclock_core", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 4, GFLAGS),
    GATE(0, "aclock_core_prf", "aclock_core", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(17), 5, GFLAGS),
    GATE(0, "pclock_dbg_niu", "pclock_dbg", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 5, GFLAGS),
    GATE(0, "pclock_core_dbg", "pclock_dbg", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 6, GFLAGS),
    GATE(0, "pclock_core_grf", "pclock_dbg", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(17), 6, GFLAGS),

    GATE(0, "clock_jtag", "jtag_clockin", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 3, GFLAGS),
    GATE(SCLK_PVTM, "clock_pvtm", "xin24m", 0, PX30_CLKGATE_CON(17), 4, GFLAGS),

    /* PD_GPU */
    GATE(SCLK_GPU, "clock_gpu", "clock_gpu_src", 0, PX30_CLKGATE_CON(0), 10, GFLAGS),
    COMPOSITE_NOMUX(0, "aclock_gpu", "clock_gpu", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(1), 13, 2, DFLAGS, PX30_CLKGATE_CON(17), 10, GFLAGS),
    GATE(0, "aclock_gpu_niu", "aclock_gpu", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 11, GFLAGS),
    GATE(0, "aclock_gpu_prf", "aclock_gpu", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(17), 8, GFLAGS),
    GATE(0, "pclock_gpu_grf", "aclock_gpu", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(17), 9, GFLAGS),

    /*
     * Clock-Architecture Diagram 4
     */

    /* PD_DDR */
    GATE(0, "dpll_ddr", "dpll", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 7, GFLAGS),
    GATE(0, "gpll_ddr", "gpll", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 13, GFLAGS),
    COMPOSITE_DDRCLK(SCLK_DDRCLK, "sclock_ddrc", mux_ddrphy_p, CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(2), 7, 1, 0, 3, ROCKCHIP_DDRCLK_SIP_V2),
    COMPOSITE_NOGATE(0, "clock_ddrphy4x", mux_ddrphy_p, CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(2), 7, 1, MFLAGS, 0, 3, DFLAGS),
    FACTOR_GATE(0, "clock_ddrphy1x", "clock_ddrphy4x", CLK_IGNORE_UNUSED, 1, 4, PX30_CLKGATE_CON(0), 14, GFLAGS),
    FACTOR_GATE(0, "clock_stdby_2wrap", "clock_ddrphy4x", CLK_IGNORE_UNUSED, 1, 4, PX30_CLKGATE_CON(1), 0, GFLAGS),
    COMPOSITE_NODIV(0, "clock_ddrstdby", mux_ddrstdby_p, CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(2), 4, 1, MFLAGS, PX30_CLKGATE_CON(1), 13, GFLAGS),
    GATE(0, "aclock_split", "clock_ddrphy1x", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 15, GFLAGS),
    GATE(0, "clock_msch", "clock_ddrphy1x", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 8, GFLAGS),
    GATE(0, "aclock_ddrc", "clock_ddrphy1x", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 5, GFLAGS),
    GATE(0, "clock_core_ddrc", "clock_ddrphy1x", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 6, GFLAGS),
    GATE(0, "aclock_cmd_buff", "clock_ddrphy1x", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 6, GFLAGS),
    GATE(0, "clock_ddrmon", "clock_ddrphy1x", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 11, GFLAGS),

    GATE(0, "clock_ddrmon_timer", "xin24m", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(0), 15, GFLAGS),

    COMPOSITE_NOMUX(PCLK_DDR, "pclock_ddr", "gpll", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(2), 8, 5, DFLAGS, PX30_CLKGATE_CON(1), 1, GFLAGS),
    GATE(0, "pclock_ddrmon", "pclock_ddr", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 10, GFLAGS),
    GATE(0, "pclock_ddrc", "pclock_ddr", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 7, GFLAGS),
    GATE(0, "pclock_msch", "pclock_ddr", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 9, GFLAGS),
    GATE(0, "pclock_stdby", "pclock_ddr", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 12, GFLAGS),
    GATE(0, "pclock_ddr_grf", "pclock_ddr", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 14, GFLAGS),
    GATE(0, "pclock_cmdbuff", "pclock_ddr", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(1), 3, GFLAGS),

    /*
     * Clock-Architecture Diagram 5
     */

    /* PD_VI */
    COMPOSITE(ACLK_VI_PRE, "aclock_vi_pre", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(11), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(4), 8, GFLAGS),
    COMPOSITE_NOMUX(HCLK_VI_PRE, "hclock_vi_pre", "aclock_vi_pre", 0, PX30_CLKSEL_CON(11), 8, 4, DFLAGS, PX30_CLKGATE_CON(4), 12, GFLAGS),
    COMPOSITE(SCLK_ISP, "clock_isp", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(12), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(4), 9, GFLAGS),
    COMPOSITE(SCLK_CIF_OUT, "clock_cif_out", mux_cif_out_p, 0, PX30_CLKSEL_CON(13), 6, 2, MFLAGS, 0, 6, DFLAGS, PX30_CLKGATE_CON(4), 11, GFLAGS),
    GATE(PCLK_ISP, "pclkin_isp", "ext_pclkin", 0, PX30_CLKGATE_CON(4), 13, GFLAGS),
    GATE(PCLK_CIF, "pclkin_cif", "ext_pclkin", 0, PX30_CLKGATE_CON(4), 14, GFLAGS),

    /*
     * Clock-Architecture Diagram 6
     */

    /* PD_VO */
    COMPOSITE(ACLK_VO_PRE, "aclock_vo_pre", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(3), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(2), 0, GFLAGS),
    COMPOSITE_NOMUX(HCLK_VO_PRE, "hclock_vo_pre", "aclock_vo_pre", 0, PX30_CLKSEL_CON(3), 8, 4, DFLAGS, PX30_CLKGATE_CON(2), 12, GFLAGS),
    COMPOSITE_NOMUX(PCLK_VO_PRE, "pclock_vo_pre", "aclock_vo_pre", 0, PX30_CLKSEL_CON(3), 12, 4, DFLAGS, PX30_CLKGATE_CON(2), 13, GFLAGS),
    COMPOSITE(
        SCLK_RGA_CORE, "clock_rga_core", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(4), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(2), 1, GFLAGS),

    COMPOSITE(SCLK_VOPB_PWM, "clock_vopb_pwm", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(7), 7, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(2), 5, GFLAGS),
    COMPOSITE(
        0, "dclock_vopb_src", mux_cpll_npll_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(5), 11, 1, MFLAGS, 0, 8, DFLAGS,
        PX30_CLKGATE_CON(2), 2, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "dclock_vopb_frac", "dclock_vopb_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(6), 0, PX30_CLKGATE_CON(2), 3, GFLAGS,
        &px30_dclock_vopb_fracmux, 0),
    GATE(DCLK_VOPB, "dclock_vopb", "dclock_vopb_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(2), 4, GFLAGS),
    COMPOSITE(
        0, "dclock_vopl_src", mux_npll_cpll_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(8), 11, 1, MFLAGS, 0, 8, DFLAGS,
        PX30_CLKGATE_CON(2), 6, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "dclock_vopl_frac", "dclock_vopl_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(9), 0, PX30_CLKGATE_CON(2), 7, GFLAGS,
        &px30_dclock_vopl_fracmux, 0),
    GATE(DCLK_VOPL, "dclock_vopl", "dclock_vopl_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(2), 8, GFLAGS),

    /* PD_VPU */
    COMPOSITE(0, "aclock_vpu_pre", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(10), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(4), 0, GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_vpu_pre", "aclock_vpu_pre", 0, PX30_CLKSEL_CON(10), 8, 4, DFLAGS, PX30_CLKGATE_CON(4), 2, GFLAGS),
    COMPOSITE(
        SCLK_CORE_VPU, "sclock_core_vpu", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(13), 14, 2, MFLAGS, 8, 5, DFLAGS, PX30_CLKGATE_CON(4), 1, GFLAGS),

    /*
     * Clock-Architecture Diagram 7
     */

    COMPOSITE_NODIV(ACLK_PERI_SRC, "aclock_peri_src", mux_gpll_cpll_p, 0, PX30_CLKSEL_CON(14), 15, 1, MFLAGS, PX30_CLKGATE_CON(5), 7, GFLAGS),
    COMPOSITE_NOMUX(
        ACLK_PERI_PRE, "aclock_peri_pre", "aclock_peri_src", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(14), 0, 5, DFLAGS, PX30_CLKGATE_CON(5), 8, GFLAGS),
    DIV(HCLK_PERI_PRE, "hclock_peri_pre", "aclock_peri_src", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(14), 8, 5, DFLAGS),

    /* PD_MMC_NAND */
    GATE(HCLK_MMC_NAND, "hclock_mmc_nand", "hclock_peri_pre", 0, PX30_CLKGATE_CON(6), 0, GFLAGS),
    COMPOSITE(
        SCLK_NANDC_DIV, "clock_nandc_div", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(15), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(5), 11, GFLAGS),
    COMPOSITE(
        SCLK_NANDC_DIV50, "clock_nandc_div50", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(15), 6, 2, MFLAGS, 8, 5, DFLAGS, PX30_CLKGATE_CON(5), 12,
        GFLAGS),
    COMPOSITE_NODIV(
        SCLK_NANDC, "clock_nandc", mux_nandc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(15), 15, 1, MFLAGS,
        PX30_CLKGATE_CON(5), 13, GFLAGS),

    COMPOSITE(
        SCLK_SDIO_DIV, "clock_sdio_div", mux_gpll_cpll_npll_xin24m_p, 0, PX30_CLKSEL_CON(18), 14, 2, MFLAGS, 0, 8, DFLAGS, PX30_CLKGATE_CON(6), 1,
        GFLAGS),
    COMPOSITE_DIV_OFFSET(
        SCLK_SDIO_DIV50, "clock_sdio_div50", mux_gpll_cpll_npll_xin24m_p, 0, PX30_CLKSEL_CON(18), 14, 2, MFLAGS, PX30_CLKSEL_CON(19), 0, 8, DFLAGS,
        PX30_CLKGATE_CON(6), 2, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_SDIO, "clock_sdio", mux_sdio_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(19), 15, 1, MFLAGS, PX30_CLKGATE_CON(6),
        3, GFLAGS),

    COMPOSITE(
        SCLK_EMMC_DIV, "clock_emmc_div", mux_gpll_cpll_npll_xin24m_p, 0, PX30_CLKSEL_CON(20), 14, 2, MFLAGS, 0, 8, DFLAGS, PX30_CLKGATE_CON(6), 4,
        GFLAGS),
    COMPOSITE_DIV_OFFSET(
        SCLK_EMMC_DIV50, "clock_emmc_div50", mux_gpll_cpll_npll_xin24m_p, 0, PX30_CLKSEL_CON(20), 14, 2, MFLAGS, PX30_CLKSEL_CON(21), 0, 8, DFLAGS,
        PX30_CLKGATE_CON(6), 5, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_EMMC, "clock_emmc", mux_emmc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(21), 15, 1, MFLAGS, PX30_CLKGATE_CON(6),
        6, GFLAGS),

    COMPOSITE(SCLK_SFC, "clock_sfc", mux_gpll_cpll_p, 0, PX30_CLKSEL_CON(22), 7, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(6), 7, GFLAGS),

    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "clock_sdmmc", PX30_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "clock_sdmmc", PX30_SDMMC_CON1, 1),

    MMC(SCLK_SDIO_DRV, "sdio_drv", "clock_sdio", PX30_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "clock_sdio", PX30_SDIO_CON1, 1),

    MMC(SCLK_EMMC_DRV, "emmc_drv", "clock_emmc", PX30_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "clock_emmc", PX30_EMMC_CON1, 1),

    /* PD_SDCARD */
    GATE(0, "hclock_sdmmc_pre", "hclock_peri_pre", 0, PX30_CLKGATE_CON(6), 12, GFLAGS),
    COMPOSITE(
        SCLK_SDMMC_DIV, "clock_sdmmc_div", mux_gpll_cpll_npll_xin24m_p, 0, PX30_CLKSEL_CON(16), 14, 2, MFLAGS, 0, 8, DFLAGS, PX30_CLKGATE_CON(6), 13,
        GFLAGS),
    COMPOSITE_DIV_OFFSET(
        SCLK_SDMMC_DIV50, "clock_sdmmc_div50", mux_gpll_cpll_npll_xin24m_p, 0, PX30_CLKSEL_CON(16), 14, 2, MFLAGS, PX30_CLKSEL_CON(17), 0, 8, DFLAGS,
        PX30_CLKGATE_CON(6), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_SDMMC, "clock_sdmmc", mux_sdmmc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(17), 15, 1, MFLAGS,
        PX30_CLKGATE_CON(6), 15, GFLAGS),

    /* PD_USB */
    GATE(HCLK_USB, "hclock_usb", "hclock_peri_pre", 0, PX30_CLKGATE_CON(7), 2, GFLAGS),
    GATE(SCLK_OTG_ADP, "clock_otg_adp", "clock_rtc32k_pmu", 0, PX30_CLKGATE_CON(7), 3, GFLAGS),

    /* PD_GMAC */
    COMPOSITE(
        SCLK_GMAC_SRC, "clock_gmac_src", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(22), 14, 2, MFLAGS, 8, 5, DFLAGS, PX30_CLKGATE_CON(7), 11, GFLAGS),
    MUX(SCLK_GMAC, "clock_gmac", mux_gmac_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(23), 6, 1, MFLAGS),
    GATE(SCLK_MAC_REF, "clock_mac_ref", "clock_gmac", 0, PX30_CLKGATE_CON(7), 15, GFLAGS),
    GATE(SCLK_GMAC_RX_TX, "clock_gmac_rx_tx", "clock_gmac", 0, PX30_CLKGATE_CON(7), 13, GFLAGS),
    FACTOR(0, "clock_gmac_rx_tx_div2", "clock_gmac_rx_tx", 0, 1, 2),
    FACTOR(0, "clock_gmac_rx_tx_div20", "clock_gmac_rx_tx", 0, 1, 20),
    MUX(SCLK_GMAC_RMII, "clock_gmac_rmii_sel", mux_gmac_rmii_sel_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(23), 7, 1, MFLAGS),

    GATE(0, "aclock_gmac_pre", "aclock_peri_pre", 0, PX30_CLKGATE_CON(7), 10, GFLAGS),
    COMPOSITE_NOMUX(0, "pclock_gmac_pre", "aclock_gmac_pre", 0, PX30_CLKSEL_CON(23), 0, 4, DFLAGS, PX30_CLKGATE_CON(7), 12, GFLAGS),

    COMPOSITE(
        SCLK_MAC_OUT, "clock_mac_out", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(12), 14, 2, MFLAGS, 8, 5, DFLAGS, PX30_CLKGATE_CON(8), 5, GFLAGS),

    /*
     * Clock-Architecture Diagram 8
     */

    /* PD_BUS */
    COMPOSITE_NODIV(
        ACLK_BUS_SRC, "aclock_bus_src", mux_gpll_cpll_p, CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(23), 15, 1, MFLAGS, PX30_CLKGATE_CON(8), 6, GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_BUS_PRE, "hclock_bus_pre", "aclock_bus_src", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(24), 0, 5, DFLAGS, PX30_CLKGATE_CON(8), 8, GFLAGS),
    COMPOSITE_NOMUX(
        ACLK_BUS_PRE, "aclock_bus_pre", "aclock_bus_src", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(23), 8, 5, DFLAGS, PX30_CLKGATE_CON(8), 7, GFLAGS),
    COMPOSITE_NOMUX(
        PCLK_BUS_PRE, "pclock_bus_pre", "aclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKSEL_CON(24), 8, 2, DFLAGS, PX30_CLKGATE_CON(8), 9, GFLAGS),
    GATE(0, "pclock_top_pre", "pclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(8), 10, GFLAGS),

    COMPOSITE(0, "clock_pdm_src", mux_gpll_xin24m_npll_p, 0, PX30_CLKSEL_CON(26), 8, 2, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(9), 9, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_pdm_frac", "clock_pdm_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(27), 0, PX30_CLKGATE_CON(9), 10, GFLAGS, &px30_pdm_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_PDM, "clock_pdm", "clock_pdm_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(9), 11, GFLAGS),

    COMPOSITE(0, "clock_i2s0_tx_src", mux_gpll_npll_p, 0, PX30_CLKSEL_CON(28), 8, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(9), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_tx_frac", "clock_i2s0_tx_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(29), 0, PX30_CLKGATE_CON(9), 13, GFLAGS,
        &px30_i2s0_tx_fracmux, PX30_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S0_TX, "clock_i2s0_tx", mux_i2s0_tx_rx_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(28), 12, 1, MFLAGS, PX30_CLKGATE_CON(9), 14, GFLAGS),
    COMPOSITE_NODIV(0, "clock_i2s0_tx_out_pre", mux_i2s0_tx_out_p, 0, PX30_CLKSEL_CON(28), 14, 2, MFLAGS, PX30_CLKGATE_CON(9), 15, GFLAGS),
    GATE(SCLK_I2S0_TX_OUT, "clock_i2s0_tx_out", "clock_i2s0_tx_out_pre", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 8, CLK_GATE_HIWORD_MASK),

    COMPOSITE(0, "clock_i2s0_rx_src", mux_gpll_npll_p, 0, PX30_CLKSEL_CON(58), 8, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(17), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_rx_frac", "clock_i2s0_rx_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(59), 0, PX30_CLKGATE_CON(17), 1, GFLAGS,
        &px30_i2s0_rx_fracmux, PX30_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S0_RX, "clock_i2s0_rx", mux_i2s0_rx_tx_p, CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(58), 12, 1, MFLAGS, PX30_CLKGATE_CON(17), 2, GFLAGS),
    COMPOSITE_NODIV(0, "clock_i2s0_rx_out_pre", mux_i2s0_rx_out_p, 0, PX30_CLKSEL_CON(58), 14, 2, MFLAGS, PX30_CLKGATE_CON(17), 3, GFLAGS),
    GATE(SCLK_I2S0_RX_OUT, "clock_i2s0_rx_out", "clock_i2s0_rx_out_pre", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 11, CLK_GATE_HIWORD_MASK),

    COMPOSITE(0, "clock_i2s1_src", mux_gpll_npll_p, 0, PX30_CLKSEL_CON(30), 8, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(10), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_frac", "clock_i2s1_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(31), 0, PX30_CLKGATE_CON(10), 1, GFLAGS, &px30_i2s1_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1, "clock_i2s1", "clock_i2s1_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 2, GFLAGS),
    COMPOSITE_NODIV(0, "clock_i2s1_out_pre", mux_i2s1_out_p, 0, PX30_CLKSEL_CON(30), 15, 1, MFLAGS, PX30_CLKGATE_CON(10), 3, GFLAGS),
    GATE(SCLK_I2S1_OUT, "clock_i2s1_out", "clock_i2s1_out_pre", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 9, CLK_GATE_HIWORD_MASK),

    COMPOSITE(0, "clock_i2s2_src", mux_gpll_npll_p, 0, PX30_CLKSEL_CON(32), 8, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(10), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s2_frac", "clock_i2s2_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(33), 0, PX30_CLKGATE_CON(10), 5, GFLAGS, &px30_i2s2_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_I2S2, "clock_i2s2", "clock_i2s2_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 6, GFLAGS),
    COMPOSITE_NODIV(0, "clock_i2s2_out_pre", mux_i2s2_out_p, 0, PX30_CLKSEL_CON(32), 15, 1, MFLAGS, PX30_CLKGATE_CON(10), 7, GFLAGS),
    GATE(SCLK_I2S2_OUT, "clock_i2s2_out", "clock_i2s2_out_pre", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 10, CLK_GATE_HIWORD_MASK),

    COMPOSITE(
        SCLK_UART1_SRC, "clock_uart1_src", mux_uart_src_p, CLK_SET_RATE_NO_REPARENT, PX30_CLKSEL_CON(34), 14, 2, MFLAGS, 0, 5, DFLAGS,
        PX30_CLKGATE_CON(10), 12, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart1_np5", "clock_uart1_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(35), 0, 5, DFLAGS, PX30_CLKGATE_CON(10), 13, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart1_frac", "clock_uart1_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(36), 0, PX30_CLKGATE_CON(10), 14, GFLAGS, &px30_uart1_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_UART1, "clock_uart1", "clock_uart1_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(10), 15, GFLAGS),

    COMPOSITE(
        SCLK_UART2_SRC, "clock_uart2_src", mux_uart_src_p, 0, PX30_CLKSEL_CON(37), 14, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 0, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart2_np5", "clock_uart2_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(38), 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart2_frac", "clock_uart2_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(39), 0, PX30_CLKGATE_CON(11), 2, GFLAGS, &px30_uart2_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_UART2, "clock_uart2", "clock_uart2_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(11), 3, GFLAGS),

    COMPOSITE(0, "clock_uart3_src", mux_uart_src_p, 0, PX30_CLKSEL_CON(40), 14, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 4, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart3_np5", "clock_uart3_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(41), 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 5, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart3_frac", "clock_uart3_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(42), 0, PX30_CLKGATE_CON(11), 6, GFLAGS, &px30_uart3_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_UART3, "clock_uart3", "clock_uart3_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(11), 7, GFLAGS),

    COMPOSITE(0, "clock_uart4_src", mux_uart_src_p, 0, PX30_CLKSEL_CON(43), 14, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 8, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart4_np5", "clock_uart4_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(44), 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 9, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart4_frac", "clock_uart4_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(45), 0, PX30_CLKGATE_CON(11), 10, GFLAGS, &px30_uart4_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_UART4, "clock_uart4", "clock_uart4_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(11), 11, GFLAGS),

    COMPOSITE(0, "clock_uart5_src", mux_uart_src_p, 0, PX30_CLKSEL_CON(46), 14, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 12, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart5_np5", "clock_uart5_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(47), 0, 5, DFLAGS, PX30_CLKGATE_CON(11), 13, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart5_frac", "clock_uart5_src", CLK_SET_RATE_PARENT, PX30_CLKSEL_CON(48), 0, PX30_CLKGATE_CON(11), 14, GFLAGS, &px30_uart5_fracmux,
        PX30_FRAC_MAX_PRATE),
    GATE(SCLK_UART5, "clock_uart5", "clock_uart5_mux", CLK_SET_RATE_PARENT, PX30_CLKGATE_CON(11), 15, GFLAGS),

    COMPOSITE(SCLK_I2C0, "clock_i2c0", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(49), 7, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(12), 0, GFLAGS),
    COMPOSITE(SCLK_I2C1, "clock_i2c1", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(49), 15, 1, MFLAGS, 8, 7, DFLAGS, PX30_CLKGATE_CON(12), 1, GFLAGS),
    COMPOSITE(SCLK_I2C2, "clock_i2c2", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(50), 7, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(12), 2, GFLAGS),
    COMPOSITE(SCLK_I2C3, "clock_i2c3", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(50), 15, 1, MFLAGS, 8, 7, DFLAGS, PX30_CLKGATE_CON(12), 3, GFLAGS),
    COMPOSITE(SCLK_PWM0, "clock_pwm0", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(52), 7, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(12), 5, GFLAGS),
    COMPOSITE(SCLK_PWM1, "clock_pwm1", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(52), 15, 1, MFLAGS, 8, 7, DFLAGS, PX30_CLKGATE_CON(12), 6, GFLAGS),
    COMPOSITE(SCLK_SPI0, "clock_spi0", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(53), 7, 1, MFLAGS, 0, 7, DFLAGS, PX30_CLKGATE_CON(12), 7, GFLAGS),
    COMPOSITE(SCLK_SPI1, "clock_spi1", mux_gpll_xin24m_p, 0, PX30_CLKSEL_CON(53), 15, 1, MFLAGS, 8, 7, DFLAGS, PX30_CLKGATE_CON(12), 8, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, PX30_CLKGATE_CON(13), 0, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, PX30_CLKGATE_CON(13), 1, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, PX30_CLKGATE_CON(13), 2, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, PX30_CLKGATE_CON(13), 3, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, PX30_CLKGATE_CON(13), 4, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, PX30_CLKGATE_CON(13), 5, GFLAGS),

    COMPOSITE_NOMUX(SCLK_TSADC, "clock_tsadc", "xin24m", 0, PX30_CLKSEL_CON(54), 0, 11, DFLAGS, PX30_CLKGATE_CON(12), 9, GFLAGS),
    COMPOSITE_NOMUX(SCLK_SARADC, "clock_saradc", "xin24m", 0, PX30_CLKSEL_CON(55), 0, 11, DFLAGS, PX30_CLKGATE_CON(12), 10, GFLAGS),
    COMPOSITE_NOMUX(SCLK_OTP, "clock_otp", "xin24m", 0, PX30_CLKSEL_CON(56), 0, 3, DFLAGS, PX30_CLKGATE_CON(12), 11, GFLAGS),
    COMPOSITE_NOMUX(SCLK_OTP_USR, "clock_otp_usr", "clock_otp", 0, PX30_CLKSEL_CON(56), 4, 2, DFLAGS, PX30_CLKGATE_CON(13), 6, GFLAGS),

    GATE(0, "clock_cpu_boost", "xin24m", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(12), 12, GFLAGS),

    /* PD_CRYPTO */
    GATE(0, "aclock_crypto_pre", "aclock_bus_pre", 0, PX30_CLKGATE_CON(8), 12, GFLAGS),
    GATE(0, "hclock_crypto_pre", "hclock_bus_pre", 0, PX30_CLKGATE_CON(8), 13, GFLAGS),
    COMPOSITE(SCLK_CRYPTO, "clock_crypto", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(25), 6, 2, MFLAGS, 0, 5, DFLAGS, PX30_CLKGATE_CON(8), 14, GFLAGS),
    COMPOSITE(
        SCLK_CRYPTO_APK, "clock_crypto_apk", mux_gpll_cpll_npll_p, 0, PX30_CLKSEL_CON(25), 14, 2, MFLAGS, 8, 5, DFLAGS, PX30_CLKGATE_CON(8), 15,
        GFLAGS),

    /*
     * Clock-Architecture Diagram 9
     */

    /* PD_BUS_TOP */
    GATE(0, "pclock_top_niu", "pclock_top_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(16), 0, GFLAGS),
    GATE(0, "pclock_top_cru", "pclock_top_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(16), 1, GFLAGS),
    GATE(PCLK_OTP_PHY, "pclock_otp_phy", "pclock_top_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(16), 2, GFLAGS),
    GATE(0, "pclock_ddrphy", "pclock_top_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(16), 3, GFLAGS),
    GATE(PCLK_MIPIDSIPHY, "pclock_mipidsiphy", "pclock_top_pre", 0, PX30_CLKGATE_CON(16), 4, GFLAGS),
    GATE(PCLK_MIPICSIPHY, "pclock_mipicsiphy", "pclock_top_pre", 0, PX30_CLKGATE_CON(16), 5, GFLAGS),
    GATE(PCLK_USB_GRF, "pclock_usb_grf", "pclock_top_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(16), 6, GFLAGS),
    GATE(0, "pclock_cpu_hoost", "pclock_top_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(16), 7, GFLAGS),

    /* PD_VI */
    GATE(0, "aclock_vi_niu", "aclock_vi_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(4), 15, GFLAGS),
    GATE(ACLK_CIF, "aclock_cif", "aclock_vi_pre", 0, PX30_CLKGATE_CON(5), 1, GFLAGS),
    GATE(ACLK_ISP, "aclock_isp", "aclock_vi_pre", 0, PX30_CLKGATE_CON(5), 3, GFLAGS),
    GATE(0, "hclock_vi_niu", "hclock_vi_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(5), 0, GFLAGS),
    GATE(HCLK_CIF, "hclock_cif", "hclock_vi_pre", 0, PX30_CLKGATE_CON(5), 2, GFLAGS),
    GATE(HCLK_ISP, "hclock_isp", "hclock_vi_pre", 0, PX30_CLKGATE_CON(5), 4, GFLAGS),

    /* PD_VO */
    GATE(0, "aclock_vo_niu", "aclock_vo_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(3), 0, GFLAGS),
    GATE(ACLK_VOPB, "aclock_vopb", "aclock_vo_pre", 0, PX30_CLKGATE_CON(3), 3, GFLAGS),
    GATE(ACLK_RGA, "aclock_rga", "aclock_vo_pre", 0, PX30_CLKGATE_CON(3), 7, GFLAGS),
    GATE(ACLK_VOPL, "aclock_vopl", "aclock_vo_pre", 0, PX30_CLKGATE_CON(3), 5, GFLAGS),

    GATE(0, "hclock_vo_niu", "hclock_vo_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(3), 1, GFLAGS),
    GATE(HCLK_VOPB, "hclock_vopb", "hclock_vo_pre", 0, PX30_CLKGATE_CON(3), 4, GFLAGS),
    GATE(HCLK_RGA, "hclock_rga", "hclock_vo_pre", 0, PX30_CLKGATE_CON(3), 8, GFLAGS),
    GATE(HCLK_VOPL, "hclock_vopl", "hclock_vo_pre", 0, PX30_CLKGATE_CON(3), 6, GFLAGS),

    GATE(0, "pclock_vo_niu", "pclock_vo_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(3), 2, GFLAGS),
    GATE(PCLK_MIPI_DSI, "pclock_mipi_dsi", "pclock_vo_pre", 0, PX30_CLKGATE_CON(3), 9, GFLAGS),

    /* PD_BUS */
    GATE(0, "aclock_bus_niu", "aclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(13), 8, GFLAGS),
    GATE(0, "aclock_intmem", "aclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(13), 11, GFLAGS),
    GATE(ACLK_GIC, "aclock_gic", "aclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(13), 12, GFLAGS),
    GATE(ACLK_DCF, "aclock_dcf", "aclock_bus_pre", 0, PX30_CLKGATE_CON(13), 15, GFLAGS),

    GATE(0, "hclock_bus_niu", "hclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(13), 9, GFLAGS),
    GATE(0, "hclock_rom", "hclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(13), 14, GFLAGS),
    GATE(HCLK_PDM, "hclock_pdm", "hclock_bus_pre", 0, PX30_CLKGATE_CON(14), 1, GFLAGS),
    GATE(HCLK_I2S0, "hclock_i2s0", "hclock_bus_pre", 0, PX30_CLKGATE_CON(14), 2, GFLAGS),
    GATE(HCLK_I2S1, "hclock_i2s1", "hclock_bus_pre", 0, PX30_CLKGATE_CON(14), 3, GFLAGS),
    GATE(HCLK_I2S2, "hclock_i2s2", "hclock_bus_pre", 0, PX30_CLKGATE_CON(14), 4, GFLAGS),

    GATE(0, "pclock_bus_niu", "pclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(13), 10, GFLAGS),
    GATE(PCLK_DCF, "pclock_dcf", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 0, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 5, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 6, GFLAGS),
    GATE(PCLK_UART3, "pclock_uart3", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 7, GFLAGS),
    GATE(PCLK_UART4, "pclock_uart4", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 8, GFLAGS),
    GATE(PCLK_UART5, "pclock_uart5", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 9, GFLAGS),
    GATE(PCLK_I2C0, "pclock_i2c0", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 10, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 11, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 12, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 13, GFLAGS),
    GATE(PCLK_I2C4, "pclock_i2c4", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 14, GFLAGS),
    GATE(PCLK_PWM0, "pclock_pwm0", "pclock_bus_pre", 0, PX30_CLKGATE_CON(14), 15, GFLAGS),
    GATE(PCLK_PWM1, "pclock_pwm1", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 0, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 1, GFLAGS),
    GATE(PCLK_SPI1, "pclock_spi1", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 2, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 3, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 4, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 5, GFLAGS),
    GATE(PCLK_OTP_NS, "pclock_otp_ns", "pclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(15), 6, GFLAGS),
    GATE(PCLK_WDT_NS, "pclock_wdt_ns", "pclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(15), 7, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 8, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 9, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_bus_pre", 0, PX30_CLKGATE_CON(15), 10, GFLAGS),
    GATE(0, "pclock_grf", "pclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(15), 11, GFLAGS),
    GATE(0, "pclock_sgrf", "pclock_bus_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(15), 12, GFLAGS),

    /* PD_VPU */
    GATE(0, "hclock_vpu_niu", "hclock_vpu_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(4), 7, GFLAGS),
    GATE(HCLK_VPU, "hclock_vpu", "hclock_vpu_pre", 0, PX30_CLKGATE_CON(4), 6, GFLAGS),
    GATE(0, "aclock_vpu_niu", "aclock_vpu_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(4), 5, GFLAGS),
    GATE(ACLK_VPU, "aclock_vpu", "aclock_vpu_pre", 0, PX30_CLKGATE_CON(4), 4, GFLAGS),

    /* PD_CRYPTO */
    GATE(0, "hclock_crypto_niu", "hclock_crypto_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(9), 3, GFLAGS),
    GATE(HCLK_CRYPTO, "hclock_crypto", "hclock_crypto_pre", 0, PX30_CLKGATE_CON(9), 5, GFLAGS),
    GATE(0, "aclock_crypto_niu", "aclock_crypto_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(9), 2, GFLAGS),
    GATE(ACLK_CRYPTO, "aclock_crypto", "aclock_crypto_pre", 0, PX30_CLKGATE_CON(9), 4, GFLAGS),

    /* PD_SDCARD */
    GATE(0, "hclock_sdmmc_niu", "hclock_sdmmc_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(7), 0, GFLAGS),
    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_sdmmc_pre", 0, PX30_CLKGATE_CON(7), 1, GFLAGS),

    /* PD_PERI */
    GATE(0, "aclock_peri_niu", "aclock_peri_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(5), 9, GFLAGS),

    /* PD_MMC_NAND */
    GATE(HCLK_NANDC, "hclock_nandc", "hclock_mmc_nand", 0, PX30_CLKGATE_CON(5), 15, GFLAGS),
    GATE(0, "hclock_mmc_nand_niu", "hclock_mmc_nand", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(6), 8, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_mmc_nand", 0, PX30_CLKGATE_CON(6), 9, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_mmc_nand", 0, PX30_CLKGATE_CON(6), 10, GFLAGS),
    GATE(HCLK_SFC, "hclock_sfc", "hclock_mmc_nand", 0, PX30_CLKGATE_CON(6), 11, GFLAGS),

    /* PD_USB */
    GATE(0, "hclock_usb_niu", "hclock_usb", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(7), 4, GFLAGS),
    GATE(HCLK_OTG, "hclock_otg", "hclock_usb", 0, PX30_CLKGATE_CON(7), 5, GFLAGS),
    GATE(HCLK_HOST, "hclock_host", "hclock_usb", 0, PX30_CLKGATE_CON(7), 6, GFLAGS),
    GATE(HCLK_HOST_ARB, "hclock_host_arb", "hclock_usb", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(7), 8, GFLAGS),

    /* PD_GMAC */
    GATE(0, "aclock_gmac_niu", "aclock_gmac_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(8), 0, GFLAGS),
    GATE(ACLK_GMAC, "aclock_gmac", "aclock_gmac_pre", 0, PX30_CLKGATE_CON(8), 2, GFLAGS),
    GATE(0, "pclock_gmac_niu", "pclock_gmac_pre", CLK_IGNORE_UNUSED, PX30_CLKGATE_CON(8), 1, GFLAGS),
    GATE(PCLK_GMAC, "pclock_gmac", "pclock_gmac_pre", 0, PX30_CLKGATE_CON(8), 3, GFLAGS),
};

static struct rockchip_clock_branch px30_gpu_src_clock[] __initdata = {
    COMPOSITE(
        0, "clock_gpu_src", mux_gpll_dmycpll_usb480m_dmynpll_p, 0, PX30_CLKSEL_CON(1), 6, 2, MFLAGS, 0, 4, DFLAGS, PX30_CLKGATE_CON(0), 8, GFLAGS),
};

static struct rockchip_clock_branch rk3326_gpu_src_clock[] __initdata = {
    COMPOSITE(0, "clock_gpu_src", mux_gpll_dmycpll_usb480m_npll_p, 0, PX30_CLKSEL_CON(1), 6, 2, MFLAGS, 0, 4, DFLAGS, PX30_CLKGATE_CON(0), 8, GFLAGS),
};

static struct rockchip_clock_branch px30_clock_pmu_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 2
     */

    COMPOSITE_FRACMUX(
        0, "clock_rtc32k_frac", "xin24m", CLK_IGNORE_UNUSED, PX30_PMU_CLKSEL_CON(1), 0, PX30_PMU_CLKGATE_CON(0), 13, GFLAGS, &px30_rtc32k_pmu_fracmux,
        0),

    COMPOSITE_NOMUX(XIN24M_DIV, "xin24m_div", "xin24m", CLK_IGNORE_UNUSED, PX30_PMU_CLKSEL_CON(0), 8, 5, DFLAGS, PX30_PMU_CLKGATE_CON(0), 12, GFLAGS),

    COMPOSITE_NOMUX(0, "clock_wifi_pmu_src", "gpll", 0, PX30_PMU_CLKSEL_CON(2), 8, 6, DFLAGS, PX30_PMU_CLKGATE_CON(0), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_WIFI_PMU, "clock_wifi_pmu", mux_wifi_pmu_p, CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(2), 15, 1, MFLAGS, PX30_PMU_CLKGATE_CON(0), 15,
        GFLAGS),

    COMPOSITE(0, "clock_uart0_pmu_src", mux_uart_src_p, 0, PX30_PMU_CLKSEL_CON(3), 14, 2, MFLAGS, 0, 5, DFLAGS, PX30_PMU_CLKGATE_CON(1), 0, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart0_np5", "clock_uart0_pmu_src", CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(4), 0, 5, DFLAGS, PX30_PMU_CLKGATE_CON(1), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart0_frac", "clock_uart0_pmu_src", CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(5), 0, PX30_PMU_CLKGATE_CON(1), 2, GFLAGS,
        &px30_uart0_pmu_fracmux, PX30_FRAC_MAX_PRATE),
    GATE(SCLK_UART0_PMU, "clock_uart0_pmu", "clock_uart0_pmu_mux", CLK_SET_RATE_PARENT, PX30_PMU_CLKGATE_CON(1), 3, GFLAGS),

    GATE(SCLK_PVTM_PMU, "clock_pvtm_pmu", "xin24m", 0, PX30_PMU_CLKGATE_CON(1), 4, GFLAGS),

    COMPOSITE_NOMUX(PCLK_PMU_PRE, "pclock_pmu_pre", "gpll", 0, PX30_PMU_CLKSEL_CON(0), 0, 5, DFLAGS, PX30_PMU_CLKGATE_CON(0), 0, GFLAGS),

    COMPOSITE_NOMUX(SCLK_REF24M_PMU, "clock_ref24m_pmu", "gpll", 0, PX30_PMU_CLKSEL_CON(2), 0, 6, DFLAGS, PX30_PMU_CLKGATE_CON(1), 8, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_USBPHY_REF, "clock_usbphy_ref", mux_usbphy_ref_p, CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(2), 6, 1, MFLAGS, PX30_PMU_CLKGATE_CON(1), 9,
        GFLAGS),
    COMPOSITE_NODIV(
        SCLK_MIPIDSIPHY_REF, "clock_mipidsiphy_ref", mux_mipidsiphy_ref_p, CLK_SET_RATE_PARENT, PX30_PMU_CLKSEL_CON(2), 7, 1, MFLAGS,
        PX30_PMU_CLKGATE_CON(1), 10, GFLAGS),

    /*
     * Clock-Architecture Diagram 9
     */

    /* PD_PMU */
    GATE(0, "pclock_pmu_niu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, PX30_PMU_CLKGATE_CON(0), 1, GFLAGS),
    GATE(0, "pclock_pmu_sgrf", "pclock_pmu_pre", CLK_IGNORE_UNUSED, PX30_PMU_CLKGATE_CON(0), 2, GFLAGS),
    GATE(0, "pclock_pmu_grf", "pclock_pmu_pre", CLK_IGNORE_UNUSED, PX30_PMU_CLKGATE_CON(0), 3, GFLAGS),
    GATE(0, "pclock_pmu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, PX30_PMU_CLKGATE_CON(0), 4, GFLAGS),
    GATE(0, "pclock_pmu_mem", "pclock_pmu_pre", CLK_IGNORE_UNUSED, PX30_PMU_CLKGATE_CON(0), 5, GFLAGS),
    GATE(PCLK_GPIO0_PMU, "pclock_gpio0_pmu", "pclock_pmu_pre", 0, PX30_PMU_CLKGATE_CON(0), 6, GFLAGS),
    GATE(PCLK_UART0_PMU, "pclock_uart0_pmu", "pclock_pmu_pre", 0, PX30_PMU_CLKGATE_CON(0), 7, GFLAGS),
    GATE(0, "pclock_cru_pmu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, PX30_PMU_CLKGATE_CON(0), 8, GFLAGS),
};

static const char *const px30_pmucru_critical_clocks[] __initconst = {
    "aclock_bus_pre", "pclock_bus_pre", "hclock_bus_pre", "aclock_peri_pre", "hclock_peri_pre", "aclock_gpu_niu", "pclock_top_pre",
    "pclock_pmu_pre", "hclock_usb_niu", "pll_npll",       "usb480m",         "clock_uart2",     "pclock_uart2",
};

static void __iomem *px30_cru_base;
static void __iomem *px30_pmucru_base;

void px30_dump_cru(void)
{
    if (px30_cru_base) {
        pr_warn("CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4, px30_cru_base, 0x400, false);
    }

    if (px30_pmucru_base) {
        pr_warn("PMU CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4, px30_pmucru_base, 0x90, false);
    }
}

EXPORT_SYMBOL_GPL(px30_dump_cru);

static int px30_clock_panic(struct notifier_block *this, uint64_t ev, void *ptr)
{
    px30_dump_cru();
    return NOTIFY_DONE;
}

static struct notifier_block px30_clock_panic_block = {
    .notifier_call = px30_clock_panic,
};

static void __init px30_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;
    struct clk                     *clk;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    px30_cru_base = reg_base;

    ctx           = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(reg_base);
        return;
    }

    /* aclock_dmac is controlled by sgrf_soc_con1[11]. */
    clk = clock_register_fixed_factor(NULL, "aclock_dmac", "aclock_bus_pre", 0, 1, 1);

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock aclock_dmac: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, ACLK_DMAC);
    }

    rockchip_clock_register_plls(ctx, px30_pll_clocks, ARRAY_SIZE(px30_pll_clocks), PX30_GRF_SOC_STATUS0);
    rockchip_clock_register_branches(ctx, px30_clock_branches, ARRAY_SIZE(px30_clock_branches));

    if (of_machine_is_compatible("rockchip,px30")) {
        rockchip_clock_register_branches(ctx, px30_gpu_src_clock, ARRAY_SIZE(px30_gpu_src_clock));
    } else {
        rockchip_clock_register_branches(ctx, rk3326_gpu_src_clock, ARRAY_SIZE(rk3326_gpu_src_clock));
    }

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &px30_cpuclock_data, px30_cpuclock_rates, ARRAY_SIZE(px30_cpuclock_rates));

    rockchip_register_softrst(np, 12, reg_base + PX30_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, PX30_GLB_SRST_FST, NULL);

    rockchip_clock_of_add_provider(np, ctx);

    atomic_notifier_chain_register(&panic_notifier_list, &px30_clock_panic_block);
}

CLK_OF_DECLARE(px30_cru, "rockchip,px30-cru", px30_clock_init);

static void __init px30_pmu_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru pmu region\n", __func__);
        return;
    }

    px30_pmucru_base = reg_base;

    ctx              = rockchip_clock_init(np, reg_base, CLKPMU_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip pmu clk init failed\n", __func__);
        return;
    }

    rockchip_clock_register_plls(ctx, px30_pmu_pll_clocks, ARRAY_SIZE(px30_pmu_pll_clocks), PX30_GRF_SOC_STATUS0);

    rockchip_clock_register_branches(ctx, px30_clock_pmu_branches, ARRAY_SIZE(px30_clock_pmu_branches));

    rockchip_clock_protect_critical(px30_pmucru_critical_clocks, ARRAY_SIZE(px30_pmucru_critical_clocks));

    rockchip_clock_of_add_provider(np, ctx);
}

CLK_OF_DECLARE(px30_cru_pmu, "rockchip,px30-pmucru", px30_pmu_clock_init);
