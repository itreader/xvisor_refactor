/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * Inspired by sp804.c, Copyright (c) 2013 Anup Patel.
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
 * @file imx_gpt.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief i.MX6 GPT timer
 */

#include <arch_io.h>
#include <arch_math.h>
#include <basic_irq.h>
#include <timer/imx_gpt.h>

#define GPT_CR       0
#define GPT_CR_SW    (1 << 15)
#define GPT_CR_FRR   (1 << 9)
#define GPT_CR_ENMOD (1 << 1)
#define GPT_CR_EN    (1 << 0)
#define GPT_PR       0x4
#define GPT_PR_MASK  0xFFF
#define GPT_SR       0x8
#define GPT_SR_MASK  0x3F
#define GPT_IR       0xC
#define GPT_IR_MASK  0x3F
#define GPT_OC1      0x10
#define GPT_OC2      0x14
#define GPT_OC3      0x18
#define GPT_CNT      0x24
#define GPT_FREQ     32000

static uint32_t imx_gpt_base;
static uint32_t imx_gpt_irq;
static uint64_t imx_gpt_period        = 0;
static uint64_t imx_gpt_irq_count     = 0;
static uint64_t imx_gpt_irq_tstamp    = 0;
static uint64_t imx_gpt_irq_delay     = 0;
static uint64_t imx_gpt_freerun       = 0;
static uint64_t imx_gpt_timestamp_sum = 0;

void imx_gpt_enable(void)
{
    uint32_t ctrl;

    ctrl = arch_readl((void *)(imx_gpt_base + GPT_CR));
    ctrl |= GPT_CR_EN;
    arch_writel(ctrl, (void *)(imx_gpt_base + GPT_CR));
}

void imx_gpt_disable(void)
{
    uint32_t ctrl;

    ctrl = arch_readl((void *)(imx_gpt_base + GPT_CR));
    ctrl &= ~GPT_CR_EN;
    arch_writel(ctrl, (void *)(imx_gpt_base + GPT_CR));
}

static void imx_gpt_next_event(void)
{
    uint32_t cnt = 0;

    if (imx_gpt_freerun) {
        cnt = arch_readl((void *)(imx_gpt_base + GPT_CNT));
    }

    arch_writel(cnt + imx_gpt_period, (void *)(imx_gpt_base + GPT_OC1));
}

void imx_gpt_change_period(uint32_t usec)
{
    uint32_t rem;

    if (!imx_gpt_freerun) {
        imx_gpt_timestamp_sum += arch_readl((void *)(imx_gpt_base + GPT_CNT));
    }

    imx_gpt_period = do_udiv32(usec, (GPT_FREQ / 1000), &rem);
    imx_gpt_next_event();
}

uint64_t imx_gpt_irqcount(void)
{
    return imx_gpt_irq_count;
}

uint64_t imx_gpt_irqdelay(void)
{
    return imx_gpt_irq_delay;
}

uint64_t imx_gpt_timestamp(void)
{
    static uint32_t imx_gpt_counter_last = 0;
    static uint32_t imx_gpt_up           = 0;
    uint32_t        counter              = 0;
    uint64_t        timestamp;

    counter = arch_readl((void *)(imx_gpt_base + GPT_CNT));

    if (imx_gpt_freerun) {
        /*
         * In freerun mode, we only have to make a 64 bit counter
         * from a 32 bit one.
         */
        if (counter < imx_gpt_counter_last) {
            ++imx_gpt_up;
        };

        imx_gpt_counter_last = counter;

        timestamp            = imx_gpt_up;

        timestamp            = (timestamp << 32) | counter;
    } else {
        /*
         * In restart mode, we retrieve the period sum and the
         * current value
         */
        timestamp = imx_gpt_timestamp_sum + counter;
    }

    return timestamp * GPT_FREQ;
}

int imx_gpt_irqhndl(uint32_t irq_no, struct pt_regs *regs)
{
    uint64_t tstamp = 0;

    imx_gpt_disable();

    tstamp             = imx_gpt_timestamp();
    imx_gpt_irq_delay  = tstamp - imx_gpt_irq_tstamp;
    imx_gpt_irq_tstamp = tstamp;
    imx_gpt_irq_count++;

    if (!imx_gpt_freerun) {
        imx_gpt_timestamp_sum += imx_gpt_period;
    }

    imx_gpt_next_event();
    arch_writel(1, (void *)(imx_gpt_base + GPT_SR));
    imx_gpt_enable();

    return 0;
}

int imx_gpt_init(uint32_t usecs, uint32_t base, uint32_t irq, uint32_t freerun)
{
    uint32_t val;

    imx_gpt_base      = base;
    imx_gpt_irq       = irq;
    imx_gpt_irq_count = 0;

    /* Register interrupt handler */
    basic_irq_register(imx_gpt_irq, &imx_gpt_irqhndl);

    val = arch_readl((void *)(imx_gpt_base + GPT_CR));

    if (freerun) {
        imx_gpt_freerun = 1;
        val |= GPT_CR_FRR;
    }

    val |= GPT_CR_ENMOD;
    arch_writel(val, (void *)(imx_gpt_base + GPT_CR));

    imx_gpt_disable();
    imx_gpt_change_period(usecs);
    imx_gpt_enable();

    /* Setup Timer0 for generating irq */
    arch_writel(1, (void *)(imx_gpt_base + GPT_IR));

    return 0;
}
