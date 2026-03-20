// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd
 * Author: Elaine Zhang <zhangqing@rock-chips.com>
 */
#include <dt-bindings/clock/rk1808-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include "clk.h"

#define RK1808_GRF_SOC_STATUS0         0x480
#define RK1808_UART_FRAC_MAX_PRATE     800000000
#define RK1808_PDM_FRAC_MAX_PRATE      300000000
#define RK1808_I2S_FRAC_MAX_PRATE      600000000
#define RK1808_VOP_RAW_FRAC_MAX_PRATE  300000000
#define RK1808_VOP_LITE_FRAC_MAX_PRATE 400000000

enum rk1808_plls {
    apll,
    dpll,
    cpll,
    gpll,
    npll,
    ppll,
};

static struct rockchip_pll_rate_table rk1808_pll_rates[] = {
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
    RK3036_PLL_RATE(1188000000, 1, 99, 2, 1, 1, 0),
    RK3036_PLL_RATE(1104000000, 1, 46, 1, 1, 1, 0),
    RK3036_PLL_RATE(1100000000, 2, 275, 3, 1, 1, 0),
    RK3036_PLL_RATE(1008000000, 1, 84, 2, 1, 1, 0),
    RK3036_PLL_RATE(1000000000, 1, 250, 6, 1, 1, 0),
    RK3036_PLL_RATE(984000000, 1, 82, 2, 1, 1, 0),
    RK3036_PLL_RATE(960000000, 1, 80, 2, 1, 1, 0),
    RK3036_PLL_RATE(936000000, 1, 78, 2, 1, 1, 0),
    RK3036_PLL_RATE(912000000, 1, 76, 2, 1, 1, 0),
    RK3036_PLL_RATE(900000000, 1, 75, 2, 1, 1, 0),
    RK3036_PLL_RATE(888000000, 1, 74, 2, 1, 1, 0),
    RK3036_PLL_RATE(864000000, 1, 72, 2, 1, 1, 0),
    RK3036_PLL_RATE(840000000, 1, 70, 2, 1, 1, 0),
    RK3036_PLL_RATE(816000000, 1, 68, 2, 1, 1, 0),
    RK3036_PLL_RATE(800000000, 1, 100, 3, 1, 1, 0),
    RK3036_PLL_RATE(700000000, 1, 175, 2, 1, 1, 0),
    RK3036_PLL_RATE(696000000, 1, 58, 2, 1, 1, 0),
    RK3036_PLL_RATE(624000000, 1, 52, 2, 1, 1, 0),
    RK3036_PLL_RATE(600000000, 1, 75, 3, 1, 1, 0),
    RK3036_PLL_RATE(594000000, 1, 99, 4, 1, 1, 0),
    RK3036_PLL_RATE(504000000, 1, 63, 3, 1, 1, 0),
    RK3036_PLL_RATE(500000000, 1, 125, 6, 1, 1, 0),
    RK3036_PLL_RATE(416000000, 1, 52, 3, 1, 1, 0),
    RK3036_PLL_RATE(408000000, 1, 68, 2, 2, 1, 0),
    RK3036_PLL_RATE(312000000, 1, 52, 2, 2, 1, 0),
    RK3036_PLL_RATE(216000000, 1, 72, 4, 2, 1, 0),
    RK3036_PLL_RATE(200000000, 1, 200, 6, 4, 1, 0),
    RK3036_PLL_RATE(100000000, 1, 150, 6, 6, 1, 0),
    RK3036_PLL_RATE(96000000, 1, 64, 4, 4, 1, 0),
    {/* sentinel */},
};

#define RK1808_DIV_ACLKM_MASK     0x7
#define RK1808_DIV_ACLKM_SHIFT    12
#define RK1808_DIV_PCLK_DBG_MASK  0xf
#define RK1808_DIV_PCLK_DBG_SHIFT 8

#define RK1808_CLKSEL0(_aclock_core, _pclock_dbg)                                                                                                    \
    {                                                                                                                                                \
        .reg = RK1808_CLKSEL_CON(0),                                                                                                                 \
        .val = HIWORD_UPDATE(_aclock_core, RK1808_DIV_ACLKM_MASK, RK1808_DIV_ACLKM_SHIFT) |                                                          \
               HIWORD_UPDATE(_pclock_dbg, RK1808_DIV_PCLK_DBG_MASK, RK1808_DIV_PCLK_DBG_SHIFT),                                                      \
    }

#define RK1808_CPUCLK_RATE(_prate, _aclock_core, _pclock_dbg)                                                                                        \
    {                                                                                                                                                \
        .prate = _prate,                                                                                                                             \
        .divs  = {                                                                                                                                   \
            RK1808_CLKSEL0(_aclock_core, _pclock_dbg),                                                                                              \
        },                                                                                                                                          \
    }

static struct rockchip_cpuclock_rate_table rk1808_cpuclock_rates[] __initdata = {
    RK1808_CPUCLK_RATE(1608000000, 1, 7), RK1808_CPUCLK_RATE(1512000000, 1, 7), RK1808_CPUCLK_RATE(1488000000, 1, 5),
    RK1808_CPUCLK_RATE(1416000000, 1, 5), RK1808_CPUCLK_RATE(1392000000, 1, 5), RK1808_CPUCLK_RATE(1296000000, 1, 5),
    RK1808_CPUCLK_RATE(1200000000, 1, 5), RK1808_CPUCLK_RATE(1104000000, 1, 5), RK1808_CPUCLK_RATE(1008000000, 1, 5),
    RK1808_CPUCLK_RATE(912000000, 1, 5),  RK1808_CPUCLK_RATE(816000000, 1, 3),  RK1808_CPUCLK_RATE(696000000, 1, 3),
    RK1808_CPUCLK_RATE(600000000, 1, 3),  RK1808_CPUCLK_RATE(408000000, 1, 1),  RK1808_CPUCLK_RATE(312000000, 1, 1),
    RK1808_CPUCLK_RATE(216000000, 1, 1),  RK1808_CPUCLK_RATE(96000000, 1, 1),
};

static const struct rockchip_cpuclock_reg_data rk1808_cpuclock_data = {
    .core_reg       = RK1808_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0xf,
    .mux_core_alt   = 2,
    .mux_core_main  = 0,
    .mux_core_shift = 6,
    .mux_core_mask  = 0x3,
    .pll_name       = "pll_apll",
};

PNAME(mux_pll_p)                    = {"xin24m", "xin32k"};
PNAME(mux_usb480m_p)                = {"xin24m", "usb480m_phy", "xin32k"};
PNAME(mux_armclock_p)               = {"apll_core", "cpll_core", "gpll_core"};
PNAME(mux_gpll_cpll_p)              = {"gpll", "cpll"};
PNAME(mux_gpll_cpll_apll_p)         = {"gpll", "cpll", "apll"};
PNAME(mux_npu_p)                    = {"clock_npu_div", "clock_npu_np5"};
PNAME(mux_ddr_p)                    = {"dpll_ddr", "gpll_ddr"};
PNAME(mux_cpll_gpll_npll_p)         = {"cpll", "gpll", "npll"};
PNAME(mux_gpll_cpll_npll_p)         = {"gpll", "cpll", "npll"};
PNAME(mux_dclock_vopraw_p)          = {"dclock_vopraw_src", "dclock_vopraw_frac", "xin24m"};
PNAME(mux_dclock_voplite_p)         = {"dclock_voplite_src", "dclock_voplite_frac", "xin24m"};
PNAME(mux_24m_npll_gpll_usb480m_p)  = {"xin24m", "npll", "gpll", "usb480m"};
PNAME(mux_usb3_otg0_suspend_p)      = {"xin32k", "xin24m"};
PNAME(mux_pcie_aux_p)               = {"xin24m", "clock_pcie_src"};
PNAME(mux_gpll_cpll_npll_24m_p)     = {"gpll", "cpll", "npll", "xin24m"};
PNAME(mux_sdio_p)                   = {"clock_sdio_div", "clock_sdio_div50"};
PNAME(mux_sdmmc_p)                  = {"clock_sdmmc_div", "clock_sdmmc_div50"};
PNAME(mux_emmc_p)                   = {"clock_emmc_div", "clock_emmc_div50"};
PNAME(mux_cpll_npll_ppll_p)         = {"cpll", "npll", "ppll"};
PNAME(mux_gmac_p)                   = {"clock_gmac_src", "gmac_clockin"};
PNAME(mux_gmac_rgmii_speed_p)       = {"clock_gmac_tx_src", "clock_gmac_tx_src", "clock_gmac_tx_div50", "clock_gmac_tx_div5"};
PNAME(mux_gmac_rmii_speed_p)        = {"clock_gmac_rx_div20", "clock_gmac_rx_div2"};
PNAME(mux_gmac_rx_tx_p)             = {"clock_gmac_rgmii_speed", "clock_gmac_rmii_speed"};
PNAME(mux_gpll_usb480m_cpll_npll_p) = {"gpll", "usb480m", "cpll", "npll"};
PNAME(mux_uart1_p)                  = {"clock_uart1_src", "clock_uart1_np5", "clock_uart1_frac", "xin24m"};
PNAME(mux_uart2_p)                  = {"clock_uart2_src", "clock_uart2_np5", "clock_uart2_frac", "xin24m"};
PNAME(mux_uart3_p)                  = {"clock_uart3_src", "clock_uart3_np5", "clock_uart3_frac", "xin24m"};
PNAME(mux_uart4_p)                  = {"clock_uart4_src", "clock_uart4_np5", "clock_uart4_frac", "xin24m"};
PNAME(mux_uart5_p)                  = {"clock_uart5_src", "clock_uart5_np5", "clock_uart5_frac", "xin24m"};
PNAME(mux_uart6_p)                  = {"clock_uart6_src", "clock_uart6_np5", "clock_uart6_frac", "xin24m"};
PNAME(mux_uart7_p)                  = {"clock_uart7_src", "clock_uart7_np5", "clock_uart7_frac", "xin24m"};
PNAME(mux_gpll_xin24m_p)            = {"gpll", "xin24m"};
PNAME(mux_gpll_cpll_xin24m_p)       = {"gpll", "cpll", "xin24m"};
PNAME(mux_gpll_xin24m_cpll_npll_p)  = {"gpll", "xin24m", "cpll", "npll"};
PNAME(mux_pdm_p)                    = {"clock_pdm_src", "clock_pdm_frac"};
PNAME(mux_i2s0_8ch_tx_p)            = {"clock_i2s0_8ch_tx_src", "clock_i2s0_8ch_tx_frac", "mclock_i2s0_8ch_in", "xin12m"};
PNAME(mux_i2s0_8ch_tx_rx_p)         = {"clock_i2s0_8ch_tx_mux", "clock_i2s0_8ch_rx_mux"};
PNAME(mux_i2s0_8ch_tx_out_p)        = {"clock_i2s0_8ch_tx", "xin12m", "clock_i2s0_8ch_rx"};
PNAME(mux_i2s0_8ch_rx_p)            = {"clock_i2s0_8ch_rx_src", "clock_i2s0_8ch_rx_frac", "mclock_i2s0_8ch_in", "xin12m"};
PNAME(mux_i2s0_8ch_rx_tx_p)         = {"clock_i2s0_8ch_rx_mux", "clock_i2s0_8ch_tx_mux"};
PNAME(mux_i2s0_8ch_rx_out_p)        = {"clock_i2s0_8ch_rx", "xin12m", "clock_i2s0_8ch_tx"};
PNAME(mux_i2s1_2ch_p)               = {"clock_i2s1_2ch_src", "clock_i2s1_2ch_frac", "mclock_i2s1_2ch_in", "xin12m"};
PNAME(mux_i2s1_2ch_out_p)           = {"clock_i2s1_2ch", "xin12m"};
PNAME(mux_rtc32k_pmu_p)             = {"xin32k", "pmu_pvtm_32k", "clock_rtc32k_frac"};
PNAME(mux_wifi_pmu_p)               = {"xin24m", "clock_wifi_pmu_src"};
PNAME(mux_gpll_usb480m_cpll_ppll_p) = {"gpll", "usb480m", "cpll", "ppll"};
PNAME(mux_uart0_pmu_p)              = {"clock_uart0_pmu_src", "clock_uart0_np5", "clock_uart0_frac", "xin24m"};
PNAME(mux_usbphy_ref_p)             = {"xin24m", "clock_ref24m_pmu"};
PNAME(mux_mipidsiphy_ref_p)         = {"xin24m", "clock_ref24m_pmu"};
PNAME(mux_pciephy_ref_p)            = {"xin24m", "clock_pciephy_src"};
PNAME(mux_ppll_xin24m_p)            = {"ppll", "xin24m"};
PNAME(mux_xin24m_32k_p)             = {"xin24m", "xin32k"};

static struct rockchip_pll_clock rk1808_pll_clocks[] __initdata = {
    [apll] = PLL(pll_rk3036, PLL_APLL, "apll", mux_pll_p, 0, RK1808_PLL_CON(0), RK1808_MODE_CON, 0, 0, 0, rk1808_pll_rates),
    [dpll] = PLL(pll_rk3036, PLL_DPLL, "dpll", mux_pll_p, 0, RK1808_PLL_CON(8), RK1808_MODE_CON, 2, 1, 0, NULL),
    [cpll] = PLL(pll_rk3036, PLL_CPLL, "cpll", mux_pll_p, 0, RK1808_PLL_CON(16), RK1808_MODE_CON, 4, 2, 0, rk1808_pll_rates),
    [gpll] = PLL(pll_rk3036, PLL_GPLL, "gpll", mux_pll_p, 0, RK1808_PLL_CON(24), RK1808_MODE_CON, 6, 3, 0, rk1808_pll_rates),
    [npll] = PLL(pll_rk3036, PLL_NPLL, "npll", mux_pll_p, 0, RK1808_PLL_CON(32), RK1808_MODE_CON, 8, 5, 0, rk1808_pll_rates),
    [ppll] = PLL(pll_rk3036, PLL_PPLL, "ppll", mux_pll_p, 0, RK1808_PMU_PLL_CON(0), RK1808_PMU_MODE_CON, 0, 4, 0, rk1808_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)

static struct rockchip_clock_branch rk1808_uart1_fracmux __initdata =
    MUX(0, "clock_uart1_mux", mux_uart1_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(39), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart2_fracmux __initdata =
    MUX(0, "clock_uart2_mux", mux_uart2_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(42), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart3_fracmux __initdata =
    MUX(0, "clock_uart3_mux", mux_uart3_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(45), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart4_fracmux __initdata =
    MUX(0, "clock_uart4_mux", mux_uart4_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(48), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart5_fracmux __initdata =
    MUX(0, "clock_uart5_mux", mux_uart5_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(51), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart6_fracmux __initdata =
    MUX(0, "clock_uart6_mux", mux_uart6_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(54), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart7_fracmux __initdata =
    MUX(0, "clock_uart7_mux", mux_uart7_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(57), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_dclock_vopraw_fracmux __initdata =
    MUX(0, "dclock_vopraw_mux", mux_dclock_vopraw_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(5), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_dclock_voplite_fracmux __initdata =
    MUX(0, "dclock_voplite_mux", mux_dclock_voplite_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(7), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_pdm_fracmux __initdata =
    MUX(0, "clock_pdm_mux", mux_pdm_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(30), 15, 1, MFLAGS);

static struct rockchip_clock_branch rk1808_i2s0_8ch_tx_fracmux __initdata =
    MUX(SCLK_I2S0_8CH_TX_MUX, "clock_i2s0_8ch_tx_mux", mux_i2s0_8ch_tx_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(32), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_i2s0_8ch_rx_fracmux __initdata =
    MUX(SCLK_I2S0_8CH_RX_MUX, "clock_i2s0_8ch_rx_mux", mux_i2s0_8ch_rx_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(34), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_i2s1_2ch_fracmux __initdata =
    MUX(0, "clock_i2s1_2ch_mux", mux_i2s1_2ch_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(36), 10, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_rtc32k_pmu_fracmux __initdata =
    MUX(SCLK_RTC32K_PMU, "clock_rtc32k_pmu", mux_rtc32k_pmu_p, CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(0), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_uart0_pmu_fracmux __initdata =
    MUX(0, "clock_uart0_pmu_mux", mux_uart0_pmu_p, CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(4), 14, 2, MFLAGS);

static struct rockchip_clock_branch rk1808_clock_branches[] __initdata = {
    /*
     * Clock-Architecture Diagram 1
     */

    MUX(USB480M, "usb480m", mux_usb480m_p, CLK_SET_RATE_PARENT, RK1808_MODE_CON, 10, 2, MFLAGS),
    FACTOR(0, "xin12m", "xin24m", 0, 1, 2),

    /*
     * Clock-Architecture Diagram 2
     */

    GATE(0, "apll_core", "apll", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "cpll_core", "cpll", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "gpll_core", "gpll", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(0), 0, GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_core_dbg", "armclk", CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(0), 8, 4, DFLAGS | CLK_DIVIDER_READ_ONLY, RK1808_CLKGATE_CON(0), 3,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "aclock_core", "armclk", CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(0), 12, 3, DFLAGS | CLK_DIVIDER_READ_ONLY, RK1808_CLKGATE_CON(0), 2, GFLAGS),

    GATE(0, "clock_jtag", "jtag_clockin", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(0), 4, GFLAGS),

    GATE(SCLK_PVTM_CORE, "clock_pvtm_core", "xin24m", 0, RK1808_CLKGATE_CON(0), 5, GFLAGS),

    COMPOSITE_NOMUX(MSCLK_CORE_NIU, "msclock_core_niu", "gpll", 0, RK1808_CLKSEL_CON(18), 0, 5, DFLAGS, RK1808_CLKGATE_CON(0), 1, GFLAGS),

    /*
     * Clock-Architecture Diagram 3
     */

    COMPOSITE(
        ACLK_GIC_PRE, "aclock_gic_pre", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(15), 11, 1, MFLAGS, 12, 4, DFLAGS, RK1808_CLKGATE_CON(1), 0, GFLAGS),
    GATE(0, "aclock_gic_niu", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(1), 1, GFLAGS),
    GATE(ACLK_GIC, "aclock_gic", "aclock_gic_pre", 0, RK1808_CLKGATE_CON(1), 2, GFLAGS),
    GATE(0, "aclock_core2gic", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(1), 3, GFLAGS),
    GATE(0, "aclock_gic2core", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(1), 4, GFLAGS),
    GATE(0, "aclock_spinlock", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(1), 4, GFLAGS),

    COMPOSITE(0, "aclock_vpu_pre", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(16), 7, 1, MFLAGS, 0, 5, DFLAGS, RK1808_CLKGATE_CON(8), 8, GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_vpu_pre", "aclock_vpu_pre", 0, RK1808_CLKSEL_CON(16), 8, 4, DFLAGS, RK1808_CLKGATE_CON(8), 9, GFLAGS),
    GATE(ACLK_VPU, "aclock_vpu", "aclock_vpu_pre", 0, RK1808_CLKGATE_CON(8), 12, GFLAGS),
    GATE(0, "aclock_vpu_niu", "aclock_vpu_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(8), 10, GFLAGS),
    GATE(HCLK_VPU, "hclock_vpu", "hclock_vpu_pre", 0, RK1808_CLKGATE_CON(8), 13, GFLAGS),
    GATE(0, "hclock_vpu_niu", "hclock_vpu_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(8), 11, GFLAGS),

    /*
     * Clock-Architecture Diagram 4
     */
    COMPOSITE_NOGATE(0, "clock_npu_div", mux_gpll_cpll_p, CLK_KEEP_REQ_RATE, RK1808_CLKSEL_CON(1), 8, 2, MFLAGS, 0, 4, DFLAGS),
    COMPOSITE_NOGATE_HALFDIV(0, "clock_npu_np5", mux_gpll_cpll_p, CLK_KEEP_REQ_RATE, RK1808_CLKSEL_CON(1), 10, 2, MFLAGS, 4, 4, DFLAGS),
    MUX(0, "clock_npu_pre", mux_npu_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(1), 15, 1, MFLAGS),
    FACTOR(0, "clock_npu_scan", "clock_npu_pre", 0, 1, 2),
    GATE(SCLK_NPU, "clock_npu", "clock_npu_pre", 0, RK1808_CLKGATE_CON(1), 10, GFLAGS),

    COMPOSITE(0, "aclock_npu_pre", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(2), 14, 1, MFLAGS, 0, 4, DFLAGS, RK1808_CLKGATE_CON(1), 8, GFLAGS),
    COMPOSITE(0, "hclock_npu_pre", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(2), 15, 1, MFLAGS, 8, 4, DFLAGS, RK1808_CLKGATE_CON(1), 9, GFLAGS),
    GATE(ACLK_NPU, "aclock_npu", "aclock_npu_pre", 0, RK1808_CLKGATE_CON(1), 11, GFLAGS),
    GATE(0, "aclock_npu_niu", "aclock_npu_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(1), 13, GFLAGS),
    COMPOSITE_NOMUX(0, "aclock_npu2mem", "aclock_npu_pre", CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(2), 4, 4, DFLAGS, RK1808_CLKGATE_CON(1), 15, GFLAGS),
    GATE(HCLK_NPU, "hclock_npu", "hclock_npu_pre", 0, RK1808_CLKGATE_CON(1), 12, GFLAGS),
    GATE(0, "hclock_npu_niu", "hclock_npu_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(1), 14, GFLAGS),

    GATE(SCLK_PVTM_NPU, "clock_pvtm_npu", "xin24m", 0, RK1808_CLKGATE_CON(0), 15, GFLAGS),

    COMPOSITE(
        ACLK_IMEM_PRE, "aclock_imem_pre", mux_gpll_cpll_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(17), 7, 1, MFLAGS, 0, 5, DFLAGS,
        RK1808_CLKGATE_CON(7), 0, GFLAGS),
    GATE(ACLK_IMEM0, "aclock_imem0", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 6, GFLAGS),
    GATE(0, "aclock_imem0_niu", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 10, GFLAGS),
    GATE(ACLK_IMEM1, "aclock_imem1", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 7, GFLAGS),
    GATE(0, "aclock_imem1_niu", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 11, GFLAGS),
    GATE(ACLK_IMEM2, "aclock_imem2", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 8, GFLAGS),
    GATE(0, "aclock_imem2_niu", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 12, GFLAGS),
    GATE(ACLK_IMEM3, "aclock_imem3", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 9, GFLAGS),
    GATE(0, "aclock_imem3_niu", "aclock_imem_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(7), 13, GFLAGS),

    COMPOSITE(HSCLK_IMEM, "hsclock_imem", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(17), 15, 1, MFLAGS, 8, 5, DFLAGS, RK1808_CLKGATE_CON(7), 5, GFLAGS),

    /*
     * Clock-Architecture Diagram 5
     */
    GATE(0, "clock_ddr_mon_timer", "xin24m", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 0, GFLAGS),

    GATE(0, "clock_ddr_mon", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 11, GFLAGS),
    GATE(0, "aclock_split", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 15, GFLAGS),
    GATE(0, "clock_ddr_msch", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 8, GFLAGS),
    GATE(0, "clock_ddrdfi_ctl", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 3, GFLAGS),
    GATE(0, "clock_stdby", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 13, GFLAGS),
    GATE(0, "aclock_ddrc", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 5, GFLAGS),
    GATE(0, "clock_core_ddrc", "clock_ddrphy1x_out", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 6, GFLAGS),

    GATE(0, "dpll_ddr", "dpll", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(8), 5, GFLAGS),
    GATE(0, "gpll_ddr", "gpll", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(8), 6, GFLAGS),
    COMPOSITE_DDRCLK(SCLK_DDRCLK, "sclock_ddrc", mux_ddr_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(3), 7, 1, 0, 5, ROCKCHIP_DDRCLK_SIP_V2),
    FACTOR(0, "clock_ddrphy1x_out", "sclock_ddrc", CLK_IGNORE_UNUSED, 1, 1),

    COMPOSITE_NOMUX(PCLK_DDR, "pclock_ddr", "gpll", 0, RK1808_CLKSEL_CON(3), 8, 5, DFLAGS, RK1808_CLKGATE_CON(2), 1, GFLAGS),
    GATE(PCLK_DDRMON, "pclock_ddrmon", "pclock_ddr", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 10, GFLAGS),
    GATE(PCLK_DDRC, "pclock_ddrc", "pclock_ddr", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 7, GFLAGS),
    GATE(PCLK_MSCH, "pclock_msch", "pclock_ddr", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 9, GFLAGS),
    GATE(PCLK_STDBY, "pclock_stdby", "pclock_ddr", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 12, GFLAGS),
    GATE(0, "pclock_ddr_grf", "pclock_ddr", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 14, GFLAGS),
    GATE(0, "pclock_ddrdfi_ctl", "pclock_ddr", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(2), 2, GFLAGS),

    /*
     * Clock-Architecture Diagram 6
     */

    COMPOSITE(HSCLK_VIO, "hsclock_vio", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(4), 7, 1, MFLAGS, 0, 5, DFLAGS, RK1808_CLKGATE_CON(3), 0, GFLAGS),
    COMPOSITE_NOMUX(LSCLK_VIO, "lsclock_vio", "hsclock_vio", 0, RK1808_CLKSEL_CON(4), 8, 4, DFLAGS, RK1808_CLKGATE_CON(3), 12, GFLAGS),
    GATE(0, "hsclock_vio_niu", "hsclock_vio", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(4), 0, GFLAGS),
    GATE(0, "lsclock_vio_niu", "lsclock_vio", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(4), 1, GFLAGS),
    GATE(ACLK_VOPRAW, "aclock_vopraw", "hsclock_vio", 0, RK1808_CLKGATE_CON(4), 2, GFLAGS),
    GATE(HCLK_VOPRAW, "hclock_vopraw", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 3, GFLAGS),
    GATE(ACLK_VOPLITE, "aclock_voplite", "hsclock_vio", 0, RK1808_CLKGATE_CON(4), 4, GFLAGS),
    GATE(HCLK_VOPLITE, "hclock_voplite", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 5, GFLAGS),
    GATE(PCLK_DSI_TX, "pclock_dsi_tx", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 6, GFLAGS),
    GATE(PCLK_CSI_TX, "pclock_csi_tx", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 7, GFLAGS),
    GATE(ACLK_RGA, "aclock_rga", "hsclock_vio", 0, RK1808_CLKGATE_CON(4), 8, GFLAGS),
    GATE(HCLK_RGA, "hclock_rga", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 9, GFLAGS),
    GATE(ACLK_ISP, "aclock_isp", "hsclock_vio", 0, RK1808_CLKGATE_CON(4), 13, GFLAGS),
    GATE(HCLK_ISP, "hclock_isp", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 14, GFLAGS),
    GATE(ACLK_CIF, "aclock_cif", "hsclock_vio", 0, RK1808_CLKGATE_CON(4), 10, GFLAGS),
    GATE(HCLK_CIF, "hclock_cif", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 11, GFLAGS),
    GATE(PCLK_CSI2HOST, "pclock_csi2host", "lsclock_vio", 0, RK1808_CLKGATE_CON(4), 12, GFLAGS),

    COMPOSITE(0, "dclock_vopraw_src", mux_cpll_gpll_npll_p, 0, RK1808_CLKSEL_CON(5), 10, 2, MFLAGS, 0, 8, DFLAGS, RK1808_CLKGATE_CON(3), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "dclock_vopraw_frac", "dclock_vopraw_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(6), 0, RK1808_CLKGATE_CON(3), 2, GFLAGS,
        &rk1808_dclock_vopraw_fracmux, RK1808_VOP_RAW_FRAC_MAX_PRATE),
    GATE(DCLK_VOPRAW, "dclock_vopraw", "dclock_vopraw_mux", 0, RK1808_CLKGATE_CON(3), 3, GFLAGS),

    COMPOSITE(0, "dclock_voplite_src", mux_cpll_gpll_npll_p, 0, RK1808_CLKSEL_CON(7), 10, 2, MFLAGS, 0, 8, DFLAGS, RK1808_CLKGATE_CON(3), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "dclock_voplite_frac", "dclock_voplite_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(8), 0, RK1808_CLKGATE_CON(3), 5, GFLAGS,
        &rk1808_dclock_voplite_fracmux, RK1808_VOP_LITE_FRAC_MAX_PRATE),
    GATE(DCLK_VOPLITE, "dclock_voplite", "dclock_voplite_mux", 0, RK1808_CLKGATE_CON(3), 6, GFLAGS),

    COMPOSITE_NOMUX(SCLK_TXESC, "clock_txesc", "gpll", 0, RK1808_CLKSEL_CON(9), 0, 12, DFLAGS, RK1808_CLKGATE_CON(3), 7, GFLAGS),

    COMPOSITE(SCLK_RGA, "clock_rga", mux_gpll_cpll_npll_p, 0, RK1808_CLKSEL_CON(10), 6, 2, MFLAGS, 0, 5, DFLAGS, RK1808_CLKGATE_CON(3), 8, GFLAGS),

    COMPOSITE(SCLK_ISP, "clock_isp", mux_gpll_cpll_npll_p, 0, RK1808_CLKSEL_CON(10), 14, 2, MFLAGS, 8, 5, DFLAGS, RK1808_CLKGATE_CON(3), 10, GFLAGS),

    COMPOSITE(DCLK_CIF, "dclock_cif", mux_cpll_gpll_npll_p, 0, RK1808_CLKSEL_CON(11), 14, 2, MFLAGS, 8, 5, DFLAGS, RK1808_CLKGATE_CON(3), 11, GFLAGS),

    COMPOSITE(
        SCLK_CIF_OUT, "clock_cif_out", mux_24m_npll_gpll_usb480m_p, 0, RK1808_CLKSEL_CON(11), 6, 2, MFLAGS, 0, 6, DFLAGS, RK1808_CLKGATE_CON(3), 9,
        GFLAGS),

    /*
     * Clock-Architecture Diagram 7
     */

    /* PD_PCIE */
    COMPOSITE_NODIV(0, "clock_pcie_src", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(12), 15, 1, MFLAGS, RK1808_CLKGATE_CON(5), 0, GFLAGS),
    DIV(HSCLK_PCIE, "hsclock_pcie", "clock_pcie_src", 0, RK1808_CLKSEL_CON(12), 0, 5, DFLAGS),
    DIV(LSCLK_PCIE, "lsclock_pcie", "clock_pcie_src", 0, RK1808_CLKSEL_CON(12), 8, 5, DFLAGS),
    GATE(0, "hsclock_pcie_niu", "hsclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 0, GFLAGS),
    GATE(0, "lsclock_pcie_niu", "lsclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 1, GFLAGS),
    GATE(0, "pclock_pcie_grf", "lsclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 5, GFLAGS),
    GATE(ACLK_USB3OTG, "aclock_usb3otg", "hsclock_pcie", 0, RK1808_CLKGATE_CON(6), 6, GFLAGS),
    GATE(HCLK_HOST, "hclock_host", "lsclock_pcie", 0, RK1808_CLKGATE_CON(6), 7, GFLAGS),
    GATE(HCLK_HOST_ARB, "hclock_host_arb", "lsclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 8, GFLAGS),

    COMPOSITE(ACLK_PCIE, "aclock_pcie", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(15), 8, 1, MFLAGS, 0, 4, DFLAGS, RK1808_CLKGATE_CON(5), 5, GFLAGS),
    DIV(0, "pclock_pcie_pre", "aclock_pcie", 0, RK1808_CLKSEL_CON(15), 4, 4, DFLAGS),
    GATE(0, "aclock_pcie_niu", "aclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 10, GFLAGS),
    GATE(ACLK_PCIE_MST, "aclock_pcie_mst", "aclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 2, GFLAGS),
    GATE(ACLK_PCIE_SLV, "aclock_pcie_slv", "aclock_pcie", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 3, GFLAGS),
    GATE(0, "pclock_pcie_niu", "pclock_pcie_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 11, GFLAGS),
    GATE(0, "pclock_pcie_dbi", "pclock_pcie_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(6), 4, GFLAGS),
    GATE(PCLK_PCIE, "pclock_pcie", "pclock_pcie_pre", 0, RK1808_CLKGATE_CON(6), 9, GFLAGS),

    COMPOSITE(0, "clock_pcie_aux_src", mux_cpll_gpll_npll_p, 0, RK1808_CLKSEL_CON(14), 8, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(5), 3, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_PCIE_AUX, "clock_pcie_aux", mux_pcie_aux_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(14), 12, 1, MFLAGS, RK1808_CLKGATE_CON(5), 4, GFLAGS),

    GATE(SCLK_USB3_OTG0_REF, "clock_usb3_otg0_ref", "xin24m", 0, RK1808_CLKGATE_CON(5), 1, GFLAGS),

    COMPOSITE(
        SCLK_USB3_OTG0_SUSPEND, "clock_usb3_otg0_suspend", mux_usb3_otg0_suspend_p, 0, RK1808_CLKSEL_CON(13), 12, 1, MFLAGS, 0, 10, DFLAGS,
        RK1808_CLKGATE_CON(5), 2, GFLAGS),

    /*
     * Clock-Architecture Diagram 8
     */

    /* PD_PHP */

    COMPOSITE_NODIV(0, "clock_peri_src", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(19), 15, 1, MFLAGS, RK1808_CLKGATE_CON(8), 0, GFLAGS),
    COMPOSITE_NOMUX(MSCLK_PERI, "msclock_peri", "clock_peri_src", 0, RK1808_CLKSEL_CON(19), 0, 5, DFLAGS, RK1808_CLKGATE_CON(8), 1, GFLAGS),
    COMPOSITE_NOMUX(LSCLK_PERI, "lsclock_peri", "clock_peri_src", 0, RK1808_CLKSEL_CON(19), 8, 5, DFLAGS, RK1808_CLKGATE_CON(8), 2, GFLAGS),
    GATE(0, "msclock_peri_niu", "msclock_peri", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(8), 3, GFLAGS),
    GATE(0, "lsclock_peri_niu", "lsclock_peri", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(8), 4, GFLAGS),

    /* PD_MMC */

    GATE(0, "hclock_mmc_sfc", "msclock_peri", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(9), 0, GFLAGS),
    GATE(0, "hclock_mmc_sfc_niu", "hclock_mmc_sfc", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(9), 11, GFLAGS),
    GATE(HCLK_EMMC, "hclock_emmc", "hclock_mmc_sfc", 0, RK1808_CLKGATE_CON(9), 12, GFLAGS),
    GATE(HCLK_SFC, "hclock_sfc", "hclock_mmc_sfc", 0, RK1808_CLKGATE_CON(9), 13, GFLAGS),

    COMPOSITE(
        SCLK_SDIO_DIV, "clock_sdio_div", mux_gpll_cpll_npll_24m_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(22), 14, 2, MFLAGS, 0, 8, DFLAGS,
        RK1808_CLKGATE_CON(9), 1, GFLAGS),
    COMPOSITE_DIV_OFFSET(
        SCLK_SDIO_DIV50, "clock_sdio_div50", mux_gpll_cpll_npll_24m_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(22), 14, 2, MFLAGS, RK1808_CLKSEL_CON(23),
        0, 8, DFLAGS, RK1808_CLKGATE_CON(9), 2, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_SDIO, "clock_sdio", mux_sdio_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK1808_CLKSEL_CON(23), 15, 1, MFLAGS,
        RK1808_CLKGATE_CON(9), 3, GFLAGS),

    MMC(SCLK_SDIO_DRV, "sdio_drv", "clock_sdio", RK1808_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "clock_sdio", RK1808_SDIO_CON1, 1),

    COMPOSITE(
        SCLK_EMMC_DIV, "clock_emmc_div", mux_gpll_cpll_npll_24m_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(24), 14, 2, MFLAGS, 0, 8, DFLAGS,
        RK1808_CLKGATE_CON(9), 4, GFLAGS),
    COMPOSITE_DIV_OFFSET(
        SCLK_EMMC_DIV50, "clock_emmc_div50", mux_gpll_cpll_npll_24m_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(24), 14, 2, MFLAGS, RK1808_CLKSEL_CON(25),
        0, 8, DFLAGS, RK1808_CLKGATE_CON(9), 5, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_EMMC, "clock_emmc", mux_emmc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK1808_CLKSEL_CON(25), 15, 1, MFLAGS,
        RK1808_CLKGATE_CON(9), 6, GFLAGS),
    MMC(SCLK_EMMC_DRV, "emmc_drv", "clock_emmc", RK1808_EMMC_CON0, 1),
    MMC(SCLK_EMMC_SAMPLE, "emmc_sample", "clock_emmc", RK1808_EMMC_CON1, 1),

    COMPOSITE(
        SCLK_SDMMC_DIV, "clock_sdmmc_div", mux_gpll_cpll_npll_24m_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(20), 14, 2, MFLAGS, 0, 8, DFLAGS,
        RK1808_CLKGATE_CON(9), 7, GFLAGS),
    COMPOSITE_DIV_OFFSET(
        SCLK_SDMMC_DIV50, "clock_sdmmc_div50", mux_gpll_cpll_npll_24m_p, CLK_IGNORE_UNUSED, RK1808_CLKSEL_CON(20), 14, 2, MFLAGS,
        RK1808_CLKSEL_CON(21), 0, 8, DFLAGS, RK1808_CLKGATE_CON(9), 8, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_SDMMC, "clock_sdmmc", mux_sdmmc_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK1808_CLKSEL_CON(21), 15, 1, MFLAGS,
        RK1808_CLKGATE_CON(9), 10, GFLAGS),
    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "clock_sdmmc", RK1808_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "clock_sdmmc", RK1808_SDMMC_CON1, 1),

    COMPOSITE(SCLK_SFC, "clock_sfc", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(26), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(9), 10, GFLAGS),

    /* PD_MAC */

    GATE(0, "pclock_sd_gmac", "lsclock_peri", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(10), 2, GFLAGS),
    GATE(0, "aclock_sd_gmac", "msclock_peri", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(10), 0, GFLAGS),
    GATE(0, "hclock_sd_gmac", "msclock_peri", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(10), 1, GFLAGS),
    GATE(0, "pclock_gmac_niu", "pclock_sd_gmac", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(10), 10, GFLAGS),
    GATE(PCLK_GMAC, "pclock_gmac", "pclock_sd_gmac", 0, RK1808_CLKGATE_CON(10), 12, GFLAGS),
    GATE(0, "aclock_gmac_niu", "aclock_sd_gmac", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(10), 8, GFLAGS),
    GATE(ACLK_GMAC, "aclock_gmac", "aclock_sd_gmac", 0, RK1808_CLKGATE_CON(10), 11, GFLAGS),
    GATE(0, "hclock_gmac_niu", "hclock_sd_gmac", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(10), 9, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_sd_gmac", 0, RK1808_CLKGATE_CON(10), 13, GFLAGS),
    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_sd_gmac", 0, RK1808_CLKGATE_CON(10), 14, GFLAGS),

    COMPOSITE(
        SCLK_GMAC_OUT, "clock_gmac_out", mux_cpll_npll_ppll_p, 0, RK1808_CLKSEL_CON(18), 14, 2, MFLAGS, 8, 5, DFLAGS, RK1808_CLKGATE_CON(10), 15,
        GFLAGS),

    COMPOSITE(
        SCLK_GMAC_SRC, "clock_gmac_src", mux_cpll_npll_ppll_p, 0, RK1808_CLKSEL_CON(26), 14, 2, MFLAGS, 8, 5, DFLAGS, RK1808_CLKGATE_CON(10), 3,
        GFLAGS),
    MUX(SCLK_GMAC, "clock_gmac", mux_gmac_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK1808_CLKSEL_CON(27), 0, 1, MFLAGS),
    GATE(SCLK_GMAC_REF, "clock_gmac_ref", "clock_gmac", 0, RK1808_CLKGATE_CON(10), 4, GFLAGS),
    GATE(0, "clock_gmac_tx_src", "clock_gmac", 0, RK1808_CLKGATE_CON(10), 7, GFLAGS),
    GATE(0, "clock_gmac_rx_src", "clock_gmac", 0, RK1808_CLKGATE_CON(10), 6, GFLAGS),
    GATE(SCLK_GMAC_REFOUT, "clock_gmac_refout", "clock_gmac", 0, RK1808_CLKGATE_CON(10), 5, GFLAGS),
    FACTOR(0, "clock_gmac_tx_div5", "clock_gmac_tx_src", 0, 1, 5),
    FACTOR(0, "clock_gmac_tx_div50", "clock_gmac_tx_src", 0, 1, 50),
    FACTOR(0, "clock_gmac_rx_div2", "clock_gmac_rx_src", 0, 1, 2),
    FACTOR(0, "clock_gmac_rx_div20", "clock_gmac_rx_src", 0, 1, 20),
    MUX(SCLK_GMAC_RGMII_SPEED, "clock_gmac_rgmii_speed", mux_gmac_rgmii_speed_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(27), 2, 2, MFLAGS),
    MUX(SCLK_GMAC_RMII_SPEED, "clock_gmac_rmii_speed", mux_gmac_rmii_speed_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(27), 1, 1, MFLAGS),
    MUX(SCLK_GMAC_RX_TX, "clock_gmac_rx_tx", mux_gmac_rx_tx_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(27), 4, 1, MFLAGS),

    /*
     * Clock-Architecture Diagram 9
     */

    /* PD_BUS */

    COMPOSITE_NODIV(0, "clock_bus_src", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(27), 15, 1, MFLAGS, RK1808_CLKGATE_CON(11), 0, GFLAGS),
    COMPOSITE_NOMUX(HSCLK_BUS_PRE, "hsclock_bus_pre", "clock_bus_src", 0, RK1808_CLKSEL_CON(27), 8, 5, DFLAGS, RK1808_CLKGATE_CON(11), 1, GFLAGS),
    COMPOSITE_NOMUX(MSCLK_BUS_PRE, "msclock_bus_pre", "clock_bus_src", 0, RK1808_CLKSEL_CON(28), 0, 5, DFLAGS, RK1808_CLKGATE_CON(11), 2, GFLAGS),
    COMPOSITE_NOMUX(LSCLK_BUS_PRE, "lsclock_bus_pre", "clock_bus_src", 0, RK1808_CLKSEL_CON(28), 8, 5, DFLAGS, RK1808_CLKGATE_CON(11), 3, GFLAGS),
    GATE(0, "hsclock_bus_niu", "hsclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(15), 0, GFLAGS),
    GATE(0, "msclock_bus_niu", "msclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(15), 1, GFLAGS),
    GATE(0, "msclock_sub", "msclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(15), 2, GFLAGS),
    GATE(ACLK_DMAC, "aclock_dmac", "msclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(14), 15, GFLAGS),
    GATE(HCLK_ROM, "hclock_rom", "msclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(15), 4, GFLAGS),
    GATE(ACLK_CRYPTO, "aclock_crypto", "msclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 5, GFLAGS),
    GATE(HCLK_CRYPTO, "hclock_crypto", "msclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 6, GFLAGS),
    GATE(ACLK_DCF, "aclock_dcf", "msclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 7, GFLAGS),
    GATE(0, "lsclock_bus_niu", "lsclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(15), 3, GFLAGS),
    GATE(PCLK_DCF, "pclock_dcf", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 8, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 9, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 10, GFLAGS),
    GATE(PCLK_UART3, "pclock_uart3", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 11, GFLAGS),
    GATE(PCLK_UART4, "pclock_uart4", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 12, GFLAGS),
    GATE(PCLK_UART5, "pclock_uart5", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 13, GFLAGS),
    GATE(PCLK_UART6, "pclock_uart6", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 14, GFLAGS),
    GATE(PCLK_UART7, "pclock_uart7", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(15), 15, GFLAGS),
    GATE(PCLK_I2C1, "pclock_i2c1", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 0, GFLAGS),
    GATE(PCLK_I2C2, "pclock_i2c2", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 1, GFLAGS),
    GATE(PCLK_I2C3, "pclock_i2c3", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 2, GFLAGS),
    GATE(PCLK_I2C4, "pclock_i2c4", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(17), 4, GFLAGS),
    GATE(PCLK_I2C5, "pclock_i2c5", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(17), 5, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 3, GFLAGS),
    GATE(PCLK_SPI1, "pclock_spi1", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 4, GFLAGS),
    GATE(PCLK_SPI2, "pclock_spi2", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 5, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 9, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 10, GFLAGS),
    GATE(PCLK_EFUSE, "pclock_efuse", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 11, GFLAGS),
    GATE(PCLK_GPIO1, "pclock_gpio1", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 12, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 13, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 14, GFLAGS),
    GATE(PCLK_GPIO4, "pclock_gpio4", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 15, GFLAGS),
    GATE(PCLK_PWM0, "pclock_pwm0", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 6, GFLAGS),
    GATE(PCLK_PWM1, "pclock_pwm1", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 7, GFLAGS),
    GATE(PCLK_PWM2, "pclock_pwm2", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(16), 8, GFLAGS),
    GATE(PCLK_TIMER, "pclock_timer", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(17), 0, GFLAGS),
    GATE(PCLK_WDT, "pclock_wdt", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(17), 1, GFLAGS),
    GATE(0, "pclock_grf", "lsclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(17), 2, GFLAGS),
    GATE(0, "pclock_sgrf", "lsclock_bus_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(17), 3, GFLAGS),
    GATE(0, "hclock_audio_pre", "msclock_bus_pre", 0, RK1808_CLKGATE_CON(17), 8, GFLAGS),
    GATE(0, "pclock_top_pre", "lsclock_bus_pre", 0, RK1808_CLKGATE_CON(11), 4, GFLAGS),

    COMPOSITE(SCLK_CRYPTO, "clock_crypto", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(29), 7, 1, MFLAGS, 0, 5, DFLAGS, RK1808_CLKGATE_CON(11), 5, GFLAGS),
    COMPOSITE(
        SCLK_CRYPTO_APK, "clock_crypto_apk", mux_gpll_cpll_p, 0, RK1808_CLKSEL_CON(29), 15, 1, MFLAGS, 8, 5, DFLAGS, RK1808_CLKGATE_CON(11), 6,
        GFLAGS),

    COMPOSITE(
        0, "clock_uart1_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(38), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(11), 8, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart1_np5", "clock_uart1_src", 0, RK1808_CLKSEL_CON(39), 0, 7, DFLAGS, RK1808_CLKGATE_CON(11), 9, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart1_frac", "clock_uart1_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(40), 0, RK1808_CLKGATE_CON(11), 10, GFLAGS,
        &rk1808_uart1_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART1, "clock_uart1", "clock_uart1_mux", 0, RK1808_CLKGATE_CON(11), 11, GFLAGS),

    COMPOSITE(
        0, "clock_uart2_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(41), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(11), 12,
        GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart2_np5", "clock_uart2_src", 0, RK1808_CLKSEL_CON(42), 0, 7, DFLAGS, RK1808_CLKGATE_CON(11), 13, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart2_frac", "clock_uart2_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(43), 0, RK1808_CLKGATE_CON(11), 14, GFLAGS,
        &rk1808_uart2_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART2, "clock_uart2", "clock_uart2_mux", 0, RK1808_CLKGATE_CON(11), 15, GFLAGS),

    COMPOSITE(
        0, "clock_uart3_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(44), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 0, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart3_np5", "clock_uart3_src", 0, RK1808_CLKSEL_CON(45), 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart3_frac", "clock_uart3_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(46), 0, RK1808_CLKGATE_CON(12), 2, GFLAGS,
        &rk1808_uart3_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART3, "clock_uart3", "clock_uart3_mux", 0, RK1808_CLKGATE_CON(12), 3, GFLAGS),

    COMPOSITE(
        0, "clock_uart4_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(47), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 4, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart4_np5", "clock_uart4_src", 0, RK1808_CLKSEL_CON(48), 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 5, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart4_frac", "clock_uart4_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(49), 0, RK1808_CLKGATE_CON(12), 6, GFLAGS,
        &rk1808_uart4_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART4, "clock_uart4", "clock_uart4_mux", 0, RK1808_CLKGATE_CON(12), 7, GFLAGS),

    COMPOSITE(
        0, "clock_uart5_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(50), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 8, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart5_np5", "clock_uart5_src", 0, RK1808_CLKSEL_CON(51), 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 9, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart5_frac", "clock_uart5_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(52), 0, RK1808_CLKGATE_CON(12), 10, GFLAGS,
        &rk1808_uart5_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART5, "clock_uart5", "clock_uart5_mux", 0, RK1808_CLKGATE_CON(12), 11, GFLAGS),

    COMPOSITE(
        0, "clock_uart6_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(53), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 12,
        GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart6_np5", "clock_uart6_src", 0, RK1808_CLKSEL_CON(54), 0, 7, DFLAGS, RK1808_CLKGATE_CON(12), 13, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart6_frac", "clock_uart6_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(55), 0, RK1808_CLKGATE_CON(12), 14, GFLAGS,
        &rk1808_uart6_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART6, "clock_uart6", "clock_uart6_mux", 0, RK1808_CLKGATE_CON(12), 15, GFLAGS),

    COMPOSITE(
        0, "clock_uart7_src", mux_gpll_usb480m_cpll_npll_p, 0, RK1808_CLKSEL_CON(56), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 0, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(0, "clock_uart7_np5", "clock_uart7_src", 0, RK1808_CLKSEL_CON(57), 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart7_frac", "clock_uart7_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(58), 0, RK1808_CLKGATE_CON(13), 2, GFLAGS,
        &rk1808_uart7_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART7, "clock_uart7", "clock_uart7_mux", 0, RK1808_CLKGATE_CON(13), 3, GFLAGS),

    COMPOSITE(SCLK_I2C1, "clock_i2c1", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(59), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 4, GFLAGS),
    COMPOSITE(SCLK_I2C2, "clock_i2c2", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(59), 15, 1, MFLAGS, 8, 7, DFLAGS, RK1808_CLKGATE_CON(13), 5, GFLAGS),
    COMPOSITE(SCLK_I2C3, "clock_i2c3", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(60), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 6, GFLAGS),
    COMPOSITE(SCLK_I2C4, "clock_i2c4", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(71), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(14), 6, GFLAGS),
    COMPOSITE(SCLK_I2C5, "clock_i2c5", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(71), 15, 1, MFLAGS, 8, 7, DFLAGS, RK1808_CLKGATE_CON(14), 7, GFLAGS),

    COMPOSITE(SCLK_SPI0, "clock_spi0", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(60), 15, 1, MFLAGS, 8, 7, DFLAGS, RK1808_CLKGATE_CON(13), 7, GFLAGS),
    COMPOSITE(SCLK_SPI1, "clock_spi1", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(61), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 8, GFLAGS),
    COMPOSITE(SCLK_SPI2, "clock_spi2", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(61), 15, 1, MFLAGS, 8, 7, DFLAGS, RK1808_CLKGATE_CON(13), 9, GFLAGS),

    COMPOSITE_NOMUX(SCLK_TSADC, "clock_tsadc", "xin24m", 0, RK1808_CLKSEL_CON(62), 0, 11, DFLAGS, RK1808_CLKGATE_CON(13), 13, GFLAGS),
    COMPOSITE_NOMUX(SCLK_SARADC, "clock_saradc", "xin24m", 0, RK1808_CLKSEL_CON(63), 0, 11, DFLAGS, RK1808_CLKGATE_CON(13), 14, GFLAGS),

    COMPOSITE(
        SCLK_EFUSE_S, "clock_efuse_s", mux_gpll_cpll_xin24m_p, 0, RK1808_CLKSEL_CON(64), 6, 2, MFLAGS, 0, 6, DFLAGS, RK1808_CLKGATE_CON(14), 0,
        GFLAGS),
    COMPOSITE(
        SCLK_EFUSE_NS, "clock_efuse_ns", mux_gpll_cpll_xin24m_p, 0, RK1808_CLKSEL_CON(64), 14, 2, MFLAGS, 8, 6, DFLAGS, RK1808_CLKGATE_CON(14), 1,
        GFLAGS),

    COMPOSITE(
        DBCLK_GPIO1, "dbclock_gpio1", mux_xin24m_32k_p, 0, RK1808_CLKSEL_CON(65), 15, 1, MFLAGS, 0, 11, DFLAGS, RK1808_CLKGATE_CON(14), 2, GFLAGS),
    COMPOSITE(
        DBCLK_GPIO2, "dbclock_gpio2", mux_xin24m_32k_p, 0, RK1808_CLKSEL_CON(66), 15, 1, MFLAGS, 0, 11, DFLAGS, RK1808_CLKGATE_CON(14), 3, GFLAGS),
    COMPOSITE(
        DBCLK_GPIO3, "dbclock_gpio3", mux_xin24m_32k_p, 0, RK1808_CLKSEL_CON(67), 15, 1, MFLAGS, 0, 11, DFLAGS, RK1808_CLKGATE_CON(14), 4, GFLAGS),
    COMPOSITE(
        DBCLK_GPIO4, "dbclock_gpio4", mux_xin24m_32k_p, 0, RK1808_CLKSEL_CON(68), 15, 1, MFLAGS, 0, 11, DFLAGS, RK1808_CLKGATE_CON(14), 5, GFLAGS),

    COMPOSITE(SCLK_PWM0, "clock_pwm0", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(69), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 10, GFLAGS),
    COMPOSITE(SCLK_PWM1, "clock_pwm1", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(69), 15, 1, MFLAGS, 8, 7, DFLAGS, RK1808_CLKGATE_CON(13), 11, GFLAGS),
    COMPOSITE(SCLK_PWM2, "clock_pwm2", mux_gpll_xin24m_p, 0, RK1808_CLKSEL_CON(70), 7, 1, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(13), 12, GFLAGS),

    GATE(SCLK_TIMER0, "sclock_timer0", "xin24m", 0, RK1808_CLKGATE_CON(14), 8, GFLAGS),
    GATE(SCLK_TIMER1, "sclock_timer1", "xin24m", 0, RK1808_CLKGATE_CON(14), 9, GFLAGS),
    GATE(SCLK_TIMER2, "sclock_timer2", "xin24m", 0, RK1808_CLKGATE_CON(14), 10, GFLAGS),
    GATE(SCLK_TIMER3, "sclock_timer3", "xin24m", 0, RK1808_CLKGATE_CON(14), 11, GFLAGS),
    GATE(SCLK_TIMER4, "sclock_timer4", "xin24m", 0, RK1808_CLKGATE_CON(14), 12, GFLAGS),
    GATE(SCLK_TIMER5, "sclock_timer5", "xin24m", 0, RK1808_CLKGATE_CON(14), 13, GFLAGS),

    /*
     * Clock-Architecture Diagram 10
     */

    /* PD_AUDIO */

    GATE(0, "hclock_audio_niu", "hclock_audio_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(18), 11, GFLAGS),
    GATE(HCLK_VAD, "hclock_vad", "hclock_audio_pre", 0, RK1808_CLKGATE_CON(18), 12, GFLAGS),
    GATE(HCLK_PDM, "hclock_pdm", "hclock_audio_pre", 0, RK1808_CLKGATE_CON(18), 13, GFLAGS),
    GATE(HCLK_I2S0_8CH, "hclock_i2s0_8ch", "hclock_audio_pre", 0, RK1808_CLKGATE_CON(18), 14, GFLAGS),
    GATE(HCLK_I2S1_2CH, "hclock_i2s1_2ch", "hclock_audio_pre", 0, RK1808_CLKGATE_CON(18), 15, GFLAGS),

    COMPOSITE(
        0, "clock_pdm_src", mux_gpll_xin24m_cpll_npll_p, 0, RK1808_CLKSEL_CON(30), 8, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(17), 9, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_pdm_frac", "clock_pdm_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(31), 0, RK1808_CLKGATE_CON(17), 10, GFLAGS, &rk1808_pdm_fracmux,
        RK1808_PDM_FRAC_MAX_PRATE),
    GATE(SCLK_PDM, "clock_pdm", "clock_pdm_mux", 0, RK1808_CLKGATE_CON(17), 11, GFLAGS),

    COMPOSITE(
        SCLK_I2S0_8CH_TX_SRC, "clock_i2s0_8ch_tx_src", mux_gpll_cpll_npll_p, 0, RK1808_CLKSEL_CON(32), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK1808_CLKGATE_CON(17), 12, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_8ch_tx_frac", "clock_i2s0_8ch_tx_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(33), 0, RK1808_CLKGATE_CON(17), 13, GFLAGS,
        &rk1808_i2s0_8ch_tx_fracmux, RK1808_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_TX, "clock_i2s0_8ch_tx", mux_i2s0_8ch_tx_rx_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(32), 12, 1, MFLAGS,
        RK1808_CLKGATE_CON(17), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_TX_OUT, "clock_i2s0_8ch_tx_out", mux_i2s0_8ch_tx_out_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(32), 14, 2, MFLAGS,
        RK1808_CLKGATE_CON(17), 15, GFLAGS),

    COMPOSITE(
        SCLK_I2S0_8CH_RX_SRC, "clock_i2s0_8ch_rx_src", mux_gpll_cpll_npll_p, 0, RK1808_CLKSEL_CON(34), 8, 2, MFLAGS, 0, 7, DFLAGS,
        RK1808_CLKGATE_CON(18), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_8ch_rx_frac", "clock_i2s0_8ch_rx_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(35), 0, RK1808_CLKGATE_CON(18), 1, GFLAGS,
        &rk1808_i2s0_8ch_rx_fracmux, RK1808_I2S_FRAC_MAX_PRATE),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_RX, "clock_i2s0_8ch_rx", mux_i2s0_8ch_rx_tx_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(34), 12, 1, MFLAGS,
        RK1808_CLKGATE_CON(18), 2, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S0_8CH_RX_OUT, "clock_i2s0_8ch_rx_out", mux_i2s0_8ch_rx_out_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(34), 14, 2, MFLAGS,
        RK1808_CLKGATE_CON(18), 3, GFLAGS),

    COMPOSITE(
        SCLK_I2S1_2CH_SRC, "clock_i2s1_2ch_src", mux_gpll_cpll_npll_p, 0, RK1808_CLKSEL_CON(36), 8, 2, MFLAGS, 0, 7, DFLAGS, RK1808_CLKGATE_CON(18),
        4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_2ch_frac", "clock_i2s1_2ch_src", CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(37), 0, RK1808_CLKGATE_CON(18), 5, GFLAGS,
        &rk1808_i2s1_2ch_fracmux, RK1808_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1_2CH, "clock_i2s1_2ch", "clock_i2s1_2ch_mux", 0, RK1808_CLKGATE_CON(18), 6, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S1_2CH_OUT, "clock_i2s1_2ch_out", mux_i2s1_2ch_out_p, CLK_SET_RATE_PARENT, RK1808_CLKSEL_CON(36), 15, 1, MFLAGS,
        RK1808_CLKGATE_CON(18), 7, GFLAGS),

    /*
     * Clock-Architecture Diagram 10
     */

    /* PD_BUS */

    GATE(0, "pclock_top_niu", "pclock_top_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(19), 0, GFLAGS),
    GATE(0, "pclock_top_cru", "pclock_top_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(19), 1, GFLAGS),
    GATE(0, "pclock_ddrphy", "pclock_top_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(19), 2, GFLAGS),
    GATE(PCLK_MIPIDSIPHY, "pclock_mipidsiphy", "pclock_top_pre", 0, RK1808_CLKGATE_CON(19), 3, GFLAGS),
    GATE(PCLK_MIPICSIPHY, "pclock_mipicsiphy", "pclock_top_pre", 0, RK1808_CLKGATE_CON(19), 4, GFLAGS),

    GATE(PCLK_USB3PHY_PIPE, "pclock_usb3phy_pipe", "pclock_top_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(19), 6, GFLAGS),
    GATE(0, "pclock_usb3_grf", "pclock_top_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(19), 7, GFLAGS),
    GATE(0, "pclock_usb_grf", "pclock_top_pre", CLK_IGNORE_UNUSED, RK1808_CLKGATE_CON(19), 8, GFLAGS),

    /*
     * Clock-Architecture Diagram 11
     */

    /* PD_PMU */

    COMPOSITE_FRACMUX(
        SCLK_RTC32K_FRAC, "clock_rtc32k_frac", "xin24m", CLK_IGNORE_UNUSED, RK1808_PMU_CLKSEL_CON(1), 0, RK1808_PMU_CLKGATE_CON(0), 13, GFLAGS,
        &rk1808_rtc32k_pmu_fracmux, 0),

    COMPOSITE_NOMUX(
        XIN24M_DIV, "xin24m_div", "xin24m", CLK_IGNORE_UNUSED, RK1808_PMU_CLKSEL_CON(0), 8, 5, DFLAGS, RK1808_PMU_CLKGATE_CON(0), 12, GFLAGS),

    COMPOSITE_NOMUX(0, "clock_wifi_pmu_src", "ppll", 0, RK1808_PMU_CLKSEL_CON(2), 8, 6, DFLAGS, RK1808_PMU_CLKGATE_CON(0), 14, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_WIFI_PMU, "clock_wifi_pmu", mux_wifi_pmu_p, CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(2), 15, 1, MFLAGS, RK1808_PMU_CLKGATE_CON(0), 15,
        GFLAGS),

    COMPOSITE(
        0, "clock_uart0_pmu_src", mux_gpll_usb480m_cpll_ppll_p, 0, RK1808_PMU_CLKSEL_CON(3), 14, 2, MFLAGS, 0, 7, DFLAGS, RK1808_PMU_CLKGATE_CON(1),
        0, GFLAGS),
    COMPOSITE_NOMUX_HALFDIV(
        0, "clock_uart0_np5", "clock_uart0_pmu_src", 0, RK1808_PMU_CLKSEL_CON(4), 0, 7, DFLAGS, RK1808_PMU_CLKGATE_CON(1), 1, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart0_frac", "clock_uart0_pmu_src", CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(5), 0, RK1808_PMU_CLKGATE_CON(1), 2, GFLAGS,
        &rk1808_uart0_pmu_fracmux, RK1808_UART_FRAC_MAX_PRATE),
    GATE(SCLK_UART0_PMU, "clock_uart0_pmu", "clock_uart0_pmu_mux", CLK_SET_RATE_PARENT, RK1808_PMU_CLKGATE_CON(1), 3, GFLAGS),

    GATE(SCLK_PVTM_PMU, "clock_pvtm_pmu", "xin24m", 0, RK1808_PMU_CLKGATE_CON(1), 4, GFLAGS),

    COMPOSITE(
        SCLK_PMU_I2C0, "clock_pmu_i2c0", mux_ppll_xin24m_p, 0, RK1808_PMU_CLKSEL_CON(7), 15, 1, MFLAGS, 8, 7, DFLAGS, RK1808_PMU_CLKGATE_CON(1), 5,
        GFLAGS),

    COMPOSITE(
        DBCLK_PMU_GPIO0, "dbclock_gpio0", mux_xin24m_32k_p, 0, RK1808_PMU_CLKSEL_CON(6), 15, 1, MFLAGS, 0, 11, DFLAGS, RK1808_PMU_CLKGATE_CON(1), 6,
        GFLAGS),

    COMPOSITE_NOMUX(SCLK_REF24M_PMU, "clock_ref24m_pmu", "ppll", 0, RK1808_PMU_CLKSEL_CON(2), 0, 6, DFLAGS, RK1808_PMU_CLKGATE_CON(1), 8, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_USBPHY_REF, "clock_usbphy_ref", mux_usbphy_ref_p, CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(2), 6, 1, MFLAGS, RK1808_PMU_CLKGATE_CON(1),
        9, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_MIPIDSIPHY_REF, "clock_mipidsiphy_ref", mux_mipidsiphy_ref_p, CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(2), 7, 1, MFLAGS,
        RK1808_PMU_CLKGATE_CON(1), 10, GFLAGS),

    FACTOR(0, "clock_ppll_ph0", "ppll", 0, 1, 2),
    COMPOSITE_NOMUX(0, "clock_pciephy_src", "clock_ppll_ph0", 0, RK1808_PMU_CLKSEL_CON(7), 0, 2, DFLAGS, RK1808_PMU_CLKGATE_CON(1), 11, GFLAGS),
    COMPOSITE_NODIV(
        SCLK_PCIEPHY_REF, "clock_pciephy_ref", mux_pciephy_ref_p, CLK_SET_RATE_PARENT, RK1808_PMU_CLKSEL_CON(7), 4, 1, MFLAGS,
        RK1808_PMU_CLKGATE_CON(1), 12, GFLAGS),

    COMPOSITE_NOMUX(PCLK_PMU_PRE, "pclock_pmu_pre", "ppll", 0, RK1808_PMU_CLKSEL_CON(0), 0, 5, DFLAGS, RK1808_PMU_CLKGATE_CON(0), 0, GFLAGS),

    GATE(0, "pclock_pmu_niu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK1808_PMU_CLKGATE_CON(0), 1, GFLAGS),
    GATE(0, "pclock_pmu_sgrf", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK1808_PMU_CLKGATE_CON(0), 2, GFLAGS),
    GATE(0, "pclock_pmu_grf", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK1808_PMU_CLKGATE_CON(0), 3, GFLAGS),
    GATE(0, "pclock_pmu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK1808_PMU_CLKGATE_CON(0), 4, GFLAGS),
    GATE(0, "pclock_pmu_mem", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK1808_PMU_CLKGATE_CON(0), 5, GFLAGS),
    GATE(PCLK_GPIO0_PMU, "pclock_gpio0_pmu", "pclock_pmu_pre", 0, RK1808_PMU_CLKGATE_CON(0), 6, GFLAGS),
    GATE(PCLK_UART0_PMU, "pclock_uart0_pmu", "pclock_pmu_pre", 0, RK1808_PMU_CLKGATE_CON(0), 7, GFLAGS),
    GATE(0, "pclock_cru_pmu", "pclock_pmu_pre", CLK_IGNORE_UNUSED, RK1808_PMU_CLKGATE_CON(0), 8, GFLAGS),
    GATE(PCLK_I2C0_PMU, "pclock_i2c0_pmu", "pclock_pmu_pre", 0, RK1808_PMU_CLKGATE_CON(0), 9, GFLAGS),
};

static const char *const rk1808_critical_clocks[] __initconst = {
    "msclock_core_niu", "aclock_gic_niu",   "aclock_npu_niu",   "hclock_npu_niu",   "aclock_imem0_niu", "aclock_imem1_niu",
    "aclock_imem2_niu", "aclock_imem3_niu", "msclock_peri_niu", "lsclock_peri_niu", "hsclock_bus_niu",  "msclock_bus_niu",
    "lsclock_bus_niu",  "pclock_pmu_niu",   "pclock_top_pre",   "pclock_ddr_grf",   "aclock_gic",       "hsclock_imem",
};

static void __iomem *rk1808_cru_base;

void rk1808_dump_cru(void)
{
    if (rk1808_cru_base) {
        pr_warn("CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4, rk1808_cru_base, 0x500, false);
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 32, 4, rk1808_cru_base + 0x4000, 0x100, false);
    }
}

EXPORT_SYMBOL_GPL(rk1808_dump_cru);

static int rk1808_clock_panic(struct notifier_block *this, uint64_t ev, void *ptr)
{
    rk1808_dump_cru();
    return NOTIFY_DONE;
}

static struct notifier_block rk1808_clock_panic_block = {
    .notifier_call = rk1808_clock_panic,
};

static void __init rk1808_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    rk1808_cru_base = reg_base;

    ctx             = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        iounmap(reg_base);
        return;
    }

    rockchip_clock_register_plls(ctx, rk1808_pll_clocks, ARRAY_SIZE(rk1808_pll_clocks), RK1808_GRF_SOC_STATUS0);
    rockchip_clock_register_branches(ctx, rk1808_clock_branches, ARRAY_SIZE(rk1808_clock_branches));
    rockchip_clock_protect_critical(rk1808_critical_clocks, ARRAY_SIZE(rk1808_critical_clocks));

    rockchip_clock_register_armclk(
        ctx, ARMCLK, "armclk", mux_armclock_p, ARRAY_SIZE(mux_armclock_p), &rk1808_cpuclock_data, rk1808_cpuclock_rates,
        ARRAY_SIZE(rk1808_cpuclock_rates));

    rockchip_register_softrst(np, 16, reg_base + RK1808_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK1808_GLB_SRST_FST, NULL);

    rockchip_clock_of_add_provider(np, ctx);

    atomic_notifier_chain_register(&panic_notifier_list, &rk1808_clock_panic_block);
}

CLK_OF_DECLARE(rk1808_cru, "rockchip,rk1808-cru", rk1808_clock_init);
