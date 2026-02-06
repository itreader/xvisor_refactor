/*
 * Copyright (c) 2016 Rockchip Electronics Co. Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
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

#include <dt-bindings/clock/rk3399-cru.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "clk.h"
#include "vmm_initfn.h"

#define RK3399_I2S_FRAC_MAX_PRATE   600000000
#define RK3399_UART_FRAC_MAX_PRATE  600000000
#define RK3399_SPDIF_FRAC_MAX_PRATE 600000000
#define RK3399_VOP_FRAC_MAX_PRATE   600000000
#define RK3399_WIFI_FRAC_MAX_PRATE  600000000

enum rk3399_plls {
    lpll,
    bpll,
    dpll,
    cpll,
    gpll,
    npll,
    vpll,
};

enum rk3399_pmu_plls {
    ppll,
};

static struct rockchip_pll_rate_table rk3399_pll_rates[] = {
    /* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
    RK3036_PLL_RATE(2208000000, 1, 92, 1, 1, 1, 0),   RK3036_PLL_RATE(2184000000, 1, 91, 1, 1, 1, 0), RK3036_PLL_RATE(2160000000, 1, 90, 1, 1, 1, 0),
    RK3036_PLL_RATE(2136000000, 1, 89, 1, 1, 1, 0),   RK3036_PLL_RATE(2112000000, 1, 88, 1, 1, 1, 0), RK3036_PLL_RATE(2088000000, 1, 87, 1, 1, 1, 0),
    RK3036_PLL_RATE(2064000000, 1, 86, 1, 1, 1, 0),   RK3036_PLL_RATE(2040000000, 1, 85, 1, 1, 1, 0), RK3036_PLL_RATE(2016000000, 1, 84, 1, 1, 1, 0),
    RK3036_PLL_RATE(1992000000, 1, 83, 1, 1, 1, 0),   RK3036_PLL_RATE(1968000000, 1, 82, 1, 1, 1, 0), RK3036_PLL_RATE(1944000000, 1, 81, 1, 1, 1, 0),
    RK3036_PLL_RATE(1920000000, 1, 80, 1, 1, 1, 0),   RK3036_PLL_RATE(1896000000, 1, 79, 1, 1, 1, 0), RK3036_PLL_RATE(1872000000, 1, 78, 1, 1, 1, 0),
    RK3036_PLL_RATE(1848000000, 1, 77, 1, 1, 1, 0),   RK3036_PLL_RATE(1824000000, 1, 76, 1, 1, 1, 0), RK3036_PLL_RATE(1800000000, 1, 75, 1, 1, 1, 0),
    RK3036_PLL_RATE(1776000000, 1, 74, 1, 1, 1, 0),   RK3036_PLL_RATE(1752000000, 1, 73, 1, 1, 1, 0), RK3036_PLL_RATE(1728000000, 1, 72, 1, 1, 1, 0),
    RK3036_PLL_RATE(1704000000, 1, 71, 1, 1, 1, 0),   RK3036_PLL_RATE(1680000000, 1, 70, 1, 1, 1, 0), RK3036_PLL_RATE(1656000000, 1, 69, 1, 1, 1, 0),
    RK3036_PLL_RATE(1632000000, 1, 68, 1, 1, 1, 0),   RK3036_PLL_RATE(1608000000, 1, 67, 1, 1, 1, 0), RK3036_PLL_RATE(1600000000, 3, 200, 1, 1, 1, 0),
    RK3036_PLL_RATE(1584000000, 1, 66, 1, 1, 1, 0),   RK3036_PLL_RATE(1560000000, 1, 65, 1, 1, 1, 0), RK3036_PLL_RATE(1536000000, 1, 64, 1, 1, 1, 0),
    RK3036_PLL_RATE(1512000000, 1, 63, 1, 1, 1, 0),   RK3036_PLL_RATE(1488000000, 1, 62, 1, 1, 1, 0), RK3036_PLL_RATE(1464000000, 1, 61, 1, 1, 1, 0),
    RK3036_PLL_RATE(1440000000, 1, 60, 1, 1, 1, 0),   RK3036_PLL_RATE(1416000000, 1, 59, 1, 1, 1, 0), RK3036_PLL_RATE(1392000000, 1, 58, 1, 1, 1, 0),
    RK3036_PLL_RATE(1368000000, 1, 57, 1, 1, 1, 0),   RK3036_PLL_RATE(1344000000, 1, 56, 1, 1, 1, 0), RK3036_PLL_RATE(1320000000, 1, 55, 1, 1, 1, 0),
    RK3036_PLL_RATE(1296000000, 1, 54, 1, 1, 1, 0),   RK3036_PLL_RATE(1272000000, 1, 53, 1, 1, 1, 0), RK3036_PLL_RATE(1248000000, 1, 52, 1, 1, 1, 0),
    RK3036_PLL_RATE(1200000000, 1, 50, 1, 1, 1, 0),   RK3036_PLL_RATE(1188000000, 2, 99, 1, 1, 1, 0), RK3036_PLL_RATE(1104000000, 1, 46, 1, 1, 1, 0),
    RK3036_PLL_RATE(1100000000, 12, 550, 1, 1, 1, 0), RK3036_PLL_RATE(1008000000, 1, 84, 2, 1, 1, 0), RK3036_PLL_RATE(1000000000, 1, 125, 3, 1, 1, 0),
    RK3036_PLL_RATE(984000000, 1, 82, 2, 1, 1, 0),    RK3036_PLL_RATE(960000000, 1, 80, 2, 1, 1, 0),  RK3036_PLL_RATE(936000000, 1, 78, 2, 1, 1, 0),
    RK3036_PLL_RATE(912000000, 1, 76, 2, 1, 1, 0),    RK3036_PLL_RATE(900000000, 4, 300, 2, 1, 1, 0), RK3036_PLL_RATE(888000000, 1, 74, 2, 1, 1, 0),
    RK3036_PLL_RATE(864000000, 1, 72, 2, 1, 1, 0),    RK3036_PLL_RATE(840000000, 1, 70, 2, 1, 1, 0),  RK3036_PLL_RATE(816000000, 1, 68, 2, 1, 1, 0),
    RK3036_PLL_RATE(800000000, 1, 100, 3, 1, 1, 0),   RK3036_PLL_RATE(700000000, 6, 350, 2, 1, 1, 0), RK3036_PLL_RATE(696000000, 1, 58, 2, 1, 1, 0),
    RK3036_PLL_RATE(676000000, 3, 169, 2, 1, 1, 0),   RK3036_PLL_RATE(600000000, 1, 75, 3, 1, 1, 0),  RK3036_PLL_RATE(594000000, 1, 99, 4, 1, 1, 0),
    RK3036_PLL_RATE(533250000, 8, 711, 4, 1, 1, 0),   RK3036_PLL_RATE(504000000, 1, 63, 3, 1, 1, 0),  RK3036_PLL_RATE(500000000, 6, 250, 2, 1, 1, 0),
    RK3036_PLL_RATE(408000000, 1, 68, 2, 2, 1, 0),    RK3036_PLL_RATE(312000000, 1, 52, 2, 2, 1, 0),  RK3036_PLL_RATE(297000000, 1, 99, 4, 2, 1, 0),
    RK3036_PLL_RATE(216000000, 1, 72, 4, 2, 1, 0),    RK3036_PLL_RATE(148500000, 1, 99, 4, 4, 1, 0),  RK3036_PLL_RATE(106500000, 1, 71, 4, 4, 1, 0),
    RK3036_PLL_RATE(96000000, 1, 64, 4, 4, 1, 0),     RK3036_PLL_RATE(74250000, 2, 99, 4, 4, 1, 0),   RK3036_PLL_RATE(65000000, 1, 65, 6, 4, 1, 0),
    RK3036_PLL_RATE(54000000, 1, 54, 6, 4, 1, 0),     RK3036_PLL_RATE(27000000, 1, 27, 6, 4, 1, 0),   {/* sentinel */},
};

static struct rockchip_pll_rate_table rk3399_vpll_rates[] = {
    /* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd, _frac */
    RK3036_PLL_RATE(594000000, 1, 123, 5, 1, 0, 12582912), /* vco = 2970000000 */
    RK3036_PLL_RATE(593406593, 1, 123, 5, 1, 0, 10508804), /* vco = 2967032965 */
    RK3036_PLL_RATE(297000000, 1, 123, 5, 2, 0, 12582912), /* vco = 2970000000 */
    RK3036_PLL_RATE(296703297, 1, 123, 5, 2, 0, 10508807), /* vco = 2967032970 */
    RK3036_PLL_RATE(148500000, 1, 129, 7, 3, 0, 15728640), /* vco = 3118500000 */
    RK3036_PLL_RATE(148351648, 1, 123, 5, 4, 0, 10508800), /* vco = 2967032960 */
    RK3036_PLL_RATE(106500000, 1, 124, 7, 4, 0, 4194304),  /* vco = 2982000000 */
    RK3036_PLL_RATE(74250000, 1, 129, 7, 6, 0, 15728640),  /* vco = 3118500000 */
    RK3036_PLL_RATE(74175824, 1, 129, 7, 6, 0, 13550823),  /* vco = 3115384608 */
    RK3036_PLL_RATE(65000000, 1, 113, 7, 6, 0, 12582912),  /* vco = 2730000000 */
    RK3036_PLL_RATE(59340659, 1, 121, 7, 7, 0, 2581098),   /* vco = 2907692291 */
    RK3036_PLL_RATE(54000000, 1, 110, 7, 7, 0, 4194304),   /* vco = 2646000000 */
    RK3036_PLL_RATE(27000000, 1, 55, 7, 7, 0, 2097152),    /* vco = 1323000000 */
    RK3036_PLL_RATE(26973027, 1, 55, 7, 7, 0, 1173232),    /* vco = 1321678323 */
    {/* sentinel */},
};

/* CRU parents */
PNAME(mux_pll_p)                    = {"xin24m", "xin32k"};

PNAME(mux_armclkl_p)                = {"clock_core_l_lpll_src", "clock_core_l_bpll_src", "clock_core_l_dpll_src", "clock_core_l_gpll_src"};
PNAME(mux_armclkb_p)                = {"clock_core_b_lpll_src", "clock_core_b_bpll_src", "clock_core_b_dpll_src", "clock_core_b_gpll_src"};
PNAME(mux_ddrclock_p)               = {"clock_ddrc_lpll_src", "clock_ddrc_bpll_src", "clock_ddrc_dpll_src", "clock_ddrc_gpll_src"};

PNAME(mux_pll_src_vpll_cpll_gpll_p) = {"vpll", "cpll", "gpll"};
#ifndef RK3399_TWO_PLL_FOR_VOP
PNAME(mux_pll_src_dmyvpll_cpll_gpll_p) = {"dummy_vpll", "cpll", "gpll"};
#endif

#ifdef RK3399_TWO_PLL_FOR_VOP
PNAME(mux_aclock_cci_p)                           = {"dummy_cpll", "gpll_aclock_cci_src", "npll_aclock_cci_src", "dummy_vpll"};
PNAME(mux_cci_trace_p)                            = {"dummy_cpll", "gpll_cci_trace"};
PNAME(mux_cs_p)                                   = {"dummy_cpll", "gpll_cs", "npll_cs"};
PNAME(mux_aclock_perihp_p)                        = {"dummy_cpll", "gpll_aclock_perihp_src"};

PNAME(mux_pll_src_cpll_gpll_p)                    = {"dummy_cpll", "gpll"};
PNAME(mux_pll_src_cpll_gpll_npll_p)               = {"dummy_cpll", "gpll", "npll"};
PNAME(mux_pll_src_cpll_gpll_ppll_p)               = {"dummy_cpll", "gpll", "ppll"};
PNAME(mux_pll_src_cpll_gpll_upll_p)               = {"dummy_cpll", "gpll", "upll"};
PNAME(mux_pll_src_npll_cpll_gpll_p)               = {"npll", "dummy_cpll", "gpll"};
PNAME(mux_pll_src_cpll_gpll_npll_ppll_p)          = {"dummy_cpll", "gpll", "npll", "ppll"};
PNAME(mux_pll_src_cpll_gpll_npll_24m_p)           = {"dummy_cpll", "gpll", "npll", "xin24m"};
PNAME(mux_pll_src_cpll_gpll_npll_usbphy480m_p)    = {"dummy_cpll", "gpll", "npll", "clock_usbphy_480m"};
PNAME(mux_pll_src_ppll_cpll_gpll_npll_p)          = {"ppll", "dummy_cpll", "gpll", "npll", "upll"};
PNAME(mux_pll_src_cpll_gpll_npll_upll_24m_p)      = {"dummy_cpll", "gpll", "npll", "upll", "xin24m"};
PNAME(mux_pll_src_cpll_gpll_npll_ppll_upll_24m_p) = {"dummy_cpll", "gpll", "npll", "ppll", "upll", "xin24m"};
/*
 * We hope to be able to HDMI/DP can obtain better signal quality,
 * therefore, we move VOP pwm and aclk clocks to other PLLs, let
 * HDMI/DP phyclock can monopolize VPLL.
 */
PNAME(mux_pll_src_dmyvpll_cpll_gpll_npll_p)       = {"dummy_vpll", "dummy_cpll", "gpll", "npll"};
PNAME(mux_pll_src_dmyvpll_cpll_gpll_gpll_p)       = {"dummy_vpll", "dummy_cpll", "gpll", "gpll"};
PNAME(mux_pll_src_24m_32k_cpll_gpll_p)            = {"xin24m", "xin32k", "dummy_cpll", "gpll"};

PNAME(mux_aclock_emmc_p)                          = {"dummy_cpll", "gpll_aclock_emmc_src"};

PNAME(mux_aclock_perilp0_p)                       = {"dummy_cpll", "gpll_aclock_perilp0_src"};

PNAME(mux_fclock_cm0s_p)                          = {"dummy_cpll", "gpll_fclock_cm0s_src"};

PNAME(mux_hclock_perilp1_p)                       = {"dummy_cpll", "gpll_hclock_perilp1_src"};
PNAME(mux_aclock_gmac_p)                          = {"dummy_cpll", "gpll_aclock_gmac_src"};
#else
PNAME(mux_aclock_cci_p)                           = {"cpll_aclock_cci_src", "gpll_aclock_cci_src", "npll_aclock_cci_src", "dummy_vpll"};
PNAME(mux_cci_trace_p)                            = {"cpll_cci_trace", "gpll_cci_trace"};
PNAME(mux_cs_p)                                   = {"cpll_cs", "gpll_cs", "npll_cs"};
PNAME(mux_aclock_perihp_p)                        = {"cpll_aclock_perihp_src", "gpll_aclock_perihp_src"};

PNAME(mux_pll_src_cpll_gpll_p)                    = {"cpll", "gpll"};
PNAME(mux_pll_src_cpll_gpll_npll_p)               = {"cpll", "gpll", "npll"};
PNAME(mux_pll_src_cpll_gpll_ppll_p)               = {"cpll", "gpll", "ppll"};
PNAME(mux_pll_src_cpll_gpll_upll_p)               = {"cpll", "gpll", "upll"};
PNAME(mux_pll_src_npll_cpll_gpll_p)               = {"npll", "cpll", "gpll"};
PNAME(mux_pll_src_cpll_gpll_npll_ppll_p)          = {"cpll", "gpll", "npll", "ppll"};
PNAME(mux_pll_src_cpll_gpll_npll_24m_p)           = {"cpll", "gpll", "npll", "xin24m"};
PNAME(mux_pll_src_cpll_gpll_npll_usbphy480m_p)    = {"cpll", "gpll", "npll", "clock_usbphy_480m"};
PNAME(mux_pll_src_ppll_cpll_gpll_npll_p)          = {"ppll", "cpll", "gpll", "npll", "upll"};
PNAME(mux_pll_src_cpll_gpll_npll_upll_24m_p)      = {"cpll", "gpll", "npll", "upll", "xin24m"};
PNAME(mux_pll_src_cpll_gpll_npll_ppll_upll_24m_p) = {"cpll", "gpll", "npll", "ppll", "upll", "xin24m"};
/*
 * We hope to be able to HDMI/DP can obtain better signal quality,
 * therefore, we move VOP pwm and aclk clocks to other PLLs, let
 * HDMI/DP phyclock can monopolize VPLL.
 */
PNAME(mux_pll_src_dmyvpll_cpll_gpll_npll_p)       = {"dummy_vpll", "cpll", "gpll", "npll"};
PNAME(mux_pll_src_dmyvpll_cpll_gpll_gpll_p)       = {"dummy_vpll", "cpll", "gpll", "gpll"};
PNAME(mux_pll_src_24m_32k_cpll_gpll_p)            = {"xin24m", "xin32k", "cpll", "gpll"};

PNAME(mux_aclock_emmc_p)                          = {"cpll_aclock_emmc_src", "gpll_aclock_emmc_src"};

PNAME(mux_aclock_perilp0_p)                       = {"cpll_aclock_perilp0_src", "gpll_aclock_perilp0_src"};

PNAME(mux_fclock_cm0s_p)                          = {"cpll_fclock_cm0s_src", "gpll_fclock_cm0s_src"};

PNAME(mux_hclock_perilp1_p)                       = {"cpll_hclock_perilp1_src", "gpll_hclock_perilp1_src"};
PNAME(mux_aclock_gmac_p)                          = {"cpll_aclock_gmac_src", "gpll_aclock_gmac_src"};
#endif

PNAME(mux_dclock_vop0_p)                                        = {"dclock_vop0_div", "dummy_dclock_vop0_frac"};
PNAME(mux_dclock_vop1_p)                                        = {"dclock_vop1_div", "dummy_dclock_vop1_frac"};

PNAME(mux_clock_cif_p)                                          = {"clock_cifout_src", "xin24m"};

PNAME(mux_pll_src_24m_usbphy480m_p)                             = {"xin24m", "clock_usbphy_480m"};
PNAME(mux_pll_src_24m_pciephy_p)                                = {"xin24m", "clock_pciephy_ref100m"};
PNAME(mux_pciecore_cru_phy_p)                                   = {"clock_pcie_core_cru", "clock_pcie_core_phy"};
PNAME(mux_clock_testout1_p)                                     = {"clock_testout1_pll_src", "xin24m"};
PNAME(mux_clock_testout2_p)                                     = {"clock_testout2_pll_src", "xin24m"};

PNAME(mux_usbphy_480m_p)                                        = {"clock_usbphy0_480m_src", "clock_usbphy1_480m_src"};
PNAME(mux_rmii_p)                                               = {"clock_gmac", "clkin_gmac"};
PNAME(mux_spdif_p)                                              = {"clock_spdif_div", "clock_spdif_frac", "clkin_i2s", "xin12m"};
PNAME(mux_i2s0_p)                                               = {"clock_i2s0_div", "clock_i2s0_frac", "clkin_i2s", "xin12m"};
PNAME(mux_i2s1_p)                                               = {"clock_i2s1_div", "clock_i2s1_frac", "clkin_i2s", "xin12m"};
PNAME(mux_i2s2_p)                                               = {"clock_i2s2_div", "clock_i2s2_frac", "clkin_i2s", "xin12m"};
PNAME(mux_i2sch_p)                                              = {"clock_i2s0", "clock_i2s1", "clock_i2s2"};
PNAME(mux_i2sout_p)                                             = {"clock_i2sout_src", "xin12m"};

PNAME(mux_uart0_p)                                              = {"clock_uart0_div", "clock_uart0_frac", "xin24m"};
PNAME(mux_uart1_p)                                              = {"clock_uart1_div", "clock_uart1_frac", "xin24m"};
PNAME(mux_uart2_p)                                              = {"clock_uart2_div", "clock_uart2_frac", "xin24m"};
PNAME(mux_uart3_p)                                              = {"clock_uart3_div", "clock_uart3_frac", "xin24m"};

/* PMU CRU parents */
PNAME(mux_ppll_24m_p)                                           = {"ppll", "xin24m"};
PNAME(mux_24m_ppll_p)                                           = {"xin24m", "ppll"};
PNAME(mux_fclock_cm0s_pmu_ppll_p)                               = {"fclock_cm0s_pmu_ppll_src", "xin24m"};
PNAME(mux_wifi_pmu_p)                                           = {"clock_wifi_div", "clock_wifi_frac"};
PNAME(mux_uart4_pmu_p)                                          = {"clock_uart4_div", "clock_uart4_frac", "xin24m"};
PNAME(mux_clock_testout2_2io_p)                                 = {"clock_testout2", "clock_32k_suspend_pmu"};

static struct rockchip_pll_clock rk3399_pll_clocks[] __initdata = {
    [lpll] = PLL(pll_rk3399, PLL_APLLL, "lpll", mux_pll_p, 0, RK3399_PLL_CON(0), RK3399_PLL_CON(3), 8, 31, 0, rk3399_pll_rates),
    [bpll] = PLL(pll_rk3399, PLL_APLLB, "bpll", mux_pll_p, 0, RK3399_PLL_CON(8), RK3399_PLL_CON(11), 8, 31, 0, rk3399_pll_rates),
    [dpll] = PLL(pll_rk3399, PLL_DPLL, "dpll", mux_pll_p, 0, RK3399_PLL_CON(16), RK3399_PLL_CON(19), 8, 31, 0, NULL),
#ifdef RK3399_TWO_PLL_FOR_VOP
    [cpll] = PLL(pll_rk3399, PLL_CPLL, "cpll", mux_pll_p, 0, RK3399_PLL_CON(24), RK3399_PLL_CON(27), 8, 31, 0, rk3399_pll_rates),
#else
    [cpll] = PLL(pll_rk3399, PLL_CPLL, "cpll", mux_pll_p, 0, RK3399_PLL_CON(24), RK3399_PLL_CON(27), 8, 31, ROCKCHIP_PLL_SYNC_RATE, rk3399_pll_rates),
#endif
    [gpll] = PLL(pll_rk3399, PLL_GPLL, "gpll", mux_pll_p, 0, RK3399_PLL_CON(32), RK3399_PLL_CON(35), 8, 31, 0, rk3399_pll_rates),
    [npll] = PLL(pll_rk3399, PLL_NPLL, "npll", mux_pll_p, 0, RK3399_PLL_CON(40), RK3399_PLL_CON(43), 8, 31, ROCKCHIP_PLL_SYNC_RATE, rk3399_pll_rates),
    [vpll] = PLL(pll_rk3399, PLL_VPLL, "vpll", mux_pll_p, 0, RK3399_PLL_CON(48), RK3399_PLL_CON(51), 8, 31, 0, rk3399_vpll_rates),
};

static struct rockchip_pll_clock rk3399_pmu_pll_clocks[] __initdata = {
    [ppll] = PLL(
        pll_rk3399, PLL_PPLL, "ppll", mux_pll_p, 0, RK3399_PMU_PLL_CON(0), RK3399_PMU_PLL_CON(3), 8, 31, ROCKCHIP_PLL_SYNC_RATE, rk3399_pll_rates),
};

#define MFLAGS CLK_MUX_HIWORD_MASK
#define DFLAGS CLK_DIVIDER_HIWORD_MASK
#define GFLAGS (CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE)
#define IFLAGS ROCKCHIP_INVERTER_HIWORD_MASK

static struct rockchip_clock_branch rk3399_spdif_fracmux __initdata =
    MUX(0, "clock_spdif_mux", mux_spdif_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(32), 13, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_i2s0_fracmux __initdata =
    MUX(0, "clock_i2s0_mux", mux_i2s0_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(28), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_i2s1_fracmux __initdata =
    MUX(0, "clock_i2s1_mux", mux_i2s1_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(29), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_i2s2_fracmux __initdata =
    MUX(0, "clock_i2s2_mux", mux_i2s2_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(30), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_uart0_fracmux __initdata =
    MUX(SCLK_UART0, "clock_uart0", mux_uart0_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(33), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_uart1_fracmux __initdata =
    MUX(SCLK_UART1, "clock_uart1", mux_uart1_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(34), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_uart2_fracmux __initdata =
    MUX(SCLK_UART2, "clock_uart2", mux_uart2_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(35), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_uart3_fracmux __initdata =
    MUX(SCLK_UART3, "clock_uart3", mux_uart3_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(36), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_uart4_pmu_fracmux __initdata =
    MUX(SCLK_UART4_PMU, "clock_uart4_pmu", mux_uart4_pmu_p, CLK_SET_RATE_PARENT, RK3399_PMU_CLKSEL_CON(5), 8, 2, MFLAGS);

static struct rockchip_clock_branch rk3399_dclock_vop0_fracmux __initdata =
    MUX(DCLK_VOP0, "dclock_vop0", mux_dclock_vop0_p, CLK_SET_RATE_PARENT | CLK_KEEP_REQ_RATE, RK3399_CLKSEL_CON(49), 11, 1, MFLAGS);

static struct rockchip_clock_branch rk3399_dclock_vop1_fracmux __initdata =
    MUX(DCLK_VOP1, "dclock_vop1", mux_dclock_vop1_p, CLK_SET_RATE_PARENT | CLK_KEEP_REQ_RATE, RK3399_CLKSEL_CON(50), 11, 1, MFLAGS);

static struct rockchip_clock_branch rk3399_pmuclock_wifi_fracmux __initdata =
    MUX(SCLK_WIFI_PMU, "clock_wifi_pmu", mux_wifi_pmu_p, CLK_SET_RATE_PARENT, RK3399_PMU_CLKSEL_CON(1), 14, 1, MFLAGS);

static const struct rockchip_cpuclock_reg_data rk3399_cpuclkl_data = {
    .core_reg       = RK3399_CLKSEL_CON(0),
    .div_core_shift = 0,
    .div_core_mask  = 0x1f,
    .mux_core_alt   = 3,
    .mux_core_main  = 0,
    .mux_core_shift = 6,
    .mux_core_mask  = 0x3,
};

static const struct rockchip_cpuclock_reg_data rk3399_cpuclkb_data = {
    .core_reg       = RK3399_CLKSEL_CON(2),
    .div_core_shift = 0,
    .div_core_mask  = 0x1f,
    .mux_core_alt   = 3,
    .mux_core_main  = 1,
    .mux_core_shift = 6,
    .mux_core_mask  = 0x3,
};

#define RK3399_DIV_ACLKM_MASK     0x1f
#define RK3399_DIV_ACLKM_SHIFT    8
#define RK3399_DIV_ATCLK_MASK     0x1f
#define RK3399_DIV_ATCLK_SHIFT    0
#define RK3399_DIV_PCLK_DBG_MASK  0x1f
#define RK3399_DIV_PCLK_DBG_SHIFT 8

#define RK3399_CLKSEL0(_offs, _aclkm)                                                                                     \
    {                                                                                                                     \
        .reg = RK3399_CLKSEL_CON(0 + _offs), .val = HIWORD_UPDATE(_aclkm, RK3399_DIV_ACLKM_MASK, RK3399_DIV_ACLKM_SHIFT), \
    }
#define RK3399_CLKSEL1(_offs, _atclk, _pdbg)                                              \
    {                                                                                     \
        .reg = RK3399_CLKSEL_CON(1 + _offs),                                              \
        .val = HIWORD_UPDATE(_atclk, RK3399_DIV_ATCLK_MASK, RK3399_DIV_ATCLK_SHIFT) |     \
               HIWORD_UPDATE(_pdbg, RK3399_DIV_PCLK_DBG_MASK, RK3399_DIV_PCLK_DBG_SHIFT), \
    }

/* cluster_l: aclkm in clksel0, rest in clksel1 */
#define RK3399_CPUCLKL_RATE(_prate, _aclkm, _atclk, _pdbg) \
    {                                                      \
        .prate = _prate##U,                                \
        .divs  = {                                         \
            RK3399_CLKSEL0(0, _aclkm),                    \
            RK3399_CLKSEL1(0, _atclk, _pdbg),             \
        },                                                \
    }

/* cluster_b: aclkm in clksel2, rest in clksel3 */
#define RK3399_CPUCLKB_RATE(_prate, _aclkm, _atclk, _pdbg) \
    {                                                      \
        .prate = _prate##U,                                \
        .divs  = {                                         \
            RK3399_CLKSEL0(2, _aclkm),                    \
            RK3399_CLKSEL1(2, _atclk, _pdbg),             \
        },                                                \
    }

static struct rockchip_cpuclock_rate_table rk3399_cpuclkl_rates[] __initdata = {
    RK3399_CPUCLKL_RATE(1800000000, 1, 8, 8), RK3399_CPUCLKL_RATE(1704000000, 1, 8, 8), RK3399_CPUCLKL_RATE(1608000000, 1, 7, 7),
    RK3399_CPUCLKL_RATE(1512000000, 1, 7, 7), RK3399_CPUCLKL_RATE(1488000000, 1, 6, 6), RK3399_CPUCLKL_RATE(1416000000, 1, 6, 6),
    RK3399_CPUCLKL_RATE(1200000000, 1, 5, 5), RK3399_CPUCLKL_RATE(1008000000, 1, 5, 5), RK3399_CPUCLKL_RATE(816000000, 1, 4, 4),
    RK3399_CPUCLKL_RATE(696000000, 1, 3, 3),  RK3399_CPUCLKL_RATE(600000000, 1, 3, 3),  RK3399_CPUCLKL_RATE(408000000, 1, 2, 2),
    RK3399_CPUCLKL_RATE(312000000, 1, 1, 1),  RK3399_CPUCLKL_RATE(216000000, 1, 1, 1),  RK3399_CPUCLKL_RATE(96000000, 1, 1, 1),
};

static struct rockchip_cpuclock_rate_table rk3399_cpuclkb_rates[] __initdata = {
    RK3399_CPUCLKB_RATE(2208000000, 1, 11, 11), RK3399_CPUCLKB_RATE(2184000000, 1, 11, 11), RK3399_CPUCLKB_RATE(2088000000, 1, 10, 10),
    RK3399_CPUCLKB_RATE(2040000000, 1, 10, 10), RK3399_CPUCLKB_RATE(2016000000, 1, 9, 9),   RK3399_CPUCLKB_RATE(1992000000, 1, 9, 9),
    RK3399_CPUCLKB_RATE(1896000000, 1, 9, 9),   RK3399_CPUCLKB_RATE(1800000000, 1, 8, 8),   RK3399_CPUCLKB_RATE(1704000000, 1, 8, 8),
    RK3399_CPUCLKB_RATE(1608000000, 1, 7, 7),   RK3399_CPUCLKB_RATE(1512000000, 1, 7, 7),   RK3399_CPUCLKB_RATE(1488000000, 1, 6, 6),
    RK3399_CPUCLKB_RATE(1416000000, 1, 6, 6),   RK3399_CPUCLKB_RATE(1200000000, 1, 5, 5),   RK3399_CPUCLKB_RATE(1008000000, 1, 5, 5),
    RK3399_CPUCLKB_RATE(816000000, 1, 4, 4),    RK3399_CPUCLKB_RATE(696000000, 1, 3, 3),    RK3399_CPUCLKB_RATE(600000000, 1, 3, 3),
    RK3399_CPUCLKB_RATE(408000000, 1, 2, 2),    RK3399_CPUCLKB_RATE(312000000, 1, 1, 1),    RK3399_CPUCLKB_RATE(216000000, 1, 1, 1),
    RK3399_CPUCLKB_RATE(96000000, 1, 1, 1),
};

static struct rockchip_clock_branch rk3399_clock_branches[] __initdata = {
    /*
     * CRU Clock-Architecture
     */

    /* usbphy */
    GATE(SCLK_USB2PHY0_REF, "clock_usb2phy0_ref", "xin24m", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(6), 5, GFLAGS),
    GATE(SCLK_USB2PHY1_REF, "clock_usb2phy1_ref", "xin24m", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(6), 6, GFLAGS),

    GATE(SCLK_USBPHY0_480M_SRC, "clock_usbphy0_480m_src", "clock_usbphy0_480m", 0, RK3399_CLKGATE_CON(13), 12, GFLAGS),
    GATE(SCLK_USBPHY1_480M_SRC, "clock_usbphy1_480m_src", "clock_usbphy1_480m", 0, RK3399_CLKGATE_CON(13), 12, GFLAGS),
    MUX(0, "clock_usbphy_480m", mux_usbphy_480m_p, 0, RK3399_CLKSEL_CON(14), 6, 1, MFLAGS),

    MUX(0, "upll", mux_pll_src_24m_usbphy480m_p, 0, RK3399_CLKSEL_CON(14), 15, 1, MFLAGS),

    COMPOSITE_NODIV(
        SCLK_HSICPHY, "clock_hsicphy", mux_pll_src_cpll_gpll_npll_usbphy480m_p, 0, RK3399_CLKSEL_CON(19), 0, 2, MFLAGS, RK3399_CLKGATE_CON(6), 4,
        GFLAGS),

    COMPOSITE(
        ACLK_USB3, "aclock_usb3", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(39), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(12), 0,
        GFLAGS),
    GATE(ACLK_USB3_NOC, "aclock_usb3_noc", "aclock_usb3", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(30), 0, GFLAGS),
    GATE(ACLK_USB3OTG0, "aclock_usb3otg0", "aclock_usb3", 0, RK3399_CLKGATE_CON(30), 1, GFLAGS),
    GATE(ACLK_USB3OTG1, "aclock_usb3otg1", "aclock_usb3", 0, RK3399_CLKGATE_CON(30), 2, GFLAGS),
    GATE(ACLK_USB3_RKSOC_AXI_PERF, "aclock_usb3_rksoc_axi_perf", "aclock_usb3", 0, RK3399_CLKGATE_CON(30), 3, GFLAGS),
    GATE(ACLK_USB3_GRF, "aclock_usb3_grf", "aclock_usb3", 0, RK3399_CLKGATE_CON(30), 4, GFLAGS),

    GATE(SCLK_USB3OTG0_REF, "clock_usb3otg0_ref", "xin24m", 0, RK3399_CLKGATE_CON(12), 1, GFLAGS),
    GATE(SCLK_USB3OTG1_REF, "clock_usb3otg1_ref", "xin24m", 0, RK3399_CLKGATE_CON(12), 2, GFLAGS),

    COMPOSITE(
        SCLK_USB3OTG0_SUSPEND, "clock_usb3otg0_suspend", mux_pll_p, 0, RK3399_CLKSEL_CON(40), 15, 1, MFLAGS, 0, 10, DFLAGS, RK3399_CLKGATE_CON(12), 3,
        GFLAGS),

    COMPOSITE(
        SCLK_USB3OTG1_SUSPEND, "clock_usb3otg1_suspend", mux_pll_p, 0, RK3399_CLKSEL_CON(41), 15, 1, MFLAGS, 0, 10, DFLAGS, RK3399_CLKGATE_CON(12), 4,
        GFLAGS),

    COMPOSITE(
        SCLK_UPHY0_TCPDPHY_REF, "clock_uphy0_tcpdphy_ref", mux_pll_p, 0, RK3399_CLKSEL_CON(64), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(13),
        4, GFLAGS),

    COMPOSITE(
        SCLK_UPHY0_TCPDCORE, "clock_uphy0_tcpdcore", mux_pll_src_24m_32k_cpll_gpll_p, 0, RK3399_CLKSEL_CON(64), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(13), 5, GFLAGS),

    COMPOSITE(
        SCLK_UPHY1_TCPDPHY_REF, "clock_uphy1_tcpdphy_ref", mux_pll_p, 0, RK3399_CLKSEL_CON(65), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(13),
        6, GFLAGS),

    COMPOSITE(
        SCLK_UPHY1_TCPDCORE, "clock_uphy1_tcpdcore", mux_pll_src_24m_32k_cpll_gpll_p, 0, RK3399_CLKSEL_CON(65), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(13), 7, GFLAGS),

    /* little core */
    GATE(0, "clock_core_l_lpll_src", "lpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(0), 0, GFLAGS),
    GATE(0, "clock_core_l_bpll_src", "bpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(0), 1, GFLAGS),
    GATE(0, "clock_core_l_dpll_src", "dpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(0), 2, GFLAGS),
    GATE(0, "clock_core_l_gpll_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(0), 3, GFLAGS),

    COMPOSITE_NOMUX(
        0, "aclkm_core_l", "armclkl", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(0), 8, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3399_CLKGATE_CON(0), 4,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "atclock_core_l", "armclkl", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(1), 0, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3399_CLKGATE_CON(0), 5,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg_core_l", "armclkl", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(1), 8, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3399_CLKGATE_CON(0), 6,
        GFLAGS),

    GATE(
        ACLK_CORE_ADB400_CORE_L_2_CCI500, "aclock_core_adb400_core_l_2_cci500", "aclkm_core_l", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 12,
        GFLAGS),
    GATE(ACLK_PERF_CORE_L, "aclock_perf_core_l", "aclkm_core_l", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 13, GFLAGS),

    GATE(0, "clock_dbg_pd_core_l", "armclkl", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 9, GFLAGS),
    GATE(ACLK_GIC_ADB400_GIC_2_CORE_L, "aclock_core_adb400_gic_2_core_l", "armclkl", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 10, GFLAGS),
    GATE(ACLK_GIC_ADB400_CORE_L_2_GIC, "aclock_core_adb400_core_l_2_gic", "armclkl", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 11, GFLAGS),
    GATE(SCLK_PVTM_CORE_L, "clock_pvtm_core_l", "xin24m", 0, RK3399_CLKGATE_CON(0), 7, GFLAGS),

    /* big core */
    GATE(0, "clock_core_b_lpll_src", "lpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(1), 0, GFLAGS),
    GATE(0, "clock_core_b_bpll_src", "bpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(1), 1, GFLAGS),
    GATE(0, "clock_core_b_dpll_src", "dpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(1), 2, GFLAGS),
    GATE(0, "clock_core_b_gpll_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(1), 3, GFLAGS),

    COMPOSITE_NOMUX(
        0, "aclkm_core_b", "armclkb", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(2), 8, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3399_CLKGATE_CON(1), 4,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "atclock_core_b", "armclkb", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(3), 0, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3399_CLKGATE_CON(1), 5,
        GFLAGS),
    COMPOSITE_NOMUX(
        0, "pclock_dbg_core_b", "armclkb", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(3), 8, 5, DFLAGS | CLK_DIVIDER_READ_ONLY, RK3399_CLKGATE_CON(1), 6,
        GFLAGS),

    GATE(
        ACLK_CORE_ADB400_CORE_B_2_CCI500, "aclock_core_adb400_core_b_2_cci500", "aclkm_core_b", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 5, GFLAGS),
    GATE(ACLK_PERF_CORE_B, "aclock_perf_core_b", "aclkm_core_b", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 6, GFLAGS),

    GATE(0, "clock_dbg_pd_core_b", "armclkb", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 1, GFLAGS),
    GATE(ACLK_GIC_ADB400_GIC_2_CORE_B, "aclock_core_adb400_gic_2_core_b", "armclkb", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 3, GFLAGS),
    GATE(ACLK_GIC_ADB400_CORE_B_2_GIC, "aclock_core_adb400_core_b_2_gic", "armclkb", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 4, GFLAGS),

    DIV(0, "pclken_dbg_core_b", "pclock_dbg_core_b", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(3), 13, 2, DFLAGS | CLK_DIVIDER_READ_ONLY),

    GATE(0, "pclock_dbg_cxcs_pd_core_b", "pclock_dbg_core_b", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(14), 2, GFLAGS),

    GATE(SCLK_PVTM_CORE_B, "clock_pvtm_core_b", "xin24m", 0, RK3399_CLKGATE_CON(1), 7, GFLAGS),

    /* gmac */
    GATE(0, "cpll_aclock_gmac_src", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(6), 9, GFLAGS),
    GATE(0, "gpll_aclock_gmac_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(6), 8, GFLAGS),
    COMPOSITE(0, "aclock_gmac_pre", mux_aclock_gmac_p, 0, RK3399_CLKSEL_CON(20), 7, 1, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(6), 10, GFLAGS),

    GATE(ACLK_GMAC, "aclock_gmac", "aclock_gmac_pre", 0, RK3399_CLKGATE_CON(32), 0, GFLAGS),
    GATE(ACLK_GMAC_NOC, "aclock_gmac_noc", "aclock_gmac_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(32), 1, GFLAGS),
    GATE(ACLK_PERF_GMAC, "aclock_perf_gmac", "aclock_gmac_pre", 0, RK3399_CLKGATE_CON(32), 4, GFLAGS),

    COMPOSITE_NOMUX(0, "pclock_gmac_pre", "aclock_gmac_pre", 0, RK3399_CLKSEL_CON(19), 8, 3, DFLAGS, RK3399_CLKGATE_CON(6), 11, GFLAGS),
    GATE(PCLK_GMAC, "pclock_gmac", "pclock_gmac_pre", 0, RK3399_CLKGATE_CON(32), 2, GFLAGS),
    GATE(PCLK_GMAC_NOC, "pclock_gmac_noc", "pclock_gmac_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(32), 3, GFLAGS),

    COMPOSITE(
        SCLK_MAC, "clock_gmac", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(20), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(5), 5,
        GFLAGS),

    MUX(SCLK_RMII_SRC, "clock_rmii_src", mux_rmii_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(19), 4, 1, MFLAGS),
    GATE(SCLK_MACREF_OUT, "clock_mac_refout", "clock_rmii_src", 0, RK3399_CLKGATE_CON(5), 6, GFLAGS),
    GATE(SCLK_MACREF, "clock_mac_ref", "clock_rmii_src", 0, RK3399_CLKGATE_CON(5), 7, GFLAGS),
    GATE(SCLK_MAC_RX, "clock_rmii_rx", "clock_rmii_src", 0, RK3399_CLKGATE_CON(5), 8, GFLAGS),
    GATE(SCLK_MAC_TX, "clock_rmii_tx", "clock_rmii_src", 0, RK3399_CLKGATE_CON(5), 9, GFLAGS),

    /* spdif */
    COMPOSITE(
        SCLK_SPDIF_DIV, "clock_spdif_div", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(32), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(8), 13,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_spdif_frac", "clock_spdif_div", 0, RK3399_CLKSEL_CON(99), 0, RK3399_CLKGATE_CON(8), 14, GFLAGS, &rk3399_spdif_fracmux,
        RK3399_SPDIF_FRAC_MAX_PRATE),
    GATE(SCLK_SPDIF_8CH, "clock_spdif", "clock_spdif_mux", CLK_SET_RATE_PARENT, RK3399_CLKGATE_CON(8), 15, GFLAGS),

    COMPOSITE(
        SCLK_SPDIF_REC_DPTX, "clock_spdif_rec_dptx", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(32), 15, 1, MFLAGS, 8, 5, DFLAGS,
        RK3399_CLKGATE_CON(10), 6, GFLAGS),
    /* i2s */
    COMPOSITE(
        SCLK_I2S0_DIV, "clock_i2s0_div", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(28), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(8), 3,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s0_frac", "clock_i2s0_div", 0, RK3399_CLKSEL_CON(96), 0, RK3399_CLKGATE_CON(8), 4, GFLAGS, &rk3399_i2s0_fracmux,
        RK3399_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S0_8CH, "clock_i2s0", "clock_i2s0_mux", CLK_SET_RATE_PARENT, RK3399_CLKGATE_CON(8), 5, GFLAGS),

    COMPOSITE(
        SCLK_I2S1_DIV, "clock_i2s1_div", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(29), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(8), 6,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s1_frac", "clock_i2s1_div", 0, RK3399_CLKSEL_CON(97), 0, RK3399_CLKGATE_CON(8), 7, GFLAGS, &rk3399_i2s1_fracmux,
        RK3399_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S1_8CH, "clock_i2s1", "clock_i2s1_mux", CLK_SET_RATE_PARENT, RK3399_CLKGATE_CON(8), 8, GFLAGS),

    COMPOSITE(
        SCLK_I2S2_DIV, "clock_i2s2_div", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(30), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(8), 9,
        GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_i2s2_frac", "clock_i2s2_div", 0, RK3399_CLKSEL_CON(98), 0, RK3399_CLKGATE_CON(8), 10, GFLAGS, &rk3399_i2s2_fracmux,
        RK3399_I2S_FRAC_MAX_PRATE),
    GATE(SCLK_I2S2_8CH, "clock_i2s2", "clock_i2s2_mux", CLK_SET_RATE_PARENT, RK3399_CLKGATE_CON(8), 11, GFLAGS),

    MUX(SCLK_I2SOUT_SRC, "clock_i2sout_src", mux_i2sch_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(31), 0, 2, MFLAGS),
    COMPOSITE_NODIV(
        SCLK_I2S_8CH_OUT, "clock_i2sout", mux_i2sout_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(31), 2, 1, MFLAGS, RK3399_CLKGATE_CON(8), 12, GFLAGS),

    /* uart */
    MUX(SCLK_UART0_SRC, "clock_uart0_src", mux_pll_src_cpll_gpll_upll_p, 0, RK3399_CLKSEL_CON(33), 12, 2, MFLAGS),
    COMPOSITE_NOMUX(0, "clock_uart0_div", "clock_uart0_src", 0, RK3399_CLKSEL_CON(33), 0, 7, DFLAGS, RK3399_CLKGATE_CON(9), 0, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart0_frac", "clock_uart0_div", 0, RK3399_CLKSEL_CON(100), 0, RK3399_CLKGATE_CON(9), 1, GFLAGS, &rk3399_uart0_fracmux,
        RK3399_UART_FRAC_MAX_PRATE),

    MUX(SCLK_UART_SRC, "clock_uart_src", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(33), 15, 1, MFLAGS),
    COMPOSITE_NOMUX(0, "clock_uart1_div", "clock_uart_src", 0, RK3399_CLKSEL_CON(34), 0, 7, DFLAGS, RK3399_CLKGATE_CON(9), 2, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart1_frac", "clock_uart1_div", 0, RK3399_CLKSEL_CON(101), 0, RK3399_CLKGATE_CON(9), 3, GFLAGS, &rk3399_uart1_fracmux,
        RK3399_UART_FRAC_MAX_PRATE),

    COMPOSITE_NOMUX(0, "clock_uart2_div", "clock_uart_src", 0, RK3399_CLKSEL_CON(35), 0, 7, DFLAGS, RK3399_CLKGATE_CON(9), 4, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart2_frac", "clock_uart2_div", 0, RK3399_CLKSEL_CON(102), 0, RK3399_CLKGATE_CON(9), 5, GFLAGS, &rk3399_uart2_fracmux,
        RK3399_UART_FRAC_MAX_PRATE),

    COMPOSITE_NOMUX(0, "clock_uart3_div", "clock_uart_src", 0, RK3399_CLKSEL_CON(36), 0, 7, DFLAGS, RK3399_CLKGATE_CON(9), 6, GFLAGS),
    COMPOSITE_FRACMUX(
        0, "clock_uart3_frac", "clock_uart3_div", 0, RK3399_CLKSEL_CON(103), 0, RK3399_CLKGATE_CON(9), 7, GFLAGS, &rk3399_uart3_fracmux,
        RK3399_UART_FRAC_MAX_PRATE),

    COMPOSITE(
        PCLK_DDR, "pclock_ddr", mux_pll_src_cpll_gpll_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(6), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(3),
        4, GFLAGS),

    GATE(PCLK_CENTER_MAIN_NOC, "pclock_center_main_noc", "pclock_ddr", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(18), 10, GFLAGS),
    GATE(PCLK_DDR_MON, "pclock_ddr_mon", "pclock_ddr", 0, RK3399_CLKGATE_CON(18), 12, GFLAGS),
    GATE(PCLK_CIC, "pclock_cic", "pclock_ddr", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(18), 15, GFLAGS),
    GATE(PCLK_DDR_SGRF, "pclock_ddr_sgrf", "pclock_ddr", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(19), 2, GFLAGS),

    GATE(SCLK_PVTM_DDR, "clock_pvtm_ddr", "xin24m", 0, RK3399_CLKGATE_CON(4), 11, GFLAGS),
    GATE(SCLK_DFIMON0_TIMER, "clock_dfimon0_timer", "xin24m", 0, RK3399_CLKGATE_CON(3), 5, GFLAGS),
    GATE(SCLK_DFIMON1_TIMER, "clock_dfimon1_timer", "xin24m", 0, RK3399_CLKGATE_CON(3), 6, GFLAGS),

    /* cci */
    GATE(0, "cpll_aclock_cci_src", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 0, GFLAGS),
    GATE(0, "gpll_aclock_cci_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 1, GFLAGS),
    GATE(0, "npll_aclock_cci_src", "npll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 2, GFLAGS),
    GATE(0, "vpll_aclock_cci_src", "vpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 3, GFLAGS),

    COMPOSITE(
        0, "aclock_cci_pre", mux_aclock_cci_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(5), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(2), 4, GFLAGS),

    GATE(ACLK_ADB400M_PD_CORE_L, "aclock_adb400m_pd_core_l", "aclock_cci_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 0, GFLAGS),
    GATE(ACLK_ADB400M_PD_CORE_B, "aclock_adb400m_pd_core_b", "aclock_cci_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 1, GFLAGS),
    GATE(ACLK_CCI, "aclock_cci", "aclock_cci_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 2, GFLAGS),
    GATE(ACLK_CCI_NOC0, "aclock_cci_noc0", "aclock_cci_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 3, GFLAGS),
    GATE(ACLK_CCI_NOC1, "aclock_cci_noc1", "aclock_cci_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 4, GFLAGS),
    GATE(ACLK_CCI_GRF, "aclock_cci_grf", "aclock_cci_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 7, GFLAGS),

    GATE(0, "cpll_cci_trace", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 5, GFLAGS),
    GATE(0, "gpll_cci_trace", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 6, GFLAGS),
    COMPOSITE(
        SCLK_CCI_TRACE, "clock_cci_trace", mux_cci_trace_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(5), 15, 1, MFLAGS, 8, 5, DFLAGS,
        RK3399_CLKGATE_CON(2), 7, GFLAGS),

    GATE(0, "cpll_cs", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 8, GFLAGS),
    GATE(0, "gpll_cs", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 9, GFLAGS),
    GATE(0, "npll_cs", "npll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(2), 10, GFLAGS),
    COMPOSITE_NOGATE(SCLK_CS, "clock_cs", mux_cs_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(4), 6, 2, MFLAGS, 0, 5, DFLAGS),
    GATE(0, "clock_dbg_cxcs", "clock_cs", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 5, GFLAGS),
    GATE(0, "clock_dbg_noc", "clock_cs", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(15), 6, GFLAGS),

    /* vcodec */
    COMPOSITE(
        0, "aclock_vcodec_pre", mux_pll_src_cpll_gpll_npll_ppll_p, 0, RK3399_CLKSEL_CON(7), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(4), 0,
        GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_vcodec_pre", "aclock_vcodec_pre", 0, RK3399_CLKSEL_CON(7), 8, 5, DFLAGS, RK3399_CLKGATE_CON(4), 1, GFLAGS),
    GATE(HCLK_VCODEC, "hclock_vcodec", "hclock_vcodec_pre", 0, RK3399_CLKGATE_CON(17), 2, GFLAGS),
    GATE(0, "hclock_vcodec_noc", "hclock_vcodec_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(17), 3, GFLAGS),

    GATE(ACLK_VCODEC, "aclock_vcodec", "aclock_vcodec_pre", 0, RK3399_CLKGATE_CON(17), 0, GFLAGS),
    GATE(0, "aclock_vcodec_noc", "aclock_vcodec_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(17), 1, GFLAGS),

    /* vdu */
    COMPOSITE(
        SCLK_VDU_CORE, "clock_vdu_core", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(9), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(4), 4,
        GFLAGS),
    COMPOSITE(
        SCLK_VDU_CA, "clock_vdu_ca", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(9), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(4), 5,
        GFLAGS),

    COMPOSITE(
        0, "aclock_vdu_pre", mux_pll_src_cpll_gpll_npll_ppll_p, 0, RK3399_CLKSEL_CON(8), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(4), 2,
        GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_vdu_pre", "aclock_vdu_pre", 0, RK3399_CLKSEL_CON(8), 8, 5, DFLAGS, RK3399_CLKGATE_CON(4), 3, GFLAGS),
    GATE(HCLK_VDU, "hclock_vdu", "hclock_vdu_pre", 0, RK3399_CLKGATE_CON(17), 10, GFLAGS),
    GATE(HCLK_VDU_NOC, "hclock_vdu_noc", "hclock_vdu_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(17), 11, GFLAGS),

    GATE(ACLK_VDU, "aclock_vdu", "aclock_vdu_pre", 0, RK3399_CLKGATE_CON(17), 8, GFLAGS),
    GATE(ACLK_VDU_NOC, "aclock_vdu_noc", "aclock_vdu_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(17), 9, GFLAGS),

    /* iep */
    COMPOSITE(
        0, "aclock_iep_pre", mux_pll_src_cpll_gpll_npll_ppll_p, 0, RK3399_CLKSEL_CON(10), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(4), 6,
        GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_iep_pre", "aclock_iep_pre", 0, RK3399_CLKSEL_CON(10), 8, 5, DFLAGS, RK3399_CLKGATE_CON(4), 7, GFLAGS),
    GATE(HCLK_IEP, "hclock_iep", "hclock_iep_pre", 0, RK3399_CLKGATE_CON(16), 2, GFLAGS),
    GATE(HCLK_IEP_NOC, "hclock_iep_noc", "hclock_iep_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(16), 3, GFLAGS),

    GATE(ACLK_IEP, "aclock_iep", "aclock_iep_pre", 0, RK3399_CLKGATE_CON(16), 0, GFLAGS),
    GATE(ACLK_IEP_NOC, "aclock_iep_noc", "aclock_iep_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(16), 1, GFLAGS),

    /* rga */
    COMPOSITE(
        SCLK_RGA_CORE, "clock_rga_core", mux_pll_src_cpll_gpll_npll_ppll_p, 0, RK3399_CLKSEL_CON(12), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(4), 10, GFLAGS),

    COMPOSITE(
        0, "aclock_rga_pre", mux_pll_src_cpll_gpll_npll_ppll_p, 0, RK3399_CLKSEL_CON(11), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(4), 8,
        GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_rga_pre", "aclock_rga_pre", 0, RK3399_CLKSEL_CON(11), 8, 5, DFLAGS, RK3399_CLKGATE_CON(4), 9, GFLAGS),
    GATE(HCLK_RGA, "hclock_rga", "hclock_rga_pre", 0, RK3399_CLKGATE_CON(16), 10, GFLAGS),
    GATE(HCLK_RGA_NOC, "hclock_rga_noc", "hclock_rga_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(16), 11, GFLAGS),

    GATE(ACLK_RGA, "aclock_rga", "aclock_rga_pre", 0, RK3399_CLKGATE_CON(16), 8, GFLAGS),
    GATE(ACLK_RGA_NOC, "aclock_rga_noc", "aclock_rga_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(16), 9, GFLAGS),

    /* center */
    COMPOSITE(
        ACLK_CENTER, "aclock_center", mux_pll_src_cpll_gpll_npll_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(12), 14, 2, MFLAGS, 8, 5, DFLAGS,
        RK3399_CLKGATE_CON(3), 7, GFLAGS),
    GATE(ACLK_CENTER_MAIN_NOC, "aclock_center_main_noc", "aclock_center", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(19), 0, GFLAGS),
    GATE(ACLK_CENTER_PERI_NOC, "aclock_center_peri_noc", "aclock_center", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(19), 1, GFLAGS),

    /* gpu */
    COMPOSITE(
        0, "aclock_gpu_pre", mux_pll_src_ppll_cpll_gpll_npll_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(13), 5, 3, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(13), 0, GFLAGS),
    GATE(ACLK_GPU, "aclock_gpu", "aclock_gpu_pre", 0, RK3399_CLKGATE_CON(30), 8, GFLAGS),
    GATE(ACLK_PERF_GPU, "aclock_perf_gpu", "aclock_gpu_pre", 0, RK3399_CLKGATE_CON(30), 10, GFLAGS),
    GATE(ACLK_GPU_GRF, "aclock_gpu_grf", "aclock_gpu_pre", 0, RK3399_CLKGATE_CON(30), 11, GFLAGS),
    GATE(SCLK_PVTM_GPU, "aclock_pvtm_gpu", "xin24m", 0, RK3399_CLKGATE_CON(13), 1, GFLAGS),

    /* perihp */
    GATE(0, "cpll_aclock_perihp_src", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(5), 1, GFLAGS),
    GATE(0, "gpll_aclock_perihp_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(5), 0, GFLAGS),
    COMPOSITE(
        ACLK_PERIHP, "aclock_perihp", mux_aclock_perihp_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(14), 7, 1, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(5), 2, GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_PERIHP, "hclock_perihp", "aclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(14), 8, 2, DFLAGS, RK3399_CLKGATE_CON(5), 3, GFLAGS),
    COMPOSITE_NOMUX(
        PCLK_PERIHP, "pclock_perihp", "aclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(14), 12, 3, DFLAGS, RK3399_CLKGATE_CON(5), 4, GFLAGS),

    GATE(ACLK_PERF_PCIE, "aclock_perf_pcie", "aclock_perihp", 0, RK3399_CLKGATE_CON(20), 2, GFLAGS),
    GATE(ACLK_PCIE, "aclock_pcie", "aclock_perihp", 0, RK3399_CLKGATE_CON(20), 10, GFLAGS),
    GATE(0, "aclock_perihp_noc", "aclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(20), 12, GFLAGS),

    GATE(HCLK_HOST0, "hclock_host0", "hclock_perihp", 0, RK3399_CLKGATE_CON(20), 5, GFLAGS),
    GATE(HCLK_HOST0_ARB, "hclock_host0_arb", "hclock_perihp", 0, RK3399_CLKGATE_CON(20), 6, GFLAGS),
    GATE(HCLK_HOST1, "hclock_host1", "hclock_perihp", 0, RK3399_CLKGATE_CON(20), 7, GFLAGS),
    GATE(HCLK_HOST1_ARB, "hclock_host1_arb", "hclock_perihp", 0, RK3399_CLKGATE_CON(20), 8, GFLAGS),
    GATE(HCLK_HSIC, "hclock_hsic", "hclock_perihp", 0, RK3399_CLKGATE_CON(20), 9, GFLAGS),
    GATE(0, "hclock_perihp_noc", "hclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(20), 13, GFLAGS),
    GATE(0, "hclock_ahb1tom", "hclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(20), 15, GFLAGS),

    GATE(PCLK_PERIHP_GRF, "pclock_perihp_grf", "pclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(20), 4, GFLAGS),
    GATE(PCLK_PCIE, "pclock_pcie", "pclock_perihp", 0, RK3399_CLKGATE_CON(20), 11, GFLAGS),
    GATE(0, "pclock_perihp_noc", "pclock_perihp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(20), 14, GFLAGS),
    GATE(PCLK_HSICPHY, "pclock_hsicphy", "pclock_perihp", 0, RK3399_CLKGATE_CON(31), 8, GFLAGS),

    /* sdio & sdmmc */
    COMPOSITE(
        HCLK_SD, "hclock_sd", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(13), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(12), 13, GFLAGS),
    GATE(HCLK_SDMMC, "hclock_sdmmc", "hclock_sd", 0, RK3399_CLKGATE_CON(33), 8, GFLAGS),
    GATE(0, "hclock_sdmmc_noc", "hclock_sd", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 9, GFLAGS),

    COMPOSITE(
        SCLK_SDIO, "clock_sdio", mux_pll_src_cpll_gpll_npll_ppll_upll_24m_p, 0, RK3399_CLKSEL_CON(15), 8, 3, MFLAGS, 0, 7, DFLAGS,
        RK3399_CLKGATE_CON(6), 0, GFLAGS),

    COMPOSITE(
        SCLK_SDMMC, "clock_sdmmc", mux_pll_src_cpll_gpll_npll_ppll_upll_24m_p, 0, RK3399_CLKSEL_CON(16), 8, 3, MFLAGS, 0, 7, DFLAGS,
        RK3399_CLKGATE_CON(6), 1, GFLAGS),

    MMC(SCLK_SDMMC_DRV, "sdmmc_drv", "clock_sdmmc", RK3399_SDMMC_CON0, 1),
    MMC(SCLK_SDMMC_SAMPLE, "sdmmc_sample", "clock_sdmmc", RK3399_SDMMC_CON1, 1),

    MMC(SCLK_SDIO_DRV, "sdio_drv", "clock_sdio", RK3399_SDIO_CON0, 1),
    MMC(SCLK_SDIO_SAMPLE, "sdio_sample", "clock_sdio", RK3399_SDIO_CON1, 1),

    /* pcie */
    COMPOSITE(
        SCLK_PCIE_PM, "clock_pcie_pm", mux_pll_src_cpll_gpll_npll_24m_p, 0, RK3399_CLKSEL_CON(17), 8, 3, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(6),
        2, GFLAGS),

    COMPOSITE_NOMUX(
        SCLK_PCIEPHY_REF100M, "clock_pciephy_ref100m", "npll", 0, RK3399_CLKSEL_CON(18), 11, 5, DFLAGS, RK3399_CLKGATE_CON(12), 6, GFLAGS),
    MUX(SCLK_PCIEPHY_REF, "clock_pciephy_ref", mux_pll_src_24m_pciephy_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(18), 10, 1, MFLAGS),

    COMPOSITE(
        0, "clock_pcie_core_cru", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(18), 8, 2, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(6), 3,
        GFLAGS),
    MUX(SCLK_PCIE_CORE, "clock_pcie_core", mux_pciecore_cru_phy_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(18), 7, 1, MFLAGS),

    /* emmc */
    COMPOSITE(
        SCLK_EMMC, "clock_emmc", mux_pll_src_cpll_gpll_npll_upll_24m_p, 0, RK3399_CLKSEL_CON(22), 8, 3, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(6),
        14, GFLAGS),

    GATE(0, "cpll_aclock_emmc_src", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(6), 13, GFLAGS),
    GATE(0, "gpll_aclock_emmc_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(6), 12, GFLAGS),
    COMPOSITE_NOGATE(ACLK_EMMC, "aclock_emmc", mux_aclock_emmc_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(21), 7, 1, MFLAGS, 0, 5, DFLAGS),
    GATE(ACLK_EMMC_CORE, "aclock_emmccore", "aclock_emmc", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(32), 8, GFLAGS),
    GATE(ACLK_EMMC_NOC, "aclock_emmc_noc", "aclock_emmc", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(32), 9, GFLAGS),
    GATE(ACLK_EMMC_GRF, "aclock_emmcgrf", "aclock_emmc", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(32), 10, GFLAGS),

    /* perilp0 */
    GATE(0, "cpll_aclock_perilp0_src", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(7), 1, GFLAGS),
    GATE(0, "gpll_aclock_perilp0_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(7), 0, GFLAGS),
    COMPOSITE(
        ACLK_PERILP0, "aclock_perilp0", mux_aclock_perilp0_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(23), 7, 1, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(7), 2, GFLAGS),
    COMPOSITE_NOMUX(
        HCLK_PERILP0, "hclock_perilp0", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(23), 8, 2, DFLAGS, RK3399_CLKGATE_CON(7), 3, GFLAGS),
    COMPOSITE_NOMUX(PCLK_PERILP0, "pclock_perilp0", "aclock_perilp0", 0, RK3399_CLKSEL_CON(23), 12, 3, DFLAGS, RK3399_CLKGATE_CON(7), 4, GFLAGS),

    /* aclock_perilp0 gates */
    GATE(ACLK_INTMEM, "aclock_intmem", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 0, GFLAGS),
    GATE(ACLK_TZMA, "aclock_tzma", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 1, GFLAGS),
    GATE(SCLK_INTMEM0, "clock_intmem0", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 2, GFLAGS),
    GATE(SCLK_INTMEM1, "clock_intmem1", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 3, GFLAGS),
    GATE(SCLK_INTMEM2, "clock_intmem2", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 4, GFLAGS),
    GATE(SCLK_INTMEM3, "clock_intmem3", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 5, GFLAGS),
    GATE(SCLK_INTMEM4, "clock_intmem4", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 6, GFLAGS),
    GATE(SCLK_INTMEM5, "clock_intmem5", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(23), 7, GFLAGS),
    GATE(ACLK_DCF, "aclock_dcf", "aclock_perilp0", 0, RK3399_CLKGATE_CON(23), 8, GFLAGS),
    GATE(ACLK_DMAC0_PERILP, "aclock_dmac0_perilp", "aclock_perilp0", 0, RK3399_CLKGATE_CON(25), 5, GFLAGS),
    GATE(ACLK_DMAC1_PERILP, "aclock_dmac1_perilp", "aclock_perilp0", 0, RK3399_CLKGATE_CON(25), 6, GFLAGS),
    GATE(ACLK_PERILP0_NOC, "aclock_perilp0_noc", "aclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(25), 7, GFLAGS),

    /* hclock_perilp0 gates */
    GATE(HCLK_ROM, "hclock_rom", "hclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(24), 4, GFLAGS),
    GATE(HCLK_M_CRYPTO0, "hclock_m_crypto0", "hclock_perilp0", 0, RK3399_CLKGATE_CON(24), 5, GFLAGS),
    GATE(HCLK_S_CRYPTO0, "hclock_s_crypto0", "hclock_perilp0", 0, RK3399_CLKGATE_CON(24), 6, GFLAGS),
    GATE(HCLK_M_CRYPTO1, "hclock_m_crypto1", "hclock_perilp0", 0, RK3399_CLKGATE_CON(24), 14, GFLAGS),
    GATE(HCLK_S_CRYPTO1, "hclock_s_crypto1", "hclock_perilp0", 0, RK3399_CLKGATE_CON(24), 15, GFLAGS),
    GATE(HCLK_PERILP0_NOC, "hclock_perilp0_noc", "hclock_perilp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(25), 8, GFLAGS),

    /* pclock_perilp0 gates */
    GATE(PCLK_DCF, "pclock_dcf", "pclock_perilp0", 0, RK3399_CLKGATE_CON(23), 9, GFLAGS),

    /* crypto */
    COMPOSITE(
        SCLK_CRYPTO0, "clock_crypto0", mux_pll_src_cpll_gpll_ppll_p, 0, RK3399_CLKSEL_CON(24), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(7), 7,
        GFLAGS),

    COMPOSITE(
        SCLK_CRYPTO1, "clock_crypto1", mux_pll_src_cpll_gpll_ppll_p, 0, RK3399_CLKSEL_CON(26), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(7), 8,
        GFLAGS),

    /* cm0s_perilp */
    GATE(0, "cpll_fclock_cm0s_src", "cpll", 0, RK3399_CLKGATE_CON(7), 6, GFLAGS),
    GATE(0, "gpll_fclock_cm0s_src", "gpll", 0, RK3399_CLKGATE_CON(7), 5, GFLAGS),
    COMPOSITE(FCLK_CM0S, "fclock_cm0s", mux_fclock_cm0s_p, 0, RK3399_CLKSEL_CON(24), 15, 1, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(7), 9, GFLAGS),

    /* fclock_cm0s gates */
    GATE(SCLK_M0_PERILP, "sclock_m0_perilp", "fclock_cm0s", 0, RK3399_CLKGATE_CON(24), 8, GFLAGS),
    GATE(HCLK_M0_PERILP, "hclock_m0_perilp", "fclock_cm0s", 0, RK3399_CLKGATE_CON(24), 9, GFLAGS),
    GATE(DCLK_M0_PERILP, "dclock_m0_perilp", "fclock_cm0s", 0, RK3399_CLKGATE_CON(24), 10, GFLAGS),
    GATE(SCLK_M0_PERILP_DEC, "clock_m0_perilp_dec", "fclock_cm0s", 0, RK3399_CLKGATE_CON(24), 11, GFLAGS),
    GATE(HCLK_M0_PERILP_NOC, "hclock_m0_perilp_noc", "fclock_cm0s", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(25), 11, GFLAGS),

    /* perilp1 */
    GATE(0, "cpll_hclock_perilp1_src", "cpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(8), 1, GFLAGS),
    GATE(0, "gpll_hclock_perilp1_src", "gpll", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(8), 0, GFLAGS),
    COMPOSITE_NOGATE(HCLK_PERILP1, "hclock_perilp1", mux_hclock_perilp1_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(25), 7, 1, MFLAGS, 0, 5, DFLAGS),
    COMPOSITE_NOMUX(
        PCLK_PERILP1, "pclock_perilp1", "hclock_perilp1", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(25), 8, 3, DFLAGS, RK3399_CLKGATE_CON(8), 2, GFLAGS),

    /* hclock_perilp1 gates */
    GATE(0, "hclock_perilp1_noc", "hclock_perilp1", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(25), 9, GFLAGS),
    GATE(0, "hclock_sdio_noc", "hclock_perilp1", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(25), 12, GFLAGS),
    GATE(HCLK_I2S0_8CH, "hclock_i2s0", "hclock_perilp1", 0, RK3399_CLKGATE_CON(34), 0, GFLAGS),
    GATE(HCLK_I2S1_8CH, "hclock_i2s1", "hclock_perilp1", 0, RK3399_CLKGATE_CON(34), 1, GFLAGS),
    GATE(HCLK_I2S2_8CH, "hclock_i2s2", "hclock_perilp1", 0, RK3399_CLKGATE_CON(34), 2, GFLAGS),
    GATE(HCLK_SPDIF, "hclock_spdif", "hclock_perilp1", 0, RK3399_CLKGATE_CON(34), 3, GFLAGS),
    GATE(HCLK_SDIO, "hclock_sdio", "hclock_perilp1", 0, RK3399_CLKGATE_CON(34), 4, GFLAGS),
    GATE(PCLK_SPI5, "pclock_spi5", "hclock_perilp1", 0, RK3399_CLKGATE_CON(34), 5, GFLAGS),
    GATE(0, "hclock_sdioaudio_noc", "hclock_perilp1", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(34), 6, GFLAGS),

    /* pclock_perilp1 gates */
    GATE(PCLK_UART0, "pclock_uart0", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 0, GFLAGS),
    GATE(PCLK_UART1, "pclock_uart1", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 1, GFLAGS),
    GATE(PCLK_UART2, "pclock_uart2", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 2, GFLAGS),
    GATE(PCLK_UART3, "pclock_uart3", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 3, GFLAGS),
    GATE(PCLK_I2C7, "pclock_rki2c7", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 5, GFLAGS),
    GATE(PCLK_I2C1, "pclock_rki2c1", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 6, GFLAGS),
    GATE(PCLK_I2C5, "pclock_rki2c5", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 7, GFLAGS),
    GATE(PCLK_I2C6, "pclock_rki2c6", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 8, GFLAGS),
    GATE(PCLK_I2C2, "pclock_rki2c2", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 9, GFLAGS),
    GATE(PCLK_I2C3, "pclock_rki2c3", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 10, GFLAGS),
    GATE(PCLK_MAILBOX0, "pclock_mailbox0", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 11, GFLAGS),
    GATE(PCLK_SARADC, "pclock_saradc", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 12, GFLAGS),
    GATE(PCLK_TSADC, "pclock_tsadc", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 13, GFLAGS),
    GATE(PCLK_EFUSE1024NS, "pclock_efuse1024ns", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 14, GFLAGS),
    GATE(PCLK_EFUSE1024S, "pclock_efuse1024s", "pclock_perilp1", 0, RK3399_CLKGATE_CON(22), 15, GFLAGS),
    GATE(PCLK_SPI0, "pclock_spi0", "pclock_perilp1", 0, RK3399_CLKGATE_CON(23), 10, GFLAGS),
    GATE(PCLK_SPI1, "pclock_spi1", "pclock_perilp1", 0, RK3399_CLKGATE_CON(23), 11, GFLAGS),
    GATE(PCLK_SPI2, "pclock_spi2", "pclock_perilp1", 0, RK3399_CLKGATE_CON(23), 12, GFLAGS),
    GATE(PCLK_SPI4, "pclock_spi4", "pclock_perilp1", 0, RK3399_CLKGATE_CON(23), 13, GFLAGS),
    GATE(PCLK_PERIHP_GRF, "pclock_perilp_sgrf", "pclock_perilp1", 0, RK3399_CLKGATE_CON(24), 13, GFLAGS),
    GATE(0, "pclock_perilp1_noc", "pclock_perilp1", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(25), 10, GFLAGS),

    /* saradc */
    COMPOSITE_NOMUX(SCLK_SARADC, "clock_saradc", "xin24m", 0, RK3399_CLKSEL_CON(26), 8, 8, DFLAGS, RK3399_CLKGATE_CON(9), 11, GFLAGS),

    /* tsadc */
    COMPOSITE(SCLK_TSADC, "clock_tsadc", mux_pll_p, 0, RK3399_CLKSEL_CON(27), 15, 1, MFLAGS, 0, 10, DFLAGS, RK3399_CLKGATE_CON(9), 10, GFLAGS),

    /* cif_testout */
    MUX(0, "clock_testout1_pll_src", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(38), 6, 2, MFLAGS),
    COMPOSITE(
        SCLK_TESTCLKOUT1, "clock_testout1", mux_clock_testout1_p, 0, RK3399_CLKSEL_CON(38), 5, 1, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(13), 14,
        GFLAGS),

    MUX(0, "clock_testout2_pll_src", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(38), 14, 2, MFLAGS),
    COMPOSITE(
        SCLK_TESTCLKOUT2, "clock_testout2", mux_clock_testout2_p, 0, RK3399_CLKSEL_CON(38), 13, 1, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(13), 15,
        GFLAGS),

    /* vio */
    COMPOSITE(
        ACLK_VIO, "aclock_vio", mux_pll_src_cpll_gpll_ppll_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(42), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(11), 0, GFLAGS),
    COMPOSITE_NOMUX(PCLK_VIO, "pclock_vio", "aclock_vio", 0, RK3399_CLKSEL_CON(43), 0, 5, DFLAGS, RK3399_CLKGATE_CON(11), 1, GFLAGS),

    GATE(ACLK_VIO_NOC, "aclock_vio_noc", "aclock_vio", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(29), 0, GFLAGS),

    GATE(PCLK_MIPI_DSI0, "pclock_mipi_dsi0", "pclock_vio", 0, RK3399_CLKGATE_CON(29), 1, GFLAGS),
    GATE(PCLK_MIPI_DSI1, "pclock_mipi_dsi1", "pclock_vio", 0, RK3399_CLKGATE_CON(29), 2, GFLAGS),
    GATE(PCLK_VIO_GRF, "pclock_vio_grf", "pclock_vio", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(29), 12, GFLAGS),

    /* hdcp */
    COMPOSITE_NOGATE(ACLK_HDCP, "aclock_hdcp", mux_pll_src_cpll_gpll_ppll_p, 0, RK3399_CLKSEL_CON(42), 14, 2, MFLAGS, 8, 5, DFLAGS),
    COMPOSITE_NOMUX(HCLK_HDCP, "hclock_hdcp", "aclock_hdcp", 0, RK3399_CLKSEL_CON(43), 5, 5, DFLAGS, RK3399_CLKGATE_CON(11), 3, GFLAGS),
    COMPOSITE_NOMUX(PCLK_HDCP, "pclock_hdcp", "aclock_hdcp", 0, RK3399_CLKSEL_CON(43), 10, 5, DFLAGS, RK3399_CLKGATE_CON(11), 10, GFLAGS),

    GATE(ACLK_HDCP_NOC, "aclock_hdcp_noc", "aclock_hdcp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(29), 4, GFLAGS),
    GATE(ACLK_HDCP22, "aclock_hdcp22", "aclock_hdcp", 0, RK3399_CLKGATE_CON(29), 10, GFLAGS),

    GATE(HCLK_HDCP_NOC, "hclock_hdcp_noc", "hclock_hdcp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(29), 5, GFLAGS),
    GATE(HCLK_HDCP22, "hclock_hdcp22", "hclock_hdcp", 0, RK3399_CLKGATE_CON(29), 9, GFLAGS),

    GATE(PCLK_HDCP_NOC, "pclock_hdcp_noc", "pclock_hdcp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(29), 3, GFLAGS),
    GATE(PCLK_HDMI_CTRL, "pclock_hdmi_ctrl", "pclock_hdcp", 0, RK3399_CLKGATE_CON(29), 6, GFLAGS),
    GATE(PCLK_DP_CTRL, "pclock_dp_ctrl", "pclock_hdcp", 0, RK3399_CLKGATE_CON(29), 7, GFLAGS),
    GATE(PCLK_HDCP22, "pclock_hdcp22", "pclock_hdcp", 0, RK3399_CLKGATE_CON(29), 8, GFLAGS),
    GATE(PCLK_GASKET, "pclock_gasket", "pclock_hdcp", 0, RK3399_CLKGATE_CON(29), 11, GFLAGS),

    /* edp */
    COMPOSITE(
        SCLK_DP_CORE, "clock_dp_core", mux_pll_src_npll_cpll_gpll_p, 0, RK3399_CLKSEL_CON(46), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(11), 8,
        GFLAGS),

    COMPOSITE(
        PCLK_EDP, "pclock_edp", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(44), 15, 1, MFLAGS, 8, 6, DFLAGS, RK3399_CLKGATE_CON(11), 11, GFLAGS),
    GATE(PCLK_EDP_NOC, "pclock_edp_noc", "pclock_edp", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(32), 12, GFLAGS),
    GATE(PCLK_EDP_CTRL, "pclock_edp_ctrl", "pclock_edp", 0, RK3399_CLKGATE_CON(32), 13, GFLAGS),

    /* hdmi */
    GATE(SCLK_HDMI_SFR, "clock_hdmi_sfr", "xin24m", 0, RK3399_CLKGATE_CON(11), 6, GFLAGS),

    COMPOSITE(SCLK_HDMI_CEC, "clock_hdmi_cec", mux_pll_p, 0, RK3399_CLKSEL_CON(45), 15, 1, MFLAGS, 0, 10, DFLAGS, RK3399_CLKGATE_CON(11), 7, GFLAGS),

    /* vop0 */
    COMPOSITE(
        ACLK_VOP0_PRE, "aclock_vop0_pre", mux_pll_src_dmyvpll_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(47), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(10), 8, GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_vop0_pre", "aclock_vop0_pre", 0, RK3399_CLKSEL_CON(47), 8, 5, DFLAGS, RK3399_CLKGATE_CON(10), 9, GFLAGS),

    GATE(ACLK_VOP0, "aclock_vop0", "aclock_vop0_pre", 0, RK3399_CLKGATE_CON(28), 3, GFLAGS),
    GATE(ACLK_VOP0_NOC, "aclock_vop0_noc", "aclock_vop0_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(28), 1, GFLAGS),

    GATE(HCLK_VOP0, "hclock_vop0", "hclock_vop0_pre", 0, RK3399_CLKGATE_CON(28), 2, GFLAGS),
    GATE(HCLK_VOP0_NOC, "hclock_vop0_noc", "hclock_vop0_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(28), 0, GFLAGS),

#ifdef RK3399_TWO_PLL_FOR_VOP
    COMPOSITE(
        DCLK_VOP0_DIV, "dclock_vop0_div", mux_pll_src_vpll_cpll_gpll_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3399_CLKSEL_CON(49), 8, 2,
        MFLAGS, 0, 8, DFLAGS, RK3399_CLKGATE_CON(10), 12, GFLAGS),
#else
    COMPOSITE(
        DCLK_VOP0_DIV, "dclock_vop0_div", mux_pll_src_vpll_cpll_gpll_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(49), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3399_CLKGATE_CON(10), 12, GFLAGS),
#endif

    /* The VOP0 is main screen, it is able to re-set parent rate. */
    COMPOSITE_FRACMUX_NOGATE(
        DCLK_VOP0_FRAC, "dclock_vop0_frac", "dclock_vop0_div", 0, RK3399_CLKSEL_CON(106), 0, &rk3399_dclock_vop0_fracmux, RK3399_VOP_FRAC_MAX_PRATE),

    COMPOSITE(
        SCLK_VOP0_PWM, "clock_vop0_pwm", mux_pll_src_dmyvpll_cpll_gpll_gpll_p, 0, RK3399_CLKSEL_CON(51), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(10), 14, GFLAGS),

    /* vop1 */
    COMPOSITE(
        ACLK_VOP1_PRE, "aclock_vop1_pre", mux_pll_src_dmyvpll_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(48), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(10), 10, GFLAGS),
    COMPOSITE_NOMUX(0, "hclock_vop1_pre", "aclock_vop1_pre", 0, RK3399_CLKSEL_CON(48), 8, 5, DFLAGS, RK3399_CLKGATE_CON(10), 11, GFLAGS),

    GATE(ACLK_VOP1, "aclock_vop1", "aclock_vop1_pre", 0, RK3399_CLKGATE_CON(28), 7, GFLAGS),
    GATE(ACLK_VOP1_NOC, "aclock_vop1_noc", "aclock_vop1_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(28), 5, GFLAGS),

    GATE(HCLK_VOP1, "hclock_vop1", "hclock_vop1_pre", 0, RK3399_CLKGATE_CON(28), 6, GFLAGS),
    GATE(HCLK_VOP1_NOC, "hclock_vop1_noc", "hclock_vop1_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(28), 4, GFLAGS),

/* The VOP1 is sub screen, it is note able to re-set parent rate. */
#ifdef RK3399_TWO_PLL_FOR_VOP
    COMPOSITE(
        DCLK_VOP1_DIV, "dclock_vop1_div", mux_pll_src_vpll_cpll_gpll_p, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, RK3399_CLKSEL_CON(50), 8, 2,
        MFLAGS, 0, 8, DFLAGS, RK3399_CLKGATE_CON(10), 13, GFLAGS),
#else
    COMPOSITE(
        DCLK_VOP1_DIV, "dclock_vop1_div", mux_pll_src_dmyvpll_cpll_gpll_p, 0, RK3399_CLKSEL_CON(50), 8, 2, MFLAGS, 0, 8, DFLAGS,
        RK3399_CLKGATE_CON(10), 13, GFLAGS),
#endif

    COMPOSITE_FRACMUX_NOGATE(
        DCLK_VOP1_FRAC, "dclock_vop1_frac", "dclock_vop1_div", 0, RK3399_CLKSEL_CON(107), 0, &rk3399_dclock_vop1_fracmux, RK3399_VOP_FRAC_MAX_PRATE),

    COMPOSITE(
        SCLK_VOP1_PWM, "clock_vop1_pwm", mux_pll_src_dmyvpll_cpll_gpll_gpll_p, 0, RK3399_CLKSEL_CON(52), 6, 2, MFLAGS, 0, 5, DFLAGS,
        RK3399_CLKGATE_CON(10), 15, GFLAGS),

    /* isp */
    COMPOSITE(
        ACLK_ISP0, "aclock_isp0", mux_pll_src_cpll_gpll_ppll_p, 0, RK3399_CLKSEL_CON(53), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(12), 8,
        GFLAGS),
    COMPOSITE_NOMUX(HCLK_ISP0, "hclock_isp0", "aclock_isp0", 0, RK3399_CLKSEL_CON(53), 8, 5, DFLAGS, RK3399_CLKGATE_CON(12), 9, GFLAGS),

    GATE(ACLK_ISP0_NOC, "aclock_isp0_noc", "aclock_isp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(27), 1, GFLAGS),
    GATE(ACLK_ISP0_WRAPPER, "aclock_isp0_wrapper", "aclock_isp0", 0, RK3399_CLKGATE_CON(27), 5, GFLAGS),
    GATE(HCLK_ISP1_WRAPPER, "hclock_isp1_wrapper", "aclock_isp0", 0, RK3399_CLKGATE_CON(27), 7, GFLAGS),

    GATE(HCLK_ISP0_NOC, "hclock_isp0_noc", "hclock_isp0", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(27), 0, GFLAGS),
    GATE(HCLK_ISP0_WRAPPER, "hclock_isp0_wrapper", "hclock_isp0", 0, RK3399_CLKGATE_CON(27), 4, GFLAGS),

    COMPOSITE(
        SCLK_ISP0, "clock_isp0", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(55), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(11), 4,
        GFLAGS),

    COMPOSITE(
        ACLK_ISP1, "aclock_isp1", mux_pll_src_cpll_gpll_ppll_p, 0, RK3399_CLKSEL_CON(54), 6, 2, MFLAGS, 0, 5, DFLAGS, RK3399_CLKGATE_CON(12), 10,
        GFLAGS),
    COMPOSITE_NOMUX(HCLK_ISP1, "hclock_isp1", "aclock_isp1", 0, RK3399_CLKSEL_CON(54), 8, 5, DFLAGS, RK3399_CLKGATE_CON(12), 11, GFLAGS),

    GATE(ACLK_ISP1_NOC, "aclock_isp1_noc", "aclock_isp1", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(27), 3, GFLAGS),

    GATE(HCLK_ISP1_NOC, "hclock_isp1_noc", "hclock_isp1", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(27), 2, GFLAGS),
    GATE(ACLK_ISP1_WRAPPER, "aclock_isp1_wrapper", "hclock_isp1", 0, RK3399_CLKGATE_CON(27), 8, GFLAGS),

    COMPOSITE(
        SCLK_ISP1, "clock_isp1", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(55), 14, 2, MFLAGS, 8, 5, DFLAGS, RK3399_CLKGATE_CON(11), 5,
        GFLAGS),

    /*
     * We use pclkin_cifinv by default GRF_SOC_CON20[9] (GSC20_9) setting in system,
     * so we ignore the mux and make clocks nodes as following,
     *
     * pclkin_cifinv --|-------\
     *                 |GSC20_9|-- pclkin_cifmux -- |G27_6| -- pclkin_isp1_wrapper
     * pclkin_cif    --|-------/
     */
    GATE(PCLK_ISP1_WRAPPER, "pclkin_isp1_wrapper", "pclkin_cif", 0, RK3399_CLKGATE_CON(27), 6, GFLAGS),

    /* cif */
    COMPOSITE_NODIV(
        SCLK_CIF_OUT_SRC, "clock_cifout_src", mux_pll_src_cpll_gpll_npll_p, 0, RK3399_CLKSEL_CON(56), 6, 2, MFLAGS, RK3399_CLKGATE_CON(10), 7,
        GFLAGS),

    COMPOSITE_NOGATE(SCLK_CIF_OUT, "clock_cifout", mux_clock_cif_p, 0, RK3399_CLKSEL_CON(56), 5, 1, MFLAGS, 0, 5, DFLAGS),

    /* gic */
    COMPOSITE(
        ACLK_GIC_PRE, "aclock_gic_pre", mux_pll_src_cpll_gpll_p, CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(56), 15, 1, MFLAGS, 8, 5, DFLAGS,
        RK3399_CLKGATE_CON(12), 12, GFLAGS),

    GATE(ACLK_GIC, "aclock_gic", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 0, GFLAGS),
    GATE(ACLK_GIC_NOC, "aclock_gic_noc", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 1, GFLAGS),
    GATE(ACLK_GIC_ADB400_CORE_L_2_GIC, "aclock_gic_adb400_core_l_2_gic", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 2, GFLAGS),
    GATE(ACLK_GIC_ADB400_CORE_B_2_GIC, "aclock_gic_adb400_core_b_2_gic", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 3, GFLAGS),
    GATE(ACLK_GIC_ADB400_GIC_2_CORE_L, "aclock_gic_adb400_gic_2_core_l", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 4, GFLAGS),
    GATE(ACLK_GIC_ADB400_GIC_2_CORE_B, "aclock_gic_adb400_gic_2_core_b", "aclock_gic_pre", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(33), 5, GFLAGS),

    /* alive */
    /* pclock_alive_gpll_src is controlled by PMUGRF_SOC_CON0[6] */
    DIV(PCLK_ALIVE, "pclock_alive", "gpll", 0, RK3399_CLKSEL_CON(57), 0, 5, DFLAGS),

    GATE(PCLK_USBPHY_MUX_G, "pclock_usbphy_mux_g", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(21), 4, GFLAGS),
    GATE(PCLK_UPHY0_TCPHY_G, "pclock_uphy0_tcphy_g", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(21), 5, GFLAGS),
    GATE(PCLK_UPHY0_TCPD_G, "pclock_uphy0_tcpd_g", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(21), 6, GFLAGS),
    GATE(PCLK_UPHY1_TCPHY_G, "pclock_uphy1_tcphy_g", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(21), 8, GFLAGS),
    GATE(PCLK_UPHY1_TCPD_G, "pclock_uphy1_tcpd_g", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(21), 9, GFLAGS),

    GATE(PCLK_GRF, "pclock_grf", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(31), 1, GFLAGS),
    GATE(PCLK_INTR_ARB, "pclock_intr_arb", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(31), 2, GFLAGS),
    GATE(PCLK_GPIO2, "pclock_gpio2", "pclock_alive", 0, RK3399_CLKGATE_CON(31), 3, GFLAGS),
    GATE(PCLK_GPIO3, "pclock_gpio3", "pclock_alive", 0, RK3399_CLKGATE_CON(31), 4, GFLAGS),
    GATE(PCLK_GPIO4, "pclock_gpio4", "pclock_alive", 0, RK3399_CLKGATE_CON(31), 5, GFLAGS),
    GATE(PCLK_TIMER0, "pclock_timer0", "pclock_alive", 0, RK3399_CLKGATE_CON(31), 6, GFLAGS),
    GATE(PCLK_TIMER1, "pclock_timer1", "pclock_alive", 0, RK3399_CLKGATE_CON(31), 7, GFLAGS),
    GATE(PCLK_PMU_INTR_ARB, "pclock_pmu_intr_arb", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(31), 9, GFLAGS),
    GATE(PCLK_SGRF, "pclock_sgrf", "pclock_alive", CLK_IGNORE_UNUSED, RK3399_CLKGATE_CON(31), 10, GFLAGS),

    GATE(SCLK_MIPIDPHY_REF, "clock_mipidphy_ref", "xin24m", 0, RK3399_CLKGATE_CON(11), 14, GFLAGS),
    GATE(SCLK_DPHY_PLL, "clock_dphy_pll", "clock_mipidphy_ref", 0, RK3399_CLKGATE_CON(21), 0, GFLAGS),

    GATE(SCLK_MIPIDPHY_CFG, "clock_mipidphy_cfg", "xin24m", 0, RK3399_CLKGATE_CON(11), 15, GFLAGS),
    GATE(SCLK_DPHY_TX0_CFG, "clock_dphy_tx0_cfg", "clock_mipidphy_cfg", 0, RK3399_CLKGATE_CON(21), 1, GFLAGS),
    GATE(SCLK_DPHY_TX1RX1_CFG, "clock_dphy_tx1rx1_cfg", "clock_mipidphy_cfg", 0, RK3399_CLKGATE_CON(21), 2, GFLAGS),
    GATE(SCLK_DPHY_RX0_CFG, "clock_dphy_rx0_cfg", "clock_mipidphy_cfg", 0, RK3399_CLKGATE_CON(21), 3, GFLAGS),

    /* testout */
    MUX(0, "clock_test_pre", mux_pll_src_cpll_gpll_p, CLK_SET_RATE_PARENT, RK3399_CLKSEL_CON(58), 7, 1, MFLAGS),
    COMPOSITE_FRAC(0, "clock_test_frac", "clock_test_pre", 0, RK3399_CLKSEL_CON(105), 0, RK3399_CLKGATE_CON(13), 9, GFLAGS, 0),

    DIV(0, "clock_test_24m", "xin24m", 0, RK3399_CLKSEL_CON(57), 6, 10, DFLAGS),

    /* spi */
    COMPOSITE(
        SCLK_SPI0, "clock_spi0", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(59), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(9), 12, GFLAGS),

    COMPOSITE(
        SCLK_SPI1, "clock_spi1", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(59), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3399_CLKGATE_CON(9), 13, GFLAGS),

    COMPOSITE(
        SCLK_SPI2, "clock_spi2", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(60), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(9), 14, GFLAGS),

    COMPOSITE(
        SCLK_SPI4, "clock_spi4", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(60), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3399_CLKGATE_CON(9), 15, GFLAGS),

    COMPOSITE(
        SCLK_SPI5, "clock_spi5", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(58), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3399_CLKGATE_CON(13), 13, GFLAGS),

    /* i2c */
    COMPOSITE(
        SCLK_I2C1, "clock_i2c1", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(61), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(10), 0, GFLAGS),

    COMPOSITE(
        SCLK_I2C2, "clock_i2c2", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(62), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(10), 2, GFLAGS),

    COMPOSITE(
        SCLK_I2C3, "clock_i2c3", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(63), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_CLKGATE_CON(10), 4, GFLAGS),

    COMPOSITE(
        SCLK_I2C5, "clock_i2c5", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(61), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3399_CLKGATE_CON(10), 1, GFLAGS),

    COMPOSITE(
        SCLK_I2C6, "clock_i2c6", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(62), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3399_CLKGATE_CON(10), 3, GFLAGS),

    COMPOSITE(
        SCLK_I2C7, "clock_i2c7", mux_pll_src_cpll_gpll_p, 0, RK3399_CLKSEL_CON(63), 15, 1, MFLAGS, 8, 7, DFLAGS, RK3399_CLKGATE_CON(10), 5, GFLAGS),

    /* timer */
    GATE(SCLK_TIMER00, "clock_timer00", "xin24m", 0, RK3399_CLKGATE_CON(26), 0, GFLAGS),
    GATE(SCLK_TIMER01, "clock_timer01", "xin24m", 0, RK3399_CLKGATE_CON(26), 1, GFLAGS),
    GATE(SCLK_TIMER02, "clock_timer02", "xin24m", 0, RK3399_CLKGATE_CON(26), 2, GFLAGS),
    GATE(SCLK_TIMER03, "clock_timer03", "xin24m", 0, RK3399_CLKGATE_CON(26), 3, GFLAGS),
    GATE(SCLK_TIMER04, "clock_timer04", "xin24m", 0, RK3399_CLKGATE_CON(26), 4, GFLAGS),
    GATE(SCLK_TIMER05, "clock_timer05", "xin24m", 0, RK3399_CLKGATE_CON(26), 5, GFLAGS),
    GATE(SCLK_TIMER06, "clock_timer06", "xin24m", 0, RK3399_CLKGATE_CON(26), 6, GFLAGS),
    GATE(SCLK_TIMER07, "clock_timer07", "xin24m", 0, RK3399_CLKGATE_CON(26), 7, GFLAGS),
    GATE(SCLK_TIMER08, "clock_timer08", "xin24m", 0, RK3399_CLKGATE_CON(26), 8, GFLAGS),
    GATE(SCLK_TIMER09, "clock_timer09", "xin24m", 0, RK3399_CLKGATE_CON(26), 9, GFLAGS),
    GATE(SCLK_TIMER10, "clock_timer10", "xin24m", 0, RK3399_CLKGATE_CON(26), 10, GFLAGS),
    GATE(SCLK_TIMER11, "clock_timer11", "xin24m", 0, RK3399_CLKGATE_CON(26), 11, GFLAGS),

    /* clock_test */
    /* clock_test_pre is controlled by CRU_MISC_CON[3] */
    COMPOSITE_NOMUX(0, "clock_test", "clock_test_pre", CLK_IGNORE_UNUSED, RK3399_CLKSEL_CON(58), 0, 5, DFLAGS, RK3399_CLKGATE_CON(13), 11, GFLAGS),

    /* ddrc */
    GATE(0, "clock_ddrc_lpll_src", "lpll", 0, RK3399_CLKGATE_CON(3), 0, GFLAGS),
    GATE(0, "clock_ddrc_bpll_src", "bpll", 0, RK3399_CLKGATE_CON(3), 1, GFLAGS),
    GATE(0, "clock_ddrc_dpll_src", "dpll", 0, RK3399_CLKGATE_CON(3), 2, GFLAGS),
    GATE(0, "clock_ddrc_gpll_src", "gpll", 0, RK3399_CLKGATE_CON(3), 3, GFLAGS),
    COMPOSITE_DDRCLK(SCLK_DDRCLK, "sclock_ddrc", mux_ddrclock_p, 0, RK3399_CLKSEL_CON(6), 4, 2, 0, 0, ROCKCHIP_DDRCLK_SIP),

};

static struct rockchip_clock_branch rk3399_clock_pmu_branches[] __initdata = {
    /*
     * PMU CRU Clock-Architecture
     */

    GATE(0, "fclock_cm0s_pmu_ppll_src", "ppll", 0, RK3399_PMU_CLKGATE_CON(0), 1, GFLAGS),

    COMPOSITE_NOGATE(FCLK_CM0S_SRC_PMU, "fclock_cm0s_src_pmu", mux_fclock_cm0s_pmu_ppll_p, 0, RK3399_PMU_CLKSEL_CON(0), 15, 1, MFLAGS, 8, 5, DFLAGS),

    COMPOSITE(
        SCLK_SPI3_PMU, "clock_spi3_pmu", mux_24m_ppll_p, 0, RK3399_PMU_CLKSEL_CON(1), 7, 1, MFLAGS, 0, 7, DFLAGS, RK3399_PMU_CLKGATE_CON(0), 2,
        GFLAGS),

    COMPOSITE(
        0, "clock_wifi_div", mux_ppll_24m_p, CLK_IGNORE_UNUSED, RK3399_PMU_CLKSEL_CON(1), 13, 1, MFLAGS, 8, 5, DFLAGS, RK3399_PMU_CLKGATE_CON(0), 8,
        GFLAGS),

    COMPOSITE_FRACMUX_NOGATE(
        0, "clock_wifi_frac", "clock_wifi_div", 0, RK3399_PMU_CLKSEL_CON(7), 0, &rk3399_pmuclock_wifi_fracmux, RK3399_WIFI_FRAC_MAX_PRATE),

    MUX(0, "clock_timer_src_pmu", mux_pll_p, CLK_IGNORE_UNUSED, RK3399_PMU_CLKSEL_CON(1), 15, 1, MFLAGS),

    COMPOSITE_NOMUX(SCLK_I2C0_PMU, "clock_i2c0_pmu", "ppll", 0, RK3399_PMU_CLKSEL_CON(2), 0, 7, DFLAGS, RK3399_PMU_CLKGATE_CON(0), 9, GFLAGS),

    COMPOSITE_NOMUX(SCLK_I2C4_PMU, "clock_i2c4_pmu", "ppll", 0, RK3399_PMU_CLKSEL_CON(3), 0, 7, DFLAGS, RK3399_PMU_CLKGATE_CON(0), 10, GFLAGS),

    COMPOSITE_NOMUX(SCLK_I2C8_PMU, "clock_i2c8_pmu", "ppll", 0, RK3399_PMU_CLKSEL_CON(2), 8, 7, DFLAGS, RK3399_PMU_CLKGATE_CON(0), 11, GFLAGS),

    DIV(0, "clock_32k_suspend_pmu", "xin24m", CLK_IGNORE_UNUSED, RK3399_PMU_CLKSEL_CON(4), 0, 10, DFLAGS),
    MUX(0, "clock_testout_2io", mux_clock_testout2_2io_p, CLK_IGNORE_UNUSED, RK3399_PMU_CLKSEL_CON(4), 15, 1, MFLAGS),

    MUX(SCLK_UART4_SRC, "clock_uart4_src", mux_24m_ppll_p, CLK_SET_RATE_NO_REPARENT, RK3399_PMU_CLKSEL_CON(5), 10, 1, MFLAGS),

    COMPOSITE_NOMUX(
        0, "clock_uart4_div", "clock_uart4_src", CLK_SET_RATE_PARENT, RK3399_PMU_CLKSEL_CON(5), 0, 7, DFLAGS, RK3399_PMU_CLKGATE_CON(0), 5, GFLAGS),

    COMPOSITE_FRACMUX(
        0, "clock_uart4_frac", "clock_uart4_div", 0, RK3399_PMU_CLKSEL_CON(6), 0, RK3399_PMU_CLKGATE_CON(0), 6, GFLAGS, &rk3399_uart4_pmu_fracmux,
        RK3399_UART_FRAC_MAX_PRATE),

    DIV(PCLK_SRC_PMU, "pclock_pmu_src", "ppll", CLK_IGNORE_UNUSED, RK3399_PMU_CLKSEL_CON(0), 0, 5, DFLAGS),

    /* pmu clock gates */
    GATE(SCLK_TIMER12_PMU, "clock_timer0_pmu", "clock_timer_src_pmu", 0, RK3399_PMU_CLKGATE_CON(0), 3, GFLAGS),
    GATE(SCLK_TIMER13_PMU, "clock_timer1_pmu", "clock_timer_src_pmu", 0, RK3399_PMU_CLKGATE_CON(0), 4, GFLAGS),

    GATE(SCLK_PVTM_PMU, "clock_pvtm_pmu", "xin24m", 0, RK3399_PMU_CLKGATE_CON(0), 7, GFLAGS),

    GATE(PCLK_PMU, "pclock_pmu", "pclock_pmu_src", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(1), 0, GFLAGS),
    GATE(PCLK_PMUGRF_PMU, "pclock_pmugrf_pmu", "pclock_pmu_src", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(1), 1, GFLAGS),
    GATE(PCLK_INTMEM1_PMU, "pclock_intmem1_pmu", "pclock_pmu_src", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(1), 2, GFLAGS),
    GATE(PCLK_GPIO0_PMU, "pclock_gpio0_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 3, GFLAGS),
    GATE(PCLK_GPIO1_PMU, "pclock_gpio1_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 4, GFLAGS),
    GATE(PCLK_SGRF_PMU, "pclock_sgrf_pmu", "pclock_pmu_src", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(1), 5, GFLAGS),
    GATE(PCLK_NOC_PMU, "pclock_noc_pmu", "pclock_pmu_src", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(1), 6, GFLAGS),
    GATE(PCLK_I2C0_PMU, "pclock_i2c0_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 7, GFLAGS),
    GATE(PCLK_I2C4_PMU, "pclock_i2c4_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 8, GFLAGS),
    GATE(PCLK_I2C8_PMU, "pclock_i2c8_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 9, GFLAGS),
    GATE(PCLK_RKPWM_PMU, "pclock_rkpwm_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 10, GFLAGS),
    GATE(PCLK_SPI3_PMU, "pclock_spi3_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 11, GFLAGS),
    GATE(PCLK_TIMER_PMU, "pclock_timer_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 12, GFLAGS),
    GATE(PCLK_MAILBOX_PMU, "pclock_mailbox_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 13, GFLAGS),
    GATE(PCLK_UART4_PMU, "pclock_uart4_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 14, GFLAGS),
    GATE(PCLK_WDT_M0_PMU, "pclock_wdt_m0_pmu", "pclock_pmu_src", 0, RK3399_PMU_CLKGATE_CON(1), 15, GFLAGS),

    GATE(FCLK_CM0S_PMU, "fclock_cm0s_pmu", "fclock_cm0s_src_pmu", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(2), 0, GFLAGS),
    GATE(SCLK_CM0S_PMU, "sclock_cm0s_pmu", "fclock_cm0s_src_pmu", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(2), 1, GFLAGS),
    GATE(HCLK_CM0S_PMU, "hclock_cm0s_pmu", "fclock_cm0s_src_pmu", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(2), 2, GFLAGS),
    GATE(DCLK_CM0S_PMU, "dclock_cm0s_pmu", "fclock_cm0s_src_pmu", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(2), 3, GFLAGS),
    GATE(HCLK_NOC_PMU, "hclock_noc_pmu", "fclock_cm0s_src_pmu", CLK_IGNORE_UNUSED, RK3399_PMU_CLKGATE_CON(2), 5, GFLAGS),
};

static const char *const rk3399_cru_critical_clocks[] __initconst = {
    /*
     * We need to declare that we enable all NOCs which are critical clocks
     * always and clearly and explicitly show that we have enabled them at
     * clock_summary.
     */
    "aclock_usb3_noc",
    "aclock_gmac_noc",
    "pclock_gmac_noc",
    "pclock_center_main_noc",
    "aclock_cci_noc0",
    "aclock_cci_noc1",
    "clock_dbg_noc",
    "hclock_vcodec_noc",
    "aclock_vcodec_noc",
    "hclock_vdu_noc",
    "aclock_vdu_noc",
    "hclock_iep_noc",
    "aclock_iep_noc",
    "hclock_rga_noc",
    "aclock_rga_noc",
    "aclock_center_main_noc",
    "aclock_center_peri_noc",
    "aclock_perihp_noc",
    "hclock_perihp_noc",
    "pclock_perihp_noc",
    "hclock_sdmmc_noc",
    "aclock_emmc_noc",
    "aclock_perilp0_noc",
    "hclock_perilp0_noc",
    "hclock_m0_perilp_noc",
    "hclock_perilp1_noc",
    "hclock_sdio_noc",
    "hclock_sdioaudio_noc",
    "pclock_perilp1_noc",
    "aclock_vio_noc",
    "aclock_hdcp_noc",
    "hclock_hdcp_noc",
    "pclock_hdcp_noc",
    "pclock_edp_noc",
    "aclock_vop0_noc",
    "hclock_vop0_noc",
    "aclock_vop1_noc",
    "hclock_vop1_noc",
    "aclock_isp0_noc",
    "hclock_isp0_noc",
    "aclock_isp1_noc",
    "hclock_isp1_noc",
    "aclock_gic_noc",

    /* ddrc */
    "sclock_ddrc",

    /* other critical clocks */
    "pclock_perilp0",
    "pclock_perilp0",
    "hclock_perilp0",
    "pclock_perilp1",
    "pclock_perihp",
    "hclock_perihp",
    "aclock_perihp",
    "aclock_perilp0",
    "hclock_perilp1",
    "aclock_dmac1_perilp",
    "gpll_aclock_perilp0_src",
    "gpll_aclock_perihp_src",
    "pclock_vio",
    "pclock_vio_grf",
    "pclock_perihp_grf",
};

static const char *const rk3399_pmucru_critical_clocks[] __initconst = {
    /*
     * We need to declare that we enable all NOCs which are critical clocks
     * always and clearly and explicitly show that we have enabled them at
     * clock_summary.
     */
    "pclock_noc_pmu",
    "hclock_noc_pmu",

    /* other critical clocks */
    "ppll",
    "pclock_pmu_src",
    "fclock_cm0s_src_pmu",
    "clock_timer_src_pmu",
    "pclock_rkpwm_pmu",
};

static void __iomem *rk3399_cru_base;
static void __iomem *rk3399_pmucru_base;

#if 0
void rk3399_dump_cru(void)
{
    if (rk3399_cru_base) {
        pr_warn("CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET,
                       32, 4, rk3399_cru_base,
                       0x594, false);
    }

    if (rk3399_pmucru_base) {
        pr_warn("PMU CRU:\n");
        print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET,
                       32, 4, rk3399_pmucru_base,
                       0x134, false);
    }
}
EXPORT_SYMBOL_GPL(rk3399_dump_cru);
#endif

#if 0
static int rk3399_clock_panic(struct notifier_block *this,
                              uint64_t ev, void *ptr)
{
    rk3399_dump_cru();
    return NOTIFY_DONE;
}

static struct notifier_block rk3399_clock_panic_block = {
    .notifier_call = rk3399_clock_panic,
};
#endif

static void __init rk3399_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;
    struct clk                     *clk;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru region\n", __func__);
        return;
    }

    rk3399_cru_base = reg_base;

    ctx             = rockchip_clock_init(np, reg_base, CLK_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip clk init failed\n", __func__);
        return;
    }

    /* Watchdog pclk is controlled by RK3399 SECURE_GRF_SOC_CON3[8]. */
    clk = clock_register_fixed_factor(NULL, "pclock_wdt", "pclock_alive", 0, 1, 1);

    if (IS_ERR(clk)) {
        pr_warn("%s: could not register clock pclock_wdt: %ld\n", __func__, PTR_ERR(clk));
    } else {
        rockchip_clock_add_lookup(ctx, clk, PCLK_WDT);
    }

    rockchip_clock_register_plls(ctx, rk3399_pll_clocks, ARRAY_SIZE(rk3399_pll_clocks), -1);

    rockchip_clock_register_branches(ctx, rk3399_clock_branches, ARRAY_SIZE(rk3399_clock_branches));

    rockchip_clock_protect_critical(rk3399_cru_critical_clocks, ARRAY_SIZE(rk3399_cru_critical_clocks));

    rockchip_clock_register_armclk(
        ctx, ARMCLKL, "armclkl", mux_armclkl_p, ARRAY_SIZE(mux_armclkl_p), &rk3399_cpuclkl_data, rk3399_cpuclkl_rates,
        ARRAY_SIZE(rk3399_cpuclkl_rates));

    rockchip_clock_register_armclk(
        ctx, ARMCLKB, "armclkb", mux_armclkb_p, ARRAY_SIZE(mux_armclkb_p), &rk3399_cpuclkb_data, rk3399_cpuclkb_rates,
        ARRAY_SIZE(rk3399_cpuclkb_rates));

    rockchip_register_softrst(np, 21, reg_base + RK3399_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_register_restart_notifier(ctx, RK3399_GLB_SRST_FST, NULL);

    rockchip_clock_of_add_provider(np, ctx);
}

CLK_OF_DECLARE(rk3399_cru, "rockchip,rk3399-cru", rk3399_clock_init);

static void __init rk3399_pmu_clock_init(struct device_node *np)
{
    struct rockchip_clock_provider *ctx;
    void __iomem                   *reg_base;

    reg_base = of_iomap(np, 0);

    if (!reg_base) {
        pr_err("%s: could not map cru pmu region\n", __func__);
        return;
    }

    rk3399_pmucru_base = reg_base;

    ctx                = rockchip_clock_init(np, reg_base, CLKPMU_NR_CLKS);

    if (IS_ERR(ctx)) {
        pr_err("%s: rockchip pmu clk init failed\n", __func__);
        return;
    }

    rockchip_clock_register_plls(ctx, rk3399_pmu_pll_clocks, ARRAY_SIZE(rk3399_pmu_pll_clocks), -1);

    rockchip_clock_register_branches(ctx, rk3399_clock_pmu_branches, ARRAY_SIZE(rk3399_clock_pmu_branches));

    rockchip_clock_protect_critical(rk3399_pmucru_critical_clocks, ARRAY_SIZE(rk3399_pmucru_critical_clocks));

    rockchip_register_softrst(np, 2, reg_base + RK3399_PMU_SOFTRST_CON(0), ROCKCHIP_SOFTRST_HIWORD_MASK);

    rockchip_clock_of_add_provider(np, ctx);
#if 0
    atomic_notifier_chain_register(&panic_notifier_list,
                                   &rk3399_clock_panic_block);
#endif
}

CLK_OF_DECLARE(rk3399_cru_pmu, "rockchip,rk3399-pmucru", rk3399_pmu_clock_init);
