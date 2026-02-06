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
 * @file vmm_host_ram.h
 * @author Anup patel (anup@brainfault.org)
 * @brief Header file for RAM management.
 */

#ifndef __VMM_HOST_RAM_H_
#define __VMM_HOST_RAM_H_

#include <vmm_limits.h>
#include <vmm_types.h>

/** Host RAM cache color operations */
struct vmm_host_ram_color_ops {
    char name[VMM_FIELD_NAME_SIZE];
    uint32_t (*num_colors)(void *private);
    uint32_t (*color_order)(void *private);
    bool (*color_match)(physical_addr_t pa, physical_size_t size, uint32_t color, void *private);
};

/** Set host RAM cache color operations */
void vmm_host_ram_set_color_ops(struct vmm_host_ram_color_ops *ops, void *private);

/** Get host RAM cache color operations name */
const char *vmm_host_ram_color_ops_name(void);

/** Get host RAM cache color count */
uint32_t vmm_host_ram_color_count(void);

/** Get host RAM cache color order */
uint32_t vmm_host_ram_color_order(void);

/** Allocate cache colored physical space from RAM */
physical_size_t vmm_host_ram_color_alloc(physical_addr_t *pa, uint32_t color);

/** Allocate physical space from RAM */
physical_size_t vmm_host_ram_alloc(physical_addr_t *pa, physical_size_t size, uint32_t align_order);

/** Reserve a portion of RAM forcefully */
int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t size);

/** Free physical space to RAM */
int vmm_host_ram_free(physical_addr_t pa, physical_size_t size);

/** Check if a RAM physical address is free */
bool vmm_host_ram_frame_isfree(physical_addr_t pa);

/** Total free frames of all RAM banks */
uint32_t vmm_host_ram_total_free_frames(void);

/** Total frame count of all RAM banks */
uint32_t vmm_host_ram_total_frame_count(void);

/** Start address of all RAM Banks */
physical_addr_t vmm_host_ram_start(void);

/** Last/end address of all RAM Banks */
physical_addr_t vmm_host_ram_end(void);

/** Total size of all RAM Banks */
physical_size_t vmm_host_ram_total_size(void);

/** Number of RAM Banks */
uint32_t vmm_host_ram_bank_count(void);

/** Start address of RAM Bank */
physical_addr_t vmm_host_ram_bank_start(uint32_t bank);

/** Size of RAM Bank */
physical_size_t vmm_host_ram_bank_size(uint32_t bank);

/** Frame count of RAM Bank */
uint32_t vmm_host_ram_bank_frame_count(uint32_t bank);

/** Free frames of RAM Bank */
uint32_t vmm_host_ram_bank_free_frames(uint32_t bank);

/** Estimate House-keeping size of RAM */
virtual_size_t vmm_host_ram_estimate_hksize(void);

/* Initialize RAM management */
int vmm_host_ram_init(virtual_addr_t hkbase);

#endif /* __VMM_HOST_RAM_H_ */
