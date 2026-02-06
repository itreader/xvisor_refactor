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
 * @file sp804.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM SP804 Dual-Mode Timer source
 */

#include <arch_io.h>
#include <basic_irq.h>
#include <timer/sp804.h>

#define TIMER_LOAD          0x00
#define TIMER_VALUE         0x04
#define TIMER_CTRL          0x08
#define TIMER_CTRL_ONESHOT  (1 << 0)
#define TIMER_CTRL_32BIT    (1 << 1)
#define TIMER_CTRL_DIV1     (0 << 2)
#define TIMER_CTRL_DIV16    (1 << 2)
#define TIMER_CTRL_DIV256   (2 << 2)
#define TIMER_CTRL_IE       (1 << 5) /* Interrupt Enable (versatile only) */
#define TIMER_CTRL_PERIODIC (1 << 6)
#define TIMER_CTRL_ENABLE   (1 << 7)

#define TIMER_INTCLR        0x0c
#define TIMER_RIS           0x10
#define TIMER_MIS           0x14
#define TIMER_BGLOAD        0x18

static uint32_t sp804_base;
static uint32_t sp804_irq;
static uint64_t sp804_irq_count;
static uint64_t sp804_irq_tcount;
static uint64_t sp804_irq_tstamp;
static uint64_t sp804_irq_delay;
static uint64_t sp804_counter_mask;
static uint64_t sp804_counter_shift;
static uint64_t sp804_counter_mult;
static uint64_t sp804_counter_last;
static uint64_t sp804_time_stamp;

void sp804_enable(void)
{
    uint32_t ctrl;

    ctrl = arch_readl((void *)(sp804_base + TIMER_CTRL));
    ctrl |= TIMER_CTRL_ENABLE;
    arch_writel(ctrl, (void *)(sp804_base + TIMER_CTRL));
}

void sp804_disable(void)
{
    uint32_t ctrl;

    ctrl = arch_readl((void *)(sp804_base + TIMER_CTRL));
    ctrl &= ~TIMER_CTRL_ENABLE;
    arch_writel(ctrl, (void *)(sp804_base + TIMER_CTRL));
}

void sp804_change_period(uint32_t usec)
{
    arch_writel(usec, (void *)(sp804_base + TIMER_LOAD));
}

uint64_t sp804_irqcount(void)
{
    return sp804_irq_count;
}

uint64_t sp804_irqdelay(void)
{
    return sp804_irq_delay;
}

uint64_t sp804_timestamp(void)
{
    uint64_t sp804_counter_now, sp804_counter_delta, offset;
    sp804_counter_now   = ~arch_readl((void *)(sp804_base + 0x20 + TIMER_VALUE));
    sp804_counter_delta = (sp804_counter_now - sp804_counter_last) & sp804_counter_mask;
    sp804_counter_last  = sp804_counter_now;
    offset              = (sp804_counter_delta * sp804_counter_mult) >> sp804_counter_shift;
    sp804_time_stamp += offset;
    return sp804_time_stamp;
}

int sp804_irqhndl(uint32_t irq_no, struct pt_regs *regs)
{
    uint64_t tstamp = sp804_timestamp();

    if (!sp804_irq_tstamp) {
        sp804_irq_tstamp = tstamp;
    }

    if (sp804_irq_tcount == 256) {
        sp804_irq_delay  = (tstamp - sp804_irq_tstamp) >> 8;
        sp804_irq_tcount = 0;
        sp804_irq_tstamp = tstamp;
    }

    sp804_irq_tcount++;
    sp804_irq_count++;

    arch_writel(1, (void *)(sp804_base + TIMER_INTCLR));

    return 0;
}

int sp804_init(uint32_t usecs, uint32_t base, uint32_t irq, uint64_t counter_mask, uint64_t counter_mult, uint64_t counter_shift)
{
    uint32_t val;

    sp804_base          = base;

    sp804_counter_mask  = counter_mask;
    sp804_counter_shift = counter_shift;
    sp804_counter_mult  = counter_mult;
    sp804_counter_last  = 0;
    sp804_time_stamp    = 0;

    sp804_irq           = irq;
    sp804_irq_count     = 0;
    sp804_irq_tcount    = 0;
    sp804_irq_tstamp    = 0;
    sp804_irq_delay     = 0;

    /* Register interrupt handler */
    basic_irq_register(sp804_irq, &sp804_irqhndl);

    /* Setup Timer0 for generating irq */
    val = arch_readl((void *)(sp804_base + TIMER_CTRL));
    val &= ~TIMER_CTRL_ENABLE;
    val |= (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_IE);
    arch_writel(val, (void *)(sp804_base + TIMER_CTRL));
    sp804_change_period(usecs);

    /* Setup Timer1 for free running counter */
    arch_writel(0x0, (void *)(sp804_base + 0x20 + TIMER_CTRL));
    arch_writel(0xFFFFFFFF, (void *)(sp804_base + 0x20 + TIMER_LOAD));
    val = (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_ENABLE);
    arch_writel(val, (void *)(sp804_base + 0x20 + TIMER_CTRL));

    return 0;
}
