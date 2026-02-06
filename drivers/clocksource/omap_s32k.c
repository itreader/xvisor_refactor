/**
 * Copyright (c) 2011 Sukanto Ghosh.
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
 * @file omap_s32k.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief OMAP 32K sync counter
 */

#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>

#define S32K_FREQ_HZ 32768

#define S32K_CR      0x10

static uint64_t s32k_clocksource_read(vmm_clocksource_t *cs)
{
    return vmm_readl((void *)(cs->private + S32K_CR));
}

static vmm_clocksource_t s32k_clock_src = {
    .name = "s32k_clock_src", .rating = 200, .shift = 15, .mask = VMM_CLOCKSOURCE_MASK(32), .read = &s32k_clocksource_read};

int __init s32k_clocksource_init(vmm_device_tree_node_t *node)
{
    int            rc;
    virtual_addr_t base;

    /* Map registers */
    rc = vmm_device_tree_request_regmap(node, &base, 0, "omap-s32k");

    if (rc) {
        return rc;
    }

    /* Save pointer to registers in clocksource private */
    s32k_clock_src.private = (void *)base;

    /* Compute mult for clocksource */
    s32k_clock_src.freq    = S32K_FREQ_HZ;
    vmm_clocks_calc_mult_shift(&s32k_clock_src.mult, &s32k_clock_src.shift, S32K_FREQ_HZ, VMM_NSEC_PER_SEC, 10);

    /* Register clocksource */
    if ((rc = vmm_clocksource_register(&s32k_clock_src))) {
        vmm_device_tree_regunmap_release(node, base, 0);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKSOURCE_INIT_DECLARE(omap32kclock_src, "ti,omap-counter32k", s32k_clocksource_init);
