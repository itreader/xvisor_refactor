/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file imx_gpt.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief i.MX6 GPT timer
 */
#ifndef _ARM_IMX_GPT_H__
#define _ARM_IMX_GPT_H__

#include <arch_types.h>

void     imx_gpt_enable(void);
void     imx_gpt_disable(void);
uint64_t imx_gpt_irqcount(void);
uint64_t imx_gpt_irqdelay(void);
uint64_t imx_gpt_timestamp(void);
void     imx_gpt_change_period(uint32_t usecs);
int      imx_gpt_init(uint32_t usecs, uint32_t base, uint32_t irq, uint32_t freerun);

#endif
