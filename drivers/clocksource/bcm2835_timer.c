/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file bcm2835_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 timer implementation
 */

#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_types.h>

#define REG_CONTROL     0x00
#define REG_COUNTER_LO  0x04
#define REG_COUNTER_HI  0x08
#define REG_COMPARE(n)  (0x0c + (n) * 4)
#define MAX_TIMER       3
#define DEFAULT_TIMER   3

#define MIN_REG_COMPARE 0xFF
#define MAX_REG_COMPARE 0xFFFFFFFF

struct bcm2835_clocksource {
    virtual_addr_t    base;
    void             *system_clock;
    vmm_clocksource_t clock_src;
};

static uint64_t bcm2835_clock_src_read(vmm_clocksource_t *cs)
{
    struct bcm2835_clocksource *bcs = cs->private;

    return vmm_readl(bcs->system_clock);
}

static int __init bcm2835_clocksource_init(vmm_device_tree_node_t *node)
{
    int                         rc;
    uint32_t                    clock;
    struct bcm2835_clocksource *bcs;

    /* Read clock frequency */
    rc = vmm_device_tree_clock_frequency(node, &clock);

    if (rc) {
        return rc;
    }

    bcs = vmm_zalloc(sizeof(struct bcm2835_clocksource));

    if (!bcs) {
        return VMM_ERR_NOMEM;
    }

    /* Map timer registers */
    rc = vmm_device_tree_request_regmap(node, &bcs->base, 0, "BCM2835 Timer");

    if (rc) {
        vmm_free(bcs);
        return rc;
    }

    bcs->system_clock     = (void *)(bcs->base + REG_COUNTER_LO);

    /* Setup clocksource */
    bcs->clock_src.name   = "bcm2835_timer";
    bcs->clock_src.rating = 300;
    bcs->clock_src.read   = bcm2835_clock_src_read;
    bcs->clock_src.mask   = VMM_CLOCKSOURCE_MASK(32);
    bcs->clock_src.freq   = clock;
    vmm_clocks_calc_mult_shift(&bcs->clock_src.mult, &bcs->clock_src.shift, clock, VMM_NSEC_PER_SEC, 10);
    bcs->clock_src.private = bcs;

    /* Register clocksource */
    rc                     = vmm_clocksource_register(&bcs->clock_src);

    if (rc) {
        vmm_device_tree_regunmap_release(node, bcs->base, 0);
        vmm_free(bcs);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKSOURCE_INIT_DECLARE(bcm2835clock_src, "brcm,bcm2835-system-timer", bcm2835_clocksource_init);

struct bcm2835_clock_chip {
    void            *system_clock;
    void            *control;
    void            *compare;
    uint32_t         match_mask;
    virtual_addr_t   base;
    vmm_clock_chip_t clock_chip;
};

static vmm_irq_return_t bcm2835_clock_chip_irq_handler(int irq_no, void *dev)
{
    struct bcm2835_clock_chip *bcc = dev;

    if (vmm_readl(bcc->control) & bcc->match_mask) {
        vmm_writel(bcc->match_mask, bcc->control);

        bcc->clock_chip.event_handler(&bcc->clock_chip);

        return VMM_IRQ_HANDLED;
    }

    return VMM_IRQ_NONE;
}

static void bcm2835_clock_chip_set_mode(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc)
{
    /* Timer is always running in one-shot mode */
    /* Nothing to do here !!!!! */

    switch (mode) {
        case VMM_CLOCKCHIP_MODE_PERIODIC:
        case VMM_CLOCKCHIP_MODE_ONESHOT:
        case VMM_CLOCKCHIP_MODE_UNUSED:
        case VMM_CLOCKCHIP_MODE_SHUTDOWN:
            break;

        default:
            break;
    }
}

static int bcm2835_clock_chip_set_next_event(uint64_t next, vmm_clock_chip_t *cc)
{
    struct bcm2835_clock_chip *bcc = cc->private;

    /* Configure compare register */
    vmm_writel(vmm_readl(bcc->system_clock) + next, bcc->compare);

    return VMM_OK;
}

static int __init bcm2835_clock_chip_init(vmm_device_tree_node_t *node)
{
    int                        rc;
    uint32_t                   clock, hirq;
    struct bcm2835_clock_chip *bcc;

    /* Read clock frequency */
    rc = vmm_device_tree_clock_frequency(node, &clock);

    if (rc) {
        return rc;
    }

    /* Read irq attribute */
    hirq = vmm_device_tree_irq_parse_map(node, DEFAULT_TIMER);

    if (!hirq) {
        return VMM_ERR_NODEV;
    }

    bcc = vmm_zalloc(sizeof(struct bcm2835_clock_chip));

    if (!bcc) {
        return VMM_ERR_NOMEM;
    }

    /* Map timer registers */
    rc = vmm_device_tree_regmap(node, &bcc->base, 0);

    if (rc) {
        vmm_free(bcc);
        return rc;
    }

    bcc->system_clock        = (void *)(bcc->base + REG_COUNTER_LO);
    bcc->control             = (void *)(bcc->base + REG_CONTROL);
    bcc->compare             = (void *)(bcc->base + REG_COMPARE(DEFAULT_TIMER));
    bcc->match_mask          = 1 << DEFAULT_TIMER;

    /* Setup clockchip */
    bcc->clock_chip.name     = "bcm2835-clock_chip";
    bcc->clock_chip.hirq     = hirq;
    bcc->clock_chip.rating   = 300;
    bcc->clock_chip.cpumask  = cpu_all_mask;
    bcc->clock_chip.features = VMM_CLOCKCHIP_FEAT_ONESHOT;
    bcc->clock_chip.freq     = clock;
    vmm_clocks_calc_mult_shift(&bcc->clock_chip.mult, &bcc->clock_chip.shift, VMM_NSEC_PER_SEC, clock, 10);
    bcc->clock_chip.min_delta_ns   = vmm_clock_chip_delta2ns(MIN_REG_COMPARE, &bcc->clock_chip);
    bcc->clock_chip.max_delta_ns   = vmm_clock_chip_delta2ns(MAX_REG_COMPARE, &bcc->clock_chip);
    bcc->clock_chip.set_mode       = &bcm2835_clock_chip_set_mode;
    bcc->clock_chip.set_next_event = &bcm2835_clock_chip_set_next_event;
    bcc->clock_chip.private        = bcc;

    /* Make sure compare register is set to zero */
    vmm_writel(0x0, bcc->compare);

    /* Make sure pending timer interrupts acknowledged */
    if (vmm_readl(bcc->control) & bcc->match_mask) {
        vmm_writel(bcc->match_mask, bcc->control);
    }

    /* Register interrupt handler */
    rc = vmm_host_irq_register(hirq, "bcm2835_timer", &bcm2835_clock_chip_irq_handler, bcc);

    if (rc) {
        vmm_device_tree_regunmap(node, bcc->base, 0);
        vmm_free(bcc);
        return rc;
    }

    /* Register clockchip */
    rc = vmm_clock_chip_register(&bcc->clock_chip);

    if (rc) {
        vmm_host_irq_unregister(hirq, bcc);
        vmm_device_tree_regunmap(node, bcc->base, 0);
        vmm_free(bcc);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKCHIP_INIT_DECLARE(bcm2835clock_chip, "brcm,bcm2835-system-timer", bcm2835_clock_chip_init);
