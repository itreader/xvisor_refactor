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

#include <linux/io.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include "clk.h"

struct rockchip_softrst {
    struct reset_controller_dev rcdev;
    void __iomem               *reg_base;
    int                         num_regs;
    int                         num_per_reg;
    uint8_t                     flags;
    spinlock_t                  lock;
};

static int rockchip_softrst_assert(struct reset_controller_dev *rcdev, uint64_t id)
{
    struct rockchip_softrst *softrst = container_of(rcdev, struct rockchip_softrst, rcdev);
    int                      bank    = id / softrst->num_per_reg;
    int                      offset  = id % softrst->num_per_reg;

    if (softrst->flags & ROCKCHIP_SOFTRST_HIWORD_MASK) {
        writel(BIT(offset) | (BIT(offset) << 16), softrst->reg_base + (bank * 4));
    } else {
        uint64_t flags;
        uint32_t reg;

        spin_lock_irq_save(&softrst->lock, flags);

        reg = readl(softrst->reg_base + (bank * 4));
        writel(reg | BIT(offset), softrst->reg_base + (bank * 4));

        spin_unlock_irq_restore(&softrst->lock, flags);
    }

    return 0;
}

static int rockchip_softrst_deassert(struct reset_controller_dev *rcdev, uint64_t id)
{
    struct rockchip_softrst *softrst = container_of(rcdev, struct rockchip_softrst, rcdev);
    int                      bank    = id / softrst->num_per_reg;
    int                      offset  = id % softrst->num_per_reg;

    if (softrst->flags & ROCKCHIP_SOFTRST_HIWORD_MASK) {
        writel((BIT(offset) << 16), softrst->reg_base + (bank * 4));
    } else {
        uint64_t flags;
        uint32_t reg;

        spin_lock_irq_save(&softrst->lock, flags);

        reg = readl(softrst->reg_base + (bank * 4));
        writel(reg & ~BIT(offset), softrst->reg_base + (bank * 4));

        spin_unlock_irq_restore(&softrst->lock, flags);
    }

    return 0;
}

static struct reset_control_ops rockchip_softrst_ops = {
    .assert   = rockchip_softrst_assert,
    .deassert = rockchip_softrst_deassert,
};

void __init rockchip_register_softrst(struct device_node *np, uint32_t num_regs, void __iomem *base, uint8_t flags)
{
    struct rockchip_softrst *softrst;
    int                      ret;

    softrst = kzalloc(sizeof(*softrst), GFP_KERNEL);

    if (!softrst) {
        return;
    }

    spin_lock_init(&softrst->lock);

    softrst->reg_base    = base;
    softrst->flags       = flags;
    softrst->num_regs    = num_regs;
    softrst->num_per_reg = (flags & ROCKCHIP_SOFTRST_HIWORD_MASK) ? 16 : 32;
#if 0
    softrst->rcdev.owner = THIS_MODULE;
#endif
    softrst->rcdev.nr_resets = num_regs * softrst->num_per_reg;
    softrst->rcdev.ops       = &rockchip_softrst_ops;
#if 0
    softrst->rcdev.of_node = np;
#else
    softrst->rcdev.node = np;
#endif
    ret = reset_controller_register(&softrst->rcdev);

    if (ret) {
        pr_err("%s: could not register reset controller, %d\n", __func__, ret);
        kfree(softrst);
    }
};
