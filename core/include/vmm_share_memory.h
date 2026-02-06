/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_share_memory.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for shared memory subsystem
 */
#ifndef __VMM_SHMEM_H__
#define __VMM_SHMEM_H__

#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_limits.h>
#include <vmm_types.h>

typedef struct vmm_share_memory {
    double_list_t   head;
    struct xref     ref_count;
    char            name[VMM_FIELD_NAME_SIZE];
    physical_addr_t addr;
    physical_size_t size;
    uint32_t        align_order;
    void *private;
} vmm_share_memory_t;

/** Read from shared memory instance */
uint32_t vmm_share_memory_read(vmm_share_memory_t *share_memory, physical_addr_t off, void *dst, uint32_t len, bool cacheable);

/** Write to shared memory instance */
uint32_t vmm_share_memory_write(vmm_share_memory_t *share_memory, physical_addr_t off, void *src, uint32_t len, bool cacheable);

/** Write a byte pattern to shared memory instance */
uint32_t vmm_share_memory_set(vmm_share_memory_t *share_memory, physical_addr_t off, uint8_t byte, uint32_t len, bool cacheable);

/** Iterate over each shared memory instance */
int vmm_share_memory_iterate(int (*iter)(vmm_share_memory_t *, void *), void *private);

/** Count shared memory instances */
uint32_t vmm_share_memory_count(void);

/** Find shared memory instance by name */
vmm_share_memory_t *vmm_share_memory_find_byname(const char *name);

/** Increment shared memory instance reference count */
void vmm_share_memory_ref(vmm_share_memory_t *share_memory);

/** Decrement shared memory instance reference count */
void vmm_share_memory_dref(vmm_share_memory_t *share_memory);

/** Create shared memory instance */
vmm_share_memory_t *vmm_share_memory_create(const char *name, physical_size_t size, uint32_t align_order, void *private);

/** Destroy shared memory instance */
static inline void vmm_share_memory_destroy(vmm_share_memory_t *share_memory)
{
    if (share_memory) {
        vmm_share_memory_dref(share_memory);
    }
}

/** Get name of shared memory instance */
static inline const char *vmm_share_memory_get_name(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->name : NULL;
}

/** Get address of shared memory instance */
static inline physical_addr_t vmm_share_memory_get_addr(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->addr : 0x0;
}

/** Get size of shared memory instance */
static inline physical_size_t vmm_share_memory_get_size(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->size : 0x0;
}

/** Get align order of shared memory */
static inline uint32_t vmm_share_memory_get_align_order(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->align_order : 0;
}

/** Get reference count of shared memory */
static inline long vmm_share_memory_get_ref_count(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? xref_val(&share_memory->ref_count) : 0;
}

/** Get private pointer of shared memory instance */
static inline void *vmm_share_memory_get_private(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->private : NULL;
}

/** Set private pointer of shared memory instance */
static inline void vmm_share_memory_set_private(vmm_share_memory_t *share_memory, void *private)
{
    if (share_memory) {
        share_memory->private = private;
    }
}

/** Initialize shared memory subsystem */
int vmm_share_memory_init(void);

#endif
