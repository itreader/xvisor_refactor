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
 * @file arch_device_tree.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific device tree functions
 */
#ifndef _ARCH_DEVICE_TREE_H__
#define _ARCH_DEVICE_TREE_H__

#include <vmm_types.h>

struct vmm_device_tree_node;
typedef struct vmm_device_tree_node vmm_device_tree_node_t;

/** Setup/Configure/Parse RAM banks
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_ram_bank_setup(void);

/** Get RAM bank count
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_ram_bank_count(uint32_t *bank_count);

/** Get start physical address of RAM bank
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_ram_bank_start(uint32_t bank, physical_addr_t *addr);

/** Get physical size of RAM bank
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_ram_bank_size(uint32_t bank, physical_size_t *size);

/** Count reserved RAM areas
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_reserve_count(uint32_t *count);

/** Get reserved RAM area physical address
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_reserve_addr(uint32_t index, physical_addr_t *addr);

/** Get reserved RAM area physical size
 *  Note: This function will be called before populating device tree
 */
int arch_device_tree_reserve_size(uint32_t index, physical_size_t *size);

/** Populate device tree */
int arch_device_tree_populate(vmm_device_tree_node_t **root);

#endif
