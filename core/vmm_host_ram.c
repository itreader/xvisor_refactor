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
 * @file vmm_host_ram.c
 * @author Anup patel (anup@brainfault.org)
 * @brief Source file for RAM management.
 */

#include <arch_device_tree.h>
#include <libs/bitmap.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_resource.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

struct vmm_host_ram_bank {
    physical_addr_t start;
    physical_size_t size;
    uint32_t        frame_count;

    vmm_spinlock_t bmap_lock;
    uint64_t      *bmap;
    uint32_t       bmap_sz;
    uint32_t       bmap_free;

    vmm_resource_t res;
};

struct vmm_host_ram_ctrl {
    struct vmm_host_ram_color_ops *ops;
    void                          *ops_private;
    uint32_t                       bank_count;
    struct vmm_host_ram_bank       banks[CONFIG_MAX_RAM_BANK_COUNT];
};

static struct vmm_host_ram_ctrl rctrl;

static physical_size_t __host_ram_alloc(
    physical_addr_t *pa, physical_size_t size, uint32_t align_order, uint32_t color, struct vmm_host_ram_color_ops *ops, void *ops_private)
{
    irq_flags_t               f;
    physical_addr_t           p;
    uint32_t                  i, bn, binc, bcnt, bpos, bfree;
    struct vmm_host_ram_bank *bank;

    if ((size == 0) || (align_order < VMM_PAGE_SHIFT) || (BITS_PER_LONG <= align_order)) {
        return 0;
    }

    size = roundup2_order_size(size, align_order);
    bcnt = VMM_SIZE_TO_PAGE(size);

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank = &rctrl.banks[bn];

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, f);

        if (bank->bmap_free < bcnt) {
            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, f);
            continue;
        }

        binc = order_size(align_order) >> VMM_PAGE_SHIFT;
        bpos = bank->start & order_mask(align_order);

        if (bpos) {
            bpos = VMM_SIZE_TO_PAGE(order_size(align_order) - bpos);
        }

        for (; bpos < (bank->size >> VMM_PAGE_SHIFT); bpos += binc) {
            bfree = 0;

            for (i = bpos; i < (bpos + bcnt); i++) {
                if (bitmap_isset(bank->bmap, i)) {
                    break;
                }

                bfree++;
            }

            if (bfree != bcnt) {
                continue;
            }

            p = bank->start + bpos * VMM_PAGE_SIZE;

            if (ops && !ops->color_match(p, size, color, ops_private)) {
                continue;
            }

            *pa = p;
            bitmap_set(bank->bmap, bpos, bcnt);
            bank->bmap_free -= bcnt;

            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, f);

            return size;
        }

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, f);
    }

    return 0;
}

static uint32_t default_num_colors(void *private)
{
    return U32_MAX;
}

static uint32_t default_color_order(void *private)
{
    return 16;
}

static bool default_color_match(physical_addr_t pa, physical_size_t size, uint32_t color, void *private)
{
    return TRUE;
}

static struct vmm_host_ram_color_ops default_ops = {
    .name        = "default",
    .num_colors  = default_num_colors,
    .color_order = default_color_order,
    .color_match = default_color_match,
};

void vmm_host_ram_set_color_ops(struct vmm_host_ram_color_ops *ops, void *private)
{
    if (ops) {
        if (!ops->num_colors || !ops->color_order || !ops->color_match) {
            return;
        }

        if (!ops->num_colors(private) || (ops->color_order(private) < VMM_PAGE_SHIFT) || (BITS_PER_LONG <= ops->color_order(private))) {
            return;
        }

        rctrl.ops         = ops;
        rctrl.ops_private = private;
    } else {
        rctrl.ops         = &default_ops;
        rctrl.ops_private = NULL;
    }
}

const char *vmm_host_ram_color_ops_name(void)
{
    return rctrl.ops->name;
}

uint32_t vmm_host_ram_color_count(void)
{
    return rctrl.ops->num_colors(rctrl.ops_private);
}

uint32_t vmm_host_ram_color_order(void)
{
    return rctrl.ops->color_order(rctrl.ops_private);
}

physical_size_t vmm_host_ram_color_alloc(physical_addr_t *pa, uint32_t color)
{
    uint32_t order = rctrl.ops->color_order(rctrl.ops_private);

    if (rctrl.ops->num_colors(rctrl.ops_private) <= color) {
        return 0;
    }

    return __host_ram_alloc(pa, (physical_size_t)1 << order, order, color, rctrl.ops, rctrl.ops_private);
}

physical_size_t vmm_host_ram_alloc(physical_addr_t *pa, physical_size_t size, uint32_t align_order)
{
    return __host_ram_alloc(pa, size, align_order, 0, NULL, NULL);
}

int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t size)
{
    int                       rc = VMM_EINVALID;
    uint32_t                  i, bn, bcnt, bpos, bfree;
    uint64_t                  bank_end, pa_end;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank     = &rctrl.banks[bn];

        bank_end = (uint64_t)bank->start + (uint64_t)bank->size;
        pa_end   = (uint64_t)pa + (uint64_t)size;

        if ((pa < bank->start) || (bank_end < pa_end)) {
            continue;
        }

        bpos = (pa - bank->start) >> VMM_PAGE_SHIFT;
        bcnt = VMM_SIZE_TO_PAGE(size);

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags);

        if (bank->bmap_free < bcnt) {
            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags);
            rc = VMM_ENOSPC;
            break;
        }

        bfree = 0;

        for (i = bpos; i < (bpos + bcnt); i++) {
            if (bitmap_isset(bank->bmap, i)) {
                break;
            }

            bfree++;
        }

        if (bfree != bcnt) {
            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags);
            rc = VMM_ENOSPC;
            break;
        }

        bitmap_set(bank->bmap, bpos, bcnt);
        bank->bmap_free -= bcnt;

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags);

        rc = VMM_OK;
        break;
    }

    return rc;
}

int vmm_host_ram_free(physical_addr_t pa, physical_size_t size)
{
    int                       rc = VMM_EINVALID;
    uint32_t                  bn, bcnt, bpos;
    uint64_t                  bank_end, pa_end;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank     = &rctrl.banks[bn];

        bank_end = (uint64_t)bank->start + (uint64_t)bank->size;
        pa_end   = (uint64_t)pa + (uint64_t)size;

        if ((pa < bank->start) || (bank_end < pa_end)) {
            continue;
        }

        bpos = (pa - bank->start) >> VMM_PAGE_SHIFT;
        bcnt = VMM_SIZE_TO_PAGE(size);

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags);

        bitmap_clear(bank->bmap, bpos, bcnt);
        bank->bmap_free += bcnt;

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags);

        rc = VMM_OK;
        break;
    }

    return rc;
}

bool vmm_host_ram_frame_isfree(physical_addr_t pa)
{
    uint32_t                  bn, bpos;
    uint64_t                  bank_end;
    bool                      ret = FALSE;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank     = &rctrl.banks[bn];

        bank_end = (uint64_t)bank->start + (uint64_t)bank->size;

        if ((pa < bank->start) || (bank_end <= pa)) {
            continue;
        }

        bpos = (pa - bank->start) >> VMM_PAGE_SHIFT;

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags);

        if (!bitmap_isset(bank->bmap, bpos)) {
            ret = TRUE;
        }

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags);

        break;
    }

    return ret;
}

uint32_t vmm_host_ram_total_free_frames(void)
{
    uint32_t                  bn, ret = 0;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank = &rctrl.banks[bn];

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags);
        ret += bank->bmap_free;
        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags);
    }

    return ret;
}

uint32_t vmm_host_ram_total_frame_count(void)
{
    uint32_t bn, ret = 0;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        ret += rctrl.banks[bn].frame_count;
    }

    return ret;
}

physical_addr_t vmm_host_ram_start(void)
{
    uint32_t        bn;
    physical_addr_t start, ret = 0;

    ret -= 1;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        start = rctrl.banks[bn].start;

        if (start <= ret) {
            ret = start;
        }
    }

    return ret;
}

physical_addr_t vmm_host_ram_end(void)
{
    uint32_t        bn;
    physical_addr_t end, ret = 0;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        end = rctrl.banks[bn].start + rctrl.banks[bn].size;
        end -= 1;

        if (ret <= end) {
            ret = end;
        }
    }

    return ret;
}

physical_size_t vmm_host_ram_total_size(void)
{
    uint32_t        bn;
    physical_size_t ret = 0;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        ret += rctrl.banks[bn].size;
    }

    return ret;
}

uint32_t vmm_host_ram_bank_count(void)
{
    return rctrl.bank_count;
}

physical_addr_t vmm_host_ram_bank_start(uint32_t bank)
{
    return (bank < rctrl.bank_count) ? rctrl.banks[bank].start : 0;
}

physical_size_t vmm_host_ram_bank_size(uint32_t bank)
{
    return (bank < rctrl.bank_count) ? rctrl.banks[bank].size : 0;
}

uint32_t vmm_host_ram_bank_frame_count(uint32_t bank)
{
    return (bank < rctrl.bank_count) ? rctrl.banks[bank].frame_count : 0;
}

uint32_t vmm_host_ram_bank_free_frames(uint32_t bank)
{
    uint32_t                  ret;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bankp;

    if (bank >= rctrl.bank_count) {
        return 0;
    }

    bankp = &rctrl.banks[bank];

    vmm_spin_lock_irq_save_lite(&bankp->bmap_lock, flags);
    ret = bankp->bmap_free;
    vmm_spin_unlock_irq_restore_lite(&bankp->bmap_lock, flags);

    return ret;
}

virtual_size_t __init vmm_host_ram_estimate_hksize(void)
{
    int             rc;
    uint32_t        bn, count;
    virtual_size_t  ret;
    physical_size_t size;

    if ((rc = arch_device_tree_ram_bank_count(&count))) {
        return 0;
    }

    if (!count || (count > CONFIG_MAX_RAM_BANK_COUNT)) {
        return 0;
    }

    ret = 0;

    for (bn = 0; bn < count; bn++) {
        if ((rc = arch_device_tree_ram_bank_size(bn, &size))) {
            return ret;
        }

        ret += bitmap_estimate_size(size >> VMM_PAGE_SHIFT);
    }

    return ret;
}

int __init vmm_host_ram_init(virtual_addr_t hkbase)
{
    int                       rc;
    uint32_t                  bn;
    struct vmm_host_ram_bank *bank;

    memset(&rctrl, 0, sizeof(rctrl));

    rctrl.ops         = &default_ops;
    rctrl.ops_private = NULL;

    if ((rc = arch_device_tree_ram_bank_count(&rctrl.bank_count))) {
        return rc;
    }

    if (!rctrl.bank_count) {
        return VMM_ENODEV;
    }

    if (rctrl.bank_count > CONFIG_MAX_RAM_BANK_COUNT) {
        return VMM_EINVALID;
    }

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank = &rctrl.banks[bn];

        if ((rc = arch_device_tree_ram_bank_start(bn, &bank->start))) {
            return rc;
        }

        if (bank->start & VMM_PAGE_MASK) {
            return VMM_EINVALID;
        }

        if ((rc = arch_device_tree_ram_bank_size(bn, &bank->size))) {
            return rc;
        }

        if (bank->size & VMM_PAGE_MASK) {
            return VMM_EINVALID;
        }

        bank->frame_count = bank->size >> VMM_PAGE_SHIFT;

        INIT_SPIN_LOCK(&bank->bmap_lock);

        bank->bmap      = (uint64_t *)hkbase;
        bank->bmap_sz   = bitmap_estimate_size(bank->frame_count);
        bank->bmap_free = bank->frame_count;

        bitmap_zero(bank->bmap, bank->frame_count);

        bank->res.start = bank->start;
        bank->res.end   = bank->start + bank->size - 1;
        bank->res.name  = "System RAM";
        bank->res.flags = VMM_IORESOURCE_MEM | VMM_IORESOURCE_BUSY;
        rc              = vmm_request_resource(&vmm_hostmem_resource, &bank->res);

        if (rc) {
            return rc;
        }

        vmm_init_printf("ram: bank%d phys=0x%" PRIPADDR " size=%" PRIPSIZE "\n", bn, bank->start, bank->size);

        vmm_init_printf("ram: bank%d hkbase=0x%" PRIADDR " hksize=%d\n", bn, hkbase, bank->bmap_sz);

        hkbase += bank->bmap_sz;
    }

    return VMM_OK;
}
