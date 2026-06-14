/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file irq-vic.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief PL190 Vectored Interrupt Controller source
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-vic.c
 *
 * The original code is licensed under the GPL.
 *
 *  linux/arch/arm/common/vic.c
 *
 *  Copyright (C) 1999 - 2003 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 */

#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

#define VIC_NR_IRQS             32
#define VIC_IRQ_STATUS          0x00
#define VIC_FIQ_STATUS          0x04
#define VIC_RAW_STATUS          0x08
#define VIC_INT_SELECT          0x0c /* 1 = FIQ, 0 = IRQ */
#define VIC_INT_ENABLE          0x10 /* 1 = enable, 0 = disable */
#define VIC_INT_ENABLE_CLEAR    0x14
#define VIC_INT_SOFT            0x18
#define VIC_INT_SOFT_CLEAR      0x1c
#define VIC_PROTECT             0x20
#define VIC_PL190_VECT_ADDR     0x30  /* PL190 only */
#define VIC_PL190_DEF_VECT_ADDR 0x34  /* PL190 only */

#define VIC_VECT_ADDR0          0x100 /* 0 to 15 (0..31 PL192) */
#define VIC_VECT_CNTL0          0x200 /* 0 to 15 (0..31 PL192) */
#define VIC_ITCR                0x300 /* VIC test control register */

#define VIC_VECT_CNTL_ENABLE    (1 << 5)

#define VIC_PL192_VECT_ADDR     0xF00

struct vic_chip_data {
    vmm_device_tree_node_t     *node;
    virtual_addr_t              base;
    struct vmm_host_irq_domain *domain;
};

#ifndef VIC_MAX_NR
#define VIC_MAX_NR 1
#endif

static struct vic_chip_data vic_data[VIC_MAX_NR];

static inline void *vic_base(vmm_host_irq_t *d)
{
    struct vic_chip_data *v = vmm_host_irq_get_chip_data(d);

    return (void *)v->base;
}

static uint32_t vic_active_irq(uint32_t cpu_nr, uint32_t prev_irq)
{
    uint32_t              hw_irq_num, int_status, ret = UINT_MAX;
    struct vic_chip_data *v = &vic_data[0];

    int_status              = vmm_readl((void *)v->base + VIC_IRQ_STATUS);

    if (!int_status) {
        goto done;
    }

    for (hw_irq_num = 0; hw_irq_num < VIC_NR_IRQS; hw_irq_num++) {
        if (!((int_status >> hw_irq_num) & 0x1)) {
            continue;
        }

        ret = vmm_host_irq_domain_find_mapping(v->domain, hw_irq_num);
        goto done;
    }

done:
    return ret;
}

static void vic_mask_irq(vmm_host_irq_t *d)
{
    vmm_writel(1 << d->hw_irq_num, vic_base(d) + VIC_INT_ENABLE_CLEAR);
}

static void vic_unmask_irq(vmm_host_irq_t *d)
{
    vmm_writel(1 << d->hw_irq_num, vic_base(d) + VIC_INT_ENABLE);
}

static void vic_ack_irq(vmm_host_irq_t *d)
{
    vmm_writel(1 << d->hw_irq_num, vic_base(d) + VIC_INT_ENABLE_CLEAR);
    /* moreover, clear the soft-triggered, in case it was the reason */
    vmm_writel(1 << d->hw_irq_num, vic_base(d) + VIC_INT_SOFT_CLEAR);

    vmm_writel(1 << d->hw_irq_num, vic_base(d) + VIC_INT_ENABLE);
}

static vmm_host_irq_chip_t vic_chip = {
    .name       = "VIC",
    .irq_ack    = vic_ack_irq,
    .irq_mask   = vic_mask_irq,
    .irq_unmask = vic_unmask_irq,
};

static void vic_disable(void *base)
{
    vmm_writel(0, base + VIC_INT_SELECT);
    vmm_writel(0, base + VIC_INT_ENABLE);
    vmm_writel(~0, base + VIC_INT_ENABLE_CLEAR);
    vmm_writel(0, base + VIC_ITCR);
    vmm_writel(~0, base + VIC_INT_SOFT_CLEAR);
}

static void vic_clear_interrupts(void *base)
{
    uint32_t i;

    vmm_writel(0, base + VIC_PL190_VECT_ADDR);

    for (i = 0; i < 19; i++) {
        uint32_t value;

        value = vmm_readl(base + VIC_PL190_VECT_ADDR);
        vmm_writel(value, base + VIC_PL190_VECT_ADDR);
    }
}

static void vic_init2(void *base)
{
    int i;

    for (i = 0; i < 16; i++) {
        void *reg = base + VIC_VECT_CNTL0 + (i * 4);
        vmm_writel(VIC_VECT_CNTL_ENABLE | i, reg);
    }

    vmm_writel(32, base + VIC_PL190_DEF_VECT_ADDR);
}

static struct vmm_host_irq_domain_ops vic_ops = {
    .xlate = vmm_host_irq_domain_xlate_onecell,
};

static int __init vic_device_tree_init(vmm_device_tree_node_t *node, vmm_device_tree_node_t *parent)
{
    int                   hirq, rc;
    uint32_t              hw_irq_num, irq_start = 0;
    struct vic_chip_data *v = &vic_data[0];

    v->node                 = node;

    if (vmm_device_tree_read_u32(node, "irq_start", &irq_start)) {
        irq_start = 0;
    }

    v->domain = vmm_host_irq_domain_add(node, (int)irq_start, VIC_NR_IRQS, &vic_ops, NULL);

    if (!v->domain) {
        return VMM_ERR_FAIL;
    }

    rc = vmm_device_tree_request_regmap(node, &v->base, 0, "Versatile VIC");

    if (rc) {
        vmm_host_irq_domain_remove(v->domain);
        return rc;
    }

    for (hw_irq_num = 0; hw_irq_num < VIC_NR_IRQS; hw_irq_num++) {
        hirq = vmm_host_irq_domain_create_mapping(v->domain, hw_irq_num);
        BUG_ON(hirq < 0);
        vmm_host_irq_set_chip(hirq, &vic_chip);
        vmm_host_irq_set_chip_data(hirq, v);
        vmm_host_irq_set_handler(hirq, vmm_handle_level_irq);
    }

    /* Disable all interrupts initially. */
    vic_disable((void *)v->base);

    /* Make sure we clear all existing interrupts */
    vic_clear_interrupts((void *)v->base);

    vic_init2((void *)v->base);

    vmm_host_irq_set_active_callback(vic_active_irq);

    return VMM_OK;
}

static int __init vic_init(vmm_device_tree_node_t *node)
{
    return vic_device_tree_init(node, NULL);
}

VMM_HOST_IRQ_INIT_DECLARE(vvic, "arm,versatile-vic", vic_init);
