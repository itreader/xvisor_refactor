/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_mptimer_emulator.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM MP private & watchdog timer emulator exported APIs
 */
#ifndef __ARM_MPTIMER_EMULATOR_H__
#define __ARM_MPTIMER_EMULATOR_H__
#include "arch_types.h"
#include "vmm_manager.h"
#include "vmm_types.h"

/** State is private to emulator */
struct mptimer_state;
struct vmm_emulate_device;
typedef struct vmm_emulate_device vmm_emulate_device_t;

/** MPTimer Register write */
int mptimer_reg_write(struct mptimer_state *s, uint32_t offset, uint32_t src_mask, uint32_t src);

/** MPTimer Register read */
int mptimer_reg_read(struct mptimer_state *s, uint32_t offset, uint32_t *dst);

/** Resets the MPTimer state */
int mptimer_state_reset(struct mptimer_state *mpt);

/** Allocate and initializes the MPTimer state */
struct mptimer_state *mptimer_state_alloc(
    struct vmm_guest *guest, vmm_emulate_device_t *edev, uint32_t num_cpu, uint32_t periphclk, uint32_t timer_irq, uint32_t wdt_irq);

/** Destructor for the MPTimer state */
int mptimer_state_free(struct mptimer_state *s);

#endif /* __ARM_MPTIMER_EMULATOR_H__ */
