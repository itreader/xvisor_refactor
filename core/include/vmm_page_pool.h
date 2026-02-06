/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file vmm_page_pool.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for page pool subsystem
 *
 * This subsystem provides managed page allocations so that
 * we can track page allocations and also use hugepages for
 * all page allocations.
 */
#ifndef _VMM_PAGE_POOL_H__
#define _VMM_PAGE_POOL_H__

#include <vmm_types.h>

enum vmm_page_pool_type {
    VMM_PAGE_POOL_NORMAL = 0,
    VMM_PAGE_POOL_NORMAL_NOCACHE,
    VMM_PAGE_POOL_NORMAL_WT,
    VMM_PAGE_POOL_DMA_COHERENT,
    VMM_PAGE_POOL_DMA_NONCOHERENT,
    VMM_PAGE_POOL_IO,
    VMM_PAGE_POOL_MAX
};

/** Get name of page pool type */
const char *vmm_page_pool_name(enum vmm_page_pool_type page_type);

/** Get total space in given page pool type */
virtual_size_t vmm_page_pool_space(enum vmm_page_pool_type page_type);

/** Get number of entries in given page pool type */
uint32_t vmm_page_pool_entry_count(enum vmm_page_pool_type page_type);

/** Get number of hugepages in given page pool type */
uint32_t vmm_page_pool_hugepage_count(enum vmm_page_pool_type page_type);

/** Get total number of pages in given page pool type */
uint32_t vmm_page_pool_page_count(enum vmm_page_pool_type page_type);

/** Get number of availabe pages in given page pool type */
uint32_t vmm_page_pool_page_avail_count(enum vmm_page_pool_type page_type);

/** Allocate pages from page pool */
virtual_addr_t vmm_page_pool_alloc(enum vmm_page_pool_type page_type, uint32_t page_count);

/** Free pages back to page pool */
int vmm_page_pool_free(enum vmm_page_pool_type page_type, virtual_addr_t page_va, uint32_t page_count);

/** Initialization page pool subsystem */
int vmm_page_pool_init(void);

#endif
