/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file arch_board.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief boarch specific functions Header
 */
#ifndef __ARCH_BOARD_H__
#define __ARCH_BOARD_H__

#include <arch_types.h>

void            arch_board_reset(void);
void            arch_board_init(void);
char           *arch_board_name(void);
physical_addr_t arch_board_ram_start(void);
physical_size_t arch_board_ram_size(void);
physical_addr_t arch_board_autoexec_addr(void);
uint32_t        arch_board_boot_delay(void);
void            arch_board_linux_default_cmdline(char *cmdline, uint32_t cmdline_sz);
void            arch_board_fdt_fixup(void *fdt_addr);

uint32_t        arch_board_iosection_count(void);
physical_addr_t arch_board_iosection_addr(int num);

uint32_t arch_board_pic_nr_irqs(void);
int      arch_board_pic_init(void);
uint32_t arch_board_pic_active_irq(void);
int      arch_board_pic_ack_irq(uint32_t irq);
int      arch_board_pic_eoi_irq(uint32_t irq);
int      arch_board_pic_mask(uint32_t irq);
int      arch_board_pic_unmask(uint32_t irq);

void     arch_board_timer_enable(void);
void     arch_board_timer_disable(void);
uint64_t arch_board_timer_irqcount(void);
uint64_t arch_board_timer_irqdelay(void);
uint64_t arch_board_timer_timestamp(void);
void     arch_board_timer_change_period(uint32_t usecs);
int      arch_board_timer_init(uint32_t usecs);

int  arch_board_serial_init(void);
void arch_board_serial_putc(char ch);
bool arch_board_serial_can_getc(void);
char arch_board_serial_getc(void);

#endif
