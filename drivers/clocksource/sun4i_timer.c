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
 * @file sun4i_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Allwinner Sun4i timer
 */

#include <drv/clk.h>
#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_main.h>
#include <vmm_types.h>

/* Register read/write macros */
#define readl(addr)             vmm_readl((void *)(addr))
#define writel(val, addr)       vmm_writel((val), (void *)(addr))

/* Define timer clock source */
#define AW_TMR_CLK_SRC_32KLOSC  (0)
#define AW_TMR_CLK_SRC_24MHOSC  (1)
#define AW_TMR_CLK_SRC_PLL      (2)

/* Config clock frequency   */
#define AW_HPET_CLK_SRC         AW_TMR_CLK_SRC_24MHOSC
#define AW_HPET_CLOCK_SOURCE_HZ (24000000)

#define AW_HPET_CLK_EVT         AW_TMR_CLK_SRC_24MHOSC
#define AW_HPET_CLOCK_EVENT_HZ  (24000000)

/* AW timer registers offsets */
#define AW_TMR_REG_IRQ_EN       (0x0000)
#define AW_TMR_REG_IRQ_STAT     (0x0004)
#define AW_TMR_REG_CTL(off)     ((off) + 0x0)
#define AW_TMR_REG_INTV(off)    ((off) + 0x4)
#define AW_TMR_REG_CUR(off)     ((off) + 0x8)
#define AW_TMR_REG_CNT64_CTL    (0x00A0)
#define AW_TMR_REG_CNT64_LO     (0x00A4)
#define AW_TMR_REG_CNT64_HI     (0x00A8)
#define AW_TMR_REG_CPU_CFG      (0x013C)

#define TMRx_CTL_ENABLE         (1 << 0)
#define TMRx_CTL_AUTORELOAD     (1 << 1)
#define TMRx_CTL_SRC_32KLOSC    (0 << 2)
#define TMRx_CTL_SRC_24MHOSC    (1 << 2)
#define TMRx_CTL_ONESHOT        (1 << 7)

#define CNT64_CTL_LATCH         (1 << 1)
#define CNT64_CTL_SRC_24MHOSC   (0 << 2)
#define CNT64_CTL_SRC_PLL6      (1 << 2)
#define CNT64_CTL_CLEAR         (1 << 0)

#define CPU_CFG_L2_CACHE_INV    (1 << 0)
#define CPU_CFG_L1_CACHE_INV    (1 << 1)
#define CPU_CFG_CHIP_VER_SHIFT  6
#define CPU_CFG_CHIP_VER_MASK   0x3

struct aw_clocksource {
    virtual_addr_t    base;
    vmm_clocksource_t clock_src;
};

static uint64_t aw_clock_src_read(vmm_clocksource_t *cs)
{
    uint32_t               lower, upper, tmp;
    irq_flags_t            flags;
    struct aw_clocksource *acs = cs->private;

    /* Save irq, for atomicity */
    arch_cpu_irq_save(flags);

    /* Latch 64-bit counter */
    tmp = readl(acs->base + AW_TMR_REG_CNT64_CTL);
    tmp |= CNT64_CTL_LATCH;
    writel(tmp, acs->base + AW_TMR_REG_CNT64_CTL);

    while (readl(acs->base + AW_TMR_REG_CNT64_CTL) & CNT64_CTL_LATCH)
        ;

    /* Read 64-bit counter */
    lower = readl(acs->base + AW_TMR_REG_CNT64_LO);
    upper = readl(acs->base + AW_TMR_REG_CNT64_HI);

    /* Restore irq */
    arch_cpu_irq_restore(flags);

    return (((uint64_t)upper) << 32) | ((uint64_t)lower);
}

static int __init aw_timer_clocksource_init(vmm_device_tree_node_t *node)
{
    int                    rc;
    uint32_t               tmp;
    uint64_t               rate = 0;
    struct clk            *clk;
    struct aw_clocksource *acs;

    /* Find clock for timer */
    clk = of_clock_get(node, 0);

    if (VMM_IS_ERR_OR_NULL(clk)) {
        vmm_panic("Can't get timer clock");
    }

    /* Enable clock and get rate */
    clock_prepare_enable(clk);
    rate = clock_get_rate(clk);

    /* Alloc clocksource instance */
    acs  = vmm_zalloc(sizeof(struct aw_clocksource));

    if (!acs) {
        return VMM_ENOMEM;
    }

    /* Map timer registers */
    rc = vmm_device_tree_request_regmap(node, &acs->base, 0, "Sun4i Timer");

    if (rc) {
        vmm_free(acs);
        return rc;
    }

    /* Clear counter settings */
    writel(0, acs->base + AW_TMR_REG_CNT64_CTL);
    /* __delay(50); */

    /* Config clock source for 64bits counter */
    tmp = readl(acs->base + AW_TMR_REG_CNT64_CTL);
    tmp &= ~CNT64_CTL_SRC_PLL6;
    writel(tmp, acs->base + AW_TMR_REG_CNT64_CTL);
    /* __delay(50); */

    /* Clear 64bits counter */
    tmp = readl(acs->base + AW_TMR_REG_CNT64_CTL);
    writel(tmp | CNT64_CTL_CLEAR, acs->base + AW_TMR_REG_CNT64_CTL);
    /* __delay(50); */

    /* Setup clocksource */
    acs->clock_src.name    = "aw-clock_src";
    acs->clock_src.rating  = 350;
    acs->clock_src.read    = aw_clock_src_read;
    acs->clock_src.mask    = VMM_CLOCKSOURCE_MASK(64);
    acs->clock_src.freq    = rate;
    acs->clock_src.shift   = 10;
    acs->clock_src.mult    = vmm_clocksource_hz2mult(rate, acs->clock_src.shift);
    acs->clock_src.private = acs;

    /* Register clocksource */
    rc                     = vmm_clocksource_register(&acs->clock_src);

    if (rc) {
        vmm_device_tree_regunmap_release(node, acs->base, 0);
        vmm_free(acs);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKSOURCE_INIT_DECLARE(sun4iclock_src, "allwinner,sun4i-timer", aw_timer_clocksource_init);

struct aw_clock_chip {
    uint32_t         num, off;
    virtual_addr_t   base;
    vmm_clock_chip_t clock_chip;
};

static vmm_irq_return_t aw_clock_chip_irq_handler(int irq_no, void *dev)
{
    struct aw_clock_chip *acc = dev;

    /* Clear pending irq */
    writel((1 << acc->num), acc->base + AW_TMR_REG_IRQ_STAT);

    acc->clock_chip.event_handler(&acc->clock_chip);

    return VMM_IRQ_HANDLED;
}

static void aw_clock_chip_set_mode(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc)
{
    uint32_t              ctrl;
    struct aw_clock_chip *acc = cc->private;

    /* Read timer control register */
    ctrl                      = readl(acc->base + AW_TMR_REG_CTL(acc->off));

    /* Disable timer and clear pending first */
    ctrl &= ~TMRx_CTL_ENABLE;
    writel(ctrl, acc->base + AW_TMR_REG_CTL(acc->off));

    /* Determine updates to timer control register */
    switch (mode) {
        case VMM_CLOCKCHIP_MODE_PERIODIC:
            ctrl &= ~TMRx_CTL_ONESHOT;
            ctrl |= TMRx_CTL_ENABLE;
            /* FIXME: */
            writel(0, acc->base + AW_TMR_REG_INTV(acc->off));
            break;

        case VMM_CLOCKCHIP_MODE_ONESHOT:
            ctrl |= TMRx_CTL_ONESHOT;
            break;

        case VMM_CLOCKCHIP_MODE_UNUSED:
        case VMM_CLOCKCHIP_MODE_SHUTDOWN:
            break;

        default:
            break;
    }

    /* Update timer control register */
    writel(ctrl, acc->base + AW_TMR_REG_CTL(acc->off));
}

static int aw_clock_chip_set_next_event(uint64_t next, vmm_clock_chip_t *cc)
{
    uint32_t              ctrl;
    struct aw_clock_chip *acc = cc->private;

    /* Read timer control register */
    ctrl                      = readl(acc->base + AW_TMR_REG_CTL(acc->off));

    /* Disable timer and clear pending first */
    ctrl &= ~TMRx_CTL_ENABLE;
    writel(ctrl, acc->base + AW_TMR_REG_CTL(acc->off));

    /* Set interval register */
    writel(next, acc->base + AW_TMR_REG_INTV(acc->off));

    /* Start timer */
    ctrl |= (TMRx_CTL_ENABLE | TMRx_CTL_AUTORELOAD);
    writel(ctrl, acc->base + AW_TMR_REG_CTL(acc->off));

    return VMM_OK;
}

static int __init aw_timer_clock_chip_init(vmm_device_tree_node_t *node)
{
    int                   rc;
    uint32_t              hirq, tmp;
    uint64_t              rate = 0;
    struct clk           *clk;
    struct aw_clock_chip *acc;

    /* Find clock for timer */
    clk = of_clock_get(node, 0);

    if (!clk) {
        vmm_panic("Can't get timer clock");
    }

    /* Enable clock and get rate */
    clock_prepare_enable(clk);
    rate = clock_get_rate(clk);

    /* Alloc clockchip instance */
    acc  = vmm_zalloc(sizeof(struct aw_clock_chip));

    if (!acc) {
        return VMM_ENOMEM;
    }

    /* Read reg_offset attribute */
    rc = vmm_device_tree_read_u32(node, "timer_num", &acc->num);

    if (rc) {
        vmm_free(acc);
        return VMM_ENOTAVAIL;
    }

    acc->off = 0x10 + 0x10 * acc->num;

    /* Read irq attribute */
    hirq     = vmm_device_tree_irq_parse_map(node, 0);

    if (!hirq) {
        vmm_free(acc);
        return VMM_ENODEV;
    }

    /* Map timer registers */
    rc = vmm_device_tree_regmap(node, &acc->base, 0);

    if (rc) {
        vmm_free(acc);
        return rc;
    }

    /* Clear timer control register */
    writel(0, acc->base + AW_TMR_REG_CTL(acc->off));

    /* Initialize timer interval value to zero */
    writel(0, acc->base + AW_TMR_REG_INTV(acc->off));

    /* Configure timer control register */
    tmp = readl(acc->base + AW_TMR_REG_CTL(acc->off));
    tmp |= TMRx_CTL_SRC_24MHOSC;
    tmp |= TMRx_CTL_AUTORELOAD;
    tmp &= ~(0x7 << 4);
    writel(tmp, acc->base + AW_TMR_REG_CTL(acc->off));

    /* Enable timer irq */
    tmp = readl(acc->base + AW_TMR_REG_IRQ_EN);
    tmp |= (1 << acc->num);
    writel(tmp, acc->base + AW_TMR_REG_IRQ_EN);

    /* Setup clockchip */
    acc->clock_chip.name           = "aw-clock_chip";
    acc->clock_chip.hirq           = hirq;
    acc->clock_chip.rating         = 350;
    acc->clock_chip.cpumask        = cpu_all_mask;
    acc->clock_chip.features       = VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
    acc->clock_chip.freq           = rate;
    acc->clock_chip.mult           = vmm_clock_chip_hz2mult(rate, 32);
    acc->clock_chip.shift          = 32;
    acc->clock_chip.min_delta_ns   = vmm_clock_chip_delta2ns(1, &acc->clock_chip) + 100000;
    acc->clock_chip.max_delta_ns   = vmm_clock_chip_delta2ns((0x80000000), &acc->clock_chip);
    acc->clock_chip.set_mode       = &aw_clock_chip_set_mode;
    acc->clock_chip.set_next_event = &aw_clock_chip_set_next_event;
    acc->clock_chip.private        = acc;

    /* Register interrupt handler */
    rc                             = vmm_host_irq_register(hirq, "aw-clock_chip", &aw_clock_chip_irq_handler, acc);

    if (rc) {
        vmm_device_tree_regunmap(node, acc->base, 0);
        vmm_free(acc);
        return rc;
    }

    /* Register clockchip */
    rc = vmm_clock_chip_register(&acc->clock_chip);

    if (rc) {
        vmm_host_irq_unregister(hirq, acc);
        vmm_device_tree_regunmap(node, acc->base, 0);
        vmm_free(acc);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKCHIP_INIT_DECLARE(sun4iclock_chip, "allwinner,sun4i-timer", aw_timer_clock_chip_init);
