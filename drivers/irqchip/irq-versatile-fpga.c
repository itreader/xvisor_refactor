/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file irq-versatile-fpga.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Support for Versatile FPGA-based IRQ controllers
 *
 * The source has been largely adapted from Linux
 * drivers/irqchip/irq-versatile-fpga.c
 *
 * The original code is licensed under the GPL.
 *
 * Support for Versatile FPGA-based IRQ controllers
 */

#include <libs/stringlib.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

#define NR_IRQS          32
#define IRQ_STATUS       0x00
#define IRQ_RAW_STATUS   0x04
#define IRQ_ENABLE_SET   0x08
#define IRQ_ENABLE_CLEAR 0x0c
#define INT_SOFT_SET     0x10
#define INT_SOFT_CLEAR   0x14
#define FIQ_STATUS       0x20
#define FIQ_RAW_STATUS   0x24
#define FIQ_ENABLE       0x28
#define FIQ_ENABLE_SET   0x28
#define FIQ_ENABLE_CLEAR 0x2C
#define PICEN_STATUS     0x20
#define PICEN_SET        0x20
#define PCIEN_CLEAR      0x24

/**
 * struct fpga_irq_data - irq data container for the FPGA IRQ controller
 * @base: memory offset in virtual memory
 * @chip: chip container for this instance
 * @domain: IRQ domain for this instance
 * @valid: mask for valid IRQs on this controller
 * @used_irqs: number of active IRQs on this controller
 */
struct fpga_irq_data {
    struct vmm_host_irq_domain *domain;
    vmm_device_tree_node_t     *node;
    void                       *base;
    struct vmm_host_irq_chip    chip;
    uint32_t                    valid;
    uint8_t                     used_irqs;
};

#ifndef CONFIG_VERSATILE_FPGA_IRQ_NR
#define CONFIG_VERSATILE_FPGA_IRQ_NR 4
#endif

/* we cannot allocate memory when the controllers are initially registered */
static struct fpga_irq_data fpga_irq_devices[CONFIG_VERSATILE_FPGA_IRQ_NR];
static int                  fpga_irq_id = 0;

static void fpga_irq_mask(struct vmm_host_irq *d)
{
    struct fpga_irq_data *f    = vmm_host_irq_get_chip_data(d);
    uint32_t              mask = 1 << d->hwirq;

    vmm_writel(mask, f->base + IRQ_ENABLE_CLEAR);
}

static void fpga_irq_unmask(struct vmm_host_irq *d)
{
    struct fpga_irq_data *f    = vmm_host_irq_get_chip_data(d);
    uint32_t              mask = 1 << d->hwirq;

    vmm_writel(mask, f->base + IRQ_ENABLE_SET);
}

static uint32_t fpga_find_active_irq(struct fpga_irq_data *f)
{
    uint32_t ret = UINT_MAX;
    uint32_t hwirq, int_status;

    int_status = vmm_readl(f->base + IRQ_STATUS);

    if (!int_status) {
        goto done;
    }

    for (hwirq = 0; hwirq < NR_IRQS; hwirq++) {
        if (!(int_status & (1 << hwirq))) {
            continue;
        }

        ret = vmm_host_irq_domain_find_mapping(f->domain, hwirq);
        goto done;
    }

done:
    return ret;
}

static uint32_t fpga_active_irq(uint32_t cpu_nr, uint32_t prev_irq)
{
    uint32_t i, ret;

    for (i = 0; i < fpga_irq_id; i++) {
        ret = fpga_find_active_irq(&fpga_irq_devices[i]);

        if (ret != UINT_MAX) {
            return ret;
        }
    }

    return UINT_MAX;
}

static vmm_irq_return_t fpga_handle_cascade_irq(int irq, void *dev)
{
    vmm_host_generic_irq_exec(fpga_find_active_irq(dev));

    return VMM_IRQ_HANDLED;
}

static void __init fpga_cascade_irq(struct fpga_irq_data *f, const char *name, uint32_t parent_irq)
{
    if (vmm_host_irq_register(parent_irq, name, fpga_handle_cascade_irq, f)) {
        BUG();
    }
}

static struct vmm_host_irq_domain_ops fpga_ops = {
    .xlate = vmm_host_irq_domain_xlate_onecell,
};

void __init fpga_irq_init(void *base, const char *name, uint32_t irq_start, uint32_t parent_irq, uint32_t valid, vmm_device_tree_node_t *node)
{
    int                   hirq;
    uint32_t              hwirq;
    struct fpga_irq_data *f;

    if (fpga_irq_id >= array_size(fpga_irq_devices)) {
        vmm_printf(
            "%s: too few FPGA IRQ controllers, "
            "increase CONFIG_VERSATILE_FPGA_IRQ_NR\n",
            __func__);
        return;
    }

    f         = &fpga_irq_devices[fpga_irq_id];

    f->domain = vmm_host_irq_domain_add(node, (int)irq_start, NR_IRQS, &fpga_ops, NULL);

    if (!f->domain) {
        vmm_printf("%s: failed to add vmm_host_irq_domain\n", __func__);
        return;
    }

    f->node            = node;
    f->base            = base;
    f->chip.name       = name;
    f->chip.irq_ack    = fpga_irq_mask;
    f->chip.irq_mask   = fpga_irq_mask;
    f->chip.irq_unmask = fpga_irq_unmask;
    f->valid           = valid;
    f->used_irqs       = 0;

    if (parent_irq != UINT_MAX) {
        fpga_cascade_irq(f, name, parent_irq);
    } else {
        vmm_host_irq_set_active_callback(fpga_active_irq);
    }

    /* This will allocate all valid descriptors in the linear case */
    for (hwirq = 0; hwirq < NR_IRQS; hwirq++) {
        if (!(valid & (1 << hwirq))) {
            continue;
        }

        hirq = vmm_host_irq_domain_create_mapping(f->domain, hwirq);
        BUG_ON(hirq < 0);
        vmm_host_irq_set_chip(hirq, &f->chip);
        vmm_host_irq_set_chip_data(hirq, f);
        vmm_host_irq_set_handler(hirq, vmm_handle_level_irq);

        f->used_irqs++;
    }

    fpga_irq_id++;
}

static int __init fpga_init(vmm_device_tree_node_t *node)
{
    int            rc;
    virtual_addr_t base;
    uint32_t       clear_mask;
    uint32_t       valid_mask;
    uint32_t       picen_mask;
    uint32_t       irq_start;
    uint32_t       parent_irq;

    rc = vmm_device_tree_request_regmap(node, &base, 0, "Versatile SIC");
    WARN(rc, "unable to map fpga irq registers\n");

    if (vmm_device_tree_read_u32(node, "irq_start", &irq_start)) {
        irq_start = 0;
    }

    if (vmm_device_tree_read_u32(node, "clear-mask", &clear_mask)) {
        clear_mask = 0;
    }

    if (vmm_device_tree_read_u32(node, "valid-mask", &valid_mask)) {
        valid_mask = 0;
    }

    /* Some chips are cascaded from a parent IRQ */
    parent_irq = vmm_device_tree_irq_parse_map(node, 0);

    if (!parent_irq) {
        parent_irq = UINT_MAX;
    }

    fpga_irq_init((void *)base, "FPGA", irq_start, parent_irq, valid_mask, node);

    vmm_writel(clear_mask, (void *)base + IRQ_ENABLE_CLEAR);
    vmm_writel(clear_mask, (void *)base + FIQ_ENABLE_CLEAR);

    /* For VersatilePB, we have interrupts from 21 to 31 capable
     * of being routed directly to the parent interrupt controller
     * (i.e. VIC). This is controlled by setting PIC_ENABLEx.
     */
    if (!vmm_device_tree_read_u32(node, "picen-mask", &picen_mask)) {
        vmm_writel(picen_mask, (void *)base + PICEN_SET);
    }

    return 0;
}

VMM_HOST_IRQ_INIT_DECLARE(vfpga, "arm,versatile-sic", fpga_init);
