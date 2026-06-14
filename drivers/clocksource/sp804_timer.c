/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SP804 Dual-Mode Timer Implementation
 */

#include <drv/clk.h>
#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define TIMER_LOAD          0x00
#define TIMER_VALUE         0x04
#define TIMER_CTRL          0x08
#define TIMER_CTRL_ONESHOT  (1 << 0)
#define TIMER_CTRL_32BIT    (1 << 1)
#define TIMER_CTRL_DIV1     (0 << 2)
#define TIMER_CTRL_DIV16    (1 << 2)
#define TIMER_CTRL_DIV256   (2 << 2)
#define TIMER_CTRL_IE       (1 << 5)
#define TIMER_CTRL_PERIODIC (1 << 6)
#define TIMER_CTRL_ENABLE   (1 << 7)
#define TIMER_INTCLR        0x0c
#define TIMER_RIS           0x10
#define TIMER_MIS           0x14
#define TIMER_BGLOAD        0x18

static long __init sp804_get_clock_rate(struct clk *clk)
{
    long rate;
    int  err;

    err = clock_prepare(clk);

    if (err) {
        vmm_printf("sp804: clock failed to prepare: %d\n", err);
        clock_put(clk);
        return err;
    }

    err = clock_enable(clk);

    if (err) {
        vmm_printf("sp804: clock failed to enable: %d\n", err);
        clock_unprepare(clk);
        clock_put(clk);
        return err;
    }

    rate = clock_get_rate(clk);

    if (rate < 0) {
        vmm_printf("sp804: clock failed to get rate: %ld\n", rate);
        clock_disable(clk);
        clock_unprepare(clk);
        clock_put(clk);
    }

    return rate;
}

struct sp804_clocksource {
    virtual_addr_t    base;
    vmm_clocksource_t clock_src;
};

static uint64_t sp804_clocksource_read(vmm_clocksource_t *cs)
{
    uint32_t                  count;
    struct sp804_clocksource *tcs = cs->private;

    count                         = vmm_readl((void *)(tcs->base + TIMER_VALUE));

    return ~count;
}

static int __init sp804_clocksource_init(vmm_device_tree_node_t *node)
{
    int                       rc;
    long                      hz;
    uint32_t                  ctrl, freq_hz;
    virtual_addr_t            base;
    struct clk               *clk;
    struct sp804_clocksource *cs;

    rc = vmm_device_tree_request_regmap(node, &base, 0, "SP804 Timer");

    if (rc) {
        return rc;
    }

    clk = of_clock_get(node, 1);

    if (!clk) {
        clk = of_clock_get(node, 0);
    }

    if (!clk) {
        clk = clock_get_sys("sp804", "arm,sp804");
    }

    if (!clk) {
        vmm_device_tree_regunmap_release(node, base, 0);
        return VMM_ERR_NODEV;
    }

    hz = sp804_get_clock_rate(clk);

    if (hz < 0) {
        vmm_device_tree_regunmap_release(node, base, 0);
        return (int)hz;
    }

    freq_hz = (uint32_t)hz;

    DPRINTF("%s: name=%s base=0x%08x freq_hz=%d\n", __func__, node->name, base, freq_hz);

    cs = vmm_zalloc(sizeof(struct sp804_clocksource));

    if (!cs) {
        return VMM_ERR_FAIL;
    }

    cs->base             = base;
    cs->clock_src.name   = node->name;
    cs->clock_src.rating = 300;
    cs->clock_src.read   = &sp804_clocksource_read;
    cs->clock_src.mask   = VMM_CLOCKSOURCE_MASK(32);
    cs->clock_src.freq   = freq_hz;
    vmm_clocks_calc_mult_shift(&cs->clock_src.mult, &cs->clock_src.shift, freq_hz, VMM_NSEC_PER_SEC, 10);
    cs->clock_src.private = cs;

    vmm_writel(0x0, (void *)(base + TIMER_CTRL));
    vmm_writel(0xFFFFFFFF, (void *)(base + TIMER_LOAD));
    ctrl = (TIMER_CTRL_ENABLE | TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC);
    vmm_writel(ctrl, (void *)(base + TIMER_CTRL));

    return vmm_clocksource_register(&cs->clock_src);
}

VMM_CLOCKSOURCE_INIT_DECLARE(sp804clock_src, "arm,sp804", sp804_clocksource_init);

struct sp804_clock_chip {
    virtual_addr_t   base;
    vmm_clock_chip_t clock_chip;
};

static vmm_irq_return_t sp804_clock_chip_irq_handler(int irq_no, void *dev)
{
    struct sp804_clock_chip *tcc = dev;

    /* clear the interrupt */
    vmm_writel(1, (void *)(tcc->base + TIMER_INTCLR));

    tcc->clock_chip.event_handler(&tcc->clock_chip);

    return VMM_IRQ_HANDLED;
}

static void sp804_clock_chip_set_mode(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc)
{
    struct sp804_clock_chip *tcc  = cc->private;
    uint64_t                 ctrl = TIMER_CTRL_32BIT | TIMER_CTRL_IE;

    vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));

    switch (mode) {
        case VMM_CLOCKCHIP_MODE_PERIODIC:
            /* FIXME: */
            vmm_writel(10000, (void *)(tcc->base + TIMER_LOAD));
            ctrl |= TIMER_CTRL_PERIODIC | TIMER_CTRL_ENABLE;
            break;

        case VMM_CLOCKCHIP_MODE_ONESHOT:
            ctrl |= TIMER_CTRL_ONESHOT;
            break;

        case VMM_CLOCKCHIP_MODE_UNUSED:
        case VMM_CLOCKCHIP_MODE_SHUTDOWN:
        default:
            break;
    }

    vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));
}

static int sp804_clock_chip_set_next_event(uint64_t next, vmm_clock_chip_t *cc)
{
    struct sp804_clock_chip *tcc  = cc->private;
    uint64_t                 ctrl = vmm_readl((void *)(tcc->base + TIMER_CTRL));

    vmm_writel(next, (void *)(tcc->base + TIMER_LOAD));
    vmm_writel(ctrl | TIMER_CTRL_ENABLE, (void *)(tcc->base + TIMER_CTRL));

    return VMM_OK;
}

static int __init sp804_clock_chip_init(vmm_device_tree_node_t *node)
{
    int                      rc;
    long                     hz;
    uint32_t                 hirq, freq_hz;
    virtual_addr_t           base;
    struct clk              *clk;
    struct sp804_clock_chip *cc;

    hirq = vmm_device_tree_irq_parse_map(node, 0);

    if (!hirq) {
        return VMM_ERR_NODEV;
    }

    rc = vmm_device_tree_regmap(node, &base, 0);

    if (rc) {
        return rc;
    }

    base += 0x20;

    clk = of_clock_get(node, 1);

    if (VMM_IS_ERR_OR_NULL(clk)) {
        clk = of_clock_get(node, 0);
    }

    if (VMM_IS_ERR_OR_NULL(clk)) {
        clk = clock_get_sys("sp804", "arm,sp804");
    }

    if (VMM_IS_ERR_OR_NULL(clk)) {
        vmm_device_tree_regunmap(node, base, 0);
        return VMM_PTR_ERR(clk);
    }

    hz = sp804_get_clock_rate(clk);

    if (hz < 0) {
        vmm_device_tree_regunmap(node, base, 0);
        return (int)hz;
    }

    freq_hz = (uint32_t)hz;

    DPRINTF("%s: name=%s base=0x%08x freq_hz=%d\n", __func__, node->name, base, freq_hz);

    cc = vmm_zalloc(sizeof(struct sp804_clock_chip));

    if (!cc) {
        vmm_device_tree_regunmap(node, base, 0);
        return VMM_ERR_FAIL;
    }

    cc->base                = base;
    cc->clock_chip.name     = node->name;
    cc->clock_chip.hirq     = hirq;
    cc->clock_chip.rating   = 300;
    cc->clock_chip.cpumask  = cpu_all_mask;
    cc->clock_chip.features = VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
    cc->clock_chip.freq     = freq_hz;
    vmm_clocks_calc_mult_shift(&cc->clock_chip.mult, &cc->clock_chip.shift, VMM_NSEC_PER_SEC, freq_hz, 10);
    cc->clock_chip.min_delta_ns   = vmm_clock_chip_delta2ns(0xF, &cc->clock_chip);
    cc->clock_chip.max_delta_ns   = vmm_clock_chip_delta2ns(0xFFFFFFFF, &cc->clock_chip);
    cc->clock_chip.set_mode       = &sp804_clock_chip_set_mode;
    cc->clock_chip.set_next_event = &sp804_clock_chip_set_next_event;
    cc->clock_chip.private        = cc;

    /* Register interrupt handler */
    rc                            = vmm_host_irq_register(hirq, node->name, &sp804_clock_chip_irq_handler, cc);

    if (rc) {
        vmm_free(cc);
        return rc;
    }

    /* Register clockchip */
    rc = vmm_clock_chip_register(&cc->clock_chip);

    if (rc) {
        vmm_host_irq_unregister(hirq, cc);
        vmm_free(cc);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKCHIP_INIT_DECLARE(sp804clock_chip, "arm,sp804", sp804_clock_chip_init);
