/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_guest_address_space.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for guest address space
 */
#ifndef _VMM_GUEST_ADDRESS_SPACE_H__
#define _VMM_GUEST_ADDRESS_SPACE_H__

#include <vmm_manager.h>
#include <vmm_notifier.h>

/* Notifier event when guest aspace is initialized */
#define VMM_GUEST_ADDRESS_SPACE_EVENT_INIT   0x01
/* Notifier event when guest aspace is about to be uninitialized */
#define VMM_GUEST_ADDRESS_SPACE_EVENT_DEINIT 0x02
/* Notifier event when guest aspace is reset */
#define VMM_GUEST_ADDRESS_SPACE_EVENT_RESET  0x03

/** Representation of block device notifier event */
struct vmm_guest_address_space_event {
    struct vmm_guest *guest;
    void             *data;
};

/** Register a guest address space state change notifier handler */
int vmm_guest_address_space_register_client(vmm_notifier_block_t *nb);

/** Unregister guest address space state change notifier */
int vmm_guest_address_space_unregister_client(vmm_notifier_block_t *nb);

/** Iterate over each region with matching flags
 *  Note: reg_flags = 0x0 will match all regions
 */
void vmm_guest_iterate_region(
    struct vmm_guest *guest, uint32_t reg_flags, void (*func)(struct vmm_guest *, struct vmm_region *, void *), void *private);

/** Find region corresponding to a guest physical address and also
 *  resolve aliased regions to real or virtual regions if required.
 */
struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest, physical_addr_t guest_physical_addr, uint32_t reg_flags, bool resolve_alias);

/** Find mapping for given guest physical address and guest region */
void vmm_guest_find_mapping(
    struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t *hphys_addr, physical_size_t *avail_size);

/** Iterate over each mapping of a guest region */
void vmm_guest_iterate_mapping(
    struct vmm_guest *guest, struct vmm_region *reg,
    void (*func)(
        struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t hphys_addr, physical_size_t phys_size,
        void *private),
    void *private);

/** Overwrite real device region mapping */
int vmm_guest_overwrite_real_device_mapping(
    struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t hphys_addr);

/** Read from guest memory regions (i.e. RAM or ROM regions) */
uint32_t vmm_guest_memory_read(struct vmm_guest *guest, physical_addr_t guest_physical_addr, void *dst, uint32_t len, bool cacheable);

/** Write to guest memory regions (i.e. RAM or ROM regions) */
uint32_t vmm_guest_memory_write(struct vmm_guest *guest, physical_addr_t guest_physical_addr, void *src, uint32_t len, bool cacheable);

/** Map guest physical address to some host physical address */
int vmm_guest_physical_map(
    struct vmm_guest *guest, physical_addr_t guest_physical_addr, physical_size_t gphys_size, physical_addr_t *hphys_addr, physical_size_t *phys_size,
    uint32_t *reg_flags);

/** Unmap guest physical address */
int vmm_guest_physical_unmap(struct vmm_guest *guest, physical_addr_t guest_physical_addr, physical_size_t phys_size);

/** Add a new region from a given node in DTS */
int vmm_guest_add_region_from_node(struct vmm_guest *guest, vmm_device_tree_node_t *node, void *rprivate);

/** Add new guest region */
int vmm_guest_add_region(
    struct vmm_guest *guest, vmm_device_tree_node_t *parent, const char *name, const char *device_type, const char *mainfest_type,
    const char *address_type, const char *compatible, uint32_t compatible_len, physical_addr_t guest_physical_addr, physical_addr_t aphys_addr,
    physical_size_t phys_size, uint32_t align_order, physical_addr_t hphys_addr, void *rprivate);

/** Get private pointer of guest region */
static inline void *vmm_guest_get_region_private(struct vmm_region *reg)
{
    return (reg) ? reg->private : NULL;
}

/** Set private pointer of guest region */
static inline void vmm_guest_set_region_private(struct vmm_region *reg, void *rprivate)
{
    if (reg) {
        reg->private = rprivate;
    }
}

/** Delete a guest region */
int vmm_guest_del_region(struct vmm_guest *guest, struct vmm_region *reg, bool del_node);

/** Reset guest address space */
int vmm_guest_address_space_reset(struct vmm_guest *guest);

/** Initialize guest address space */
int vmm_guest_address_space_init(struct vmm_guest *guest);

/** DeInitialize guest address space */
int vmm_guest_address_space_deinit(struct vmm_guest *guest);

#endif
