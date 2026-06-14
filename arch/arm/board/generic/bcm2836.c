/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file bcm2836.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2836 SOC specific code
 */

#include <libs/mathlib.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_stdio.h>

#include <cpu_generic_timer.h>
#include <generic_timer.h>

#include <generic_board.h>

/* When setting bits 0-3, enables PMU interrupts on that CPU. */
#define LOCAL_TIMER_PRESCALER 0x008

/*
 * Initialization functions
 */

static int __init bcm2836_early_init(vmm_device_tree_node_t *node)
{
    int                     rc = VMM_OK;
    void                   *base;
    uint32_t                prescaler, cntfreq;
    virtual_addr_t          base_va;
    vmm_device_tree_node_t *np;

    np = vmm_device_tree_find_compatible(NULL, NULL, "brcm,bcm2836-l1-intc");

    if (!np) {
        return VMM_ERR_NODEV;
    }

    rc = vmm_device_tree_regmap(np, &base_va, 0);

    if (rc) {
        goto done;
    }

    base    = (void *)base_va;

    cntfreq = generic_timer_reg_read(GENERIC_TIMER_REG_FREQ);

    switch (cntfreq) {
        case 19200000:
            prescaler = 0x80000000;

        case 1000000:
            prescaler = 0x06AAAAAB;

        default:
            prescaler = (uint32_t)udiv64((uint64_t)0x80000000 * (uint64_t)cntfreq, (uint64_t)19200000);
            break;
    };

    if (!prescaler) {
        rc = VMM_ERR_INVALID;
        goto done_unmap;
    }

    vmm_writel(prescaler, base + LOCAL_TIMER_PRESCALER);

done_unmap:
    vmm_device_tree_regunmap(node, base_va, 0);

done:
    vmm_device_tree_dref_node(np);

    return rc;
}

static int __init bcm2836_final_init(vmm_device_tree_node_t *node)
{
    /* Nothing to do here. */
    return VMM_OK;
}

static struct generic_board bcm2836_info = {
    .name       = "BCM2836",
    .early_init = bcm2836_early_init,
    .final_init = bcm2836_final_init,
};

GENERIC_BOARD_DECLARE(bcm2836, "brcm,bcm2836", &bcm2836_info);
