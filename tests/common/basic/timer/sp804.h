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
 * @file sp804.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM SP804 Dual-Mode Timer Header
 */
#ifndef _ARM_SP804_H__
#define _ARM_SP804_H__

#include <arch_types.h>

void     sp804_enable(void);
void     sp804_disable(void);
uint64_t sp804_irqcount(void);
uint64_t sp804_irqdelay(void);
uint64_t sp804_timestamp(void);
void     sp804_change_period(uint32_t usecs);
int      sp804_init(uint32_t usecs, uint32_t base, uint32_t irq, uint64_t counter_mask, uint64_t counter_mult, uint64_t counter_shift);

#endif
