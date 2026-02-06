/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file i8259.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Definitions related to i8259 PIC.
 *
 * This file has been largely adapted from QEMU source code. (i8259_internal.h)
 * Copyright (c) 2011 Jan Kiszka, Siemens AG
 */

#ifndef _I8259_INTERNAL_H
#define _I8259_INTERNAL_H

#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_types.h>

typedef struct i8259_state i8259_state_t;

struct i8259_state {
    double_list_t head;

    uint8_t           last_irr;     /* edge detection */
    uint8_t           irr;          /* interrupt request register */
    uint8_t           imr;          /* interrupt mask register */
    uint8_t           isr;          /* interrupt service register */
    uint8_t           priority_add; /* highest irq priority */
    uint8_t           int_base;     /* base of CPU programmed vector */
    uint8_t           read_reg_select;
    uint8_t           poll;
    uint8_t           special_mask;
    uint8_t           init_state;
    uint8_t           auto_eoi;
    uint8_t           rotate_on_auto_eoi;
    uint8_t           special_fully_nested_mode;
    uint8_t           init4;       /* true if 4 byte init */
    uint8_t           single_mode; /* true if slave pic is not initialized */
    uint8_t           elcr;        /* PIIX edge/trigger selection*/
    uint8_t           elcr_mask;
    uint32_t          master;      /* reflects /SP input pin */
    uint32_t          iobase;
    uint32_t          elcr_addr;
    struct vmm_guest *guest;
    vmm_spinlock_t    lock;
    uint32_t          base_irq;
    uint32_t          num_irq;
    uint32_t          parent_irq;
    uint32_t          pic_slave_id; /* valid only if slave */
};

void  arch_set_guest_master_pic(struct vmm_guest *guest, struct i8259_state *s);
void  arch_set_guest_pic_list(struct vmm_guest *guest, void *pic_list);
void *arch_get_guest_pic_list(struct vmm_guest *guest);
int   pic_read_irq(i8259_state_t *s);

#endif /* !_I8259_INTERNAL_H */
