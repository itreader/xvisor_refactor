/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file io-page_table.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Linux-like generic page table allocator for IOMMUs.
 *
 * The source has been largely adapted from Linux
 * drivers/iommu/io-page_table.c
 *
 * Copyright (C) 2014 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * The original code is licensed under the GPL.
 */

#include "io-page_table.h"

static const struct io_page_table_init_fns *io_page_table_init_table[IO_PGTABLE_NUM_FMTS] = {
#ifdef CONFIG_IOMMU_IO_PGTABLE_LPAE
    [ARM_32_LPAE_S1] = &io_page_table_arm_32_lpae_s1_init_fns,
    [ARM_32_LPAE_S2] = &io_page_table_arm_32_lpae_s2_init_fns,
    [ARM_64_LPAE_S1] = &io_page_table_arm_64_lpae_s1_init_fns,
    [ARM_64_LPAE_S2] = &io_page_table_arm_64_lpae_s2_init_fns,
#endif
#ifdef CONFIG_IOMMU_IO_PGTABLE_ARMV7S
    [ARM_V7S] = &io_page_table_arm_v7s_init_fns,
#endif
};

struct io_page_table_ops *alloc_io_page_table_ops(enum io_page_table_fmt fmt, struct io_page_table_cfg *cfg, void *cookie)
{
    struct io_page_table                *iop;
    const struct io_page_table_init_fns *fns;

    if (fmt >= IO_PGTABLE_NUM_FMTS) {
        return NULL;
    }

    fns = io_page_table_init_table[fmt];

    if (!fns) {
        return NULL;
    }

    iop = fns->alloc(cfg, cookie);

    if (!iop) {
        return NULL;
    }

    iop->fmt    = fmt;
    iop->cookie = cookie;
    iop->cfg    = *cfg;

    return &iop->ops;
}

/*
 * It is the IOMMU driver's responsibility to ensure that the page table
 * is no longer accessible to the walker by this point.
 */
void free_io_page_table_ops(struct io_page_table_ops *ops)
{
    struct io_page_table *iop;

    if (!ops) {
        return;
    }

    iop = container_of(ops, struct io_page_table, ops);
    io_page_table_tlb_flush_all(iop);
    io_page_table_init_table[iop->fmt]->free(iop);
}
