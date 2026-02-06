/**
 * Copyright (c) 2014 Anup Patel.
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file buddy.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief buddy allocator library
 */

#ifndef __BUDDY_H__
#define __BUDDY_H__

#include <libs/list.h>
#include <libs/red_black_tree_augmented.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define BUDDY_MAX_SUPPORTED_BIN 32

/** Representation of buddy allocator instance */
struct buddy_allocator {
    void                 *hk_area;
    uint64_t              hk_area_size;
    vmm_spinlock_t        hk_free_lock;
    uint64_t              hk_total_count;
    uint64_t              hk_free_count;
    double_list_t         hk_free_list;
    uint64_t              mem_start;
    uint64_t              mem_size;
    uint64_t              min_bin;
    uint64_t              max_bin;
    vmm_spinlock_t        alloc_lock;
    struct red_black_root alloc;
    vmm_spinlock_t        bins_lock[BUDDY_MAX_SUPPORTED_BIN];
    double_list_t         bins[BUDDY_MAX_SUPPORTED_BIN];
};

/* Estimate bin number for given size */
uint64_t buddy_estimate_bin(struct buddy_allocator *ba, uint64_t size);

/** Get count of free house-keeping buddy areas */
uint64_t buddy_hk_area_free(struct buddy_allocator *ba);

/** Get count of total house-keeping buddy areas */
uint64_t buddy_hk_area_total(struct buddy_allocator *ba);

/** Count buddy areas in given bin */
uint64_t buddy_bins_area_count(struct buddy_allocator *ba, uint64_t bin_num);

/** Count blocks in given bin */
uint64_t buddy_bins_block_count(struct buddy_allocator *ba, uint64_t bin_num);

/** Compute available free space in buddy allocator bins */
uint64_t buddy_bins_free_space(struct buddy_allocator *ba);

/** Alloc memory from buddy allocator */
int buddy_mem_alloc(struct buddy_allocator *ba, uint64_t size, uint64_t *addr);

/** Alloc memory aligned to 2^order from buddy allocator */
int buddy_mem_aligned_alloc(struct buddy_allocator *ba, uint64_t order, uint64_t size, uint64_t *addr);

/** Reserve memory in buddy allocator */
int buddy_mem_reserve(struct buddy_allocator *ba, uint64_t addr, uint64_t size);

/** Find alloced/reserved memory from buddy allocator */
int buddy_mem_find(struct buddy_allocator *ba, uint64_t addr, uint64_t *alloc_addr, uint64_t *alloc_bin, uint64_t *alloc_size);

/** Free memory to buddy allocator */
int buddy_mem_free(struct buddy_allocator *ba, uint64_t addr);

/** Partially free memory to buddy allocator */
int buddy_mem_partial_free(struct buddy_allocator *ba, uint64_t addr, uint64_t size);

/** Initialize buddy allocator */
int buddy_allocator_init(
    struct buddy_allocator *ba, void *hk_area, uint64_t hk_area_size, uint64_t mem_start, uint64_t mem_size, uint64_t min_bin, uint64_t max_bin);

#endif /* __BUDDY_H__ */
