/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file arm_psci.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM32, ARM32ve and ARM64 PSCI host calls
 */

#ifndef __ARM_PSCI_H__
#define __ARM_PSCI_H__

#include <psci.h>
#include <vmm_types.h>

#ifdef CONFIG_ARM_PSCI

int psci_cpu_suspend(uint64_t power_state, uint64_t entry_point);

int psci_cpu_off(uint64_t power_state);

int psci_cpu_on(uint64_t cpuid, uint64_t entry_point);

int psci_migrate(uint64_t cpuid);

uint32_t psci_get_version(void);

bool psci_available(void);

void psci_init(void);

#else

static inline bool psci_available(void)
{
    return FALSE;
}

static inline void psci_init(void) {}

#endif

#endif
