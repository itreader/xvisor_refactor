/*
 * Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 * Author: Finley Xiao <finley.xiao@rock-chips.com>
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

#include <dt-bindings/clock/rk3308-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/rockchip/cpu.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define RK3308_GRF_SOC_STATUS0      0x380
#define RK3308_VOP_FRAC_MAX_PRATE   270000000
#define RK3308B_VOP_FRAC_MAX_PRATE  800000000
#define RK3308_UART_FRAC_MAX_PRATE  800000000
#define RK3308_PDM_FRAC_MAX_PRATE   800000000
#define RK3308_SPDIF_FRAC_MAX_PRATE 800000000
#define RK3308_I2S_FRAC_MAX_PRATE   800000000

enum rk3308_plls {
    apll,
    dpll,
    vpll0,
    vpll1,
};

static struct rockchip_pll_rate_table rk3308_pll_rates[] = {
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

#define RK3308_DIV_ACLKM_MASK     0x7
#define RK3308_DIV_ACLKM_SHIFT    12
#define RK3308_DIV_PCLK_DBG_MASK  0xf
#define RK3308_DIV_PCLK_DBG_SHIFT 8

#define RK3308_CLKSEL0(_aclock_core, _pclock_dbg)                                               \
    {                                                                                           \
        .reg = RK3308_CLKSEL_CON(0),                                                            \
        .val = HIWORD_UPDATE(_aclock_core, RK3308_DIV_ACLKM_MASK, RK3308_DIV_ACLKM_SHIFT) |     \
               HIWORD_UPDATE(_pclock_dbg, RK3308_DIV_PCLK_DBG_MASK, RK3308_DIV_PCLK_DBG_SHIFT), \
    }

#define RK3308_CPUCLK_RATE(_prate, _aclock_core, _pclock_dbg) \
    {                                                         \
        .prate = _prate,                                      \
        .divs  = {                                            \
            RK3308_CLKSEL0(_aclock_core, _pclock_dbg),       \
        },                                                   \
    }

static struct rockchip_cpuclock_rate_table rk3308_cpuclock_rates[] __initdata = {
    RK3308_CPUCLK_RATE(1608000000, 1, 7), RK3308_CPUCLK_RATE(1512000000, 1, 7), RK3308_CPUCLK_RATE(1488000000, 1, 5),
    RK3308_CPUCLK_RATE(1416000000, 1, 5), RK3308_CPUCLK_RATE(1392000000, 1, 5), RK3308_CPUCLK_RATE(1296000000, 1, 5),
    RK3308_CPUCLK_RATE(1200000000, 1, 5), RK3308_CPUCLK_RATE(1104000000, 1, 5), RK3308_CPUCLK_RATE(1008000000, 1, 5),
    RK3308_CPUCLK_RATE(912000000, 1, 5),  RK3308_CPUCLK_RATE(816000000, 1, 3),  RK3308_CPUCLK_RATE(696000000, 1, 3),
    RK3308_CPUCLK_RATE(600000000, 1, 3),  RK3308_CPUCLK_RATE(408000000, 1, 1),  RK3308_CPUCLK_RATE(312000000, 1, 1),
    RK3308_CPUCLK_RATE(216000000, 1, 1),  RK3308_CPUCLK_RATE(96000000, 1, 1),
};

static const struct rockchip_cpuclock_reg_data rk3308_cpuclock_data = {
    .core_reg       = RK3308_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0xf,
    .mux_core_alt   = 1,
    .mux_core_main  = 0,
    .mux_core_shift = 6,
    .mux_core_mask  = 0x3,
    .pll_name       = "pll_apll",
};

PNAME(mux_pll_p)                                                = {"xin24m"};
PNAME(mux_usb480m_p)                                            = {"xin24m", "usb480m_phy", "clock_rtc32k"};
PNAME(mux_armclock_p)                                           = {"apll_core", "vpll0_core", "vpll1_core"};
PNAME(mux_dpll_vpll0_p)                                         = {"dpll", "vpll0"};
PNAME(mux_dpll_vpll0_xin24m_p)                                  = {"dpll", "vpll0", "xin24m"};
PNAME(mux_dpll_vpll0_vpll1_p)                                   = {"dpll", "vpll0", "vpll1"};
PNAME(mux_dpll_vpll0_vpll1_xin24m_p)                            = {"dpll", "vpll0", "vpll1", "xin24m"};
PNAME(mux_dpll_vpll0_vpll1_usb480m_xin24m_p)                    = {"dpll", "vpll0", "vpll1", "usb480m", "xin24m"};
PNAME(mux_vpll0_vpll1_p)                                        = {"vpll0", "vpll1"};
PNAME(mux_vpll0_vpll1_xin24m_p)                                 = {"vpll0", "vpll1", "xin24m"};
PNAME(mux_uart0_p)                                              = {"clock_uart0_src", "dummy", "clock_uart0_frac"};
PNAME(mux_uart1_p)                                              = {"clock_uart1_src", "dummy", "clock_uart1_frac"};
PNAME(mux_uart2_p)                                              = {"clock_uart2_src", "dummy", "clock_uart2_frac"};
PNAME(mux_uart3_p)                                              = {"clock_uart3_src", "dummy", "clock_uart3_frac"};
PNAME(mux_uart4_p)                                              = {"clock_uart4_src", "dummy", "clock_uart4_frac"};
PNAME(mux_timer_src_p)                                          = {"xin24m", "clock_rtc32k"};
PNAME(mux_dclock_vop_p)                                         = {"dclock_vop_src", "dclock_vop_frac", "xin24m"};
PNAME(mux_nandc_p)                                              = {"clock_nandc_div", "clock_nandc_div50"};
PNAME(mux_sdmmc_p)                                              = {"clock_sdmmc_div", "clock_sdmmc_div50"};
PNAME(mux_sdio_p)                                               = {"clock_sdio_div", "clock_sdio_div50"};
PNAME(mux_emmc_p)                                               = {"clock_emmc_div", "clock_emmc_div50"};
PNAME(mux_mac_p)                                                = {"clock_mac_src", "mac_clockin"};
PNAME(mux_mac_rmii_sel_p)                                       = {"clock_mac_rx_tx_div20", "clock_mac_rx_tx_div2"};
PNAME(mux_ddrstdby_p)                                           = {"clock_ddrphy1x_out", "clock_ddr_stdby_div4"};
PNAME(mux_rtc32k_p)                                             = {"xin32k", "clock_pvtm_32k", "clock_rtc32k_frac", "clock_rtc32k_div"};
PNAME(mux_usbphy_ref_p)                                         = {"xin24m", "clock_usbphy_ref_src"};
PNAME(mux_wifi_src_p)                                           = {"clock_wifi_dpll", "clock_wifi_vpll0"};
PNAME(mux_wifi_p)                                               = {"clock_wifi_osc", "clock_wifi_src"};
PNAME(mux_pdm_p)                                                = {"clock_pdm_src", "clock_pdm_frac"};
PNAME(mux_i2s0_8ch_tx_p)                                        = {"clock_i2s0_8ch_tx_src", "clock_i2s0_8ch_tx_frac", "mclock_i2s0_8ch_in"};
PNAME(mux_i2s0_8ch_tx_rx_p)                                     = {"clock_i2s0_8ch_tx_mux", "clock_i2s0_8ch_rx_mux"};
PNAME(mux_i2s0_8ch_tx_out_p)                                    = {"clock_i2s0_8ch_tx", "xin12m"};
PNAME(mux_i2s0_8ch_rx_p)                                        = {"clock_i2s0_8ch_rx_src", "clock_i2s0_8ch_rx_frac", "mclock_i2s0_8ch_in"};
PNAME(mux_i2s0_8ch_rx_tx_p)                                     = {"clock_i2s0_8ch_rx_mux", "clock_i2s0_8ch_tx_mux"};
PNAME(mux_i2s1_8ch_tx_p)                                        = {"clock_i2s1_8ch_tx_src", "clock_i2s1_8ch_tx_frac", "mclock_i2s1_8ch_in"};
PNAME(mux_i2s1_8ch_tx_rx_p)                                     = {"clock_i2s1_8ch_tx_mux", "clock_i2s1_8ch_rx_mux"};
PNAME(mux_i2s1_8ch_tx_out_p)                                    = {"clock_i2s1_8ch_tx", "xin12m"};
PNAME(mux_i2s1_8ch_rx_p)                                        = {"clock_i2s1_8ch_rx_src", "clock_i2s1_8ch_rx_frac", "mclock_i2s1_8ch_in"};
PNAME(mux_i2s1_8ch_rx_tx_p)                                     = {"clock_i2s1_8ch_rx_mux", "clock_i2s1_8ch_tx_mux"};
PNAME(mux_i2s2_8ch_tx_p)                                        = {"clock_i2s2_8ch_tx_src", "clock_i2s2_8ch_tx_frac", "mclock_i2s2_8ch_in"};
PNAME(mux_i2s2_8ch_tx_rx_p)                                     = {"clock_i2s2_8ch_tx_mux", "clock_i2s2_8ch_rx_mux"};
PNAME(mux_i2s2_8ch_tx_out_p)                                    = {"clock_i2s2_8ch_tx", "xin12m"};
PNAME(mux_i2s2_8ch_rx_p)                                        = {"clock_i2s2_8ch_rx_src", "clock_i2s2_8ch_rx_frac", "mclock_i2s2_8ch_in"};
PNAME(mux_i2s2_8ch_rx_tx_p)                                     = {"clock_i2s2_8ch_rx_mux", "clock_i2s2_8ch_tx_mux"};
PNAME(mux_i2s3_8ch_tx_p)                                        = {"clock_i2s3_8ch_tx_src", "clock_i2s3_8ch_tx_frac", "mclock_i2s3_8ch_in"};
PNAME(mux_i2s3_8ch_tx_rx_p)                                     = {"clock_i2s3_8ch_tx_mux", "clock_i2s3_8ch_rx_mux"};
PNAME(mux_i2s3_8ch_tx_out_p)                                    = {"clock_i2s3_8ch_tx", "xin12m"};
PNAME(mux_i2s3_8ch_rx_p)                                        = {"clock_i2s3_8ch_rx_src", "clock_i2s3_8ch_rx_frac", "mclock_i2s3_8ch_in"};
PNAME(mux_i2s3_8ch_rx_tx_p)                                     = {"clock_i2s3_8ch_rx_mux", "clock_i2s3_8ch_tx_mux"};
PNAME(mux_i2s0_2ch_p)                                           = {"clock_i2s0_2ch_src", "clock_i2s0_2ch_frac", "mclock_i2s0_2ch_in"};
PNAME(mux_i2s0_2ch_out_p)                                       = {"clock_i2s0_2ch", "xin12m"};
PNAME(mux_i2s1_2ch_p)                                           = {"clock_i2s1_2ch_src", "clock_i2s1_2ch_frac", "mclock_i2s1_2ch_in"};
PNAME(mux_i2s1_2ch_out_p)                                       = {"clock_i2s1_2ch", "xin12m"};
PNAME(mux_spdif_tx_src_p)                                       = {"clock_spdif_tx_div", "clock_spdif_tx_div50"};
PNAME(mux_spdif_tx_p)                                           = {"clock_spdif_tx_src", "clock_spdif_tx_frac", "mclock_i2s0_2ch_in"};
PNAME(mux_spdif_rx_src_p)                                       = {"clock_spdif_rx_div", "clock_spdif_rx_div50"};
PNAME(mux_spdif_rx_p)                                           = {"clock_spdif_rx_src", "clock_spdif_rx_frac"};

static struct rockchip_pll_clock rk3308_pll_clocks[] __initdata = {
    [apll]  = PLL(pll_rk3328, PLL_APLL, "apll", mux_pll_p, 0, RK3308_PLL_CON(0), RK3308_MODE_CON, 0, 0, 0, rk3308_pll_rates),
    [dpll]  = PLL(pll_rk3328, PLL_DPLL, "dpll", mux_pll_p, 0, RK3308_PLL_CON(8), RK3308_MODE_CON, 2, 1, 0, rk3308_pll_rates),
    [vpll0] = PLL(pll_rk3328, PLL_VPLL0, "vpll0", mux_pll_p, 0, RK3308_PLL_CON(16), RK3308_MODE_CON, 4, 2, 0, rk3308_pll_rates),
    [vpll1] = PLL(pll_rk3328, PLL_VPLL1, "vpll1", mux_pll_p, 0, RK3308_PLL_CON(24), RK3308_MODE_CON, 6, 3, 0, rk3308_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

static struct rockchip_clock_branch rk3308_uart0_fracmux __initdata =
    MUX(0, "clock_uart0_mux", mux_uart0_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(11), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_uart1_fracmux __initdata =
    MUX(0, "clock_uart1_mux", mux_uart1_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(14), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_uart2_fracmux __initdata =
    MUX(0, "clock_uart2_mux", mux_uart2_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(17), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_uart3_fracmux __initdata =
    MUX(0, "clock_uart3_mux", mux_uart3_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(20), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_uart4_fracmux __initdata =
    MUX(0, "clock_uart4_mux", mux_uart4_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(23), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_dclock_vop_fracmux __initdata =
    MUX(0, "dclock_vop_mux", mux_dclock_vop_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(8), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_rtc32k_fracmux __initdata =
    MUX(SCLK_RTC32K, "clock_rtc32k", mux_rtc32k_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(2), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_pdm_fracmux __initdata =
    MUX(0, "clock_pdm_mux", mux_pdm_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(46), 15, 1, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s0_8ch_tx_fracmux __initdata =
    MUX(SCLK_I2S0_8CH_TX_MUX, "clock_i2s0_8ch_tx_mux", mux_i2s0_8ch_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(52), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s0_8ch_rx_fracmux __initdata =
    MUX(SCLK_I2S0_8CH_RX_MUX, "clock_i2s0_8ch_rx_mux", mux_i2s0_8ch_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(54), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s1_8ch_tx_fracmux __initdata =
    MUX(SCLK_I2S1_8CH_TX_MUX, "clock_i2s1_8ch_tx_mux", mux_i2s1_8ch_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(56), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s1_8ch_rx_fracmux __initdata =
    MUX(SCLK_I2S1_8CH_RX_MUX, "clock_i2s1_8ch_rx_mux", mux_i2s1_8ch_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(58), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s2_8ch_tx_fracmux __initdata =
    MUX(SCLK_I2S2_8CH_TX_MUX, "clock_i2s2_8ch_tx_mux", mux_i2s2_8ch_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(60), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s2_8ch_rx_fracmux __initdata =
    MUX(SCLK_I2S2_8CH_RX_MUX, "clock_i2s2_8ch_rx_mux", mux_i2s2_8ch_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(62), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s3_8ch_tx_fracmux __initdata =
    MUX(SCLK_I2S3_8CH_TX_MUX, "clock_i2s3_8ch_tx_mux", mux_i2s3_8ch_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(64), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s3_8ch_rx_fracmux __initdata =
    MUX(SCLK_I2S3_8CH_RX_MUX, "clock_i2s3_8ch_rx_mux", mux_i2s3_8ch_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(66), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s0_2ch_fracmux __initdata =
    MUX(0, "clock_i2s0_2ch_mux", mux_i2s0_2ch_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(68), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_i2s1_2ch_fracmux __initdata =
    MUX(0, "clock_i2s1_2ch_mux", mux_i2s1_2ch_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(70), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_spdif_tx_fracmux __initdata =
    MUX(0, "clock_spdif_tx_mux", mux_spdif_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(48), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk3308_spdif_rx_fracmux __initdata =
    MUX(0, "clock_spdif_rx_mux", mux_spdif_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(50), 15, 1, MFLAGS);

static struct rockchip_clock_branch rk3308_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    MUX(USB480M, "usb480m", mux_usb480m_p, CLK_SET_RATE_PARENT, RK3308_MODE_CON, 8, 2, MFLAGS),
    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    /*
     * Clock-Architecture Diagram 2
     */

    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "vpll0_core", "vpll0", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "vpll1_core", "vpll1", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 0, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_core_dbg", "armclk", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(0), 8, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3308_CLKGATE_CON(0), 2,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "aclock_core", "armclk", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(0), 12, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3308_CLKGATE_CON(0), 1, GFLAGS),

    GATE(0, "clock_jtag", "jtag_clockin", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 3, GFLAGS),

    GATE(SCLK_PVTM_CORE, "clock_pvtm_core", "xin24m", 0, RK3308_CLKGATE_CON(0), 4, GFLAGS),

    /*
     * Clock-Architecture Diagram 3
     */

    COMPOSITE_NODIV(
        ACLK_BUS_SRC, "clock_bus_src", mux_dpll_vpll0_vpll1_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(5), 6, 2, MFLAGS, RK3308_CLKGATE_CON(1), 0,
        GFLAGS),
    COMPOSITE_NOMUX(PCLK_BUS, "pclock_bus", "clock_bus_src", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(6), 8, 5, DFLAGS, RK3308_CLKGATE_CON(1), 3, GFLAGS),
    GATE(PCLK_DDR, "pclock_ddr", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 15, GFLAGS),
    COMPOSITE_NOMUX(HCLK_BUS, "hclock_bus", "clock_bus_src", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(6), 0, 5, DFLAGS, RK3308_CLKGATE_CON(1), 2, GFLAGS),
    COMPOSITE_NOMUX(ACLK_BUS, "aclock_bus", "clock_bus_src", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(5), 0, 5, DFLAGS, RK3308_CLKGATE_CON(1), 1, GFLAGS),

    COMPOSITE(
        0, "clock_uart0_src", mux_dpll_vpll0_vpll1_usb480m_xin24m_p, 0, RK3308_CLKSEL_CON(10), 13, 3, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(1), 9,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart0_frac", "clock_uart0_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(12), 0, RK3308_CLKGATE_CON(1), 11, GFLAGS,
        &rk3308_uart0_fracmux, RK3308_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART0, "clock_uart0", "clock_uart0_mux", 0, RK3308_CLKGATE_CON(1), 12, GFLAGS),

    COMPOSITE(
        0, "clock_uart1_src", mux_dpll_vpll0_vpll1_usb480m_xin24m_p, 0, RK3308_CLKSEL_CON(13), 13, 3, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(1), 13,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart1_frac", "clock_uart1_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(15), 0, RK3308_CLKGATE_CON(1), 15, GFLAGS,
        &rk3308_uart1_fracmux, RK3308_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART1, "clock_uart1", "clock_uart1_mux", 0, RK3308_CLKGATE_CON(2), 0, GFLAGS),

    COMPOSITE(
        0, "clock_uart2_src", mux_dpll_vpll0_vpll1_usb480m_xin24m_p, 0, RK3308_CLKSEL_CON(16), 13, 3, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(2), 1,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart2_frac", "clock_uart2_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(18), 0, RK3308_CLKGATE_CON(2), 3, GFLAGS,
        &rk3308_uart2_fracmux, RK3308_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART2, "clock_uart2", "clock_uart2_mux", CLK_SET_RATE_PARENT, RK3308_CLKGATE_CON(2), 4, GFLAGS),

    COMPOSITE(
        0, "clock_uart3_src", mux_dpll_vpll0_vpll1_usb480m_xin24m_p, 0, RK3308_CLKSEL_CON(19), 13, 3, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(2), 5,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart3_frac", "clock_uart3_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(21), 0, RK3308_CLKGATE_CON(2), 7, GFLAGS,
        &rk3308_uart3_fracmux, RK3308_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART3, "clock_uart3", "clock_uart3_mux", 0, RK3308_CLKGATE_CON(2), 8, GFLAGS),

    COMPOSITE(
        0, "clock_uart4_src", mux_dpll_vpll0_vpll1_usb480m_xin24m_p, 0, RK3308_CLKSEL_CON(22), 13, 3, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(2), 9,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart4_frac", "clock_uart4_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(24), 0, RK3308_CLKGATE_CON(2), 11, GFLAGS,
        &rk3308_uart4_fracmux, RK3308_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART4, "clock_uart4", "clock_uart4_mux", 0, RK3308_CLKGATE_CON(2), 12, GFLAGS),

    COMPOSITE(
        SCLK_I2C0, "clock_i2c0", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(25), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(2), 13, GFLAGS),
    COMPOSITE(
        SCLK_I2C1, "clock_i2c1", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(26), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(2), 14, GFLAGS),
    COMPOSITE(
        SCLK_I2C2, "clock_i2c2", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(27), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(2), 15, GFLAGS),
    COMPOSITE(
        SCLK_I2C3, "clock_i2c3", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(28), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(3), 0, GFLAGS),

    COMPOSITE(
        SCLK_PWM0, "clock_pwm0", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(29), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(3), 1, GFLAGS),
    COMPOSITE(
        SCLK_PWM1, "clock_pwm1", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(74), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(15), 0, GFLAGS),
    COMPOSITE(
        SCLK_PWM2, "clock_pwm2", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(75), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(15), 1, GFLAGS),

    COMPOSITE(
        SCLK_SPI0, "clock_spi0", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(30), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(3), 2, GFLAGS),
    COMPOSITE(
        SCLK_SPI1, "clock_spi1", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(31), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(3), 3, GFLAGS),
    COMPOSITE(
        SCLK_SPI2, "clock_spi2", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(32), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(3), 4, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, RK3308_CLKGATE_CON(3), 10, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, RK3308_CLKGATE_CON(3), 11, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, RK3308_CLKGATE_CON(3), 12, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, RK3308_CLKGATE_CON(3), 13, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, RK3308_CLKGATE_CON(3), 14, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, RK3308_CLKGATE_CON(3), 15, GFLAGS),

    COMPOSITE_NOMUX(SCLK_TSADC, "clock_tsadc", "xin24m", 0, RK3308_CLKSEL_CON(33), 0, 11, DFLAGS, RK3308_CLKGATE_CON(3), 5, GFLAGS),
    COMPOSITE_NOMUX(SCLK_SARADC, "clock_saradc", "xin24m", 0, RK3308_CLKSEL_CON(34), 0, 11, DFLAGS, RK3308_CLKGATE_CON(3), 6, GFLAGS),

    COMPOSITE_NOMUX(SCLK_OTP, "clock_otp", "xin24m", 0, RK3308_CLKSEL_CON(35), 0, 4, DFLAGS, RK3308_CLKGATE_CON(3), 7, GFLAGS),
    COMPOSITE_NOMUX(SCLK_OTP_USR, "clock_otp_usr", "clock_otp", 0, RK3308_CLKSEL_CON(35), 4, 2, DFLAGS, RK3308_CLKGATE_CON(3), 8, GFLAGS),

    GATE(SCLK_CPU_BOOST, "clock_cpu_boost", "xin24m", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(3), 9, GFLAGS),

    COMPOSITE(
        SCLK_CRYPTO, "clock_crypto", mux_dpll_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(7), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(1), 4, GFLAGS),
    COMPOSITE(
        SCLK_CRYPTO_APK, "clock_crypto_apk", mux_dpll_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(7), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3308_CLKGATE_CON(1), 5,
        GFLAGS),

    COMPOSITE(0, "dclock_vop_src", mux_dpll_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(8), 10, 2, MFLAGS, 0, 8, DFLAGS, RK3308_CLKGATE_CON(1), 6, GFLAGS),
    GATE(DCLK_VOP, "dclock_vop", "dclock_vop_mux", 0, RK3308_CLKGATE_CON(1), 8, GFLAGS),

    /*
     * Clock-Architecture Diagram 4
     */

    COMPOSITE_NODIV(
        ACLK_PERI_SRC, "clock_peri_src", mux_dpll_vpll0_vpll1_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(36), 6, 2, MFLAGS, RK3308_CLKGATE_CON(8), 0,
        GFLAGS),
    COMPOSITE_NOMUX(
        ACLK_PERI, "aclock_peri", "clock_peri_src", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(36), 0, 5, DFLAGS, RK3308_CLKGATE_CON(8), 1, GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_PERI, "hclock_peri", "clock_peri_src", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(37), 0, 5, DFLAGS, RK3308_CLKGATE_CON(8), 2, GFLAGS),
    COMPOSITE_NOMUX(
        PCLK_PERI, "pclock_peri", "clock_peri_src", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(37), 8, 5, DFLAGS, RK3308_CLKGATE_CON(8), 3, GFLAGS),

    COMPOSITE(
        SCLK_NANDC_DIV, "clock_nandc_div", mux_dpll_vpll0_vpll1_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(38), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3308_CLKGATE_CON(8), 4, GFLAGS),
    COMPOSITE(
        SCLK_NANDC_DIV50, "clock_nandc_div50", mux_dpll_vpll0_vpll1_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(38), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3308_CLKGATE_CON(8), 4, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_NANDC, "clock_nandc", mux_nandc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3308_CLKSEL_CON(38), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(8), 5, GFLAGS),

    COMPOSITE(
        SCLK_SDMMC_DIV, "clock_sdmmc_div", mux_dpll_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(39), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3308_CLKGATE_CON(8), 6, GFLAGS),
    COMPOSITE(
        SCLK_SDMMC_DIV50, "clock_sdmmc_div50", mux_dpll_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(39), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3308_CLKGATE_CON(8), 6, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_SDMMC, "clock_sdmmc", mux_sdmmc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3308_CLKSEL_CON(39), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(8), 7, GFLAGS),
    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "clock_sdmmc", RK3308_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "clock_sdmmc", RK3308_SDMMC_CON1, 1),

    COMPOSITE(
        SCLK_SDIO_DIV, "clock_sdio_div", mux_dpll_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(40), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3308_CLKGATE_CON(8), 8, GFLAGS),
    COMPOSITE(
        SCLK_SDIO_DIV50, "clock_sdio_div50", mux_dpll_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(40), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3308_CLKGATE_CON(8), 8, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_SDIO, "clock_sdio", mux_sdio_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3308_CLKSEL_CON(40), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(8), 9, GFLAGS),
    MMC(SCLK_SDIO_DRV, "sdio_drv", "clock_sdio", RK3308_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "clock_sdio", RK3308_SDIO_CON1, 1),

    COMPOSITE(
        SCLK_EMMC_DIV, "clock_emmc_div", mux_dpll_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(41), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3308_CLKGATE_CON(8), 10, GFLAGS),
    COMPOSITE(
        SCLK_EMMC_DIV50, "clock_emmc_div50", mux_dpll_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(41), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3308_CLKGATE_CON(8), 10, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_EMMC, "clock_emmc", mux_emmc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3308_CLKSEL_CON(41), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(8), 11, GFLAGS),
    MMC(SCLK_EMMC_DRV, "emmc_drv", "clock_emmc", RK3308_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "clock_emmc", RK3308_EMMC_CON1, 1),

    COMPOSITE(
        SCLK_SFC, "clock_sfc", mux_dpll_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(42), 14, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(8), 12, GFLAGS),

    GATE(SCLK_OTG_ADP, "clock_otg_adp", "clock_rtc32k", 0, RK3308_CLKGATE_CON(8), 13, GFLAGS),

    COMPOSITE(
        SCLK_MAC_SRC, "clock_mac_src", mux_dpll_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(43), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3308_CLKGATE_CON(8), 14,
        GFLAGS),
    MUX(SCLK_MAC, "clock_mac", mux_mac_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(43), 14, 1, MFLAGS),
    GATE(SCLK_MAC_REF, "clock_mac_ref", "clock_mac", 0, RK3308_CLKGATE_CON(9), 1, GFLAGS),
    GATE(SCLK_MAC_RX_TX, "clock_mac_rx_tx", "clock_mac", 0, RK3308_CLKGATE_CON(9), 0, GFLAGS),
    FACTOR(0, "clock_mac_rx_tx_div2", "clock_mac_rx_tx", 0, 1, 2),
    FACTOR(0, "clock_mac_rx_tx_div20", "clock_mac_rx_tx", 0, 1, 20),
    MUX(SCLK_MAC_RMII, "clock_mac_rmii_sel", mux_mac_rmii_sel_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(43), 15, 1, MFLAGS),

    COMPOSITE(
        SCLK_OWIRE, "clock_owire", mux_dpll_vpll0_xin24m_p, 0, RK3308_CLKSEL_CON(44), 14, 2, MFLAGS, 8, 6, DFLAGS, RK3308_CLKGATE_CON(8), 15, GFLAGS),

    /*
     * Clock-Architecture Diagram 5
     */

    GATE(0, "clock_ddr_mon_timer", "xin24m", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 12, GFLAGS),

    GATE(0, "clock_ddr_mon", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 10, GFLAGS),
    GATE(0, "clock_ddr_upctrl", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 11, GFLAGS),
    GATE(0, "clock_ddr_msch", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 12, GFLAGS),
    GATE(0, "clock_ddr_msch_peribus", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 13, GFLAGS),

    COMPOSITE_DDRCLK(SCLK_DDRCLK, "sclock_ddrc", mux_dpll_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(1), 6, 2, 0, 0, ROCKCHIP_DDRCLK_SIP_V2),
    COMPOSITE(
        0, "clock_ddrphy4x_src", mux_dpll_vpll0_vpll1_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(1), 6, 2, MFLAGS, 0, 3, DFLAGS, RK3308_CLKGATE_CON(0),
        10, GFLAGS),
    GATE(0, "clock_ddrphy4x", "clock_ddrphy4x_src", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 11, GFLAGS),
    FACTOR_GATE(0, "clock_ddr_stdby_div4", "clock_ddrphy4x", CLK_IGNORE_UNUSED, 1, 4, RK3308_CLKGATE_CON(0), 13, GFLAGS),
    COMPOSITE_NODIV(0, "clock_ddrstdby", mux_ddrstdby_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(1), 8, 1, MFLAGS, RK3308_CLKGATE_CON(4), 14, GFLAGS),

    /*
     * Clock-Architecture Diagram 6
     */

    GATE(PCLK_PMU, "pclock_pmu", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 5, GFLAGS),
    GATE(SCLK_PMU, "clock_pmu", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(4), 6, GFLAGS),

    COMPOSITE_FRACMUX(
        0, "clock_rtc32k_frac", "xin24m", CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(3), 0, RK3308_CLKGATE_CON(4), 3, GFLAGS, &rk3308_rtc32k_fracmux, 0),
    MUX(0, "clock_rtc32k_div_src", mux_vpll0_vpll1_p, 0, RK3308_CLKSEL_CON(2), 10, 1, MFLAGS),
    COMPOSITE_NOMUX(
        0, "clock_rtc32k_div", "clock_rtc32k_div_src", CLK_IGNORE_UNUSED | CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(4), 0, 16, DFLAGS,
        RK3308_CLKGATE_CON(4), 2, GFLAGS),

    COMPOSITE(0, "clock_usbphy_ref_src", mux_dpll_vpll0_p, 0, RK3308_CLKSEL_CON(72), 6, 1, MFLAGS, 0, 6, DFLAGS, RK3308_CLKGATE_CON(4), 7, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_USBPHY_REF, "clock_usbphy_ref", mux_usbphy_ref_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(72), 7, 1, MFLAGS, RK3308_CLKGATE_CON(4), 8,
        GFLAGS),

    GATE(0, "clock_wifi_dpll", "dpll", 0, RK3308_CLKGATE_CON(15), 2, GFLAGS),
    GATE(0, "clock_wifi_vpll0", "vpll0", 0, RK3308_CLKGATE_CON(15), 3, GFLAGS),
    GATE(0, "clock_wifi_osc", "xin24m", 0, RK3308_CLKGATE_CON(15), 4, GFLAGS),
    COMPOSITE(0, "clock_wifi_src", mux_wifi_src_p, 0, RK3308_CLKSEL_CON(44), 6, 1, MFLAGS, 0, 6, DFLAGS, RK3308_CLKGATE_CON(4), 0, GFLAGS),
    COMPOSITE_NODIV(SCLK_WIFI, "clock_wifi", mux_wifi_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(44), 7, 1, MFLAGS, RK3308_CLKGATE_CON(4), 1, GFLAGS),

    GATE(SCLK_PVTM_PMU, "clock_pvtm_pmu", "xin24m", 0, RK3308_CLKGATE_CON(4), 4, GFLAGS),

    /*
     * Clock-Architecture Diagram 7
     */

    COMPOSITE_NODIV(0, "clock_audio_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(45), 6, 2, MFLAGS, RK3308_CLKGATE_CON(10), 0, GFLAGS),
    COMPOSITE_NOMUX(HCLK_AUDIO, "hclock_audio", "clock_audio_src", 0, RK3308_CLKSEL_CON(45), 0, 5, DFLAGS, RK3308_CLKGATE_CON(10), 1, GFLAGS),
    COMPOSITE_NOMUX(PCLK_AUDIO, "pclock_audio", "clock_audio_src", 0, RK3308_CLKSEL_CON(45), 8, 5, DFLAGS, RK3308_CLKGATE_CON(10), 2, GFLAGS),

    COMPOSITE(0, "clock_pdm_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(46), 8, 2, MFLAGS, 0, 7, DFLAGS, RK3308_CLKGATE_CON(10), 3, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_pdm_frac", "clock_pdm_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(47), 0, RK3308_CLKGATE_CON(10), 4, GFLAGS, &rk3308_pdm_fracmux,
        RK3308_PDM_FRAC_MAX_PRATE),
    GATE(SCLK_PDM, "clock_pdm", "clock_pdm_mux", 0, RK3308_CLKGATE_CON(10), 5, GFLAGS),

    COMPOSITE(
        SCLK_I2S0_8CH_TX_SRC, "clock_i2s0_8ch_tx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(52), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(10), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_8ch_tx_frac", "clock_i2s0_8ch_tx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(53), 0, RK3308_CLKGATE_CON(10), 13, GFLAGS,
        &rk3308_i2s0_8ch_tx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_TX, "clock_i2s0_8ch_tx", mux_i2s0_8ch_tx_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(52), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(10), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_TX_OUT, "clock_i2s0_8ch_tx_out", mux_i2s0_8ch_tx_out_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(52), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(10), 15, GFLAGS),

    COMPOSITE(
        SCLK_I2S0_8CH_RX_SRC, "clock_i2s0_8ch_rx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(54), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(11), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_8ch_rx_frac", "clock_i2s0_8ch_rx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(55), 0, RK3308_CLKGATE_CON(11), 1, GFLAGS,
        &rk3308_i2s0_8ch_rx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_RX, "clock_i2s0_8ch_rx", mux_i2s0_8ch_rx_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(54), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(11), 2, GFLAGS),
    GATE(SCLK_I2S0_8CH_RX_OUT, "clock_i2s0_8ch_rx_out", "clock_i2s0_8ch_rx", 0, RK3308_CLKGATE_CON(11), 3, GFLAGS),

    COMPOSITE(
        SCLK_I2S1_8CH_TX_SRC, "clock_i2s1_8ch_tx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(56), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(11), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_8ch_tx_frac", "clock_i2s1_8ch_tx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(57), 0, RK3308_CLKGATE_CON(11), 5, GFLAGS,
        &rk3308_i2s1_8ch_tx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S1_8CH_TX, "clock_i2s1_8ch_tx", mux_i2s1_8ch_tx_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(56), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(11), 6, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S1_8CH_TX_OUT, "clock_i2s1_8ch_tx_out", mux_i2s1_8ch_tx_out_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(56), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(11), 7, GFLAGS),

    COMPOSITE(
        SCLK_I2S1_8CH_RX_SRC, "clock_i2s1_8ch_rx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(58), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(11), 8, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_8ch_rx_frac", "clock_i2s1_8ch_rx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(59), 0, RK3308_CLKGATE_CON(11), 9, GFLAGS,
        &rk3308_i2s1_8ch_rx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S1_8CH_RX, "clock_i2s1_8ch_rx", mux_i2s1_8ch_rx_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(58), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(11), 10, GFLAGS),
    GATE(SCLK_I2S1_8CH_RX_OUT, "clock_i2s1_8ch_rx_out", "clock_i2s1_8ch_rx", 0, RK3308_CLKGATE_CON(11), 11, GFLAGS),

    COMPOSITE(
        SCLK_I2S2_8CH_TX_SRC, "clock_i2s2_8ch_tx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(60), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(11), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s2_8ch_tx_frac", "clock_i2s2_8ch_tx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(61), 0, RK3308_CLKGATE_CON(11), 13, GFLAGS,
        &rk3308_i2s2_8ch_tx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S2_8CH_TX, "clock_i2s2_8ch_tx", mux_i2s2_8ch_tx_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(60), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(11), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S2_8CH_TX_OUT, "clock_i2s2_8ch_tx_out", mux_i2s2_8ch_tx_out_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(60), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(11), 15, GFLAGS),

    COMPOSITE(
        SCLK_I2S2_8CH_RX_SRC, "clock_i2s2_8ch_rx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(62), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(12), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s2_8ch_rx_frac", "clock_i2s2_8ch_rx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(63), 0, RK3308_CLKGATE_CON(12), 1, GFLAGS,
        &rk3308_i2s2_8ch_rx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S2_8CH_RX, "clock_i2s2_8ch_rx", mux_i2s2_8ch_rx_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(62), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(12), 2, GFLAGS),
    GATE(SCLK_I2S2_8CH_RX_OUT, "clock_i2s2_8ch_rx_out", "clock_i2s2_8ch_rx", 0, RK3308_CLKGATE_CON(12), 3, GFLAGS),

    COMPOSITE(
        SCLK_I2S3_8CH_TX_SRC, "clock_i2s3_8ch_tx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(64), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(12), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s3_8ch_tx_frac", "clock_i2s3_8ch_tx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(65), 0, RK3308_CLKGATE_CON(12), 5, GFLAGS,
        &rk3308_i2s3_8ch_tx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S3_8CH_TX, "clock_i2s3_8ch_tx", mux_i2s3_8ch_tx_rx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(64), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(12), 6, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S3_8CH_TX_OUT, "clock_i2s3_8ch_tx_out", mux_i2s3_8ch_tx_out_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(64), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(12), 7, GFLAGS),

    COMPOSITE(
        SCLK_I2S3_8CH_RX_SRC, "clock_i2s3_8ch_rx_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(66), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(12), 8, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s3_8ch_rx_frac", "clock_i2s3_8ch_rx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(67), 0, RK3308_CLKGATE_CON(12), 9, GFLAGS,
        &rk3308_i2s3_8ch_rx_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S3_8CH_RX, "clock_i2s3_8ch_rx", mux_i2s3_8ch_rx_tx_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(66), 12, 1, MFLAGS,
        RK3308_CLKGATE_CON(12), 10, GFLAGS),
    GATE(SCLK_I2S3_8CH_RX_OUT, "clock_i2s3_8ch_rx_out", "clock_i2s3_8ch_rx", 0, RK3308_CLKGATE_CON(12), 11, GFLAGS),

    COMPOSITE(
        SCLK_I2S0_2CH_SRC, "clock_i2s0_2ch_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(68), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(12), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_2ch_frac", "clock_i2s0_2ch_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(69), 0, RK3308_CLKGATE_CON(12), 13, GFLAGS,
        &rk3308_i2s0_2ch_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S0_2CH, "clock_i2s0_2ch", "clock_i2s0_2ch_mux", 0, RK3308_CLKGATE_CON(12), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S0_2CH_OUT, "clock_i2s0_2ch_out", mux_i2s0_2ch_out_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(68), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(12), 15, GFLAGS),

    COMPOSITE(
        SCLK_I2S1_2CH_SRC, "clock_i2s1_2ch_src", mux_vpll0_vpll1_xin24m_p, 0, RK3308_CLKSEL_CON(70), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(13), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_2ch_frac", "clock_i2s1_2ch_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(71), 0, RK3308_CLKGATE_CON(13), 1, GFLAGS,
        &rk3308_i2s1_2ch_fracmux, RK3308_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1_2CH, "clock_i2s1_2ch", "clock_i2s1_2ch_mux", 0, RK3308_CLKGATE_CON(13), 2, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S1_2CH_OUT, "clock_i2s1_2ch_out", mux_i2s1_2ch_out_p, CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(70), 15, 1, MFLAGS,
        RK3308_CLKGATE_CON(13), 3, GFLAGS),

    COMPOSITE(
        SCLK_SPDIF_TX_DIV, "clock_spdif_tx_div", mux_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(48), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(10), 6, GFLAGS),
    COMPOSITE(
        SCLK_SPDIF_TX_DIV50, "clock_spdif_tx_div50", mux_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(48), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(10), 6, GFLAGS),
    MUX(0, "clock_spdif_tx_src", mux_spdif_tx_src_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3308_CLKSEL_CON(48), 12, 1, MFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_spdif_tx_frac", "clock_spdif_tx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(49), 0, RK3308_CLKGATE_CON(10), 7, GFLAGS,
        &rk3308_spdif_tx_fracmux, RK3308_SPDIF_FRAC_MAX_PRATE),
    GATE(SCLK_SPDIF_TX, "clock_spdif_tx", "clock_spdif_tx_mux", 0, RK3308_CLKGATE_CON(10), 8, GFLAGS),

    COMPOSITE(
        SCLK_SPDIF_RX_DIV, "clock_spdif_rx_div", mux_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(50), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(10), 9, GFLAGS),
    COMPOSITE(
        SCLK_SPDIF_RX_DIV50, "clock_spdif_rx_div50", mux_vpll0_vpll1_xin24m_p, CLK_IGNORE_UNUSED, RK3308_CLKSEL_CON(50), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK3308_CLKGATE_CON(10), 9, GFLAGS),
    MUX(0, "clock_spdif_rx_src", mux_spdif_rx_src_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3308_CLKSEL_CON(50), 14, 1, MFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_spdif_rx_frac", "clock_spdif_rx_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(51), 0, RK3308_CLKGATE_CON(10), 10, GFLAGS,
        &rk3308_spdif_rx_fracmux, RK3308_SPDIF_FRAC_MAX_PRATE),
    GATE(SCLK_SPDIF_RX, "clock_spdif_rx", "clock_spdif_rx_mux", 0, RK3308_CLKGATE_CON(10), 11, GFLAGS),

    /*
     * Clock-Architecture Diagram 8
     */

    GATE(0, "aclock_core_niu", "aclock_core", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 5, GFLAGS),
    GATE(0, "pclock_core_dbg_niu", "aclock_core", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 6, GFLAGS),
    GATE(0, "pclock_core_dbg_daplite", "pclock_core_dbg", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 7, GFLAGS),
    GATE(0, "aclock_core_perf", "pclock_core_dbg", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 8, GFLAGS),
    GATE(0, "pclock_core_grf", "pclock_core_dbg", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(0), 9, GFLAGS),

    GATE(0, "aclock_peri_niu", "aclock_peri", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(9), 2, GFLAGS),
    GATE(0, "aclock_peribus_niu", "aclock_peri", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(9), 3, GFLAGS),
    GATE(ACLK_MAC, "aclock_mac", "aclock_peri", 0, RK3308_CLKGATE_CON(9), 4, GFLAGS),

    GATE(0, "hclock_peri_niu", "hclock_peri", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(9), 5, GFLAGS),
    GATE(HCLK_NANDC, "hclock_nandc", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 6, GFLAGS),
    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 7, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 8, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 9, GFLAGS),
    GATE(HCLK_SFC, "hclock_sfc", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 10, GFLAGS),
    GATE(HCLK_OTG, "hclock_otg", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 11, GFLAGS),
    GATE(HCLK_HOST, "hclock_host", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 12, GFLAGS),
    GATE(HCLK_HOST_ARB, "hclock_host_arb", "hclock_peri", 0, RK3308_CLKGATE_CON(9), 13, GFLAGS),

    GATE(0, "pclock_peri_niu", "pclock_peri", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(9), 14, GFLAGS),
    GATE(PCLK_MAC, "pclock_mac", "pclock_peri", 0, RK3308_CLKGATE_CON(9), 15, GFLAGS),

    GATE(0, "hclock_audio_niu", "hclock_audio", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(14), 0, GFLAGS),
    GATE(HCLK_PDM, "hclock_pdm", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 1, GFLAGS),
    GATE(HCLK_SPDIFTX, "hclock_spdiftx", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 2, GFLAGS),
    GATE(HCLK_SPDIFRX, "hclock_spdifrx", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 3, GFLAGS),
    GATE(HCLK_I2S0_8CH, "hclock_i2s0_8ch", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 4, GFLAGS),
    GATE(HCLK_I2S1_8CH, "hclock_i2s1_8ch", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 5, GFLAGS),
    GATE(HCLK_I2S2_8CH, "hclock_i2s2_8ch", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 6, GFLAGS),
    GATE(HCLK_I2S3_8CH, "hclock_i2s3_8ch", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 7, GFLAGS),
    GATE(HCLK_I2S0_2CH, "hclock_i2s0_2ch", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 8, GFLAGS),
    GATE(HCLK_I2S1_2CH, "hclock_i2s1_2ch", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 9, GFLAGS),
    GATE(HCLK_VAD, "hclock_vad", "hclock_audio", 0, RK3308_CLKGATE_CON(14), 10, GFLAGS),

    GATE(0, "pclock_audio_niu", "pclock_audio", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(14), 11, GFLAGS),
    GATE(PCLK_ACODEC, "pclock_acodec", "pclock_audio", 0, RK3308_CLKGATE_CON(14), 12, GFLAGS),

    GATE(0, "aclock_bus_niu", "aclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(5), 0, GFLAGS),
    GATE(0, "aclock_intmem", "aclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(5), 1, GFLAGS),
    GATE(ACLK_CRYPTO, "aclock_crypto", "aclock_bus", 0, RK3308_CLKGATE_CON(5), 2, GFLAGS),
    GATE(ACLK_VOP, "aclock_vop", "aclock_bus", 0, RK3308_CLKGATE_CON(5), 3, GFLAGS),
    GATE(0, "aclock_gic", "aclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(5), 4, GFLAGS),

    GATE(0, "hclock_bus_niu", "hclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(5), 5, GFLAGS),
    GATE(0, "hclock_rom", "hclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(5), 6, GFLAGS),
    GATE(HCLK_CRYPTO, "hclock_crypto", "hclock_bus", 0, RK3308_CLKGATE_CON(5), 7, GFLAGS),
    GATE(HCLK_VOP, "hclock_vop", "hclock_bus", 0, RK3308_CLKGATE_CON(5), 8, GFLAGS),

    GATE(0, "pclock_bus_niu", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(5), 9, GFLAGS),
    GATE(PCLK_UART0, "pclock_uart0", "pclock_bus", 0, RK3308_CLKGATE_CON(5), 10, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_bus", 0, RK3308_CLKGATE_CON(5), 11, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_bus", 0, RK3308_CLKGATE_CON(5), 12, GFLAGS),
    GATE(PCLK_UART3, "pclock_uart3", "pclock_bus", 0, RK3308_CLKGATE_CON(5), 13, GFLAGS),
    GATE(PCLK_UART4, "pclock_uart4", "pclock_bus", 0, RK3308_CLKGATE_CON(5), 14, GFLAGS),
    GATE(PCLK_I2C0, "pclock_i2c0", "pclock_bus", 0, RK3308_CLKGATE_CON(5), 15, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 0, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 1, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 2, GFLAGS),
    GATE(PCLK_PWM0, "pclock_pwm0", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 3, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 4, GFLAGS),
    GATE(PCLK_SPI1, "pclock_spi1", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 5, GFLAGS),
    GATE(PCLK_SPI2, "pclock_spi2", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 6, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 7, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 8, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 9, GFLAGS),
    GATE(PCLK_OTP_NS, "pclock_otp_ns", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 10, GFLAGS),
    GATE(PCLK_GPIO0, "pclock_gpio0", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 12, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 13, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 14, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_bus", 0, RK3308_CLKGATE_CON(6), 15, GFLAGS),
    GATE(PCLK_GPIO4, "pclock_gpio4", "pclock_bus", 0, RK3308_CLKGATE_CON(7), 0, GFLAGS),
    GATE(PCLK_SGRF, "pclock_sgrf", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 1, GFLAGS),
    GATE(PCLK_GRF, "pclock_grf", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 2, GFLAGS),
    GATE(PCLK_USBSD_DET, "pclock_usbsd_det", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 3, GFLAGS),
    GATE(PCLK_DDR_UPCTL, "pclock_ddr_upctl", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 4, GFLAGS),
    GATE(PCLK_DDR_MON, "pclock_ddr_mon", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 5, GFLAGS),
    GATE(PCLK_DDRPHY, "pclock_ddrphy", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 6, GFLAGS),
    GATE(PCLK_DDR_STDBY, "pclock_ddr_stdby", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 7, GFLAGS),
    GATE(PCLK_USB_GRF, "pclock_usb_grf", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 8, GFLAGS),
    GATE(PCLK_CRU, "pclock_cru", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 9, GFLAGS),
    GATE(PCLK_OTP_PHY, "pclock_otp_phy", "pclock_bus", 0, RK3308_CLKGATE_CON(7), 10, GFLAGS),
    GATE(PCLK_CPU_BOOST, "pclock_cpu_boost", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 11, GFLAGS),
    GATE(PCLK_PWM1, "pclock_pwm1", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 12, GFLAGS),
    GATE(PCLK_PWM2, "pclock_pwm2", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 13, GFLAGS),
    GATE(PCLK_CAN, "pclock_can", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 14, GFLAGS),
    GATE(PCLK_OWIRE, "pclock_owire", "pclock_bus", CLK_IGNORE_UNUSED, RK3308_CLKGATE_CON(7), 15, GFLAGS),
};

static struct rockchip_clock_branch rk3308_dclock_vop_frac[] __initdata = {
    COMPOSITE_FRACMUX(
        0, "dclock_vop_frac", "dclock_vop_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(9), 0, RK3308_CLKGATE_CON(1), 7, GFLAGS,
        &rk3308_dclock_vop_fracmux, RK3308_VOP_FRAC_MAX_PRATE),
};

static struct rockchip_clock_branch rk3308b_dclock_vop_frac[] __initdata = {
    COMPOSITE_FRACMUX(
        0, "dclock_vop_frac", "dclock_vop_src", CLK_SET_RATE_PARENT, RK3308_CLKSEL_CON(9), 0, RK3308_CLKGATE_CON(1), 7, GFLAGS,
        &rk3308_dclock_vop_fracmux, RK3308B_VOP_FRAC_MAX_PRATE),
};

static const char *const rk3308_critical_clocks[] __initconst = {
    "aclock_bus", "hclock_bus", "pclock_bus", "aclock_peri", "hclock_peri", "pclock_peri", "hclock_audio", "pclock_audio", "sclock_ddrc",
};

static void __iomem *rk3308_cru_base;

void rk3308_dump_cru(void)
{
    if (rk3308_cru_base) {
        pr_warn("CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4, rk3308_cru_base, 0x500, false);
    }
}

EXPORT_SYMBOL_GPL(rk3308_dump_cru);

static int rk3308_clock_panic(struct notifier_block *this, uint64_t ev, void *ptr)
{
    rk3308_dump_cru();
    return NOTIFY_DONE;
}

static struct notifier_block rk3308_clock_panic_block = {
    .notifier_call = rk3308_clock_panic,
};

static void __init rk3308_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;
    struct clk                     *clk;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    rk3308_cru_base = reg_base;

    ctx             = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(reg_base);
        return;
    }

    /* aclock_dmac0 is controlled by sgrf. */
    clk = clock_register_fixed_factor(NULL, "aclock_dmac0", "aclock_bus", 0, 1, 1);

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock aclock_dmac0: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, ACLK_DMAC0);
    }

    /* aclock_dmac1 is controlled by sgrf. */
    clk = clock_register_fixed_factor(NULL, "aclock_dmac1", "aclock_bus", 0, 1, 1);

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock aclock_dmac1: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, ACLK_DMAC1);
    }

    /* watchdog pclk is controlled by sgrf. */
    clk = clock_register_fixed_factor(NULL, "pclock_wdt", "pclock_bus", 0, 1, 1);

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock pclock_wdt: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, PCLK_WDT);
    }

    rockchip_clock_register_plls(ctx, rk3308_pll_clocks, ARRAY_SIZE(rk3308_pll_clocks), RK3308_GRF_SOC_STATUS0);
    rockchip_clock_register_branches(ctx, rk3308_clock_branches, ARRAY_SIZE(rk3308_clock_branches));

    if (soc_is_rk3308b()) {
        rockchip_clock_register_branches(ctx, rk3308_dclock_vop_frac, ARRAY_SIZE(rk3308_dclock_vop_frac));
    } else {
        rockchip_clock_register_branches(ctx, rk3308b_dclock_vop_frac, ARRAY_SIZE(rk3308b_dclock_vop_frac));
    }

    rockchip_clock_protect_critical(rk3308_critical_clocks, ARRAY_SIZE(rk3308_critical_clocks));

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &rk3308_cpuclock_data, rk3308_cpuclock_rates,
        ARRAY_SIZE(rk3308_cpuclock_rates));

    rockchip_register_softrst(np, 10, reg_base + RK3308_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK3308_GLB_SRST_FST, NULL);

    rockchip_clock_of_add_provider(np, ctx);

    atomic_notifier_chain_register(&panic_notifier_list, &rk3308_clock_panic_block);
}

CLK_OF_DECLARE(rk3308_cru, "rockchip,rk3308-cru", rk3308_clock_init);
