/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file smp_twd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SMP Local Timer Implementation
 */

#include <drv/clk.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_per_cpu.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

#define TWD_TIMER_LOAD              0x00
#define TWD_TIMER_COUNTER           0x04
#define TWD_TIMER_CONTROL           0x08
#define TWD_TIMER_INTSTAT           0x0C

#define TWD_WDOG_LOAD               0x20
#define TWD_WDOG_COUNTER            0x24
#define TWD_WDOG_CONTROL            0x28
#define TWD_WDOG_INTSTAT            0x2C
#define TWD_WDOG_RESETSTAT          0x30
#define TWD_WDOG_DISABLE            0x34

#define TWD_TIMER_CONTROL_ENABLE    (1 << 0)
#define TWD_TIMER_CONTROL_ONESHOT   (0 << 1)
#define TWD_TIMER_CONTROL_PERIODIC  (1 << 1)
#define TWD_TIMER_CONTROL_IT_ENABLE (1 << 2)

struct twd_clock_chip {
    char             name[32];
    vmm_clock_chip_t clock_chip;
};

static DEFINE_PER_CPU(struct twd_clock_chip, twd_cc);
static uint32_t twd_freq_hz;

static struct clk    *twd_clock;
static virtual_addr_t twd_base;
static uint32_t       twd_ppi_irq;

static vmm_irq_return_t twd_clock_chip_irq_handler(int irq_no, void *dev)
{
    struct twd_clock_chip *tcc = &this_cpu(twd_cc);

    if (vmm_readl((void *)(twd_base + TWD_TIMER_INTSTAT))) {
        vmm_writel(1, (void *)(twd_base + TWD_TIMER_INTSTAT));
    }

    tcc->clock_chip.event_handler(&tcc->clock_chip);

    return VMM_IRQ_HANDLED;
}

static void twd_clock_chip_set_mode(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc)
{
    uint32_t ctrl;

    switch (mode) {
        case VMM_CLOCKCHIP_MODE_PERIODIC:
            /* timer load already set up */
            ctrl = TWD_TIMER_CONTROL_ENABLE | TWD_TIMER_CONTROL_IT_ENABLE | TWD_TIMER_CONTROL_PERIODIC;
            vmm_writel(
                twd_freq_hz / 100, /* Assuming HZ = 100 */
                (void *)(twd_base + TWD_TIMER_LOAD));
            break;

        case VMM_CLOCKCHIP_MODE_ONESHOT:
            /* period set, and timer enabled in 'next_event' hook */
            ctrl = TWD_TIMER_CONTROL_IT_ENABLE | TWD_TIMER_CONTROL_ONESHOT;
            break;

        case VMM_CLOCKCHIP_MODE_UNUSED:
        case VMM_CLOCKCHIP_MODE_SHUTDOWN:
        default:
            ctrl = 0;
            break;
    }

    vmm_writel(ctrl, (void *)(twd_base + TWD_TIMER_CONTROL));
}

static int twd_clock_chip_set_next_event(uint64_t next, vmm_clock_chip_t *cc)
{
    uint32_t ctrl = vmm_readl((void *)(twd_base + TWD_TIMER_CONTROL));

    ctrl |= TWD_TIMER_CONTROL_ENABLE;

    vmm_writel(next, (void *)(twd_base + TWD_TIMER_COUNTER));
    vmm_writel(ctrl, (void *)(twd_base + TWD_TIMER_CONTROL));

    return VMM_OK;
}

static int twd_clock_chip_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int                    rc;
    struct twd_clock_chip *cc = &this_cpu(twd_cc);

    memset(cc, 0, sizeof(struct twd_clock_chip));

    vmm_sprintf(cc->name, "twd/%d", cpu);

    cc->clock_chip.name     = cc->name;
    cc->clock_chip.hirq     = twd_ppi_irq;
    cc->clock_chip.rating   = 350;
    cc->clock_chip.cpumask  = vmm_cpumask_of(cpu);
    cc->clock_chip.features = VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
    vmm_clocks_calc_mult_shift(&cc->clock_chip.mult, &cc->clock_chip.shift, VMM_NSEC_PER_SEC, twd_freq_hz, 10);
    cc->clock_chip.min_delta_ns   = vmm_clock_chip_delta2ns(0xF, &cc->clock_chip);
    cc->clock_chip.max_delta_ns   = vmm_clock_chip_delta2ns(0xFFFFFFFF, &cc->clock_chip);
    cc->clock_chip.set_mode       = &twd_clock_chip_set_mode;
    cc->clock_chip.set_next_event = &twd_clock_chip_set_next_event;
    cc->clock_chip.private        = cc;

    /* Register interrupt handler */
    if ((rc = vmm_host_irq_register(twd_ppi_irq, "twd", &twd_clock_chip_irq_handler, cc))) {
        goto fail;
    }

    rc = vmm_clock_chip_register(&cc->clock_chip);

    if (rc) {
        goto fail_unreg_irq;
    }

    return VMM_OK;

fail_unreg_irq:
    vmm_host_irq_unregister(twd_ppi_irq, cc);
fail:
    return rc;
}

static vmm_cpu_hotplug_notify_t twd_clock_chip_cpu_hotplug = {
    .name    = "SMP_TWD",
    .state   = VMM_CPU_HOTPLUG_STATE_CLOCKCHIP,
    .startup = twd_clock_chip_startup,
};

static void __init twd_caliberate_freq(virtual_addr_t base, virtual_addr_t ref_counter_addr, uint32_t ref_counter_freq)
{
    uint32_t i, count, ref_count;
    uint64_t tmp;

    /* enable, no interrupt or reload */
    vmm_writel(0x1, (void *)(base + TWD_TIMER_CONTROL));

    /* read reference counter */
    ref_count = vmm_readl((void *)ref_counter_addr);

    /* maximum value */
    vmm_writel(0xFFFFFFFFU, (void *)(base + TWD_TIMER_COUNTER));

    /* wait some arbitary amount of time */
    for (i = 0; i < 1000000; i++)
        ;

    /* read counter */
    count     = vmm_readl((void *)(base + TWD_TIMER_COUNTER));
    count     = 0xFFFFFFFFU - count;

    /* take reference counter difference */
    ref_count = vmm_readl((void *)ref_counter_addr) - ref_count;

    /* disable */
    vmm_writel(0x0, (void *)(base + TWD_TIMER_CONTROL));

    /* determine frequency */
    tmp         = (uint64_t)count * (uint64_t)ref_counter_freq;
    twd_freq_hz = udiv64(tmp, ref_count);
}

static int __init twd_clock_chip_init(vmm_device_tree_node_t *node)
{
    int            rc = VMM_OK;
    uint32_t       ref_cnt_freq;
    virtual_addr_t ref_cnt_addr;

    if (!twd_base) {
        rc = vmm_device_tree_request_regmap(node, &twd_base, 0, "ARM Local Timer");

        if (rc) {
            goto fail;
        }
    }

    if (!twd_ppi_irq) {
        twd_ppi_irq = vmm_device_tree_irq_parse_map(node, 0);

        if (!twd_ppi_irq) {
            rc = VMM_ENODEV;
            goto fail_regunmap;
        }
    }

    if (!twd_freq_hz) {
        /* First try to find TWD clock */
        if (!twd_clock) {
            twd_clock = of_clock_get(node, 0);
        }

        if (VMM_IS_ERR_OR_NULL(twd_clock)) {
            twd_clock = clock_get_sys("smp_twd", NULL);
        }

        if (!VMM_IS_ERR_OR_NULL(twd_clock)) {
            /* Use TWD clock to find frequency */
            rc = clock_prepare_enable(twd_clock);

            if (rc) {
                clock_put(twd_clock);
                goto fail_regunmap;
            }

            twd_freq_hz = clock_get_rate(twd_clock);
        } else {
            /* No TWD clock found hence caliberate */
            rc = vmm_device_tree_regmap(node, &ref_cnt_addr, 1);

            if (rc) {
                goto fail_regunmap;
            }

            if (vmm_device_tree_read_u32(node, "ref-counter-freq", &ref_cnt_freq)) {
                vmm_device_tree_regunmap(node, ref_cnt_addr, 1);
                goto fail_regunmap;
            }

            twd_caliberate_freq(twd_base, ref_cnt_addr, ref_cnt_freq);
            vmm_device_tree_regunmap(node, ref_cnt_addr, 1);
        }
    }

    rc = vmm_cpu_hotplug_register(&twd_clock_chip_cpu_hotplug, TRUE);

    if (rc) {
        goto fail_regunmap;
    }

    return VMM_OK;

fail_regunmap:
    vmm_device_tree_regunmap_release(node, twd_base, 0);
fail:
    return rc;
}

VMM_CLOCKCHIP_INIT_DECLARE(ca9twd, "arm,cortex-a9-twd-timer", twd_clock_chip_init);
VMM_CLOCKCHIP_INIT_DECLARE(ca5twd, "arm,cortex-a5-twd-timer", twd_clock_chip_init);
VMM_CLOCKCHIP_INIT_DECLARE(arm11mptwd, "arm,arm11mp-twd-timer", twd_clock_chip_init);
