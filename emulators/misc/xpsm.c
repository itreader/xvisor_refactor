/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file xpsm.c
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Xvisor PCI Shared Memory Driver.
 */

#include <emu/pci/pci_emu_core.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>

#define XPSM_EMU_IPRIORITY (PCI_EMU_CORE_IPRIORITY + 1)

#define MODULE_DESC        "PCI Shared Memory Device"
#define MODULE_AUTHOR      "Himanshu Chauhan"
#define MODULE_LICENSE     "GPL"
#define MODULE_IPRIORITY   XPSM_EMU_IPRIORITY
#define MODULE_INIT        xpsm_emulator_init
#define MODULE_EXIT        xpsm_emulator_exit

static int xpsm_bar_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_reset(vmm_emulate_device_t *edev)
{
    vmm_printf("xpsm bar emulator reset!\n");
    return VMM_OK;
}

static int xpsm_bar_emulator_remove(vmm_emulate_device_t *edev)
{
    return VMM_OK;
}

static int xpsm_bar_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    vmm_printf("xpsm bar emulator probe!\n");
    return VMM_OK;
}

static int xpsm_emulator_reset(struct pci_device *pdev)
{
    return VMM_OK;
}

static int xpsm_emulator_probe(struct pci_device *pdev, struct vmm_guest *guest, const struct vmm_device_tree_nodeid *eid)
{
    struct pci_class *class      = (struct pci_class *)pdev;
    class->conf_header.vendor_id = 0x1857;
    class->conf_header.device_id = 0x1947;

    pdev->private                = NULL;

    return VMM_OK;
}

static int xpsm_emulator_remove(struct pci_device *pdev)
{
    return VMM_OK;
}

static struct vmm_device_tree_nodeid xpsm_emuid_table[] = {
    {
     .type       = "psm",
     .compatible = "xpsm",
     },
    {/* end of list */                  },
};

static struct vmm_device_tree_nodeid xpsm_bar_emulator_emuid_table[] = {
    {
     .type       = "psm",
     .compatible = "xpsm,bar",
     },
    {/* end of list */                  },
};

static vmm_emulator_t xpsm_bar_emulator = {
    .name        = "xpsm-bar",
    .match_table = xpsm_bar_emulator_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = xpsm_bar_emulator_probe,
    .read8       = xpsm_bar_emulator_read8,
    .write8      = xpsm_bar_emulator_write8,
    .read16      = xpsm_bar_emulator_read16,
    .write16     = xpsm_bar_emulator_write16,
    .read32      = xpsm_bar_emulator_read32,
    .write32     = xpsm_bar_emulator_write32,
    .reset       = xpsm_bar_emulator_reset,
    .remove      = xpsm_bar_emulator_remove,
};

static struct pci_dev_emulator xpsm_emulator = {
    .name        = "xpsm",
    .match_table = xpsm_emuid_table,
    .probe       = xpsm_emulator_probe,
    .reset       = xpsm_emulator_reset,
    .remove      = xpsm_emulator_remove,
};

static int __init xpsm_emulator_init(void)
{
    int rc;

    if ((rc = pci_emu_register_device(&xpsm_emulator)) != VMM_OK) {
        return rc;
    }

    return vmm_device_emulate_register_emulator(&xpsm_bar_emulator);
}

static void __exit xpsm_emulator_exit(void)
{
    pci_emu_unregister_device(&xpsm_emulator);
    vmm_device_emulate_unregister_emulator(&xpsm_bar_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
