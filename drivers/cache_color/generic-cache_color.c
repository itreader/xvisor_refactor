/**
 * Copyright (c) 2017 Paolo Modica.
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
 * @file generic-cache_color.c
 * @author Paolo Modica <p.modica90@gmail.com>
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic cache color driver.
 */

#include <libs/bitops.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_ram.h>
#include <vmm_initfn.h>
#include <vmm_stdio.h>

struct generic_cache_color {
    uint32_t first_color_bit;
    uint32_t num_color_bits;
    uint32_t color_order;
};

static uint32_t generic_num_colors(void *private)
{
    struct generic_cache_color *cc = private;

    return 1 << cc->num_color_bits;
}

static uint32_t generic_color_order(void *private)
{
    struct generic_cache_color *cc = private;

    return cc->color_order;
}

static bool generic_color_match(physical_addr_t pa, physical_size_t size, uint32_t color, void *private)
{
    struct generic_cache_color *cc         = private;
    uint32_t                    color_mask = (1 << cc->num_color_bits) - 1;
    uint32_t                    color_num  = (pa >> cc->first_color_bit) & color_mask;
    physical_size_t             color_sz   = (physical_size_t)1 << cc->color_order;

    if (size != color_sz) {
        return FALSE;
    }

    if (color != color_num) {
        return FALSE;
    }

    return TRUE;
}

static struct vmm_host_ram_color_ops generic_cache_color_ops = {
    .name        = "generic-cache_color",
    .num_colors  = generic_num_colors,
    .color_order = generic_color_order,
    .color_match = generic_color_match,
};

static int __init generic_cache_color_init(vmm_device_tree_node_t *node)
{
    struct generic_cache_color *cc;

    cc = vmm_zalloc(sizeof(*cc));

    if (!cc) {
        return VMM_ENOMEM;
    }

    if (vmm_device_tree_read_u32(node, "first_color_bit", &cc->first_color_bit)) {
        vmm_free(cc);
        return VMM_EINVALID;
    }

    if (vmm_device_tree_read_u32(node, "num_color_bits", &cc->num_color_bits)) {
        vmm_free(cc);
        return VMM_EINVALID;
    }

    if (vmm_device_tree_read_u32(node, "color_order", &cc->color_order)) {
        vmm_free(cc);
        return VMM_EINVALID;
    }

    if (BITS_PER_LONG <= cc->color_order) {
        vmm_free(cc);
        return VMM_ENODEV;
    }

    if (BITS_PER_LONG <= cc->first_color_bit) {
        vmm_free(cc);
        return VMM_ENODEV;
    }

    if (BITS_PER_LONG <= (cc->first_color_bit + cc->num_color_bits)) {
        vmm_free(cc);
        return VMM_ENODEV;
    }

    vmm_host_ram_set_color_ops(&generic_cache_color_ops, cc);

    return VMM_OK;
}

VMM_INITFN_DECLARE_EARLY(gcache_color, "generic,cache_color", generic_cache_color_init);
