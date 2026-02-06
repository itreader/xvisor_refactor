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
 * @file vmm_block_partition.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block device partition management header
 */

#ifndef __VMM_BLOCKPART_H_
#define __VMM_BLOCKPART_H_

#include <block/vmm_block_device.h>
#include <vmm_limits.h>

#define VMM_BLOCKPART_IPRIORITY (VMM_BLOCK_DEVICE_CLASS_IPRIORITY + 1)

struct vmm_block_partition_manager {
    double_list_t head;
    uint32_t      sign;
    char          name[VMM_FIELD_NAME_SIZE];
    int (*parse_part)(vmm_block_device_t *block_device);
    void (*cleanup_part)(vmm_block_device_t *block_device);
};

/** Get block device private context of partiton manager */
static inline void *vmm_block_partition_manager_get_private(vmm_block_device_t *block_device)
{
    return (block_device) ? block_device->part_manager_private : NULL;
}

/** Set block device private context of partiton manager */
static inline void vmm_block_partition_manager_set_private(vmm_block_device_t *block_device, void *private)
{
    if (block_device) {
        block_device->part_manager_private = private;
    }
}

/** Register partition manager */
int vmm_block_partition_manager_register(struct vmm_block_partition_manager *mngr);

/** Unregister partition manager */
int vmm_block_partition_manager_unregister(struct vmm_block_partition_manager *mngr);

/** Get partition manager with given number */
struct vmm_block_partition_manager *vmm_block_partition_manager_get(int num);

/** Count number of partition managers */
uint32_t vmm_block_partition_manager_count(void);

#endif /* __VMM_BLOCKPART_H_ */
