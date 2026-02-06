/*
 * Copyright (c) 2016 Rockchip Electronics Co. Ltd.
 * Author: Lin Huang <hl@rock-chips.com>
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

#include <drm/drmP.h>
#include <drm/drm_modeset_lock.h>
#include <dt-bindings/display/rk_frame_buffer.h>
#include <linux/arm-smccc.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/rockchip/rockchip_sip.h>
#include <linux/slab.h>
#include <soc/rockchip/rockchip_dmc.h>
#include <soc/rockchip/rockchip_sip.h>
#include <soc/rockchip/scpi.h>
#include <uapi/drm/drm_mode.h>
#ifdef CONFIG_ARM
#include <asm/psci.h>
#endif

#include "clk.h"

#define MHZ (1000000)

struct rockchip_ddrclk {
    struct clock_hw hw;
    void __iomem   *reg_base;
    int             mux_offset;
    int             mux_shift;
    int             mux_width;
    int             div_shift;
    int             div_width;
    int             ddr_flag;
};

#define to_rockchip_ddrclock_hw(hw) container_of(hw, struct rockchip_ddrclk, hw)

static int rk_drm_get_lcdc_type(void)
{
    struct drm_device *drm;
    uint32_t           lcdc_type = 0;

    drm                          = drm_device_get_by_name("rockchip");

    if (drm) {
        struct drm_connector *conn;

        list_for_each_entry(conn, &drm->mode_config.connector_list, head)
        {
            if (conn->encoder) {
                lcdc_type = conn->connector_type;
                break;
            }
        }
    }

    switch (lcdc_type) {
        case DRM_MODE_CONNECTOR_DPI:
        case DRM_MODE_CONNECTOR_LVDS:
            lcdc_type = SCREEN_LVDS;
            break;

        case DRM_MODE_CONNECTOR_DisplayPort:
            lcdc_type = SCREEN_DP;
            break;

        case DRM_MODE_CONNECTOR_HDMIA:
        case DRM_MODE_CONNECTOR_HDMIB:
            lcdc_type = SCREEN_HDMI;
            break;

        case DRM_MODE_CONNECTOR_TV:
            lcdc_type = SCREEN_TVOUT;
            break;

        case DRM_MODE_CONNECTOR_eDP:
            lcdc_type = SCREEN_EDP;
            break;

        case DRM_MODE_CONNECTOR_DSI:
            lcdc_type = SCREEN_MIPI;
            break;

        default:
            lcdc_type = SCREEN_NULL;
            break;
    }

    return lcdc_type;
}

static int rockchip_ddrclock_sip_set_rate(struct clock_hw *hw, uint64_t drate, uint64_t prate)
{
    struct arm_smccc_res res;

    res = sip_smc_dram(drate, 0, ROCKCHIP_SIP_CONFIG_DRAM_SET_RATE);

    return res.a0;
}

static uint64_t rockchip_ddrclock_sip_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct arm_smccc_res res;

    res = sip_smc_dram(0, 0, ROCKCHIP_SIP_CONFIG_DRAM_GET_RATE);

    return res.a0;
}

static long rockchip_ddrclock_sip_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct arm_smccc_res res;

    res = sip_smc_dram(rate, 0, ROCKCHIP_SIP_CONFIG_DRAM_ROUND_RATE);

    return res.a0;
}

static uint8_t rockchip_ddrclock_get_parent(struct clock_hw *hw)
{
    struct rockchip_ddrclk *ddrclk      = to_rockchip_ddrclock_hw(hw);
    int                     num_parents = clock_hw_get_num_parents(hw);
    uint32_t                val;

    val = clock_readl(ddrclk->reg_base + ddrclk->mux_offset) >> ddrclk->mux_shift;
    val &= GENMASK(ddrclk->mux_width - 1, 0);

    if (val >= num_parents) {
        return -EINVAL;
    }

    return val;
}

static const struct clock_ops rockchip_ddrclock_sip_ops = {
    .recalc_rate = rockchip_ddrclock_sip_recalc_rate,
    .set_rate    = rockchip_ddrclock_sip_set_rate,
    .round_rate  = rockchip_ddrclock_sip_round_rate,
    .get_parent  = rockchip_ddrclock_get_parent,
};

static uint32_t ddr_clock_cached;

static int rockchip_ddrclock_scpi_set_rate(struct clock_hw *hw, uint64_t drate, uint64_t prate)
{
    uint32_t ret;
    uint32_t lcdc_type;

    lcdc_type = rk_drm_get_lcdc_type();

    ret       = scpi_ddr_set_clock_rate(drate / MHZ, lcdc_type);

    if (ret) {
        ddr_clock_cached = ret;
        ret              = 0;
    } else {
        ddr_clock_cached = 0;
        ret              = -1;
    }

    return ret;
}

static uint64_t rockchip_ddrclock_scpi_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    if (ddr_clock_cached) {
        return (MHZ * ddr_clock_cached);
    } else {
        return (MHZ * scpi_ddr_get_clock_rate());
    }
}

static long rockchip_ddrclock_scpi_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    rate = rate / MHZ;
    rate = (rate / 12) * 12;

    return (rate * MHZ);
}

static const struct clock_ops rockchip_ddrclock_scpi_ops = {
    .recalc_rate = rockchip_ddrclock_scpi_recalc_rate,
    .set_rate    = rockchip_ddrclock_scpi_set_rate,
    .round_rate  = rockchip_ddrclock_scpi_round_rate,
    .get_parent  = rockchip_ddrclock_get_parent,
};

struct share_params {
    uint32_t hz;
    uint32_t lcdc_type;
    uint32_t vop;
    uint32_t vop_dclock_mode;
    uint32_t sr_idle_en;
    uint32_t addr_mcu_el3;
    /*
     * 1: need to wait flag1
     * 0: never wait flag1
     */
    uint32_t wait_flag1;
    /*
     * 1: need to wait flag1
     * 0: never wait flag1
     */
    uint32_t wait_flag0;
    uint32_t complt_hwirq;
    /* if need, add parameter after */
};

struct rockchip_ddrclock_data {
    uint32_t      inited_flag;
    void __iomem *share_memory;
};

static struct rockchip_ddrclock_data ddr_data;

static void rockchip_ddrclock_data_init(void)
{
    struct arm_smccc_res res;

    res = sip_smc_request_share_mem(1, SHARE_PAGE_TYPE_DDR);

    if (!res.a0) {
        ddr_data.share_memory = (void __iomem *)res.a1;
        ddr_data.inited_flag  = 1;
    }
}

static int rockchip_ddrclock_sip_set_rate_v2(struct clock_hw *hw, uint64_t drate, uint64_t prate)
{
    struct share_params *p;
    struct arm_smccc_res res;

    if (!ddr_data.inited_flag) {
        rockchip_ddrclock_data_init();
    }

    p             = (struct share_params *)ddr_data.share_memory;

    p->hz         = drate;
    p->lcdc_type  = rk_drm_get_lcdc_type();
    p->wait_flag1 = 1;
    p->wait_flag0 = 1;

    res           = sip_smc_dram(SHARE_PAGE_TYPE_DDR, 0, ROCKCHIP_SIP_CONFIG_DRAM_SET_RATE);

    if ((int)res.a1 == SIP_RET_SET_RATE_TIMEOUT) {
        rockchip_dmcfreq_wait_complete();
    }

    return res.a0;
}

static uint64_t rockchip_ddrclock_sip_recalc_rate_v2(struct clock_hw *hw, uint64_t parent_rate)
{
    struct arm_smccc_res res;

    res = sip_smc_dram(SHARE_PAGE_TYPE_DDR, 0, ROCKCHIP_SIP_CONFIG_DRAM_GET_RATE);

    if (!res.a0) {
        return res.a1;
    } else {
        return 0;
    }
}

static long rockchip_ddrclock_sip_round_rate_v2(struct clock_hw *hw, uint64_t rate, uint64_t *prate)
{
    struct share_params *p;
    struct arm_smccc_res res;

    if (!ddr_data.inited_flag) {
        rockchip_ddrclock_data_init();
    }

    p     = (struct share_params *)ddr_data.share_memory;

    p->hz = rate;

    res   = sip_smc_dram(SHARE_PAGE_TYPE_DDR, 0, ROCKCHIP_SIP_CONFIG_DRAM_ROUND_RATE);

    if (!res.a0) {
        return res.a1;
    } else {
        return 0;
    }
}

static const struct clock_ops rockchip_ddrclock_sip_ops_v2 = {
    .recalc_rate = rockchip_ddrclock_sip_recalc_rate_v2,
    .set_rate    = rockchip_ddrclock_sip_set_rate_v2,
    .round_rate  = rockchip_ddrclock_sip_round_rate_v2,
    .get_parent  = rockchip_ddrclock_get_parent,
};

struct clk *__init rockchip_clock_register_ddrclk(
    const char *name, int flags, const char *const *parent_names, uint8_t num_parents, int mux_offset, int mux_shift, int mux_width, int div_shift,
    int div_width, int ddr_flag, void __iomem *reg_base)
{
    struct rockchip_ddrclk *ddrclk;
    struct clock_init_data  init;
    struct clk             *clk;

#ifdef CONFIG_ARM

    if (!psci_smp_available()) {
        return NULL;
    }

#endif

    ddrclk = kzalloc(sizeof(*ddrclk), GFP_KERNEL);

    if (!ddrclk) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.parent_names = parent_names;
    init.num_parents  = num_parents;

    init.flags        = flags;
    init.flags |= CLK_SET_RATE_NO_REPARENT;
    init.flags |= CLK_GET_RATE_NOCACHE;

    switch (ddr_flag) {
        case ROCKCHIP_DDRCLK_SIP:
            init.ops = &rockchip_ddrclock_sip_ops;
            break;

        case ROCKCHIP_DDRCLK_SCPI:
            init.ops = &rockchip_ddrclock_scpi_ops;
            break;

        case ROCKCHIP_DDRCLK_SIP_V2:
            init.ops = &rockchip_ddrclock_sip_ops_v2;
            break;

        default:
            pr_err("%s: unsupported ddrclk type %d\n", __func__, ddr_flag);
            kfree(ddrclk);
            return ERR_PTR(-EINVAL);
    }

    ddrclk->reg_base   = reg_base;
    ddrclk->hw.init    = &init;
    ddrclk->mux_offset = mux_offset;
    ddrclk->mux_shift  = mux_shift;
    ddrclk->mux_width  = mux_width;
    ddrclk->div_shift  = div_shift;
    ddrclk->div_width  = div_width;
    ddrclk->ddr_flag   = ddr_flag;

    clk                = clock_register(NULL, &ddrclk->hw);

    if (IS_ERR(clk)) {
        kfree(ddrclk);
    }

    return clk;
}
