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
 * @file zero.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Zero read-only memory emulator.
 */

#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>

#define MODULE_DESC      "Zero Device Emulator"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      zero_emulator_init
#define MODULE_EXIT      zero_emulator_exit

static int zero_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    /* Always read zero */
    *dst = 0x0;
    return VMM_OK;
}

static int zero_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    /* Always read zero */
    *dst = 0x0;
    return VMM_OK;
}

static int zero_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    /* Always read zero */
    *dst = 0x0;
    return VMM_OK;
}

static int zero_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    /* Ignore it. */
    return VMM_OK;
}

static int zero_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    /* Ignore it. */
    return VMM_OK;
}

static int zero_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    /* Ignore it. */
    return VMM_OK;
}

static int zero_emulator_reset(vmm_emulate_device_t *edev)
{
    return VMM_OK;
}

static int zero_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    edev->private = NULL;

    return VMM_OK;
}

static int zero_emulator_remove(vmm_emulate_device_t *edev)
{
    return VMM_OK;
}

static struct vmm_device_tree_nodeid zero_emuid_table[] = {
    {
     .type       = "misc",
     .compatible = "zero",
     },
    {/* end of list */                   },
};

static vmm_emulator_t zero_emulator = {
    .name        = "zero",
    .match_table = zero_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_NATIVE_ENDIAN,
    .probe       = zero_emulator_probe,
    .read8       = zero_emulator_read8,
    .write8      = zero_emulator_write8,
    .read16      = zero_emulator_read16,
    .write16     = zero_emulator_write16,
    .read32      = zero_emulator_read32,
    .write32     = zero_emulator_write32,
    .reset       = zero_emulator_reset,
    .remove      = zero_emulator_remove,
};

static int __init zero_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&zero_emulator);
}

static void __exit zero_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&zero_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
