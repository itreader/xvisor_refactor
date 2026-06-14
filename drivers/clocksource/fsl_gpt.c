/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
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
 * @file gpt.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief source file for GPT timer support, based on epit.c.
 */

#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

#define GPTCR            (0x00)
#define GPTPR            (0x04)
#define GPTSR            (0x08)
#define GPTIR            (0x0C)
#define GPTOCR1          (0x10)
#define GPTOCR2          (0x14)
#define GPTOCR3          (0x18)
#define GPTICR1          (0x1c)
#define GPTICR2          (0x20)
#define GPTCNT           (0x24)

#define GPTCR_SWRST      (1 << 15)
#define GPTCR_CLK_MASK   (7 << 6)
#define GPTCR_CLK_32K    (4 << 6)
#define GPTCR_EN         (1 << 0)

#define GPTPR_PRESC_MASK (0xFFF)

#define GPTIR_ROV        (1 << 5)
#define GPTIR_IF2        (1 << 4)
#define GPTIR_IF1        (1 << 3)
#define GPTIR_OF3        (1 << 2)
#define GPTIR_OF2        (1 << 1)
#define GPTIR_OF1        (1 << 0)

#define GPTSR_ROV        (1 << 5)
#define GPTSR_IF2        (1 << 4)
#define GPTSR_IF1        (1 << 3)
#define GPTSR_OF3        (1 << 2)
#define GPTSR_OF2        (1 << 1)
#define GPTSR_OF1        (1 << 0)
#define GPTSR_ALL        (GPTSR_ROV | GPTSR_IF2 | GPTSR_IF1 | GPTSR_OF3 | GPTSR_OF2 | GPTSR_OF1)

struct gpt_clocksource {
    virtual_addr_t    base;
    vmm_clocksource_t clock_src;
};

static uint64_t gpt_clock_src_read(vmm_clocksource_t *cs)
{
    struct gpt_clocksource *gcs = cs->private;

    return vmm_readl((uint32_t *)(gcs->base + GPTCNT));
}

static int __init gpt_clocksource_init(vmm_device_tree_node_t *node)
{
    int                     rc;
    uint32_t                clock;
    struct gpt_clocksource *gcs;
    uint32_t                status;

    /* Read clock frequency from node */
    rc = vmm_device_tree_clock_frequency(node, &clock);

    if (rc) {
        goto fail;
    }

    /* allocate our struct */
    gcs = vmm_zalloc(sizeof(struct gpt_clocksource));

    if (!gcs) {
        rc = VMM_ERR_NOMEM;
        goto fail;
    }

    /* Map timer registers */
    rc = vmm_device_tree_request_regmap(node, &gcs->base, 0, "Freescale GPT");

    if (rc) {
        goto regmap_fail;
    }

    /*
     * Disable all interrupts
     */
    vmm_writel(0, (uint32_t *)(gcs->base + GPTIR));

    /*
     * Enable the timer.
     * We also need to make sure a valid clocksource is selected.
     */
    status = vmm_readl((uint32_t *)(gcs->base + GPTCR));

    /*
     * If no clocksource is selected then we select the default
     * 32KHz clock
     * If a clock source is selected, we assume the value in the
     * device tree is he correct one.
     */
    if (!(status & GPTCR_CLK_MASK)) {
        /*
         * We need to disable the timer to change the clocksource
         */
        vmm_writel(status & ~GPTCR_EN, (uint32_t *)(gcs->base + GPTCR));

        clock = 32768;
        status |= GPTCR_CLK_32K;

        /*
         * Change the value of frequency in the device tree in order
         * to match the value we are going to set.
         */
        vmm_device_tree_setattr(node, VMM_DEVICE_TREE_CLOCK_FREQ_ATTR_NAME, &clock, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(clock), FALSE);
    }

    /* Setup clocksource */
    gcs->clock_src.name   = node->name;
    gcs->clock_src.rating = 300;
    gcs->clock_src.read   = gpt_clock_src_read;
    gcs->clock_src.mask   = VMM_CLOCKSOURCE_MASK(32);
    gcs->clock_src.freq   = clock;
    vmm_clocks_calc_mult_shift(&gcs->clock_src.mult, &gcs->clock_src.shift, clock, VMM_NSEC_PER_SEC, 10);
    gcs->clock_src.private = gcs;

    /*
     * Enable the timer.
     */
    vmm_writel(status | GPTCR_EN, (uint32_t *)(gcs->base + GPTCR));

    /* Register clocksource */
    rc = vmm_clocksource_register(&gcs->clock_src);

    if (rc) {
        goto register_fail;
    }

    return VMM_OK;

register_fail:
    vmm_device_tree_regunmap(node, gcs->base, 0);
regmap_fail:
    vmm_free(gcs);
fail:
    return rc;
}

VMM_CLOCKSOURCE_INIT_DECLARE(gptclock_src, "freescale,gpt-timer", gpt_clocksource_init);
