// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Fuzhou Rockchip Electronics Co., Ltd
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/string.h>

#define CLK_SEL_EXTERNAL_32K    0
#define CLK_SEL_INTERNAL_PVTM   1

#define wr_msk_bit(v, off, msk) ((v) << (off) | (msk << (16 + (off))))

struct rockchip_clock_pvtm;

struct rockchip_clock_pvtm_info {
    uint32_t con;
    uint32_t sta;
    uint32_t sel_con;
    uint32_t sel_shift;
    uint32_t sel_value;
    uint32_t sel_mask;
    uint32_t div_shift;
    uint32_t div_mask;

    uint32_t (*get_value)(struct rockchip_clock_pvtm *pvtm, uint32_t time_us);
    int (*init_freq)(struct rockchip_clock_pvtm *pvtm);
    int (*sel_enable)(struct rockchip_clock_pvtm *pvtm);
};

struct rockchip_clock_pvtm {
    const struct rockchip_clock_pvtm_info *info;
    struct regmap                         *grf;
    struct clk                            *pvtm_clock;
    struct clk                            *clk;
    uint64_t                               rate;
};

static uint64_t xin32k_pvtm_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    return 32768;
}

static const struct clock_ops xin32k_pvtm = {
    .recalc_rate = xin32k_pvtm_recalc_rate,
};

static void rockchip_clock_pvtm_delay(uint32_t delay)
{
    uint32_t ms = delay / 1000;
    uint32_t us = delay % 1000;

    if (ms > 0) {
        if (ms < 20) {
            us += ms * 1000;
        } else {
            msleep(ms);
        }
    }

    if (us >= 10) {
        usleep_range(us, us + 100);
    } else {
        udelay(us);
    }
}

static int rockchip_clock_sel_internal_pvtm(struct rockchip_clock_pvtm *pvtm)
{
    int ret = 0;

    ret     = regmap_write(pvtm->grf, pvtm->info->sel_con, wr_msk_bit(pvtm->info->sel_value, pvtm->info->sel_shift, pvtm->info->sel_mask));

    if (ret != 0) {
        pr_err("%s: fail to write register\n", __func__);
    }

    return ret;
}

/* get pmu pvtm value */
static uint32_t rockchip_clock_pvtm_get_value(struct rockchip_clock_pvtm *pvtm, uint32_t time_us)
{
    const struct rockchip_clock_pvtm_info *info = pvtm->info;
    uint32_t                               val = 0, sta = 0;
    uint32_t                               clock_cnt, check_cnt;

    /* 24m clk ,24cnt=1us */
    clock_cnt = time_us * 24;

    regmap_write(pvtm->grf, info->con + 0x4, clock_cnt);
    regmap_write(pvtm->grf, info->con, wr_msk_bit(3, 0, 0x3));

    rockchip_clock_pvtm_delay(time_us);

    check_cnt = 100;

    while (check_cnt--) {
        regmap_read(pvtm->grf, info->sta, &sta);

        if (sta & 0x1) {
            break;
        }

        udelay(4);
    }

    if (check_cnt) {
        regmap_read(pvtm->grf, info->sta + 0x4, &val);
    } else {
        pr_err("%s: wait pvtm_done timeout!\n", __func__);
        val = 0;
    }

    regmap_write(pvtm->grf, info->con, wr_msk_bit(0, 0, 0x3));

    return val;
}

static int rockchip_clock_pvtm_init_freq(struct rockchip_clock_pvtm *pvtm)
{
    uint32_t pvtm_cnt = 0;
    uint32_t div, time_us;
    int      ret = 0;

    time_us      = 1000;
    pvtm_cnt     = pvtm->info->get_value(pvtm, time_us);
    pr_debug("get pvtm_cnt = %d\n", pvtm_cnt);

    /* set pvtm_div to get rate */
    div = DIV_ROUND_UP(1000 * pvtm_cnt, pvtm->rate);

    if (div > pvtm->info->div_mask) {
        pr_err("pvtm_div out of bounary! set max instead\n");
        div = pvtm->info->div_mask;
    }

    pr_debug("set div %d, rate %luKHZ\n", div, pvtm->rate);
    ret = regmap_write(pvtm->grf, pvtm->info->con, wr_msk_bit(div, pvtm->info->div_shift, pvtm->info->div_mask));

    if (ret != 0) {
        goto out;
    }

    /* pmu pvtm oscilator enable */
    ret = regmap_write(pvtm->grf, pvtm->info->con, wr_msk_bit(1, 1, 0x1));

    if (ret != 0) {
        goto out;
    }

    ret = pvtm->info->sel_enable(pvtm);
out:

    if (ret != 0) {
        pr_err("%s: fail to write register\n", __func__);
    }

    return ret;
}

static int clock_pvtm_regitstor(struct device *dev, struct rockchip_clock_pvtm *pvtm)
{
    struct clock_init_data init = {};
    struct clock_hw       *clock_hw;

    /* Init the xin32k_pvtm */
    pvtm->info->init_freq(pvtm);

    init.flags        = CLK_IS_ROOT;
    init.parent_names = NULL;
    init.num_parents  = 0;
    init.name         = "xin32k_pvtm";
    init.ops          = &xin32k_pvtm;

    clock_hw          = devm_kzalloc(dev, sizeof(*clock_hw), GFP_KERNEL);

    if (!clock_hw) {
        return -ENOMEM;
    }

    clock_hw->init = &init;

    /* optional override of the clockname */
    of_property_read_string_index(dev->of_node, "clock-output-names", 0, &init.name);
    pvtm->clk = devm_clock_register(dev, clock_hw);

    if (IS_ERR(pvtm->clk)) {
        return PTR_ERR(pvtm->clk);
    }

    return of_clock_add_provider(dev->of_node, of_clock_src_simple_get, pvtm->clk);
}

static const struct rockchip_clock_pvtm_info rk3368_pvtm_data = {
    .con        = 0x180,
    .sta        = 0x190,
    .sel_con    = 0x100,
    .sel_shift  = 6,
    .sel_value  = CLK_SEL_INTERNAL_PVTM,
    .sel_mask   = 0x1,
    .div_shift  = 2,
    .div_mask   = 0x3f,

    .sel_enable = rockchip_clock_sel_internal_pvtm,
    .get_value  = rockchip_clock_pvtm_get_value,
    .init_freq  = rockchip_clock_pvtm_init_freq,
};
MODULE_DEVICE_TABLE(of, rockchip_clock_pvtm_match);

static const struct of_device_id rockchip_clock_pvtm_match[] = {
    {
     .compatible = "rockchip,rk3368-pvtm-clock",
     .data       = (void *)&rk3368_pvtm_data,
     },
};

static int rockchip_clock_pvtm_probe(struct platform_device *pdev)
{
    struct device              *dev = &pdev->dev;
    struct device_node         *np  = pdev->dev.of_node;
    const struct of_device_id  *match;
    struct rockchip_clock_pvtm *pvtm;
    int                         error;
    uint32_t                    rate;

    pvtm = devm_kzalloc(dev, sizeof(*pvtm), GFP_KERNEL);

    if (!pvtm) {
        return -ENOMEM;
    }

    match = of_match_node(rockchip_clock_pvtm_match, np);

    if (!match) {
        return -ENXIO;
    }

    pvtm->info = (const struct rockchip_clock_pvtm_info *)match->data;

    if (!pvtm->info) {
        return -EINVAL;
    }

    if (!dev->parent || !dev->parent->of_node) {
        return -EINVAL;
    }

    pvtm->grf = syscon_node_to_regmap(dev->parent->of_node);

    if (IS_ERR(pvtm->grf)) {
        return PTR_ERR(pvtm->grf);
    }

    if (!of_property_read_u32(np, "pvtm-rate", &rate)) {
        pvtm->rate = rate;
    } else {
        pvtm->rate = 32768;
    }

    pvtm->pvtm_clock = devm_clock_get(&pdev->dev, "pvtm_pmu_clock");

    if (IS_ERR(pvtm->pvtm_clock)) {
        error = PTR_ERR(pvtm->pvtm_clock);

        if (error != -EPROBE_DEFER) {
            dev_err(&pdev->dev, "failed to get pvtm core clock: %d\n", error);
        }

        goto out_probe;
    }

    error = clock_prepare_enable(pvtm->pvtm_clock);

    if (error) {
        dev_err(&pdev->dev, "failed to enable the clock: %d\n", error);
        goto out_probe;
    }

    platform_set_drvdata(pdev, pvtm);

    error = clock_pvtm_regitstor(&pdev->dev, pvtm);

    if (error) {
        dev_err(&pdev->dev, "failed to registor clock: %d\n", error);
        goto out_clock_put;
    }

    return error;

out_clock_put:
    clock_disable_unprepare(pvtm->pvtm_clock);
out_probe:
    return error;
}

static int rockchip_clock_pvtm_remove(struct platform_device *pdev)
{
    struct rockchip_clock_pvtm *pvtm = platform_get_drvdata(pdev);
    struct device_node         *np   = pdev->dev.of_node;

    of_clock_del_provider(np);
    clock_disable_unprepare(pvtm->pvtm_clock);

    return 0;
}

static struct platform_driver rockchip_clock_pvtm_driver = {
    .driver =
        {
                 .name           = "rockchip-clcok-pvtm",
                 .of_match_table = rockchip_clock_pvtm_match,
                 },
    .probe  = rockchip_clock_pvtm_probe,
    .remove = rockchip_clock_pvtm_remove,
};

module_platform_driver(rockchip_clock_pvtm_driver);

MODULE_DESCRIPTION("Rockchip Clock Pvtm Driver");
MODULE_LICENSE("GPL v2");
