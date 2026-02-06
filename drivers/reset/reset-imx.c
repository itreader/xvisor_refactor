/*
 * Copyright 2011, 2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file reset-imx.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief iMX reset driver source.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/reset-controller.h>
#include <linux/reset.h>
#include <linux/spinlock.h>

#define MODULE_AUTHOR                "Jimmy Durand Wesolowski"
#define MODULE_LICENSE               "GPL"
#define MODULE_DESC                  "i.MX Reset driver"
#define MODULE_IPRIORITY             (RESET_CONTROLLER_IPRIORITY + 1)
#define MODULE_INIT                  imx_src_init
#define MODULE_EXIT                  imx_src_exit

#define SRC_SCR                      0x000
#define SRC_GPR1                     0x020
#define BP_SRC_SCR_WARM_RESET_ENABLE 0
#define BP_SRC_SCR_SW_GPU_RST        1
#define BP_SRC_SCR_SW_VPU_RST        2
#define BP_SRC_SCR_SW_IPU1_RST       3
#define BP_SRC_SCR_SW_OPEN_VG_RST    4
#define BP_SRC_SCR_SW_IPU2_RST       12
#define BP_SRC_SCR_CORE1_RST         14
#define BP_SRC_SCR_CORE1_ENABLE      22

static void __iomem *src_base;
static DEFINE_SPINLOCK(scr_lock);

static const int sw_reset_bits[5] = {
    BP_SRC_SCR_SW_GPU_RST, BP_SRC_SCR_SW_VPU_RST, BP_SRC_SCR_SW_IPU1_RST, BP_SRC_SCR_SW_OPEN_VG_RST, BP_SRC_SCR_SW_IPU2_RST};

static int imx_src_reset_module(struct reset_controller_dev *rcdev, uint64_t sw_reset_idx)
{
    uint64_t timeout;
    uint64_t flags;
    int      bit;
    uint32_t val;

    if (!src_base) {
        return -ENODEV;
    }

    if (sw_reset_idx >= ARRAY_SIZE(sw_reset_bits)) {
        return -EINVAL;
    }

    bit = 1 << sw_reset_bits[sw_reset_idx];

    spin_lock_irq_save(&scr_lock, flags);
    val = readl_relaxed(src_base + SRC_SCR);
    val |= bit;
    writel_relaxed(val, src_base + SRC_SCR);
    spin_unlock_irq_restore(&scr_lock, flags);

    timeout = jiffies + msecs_to_jiffies(1000);

    while (readl(src_base + SRC_SCR) & bit) {
        if (time_after(jiffies, timeout)) {
            return -ETIME;
        }

        cpu_relax();
    }

    return 0;
}

static struct reset_control_ops imx_src_ops = {
    .reset = imx_src_reset_module,
};

static struct reset_controller_dev imx_reset_controller = {
    .ops       = &imx_src_ops,
    .nr_resets = ARRAY_SIZE(sw_reset_bits),
};

void imx_enable_cpu(int cpu, bool enable)
{
    uint32_t mask, val;

#if 0
    cpu = cpu_logical_map(cpu);
#endif /* 0 */
    mask = 1 << (BP_SRC_SCR_CORE1_ENABLE + cpu - 1);
    spin_lock(&scr_lock);
    val = readl_relaxed(src_base + SRC_SCR);
    val = enable ? val | mask : val & ~mask;
    val |= 1 << (BP_SRC_SCR_CORE1_RST + cpu - 1);
    writel_relaxed(val, src_base + SRC_SCR);
    spin_unlock(&scr_lock);
}

void imx_set_cpu_jump(int cpu, void *jump_addr)
{
    physical_addr_t paddr;

#if 0
    cpu = cpu_logical_map(cpu);
#endif /* 0 */

    if (VMM_OK != vmm_host_va2pa((virtual_addr_t)jump_addr, &paddr)) {
        vmm_printf("Failed to get cpu jump physical address (0x%p)\n", jump_addr);
    }

    writel_relaxed(paddr, src_base + SRC_GPR1 + cpu * 8);
}

uint32_t imx_get_cpu_arg(int cpu)
{
#if 0
    cpu = cpu_logical_map(cpu);
#endif /* 0 */
    return readl_relaxed(src_base + SRC_GPR1 + cpu * 8 + 4);
}

void imx_set_cpu_arg(int cpu, uint32_t arg)
{
#if 0
    cpu = cpu_logical_map(cpu);
#endif /* 0 */
    writel_relaxed(arg, src_base + SRC_GPR1 + cpu * 8 + 4);
}

static int imx_src_probe(vmm_device_t *dev)
{
    int                     ret = VMM_OK;
    vmm_device_tree_node_t *np  = dev->of_node;
    uint32_t                val;

    ret = vmm_device_tree_request_regmap(np, (virtual_addr_t *)&src_base, 0, "i.MX Reset Control");

    if (VMM_OK != ret) {
        vmm_printf("Failed to retrive %s register mapping\n", np->name);
        return ret;
    }

    imx_reset_controller.node = np;
#ifdef CONFIG_RESET_CONTROLLER
    reset_controller_register(&imx_reset_controller);
#endif /* CONFIG_RESET_CONTROLLER */

    /*
     * force warm reset sources to generate cold reset
     * for a more reliable restart
     */
    spin_lock(&scr_lock);
    val = readl_relaxed(src_base + SRC_SCR);
    val &= ~(1 << BP_SRC_SCR_WARM_RESET_ENABLE);
    writel_relaxed(val, src_base + SRC_SCR);
    spin_unlock(&scr_lock);

    return 0;
}

static int imx_src_remove(vmm_device_t *dev)
{
#ifdef CONFIG_RESET_CONTROLLER
    reset_controller_unregister(&imx_reset_controller);
#endif /* CONFIG_RESET_CONTROLLER */

    vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)src_base, 0);
    src_base = NULL;

    return 0;
}

static const struct vmm_device_tree_nodeid imx_src_device_tree_ids[] = {
    {.compatible = "fsl,imx51-src"}, {.compatible = "fsl,imx6-src"}, {/* sentinel */}};

static vmm_driver_t imx_src_driver = {
    .name        = "i.MX reset driver",
    .match_table = imx_src_device_tree_ids,
    .probe       = imx_src_probe,
    .remove      = imx_src_remove,
};

static int __init imx_src_init(void)
{
    return vmm_device_driver_register_driver(&imx_src_driver);
}

static void __exit imx_src_exit(void)
{
    vmm_device_driver_unregister_driver(&imx_src_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
