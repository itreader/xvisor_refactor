/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file pci_emu_core.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for core PCI emulation.
 */

#include <emu/pci/pci_emu_core.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <vmm_cache.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

#define MODULE_DESC      "PCI Bus Emulator"
#define MODULE_AUTHOR    "Himanshu Chauhan"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY PCI_EMU_CORE_IPRIORITY
#define MODULE_INIT      pci_emulator_core_init
#define MODULE_EXIT      pci_emulator_core_exit

struct pci_device_emulate_ctrl {
    vmm_mutex_t   emu_lock;
    double_list_t emu_list;
};

static struct pci_device_emulate_ctrl pci_emu_dectrl;

struct pci_bus *pci_find_bus_by_id(struct pci_host_controller *controller, uint32_t bus_id)
{
    struct pci_bus *bus = NULL;
    irq_flags_t     flags;

    vmm_spin_lock_irq_save(&controller->lock, flags);

    list_for_each_entry(bus, &controller->attached_buses, head)
    {
        if (bus->bus_id == bus_id) {
            vmm_spin_unlock_irq_restore(&controller->lock, flags);
            return bus;
        }
    }

    vmm_spin_unlock_irq_restore(&controller->lock, flags);

    return NULL;
}

int pci_emu_find_pci_device(struct pci_host_controller *controller, int bus_id, int dev_id, struct pci_device **pdev)
{
    struct pci_bus    *bus = pci_find_bus_by_id(controller, bus_id);
    struct pci_device *ldev;
    irq_flags_t        flags;

    if (!bus) {
        return VMM_ERR_NODEV;
    }

    vmm_spin_lock_irq_save(&bus->lock, flags);

    list_for_each_entry(ldev, &bus->attached_devices, head)
    {
        if (ldev->device_id == dev_id) {
            vmm_spin_unlock_irq_restore(&bus->lock, flags);
            *pdev = ldev;
            return VMM_OK;
        }
    }

    vmm_spin_unlock_irq_restore(&bus->lock, flags);

    return VMM_ERR_NODEV;
}

struct pci_device *pci_emu_pci_dev_find_by_addr(struct pci_host_controller *controller, uint32_t addr)
{
    uint8_t            bus_num = addr >> 16;
    uint8_t            devfn   = addr >> 8;
    uint32_t           devid   = (bus_num << 8 | devfn);
    struct pci_bus    *bus;
    irq_flags_t        flags;
    struct pci_device *pdev = NULL;

    bus                     = pci_find_bus_by_id(controller, bus_num);

    if (!bus) {
        return NULL;
    }

    vmm_spin_lock_irq_save(&bus->lock, flags);

    list_for_each_entry(pdev, &bus->attached_devices, head)
    {
        if (pdev->device_id == devid) {
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&bus->lock, flags);

    return pdev;
}

struct pci_dev_emulator *pci_emu_find_device(const char *name)
{
    struct pci_dev_emulator *emu;

    if (!name) {
        return NULL;
    }

    emu = NULL;

    vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

    list_for_each_entry(emu, &pci_emu_dectrl.emu_list, head)
    {
        if (strcmp(emu->name, name) == 0) {
            vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
            return emu;
        }
    }

    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

    return NULL;
}

static int pci_emu_attach_pci_device(struct pci_host_controller *controller, struct pci_device *dev, uint32_t bus_id)
{
    struct pci_bus *bus = pci_find_bus_by_id(controller, bus_id);
    irq_flags_t     flags;

    if (!bus) {
        return VMM_ERR_NODEV;
    }

    vmm_spin_lock_irq_save(&bus->lock, flags);
    list_add_tail(&dev->head, &bus->attached_devices);
    vmm_spin_unlock_irq_restore(&bus->lock, flags);

    return VMM_OK;
}

#if 0
static int pci_emu_detach_pci_device(struct pci_host_controller *controller,
                                     struct pci_device *dev, uint32_t bus_id)
{
    return VMM_OK;
}
#endif

int pci_emu_register_device(struct pci_dev_emulator *emu)
{
    struct pci_dev_emulator *e;

    if (!emu) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

    e = NULL;
    list_for_each_entry(e, &pci_emu_dectrl.emu_list, head)
    {
        if (strcmp(e->name, emu->name) == 0) {
            vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
            return VMM_ERR_INVALID;
        }
    }

    INIT_LIST_HEAD(&emu->head);

    list_add_tail(&emu->head, &pci_emu_dectrl.emu_list);

    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

    return VMM_OK;
}

int pci_emu_unregister_device(struct pci_dev_emulator *emu)
{
    struct pci_dev_emulator *e;

    if (!emu) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

    e = NULL;
    list_for_each_entry(e, &pci_emu_dectrl.emu_list, head)
    {
        if (strcmp(e->name, emu->name) == 0) {
            vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
            return VMM_ERR_NOTAVAIL;
        }
    }

    list_del(&e->head);

    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

    return VMM_OK;
}

static int pci_emu_register_bar(struct vmm_guest *guest, const char *name, struct pci_class *class, uint32_t barnum, vmm_device_tree_node_t *bar_node)
{
    int             rc;
    physical_addr_t addr;

    if (vmm_device_tree_read_physaddr(bar_node, VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME, &addr)) {
        return VMM_ERR_FAIL;
    }

    if ((rc = vmm_guest_add_region_from_node(guest, bar_node, NULL)) != VMM_OK) {
        return rc;
    }

    class->conf_header.bars[barnum] = addr;

    return VMM_OK;
}

static int pci_emu_enumerate_bars(struct vmm_guest *guest, struct pci_device *pdev, vmm_device_tree_node_t *bus_node)
{
    vmm_device_tree_node_t *bar_node = NULL, *bars = NULL;
    char                    reg_name[64];
    int                     rc = VMM_OK;
    uint32_t                barnum;
    struct pci_class *class = (struct pci_class *)pdev;

    bar_node                = vmm_device_tree_getchild(bus_node, "bars");

    if (!bar_node) {
        /* Its okay if device tree doesn't have bars */
        return VMM_OK;
    }

    vmm_device_tree_for_each_child(bars, bar_node)
    {
        rc = vmm_device_tree_read_u32(bars, "barnum", &barnum);

        if (rc) {
            vmm_printf("%s: Bar number not specified for %s\n", __func__, bars->name);
            vmm_device_tree_dref_node(bars);
            goto done;
        }

        if (barnum >= 6) {
            vmm_printf("%s: Bar number for %s is out of range: %d\n", __func__, bars->name, barnum);
            rc = VMM_ERR_INVALID;
            vmm_device_tree_dref_node(bars);
            goto done;
        }

        vmm_sprintf(reg_name, "%s@%s", bars->name, bus_node->name);

        /* FIXME: Unmap previously register bars, or let it go??? */
        if ((rc = pci_emu_register_bar(guest, reg_name, class, barnum, bars)) != VMM_OK) {
            vmm_printf("%s: Failed to register bar region %s\n", __func__, reg_name);
            vmm_device_tree_dref_node(bars);
            goto done;
        }
    }

done:
    vmm_device_tree_dref_node(bar_node);
    return rc;
}

int pci_emu_probe_devices(struct vmm_guest *guest, struct pci_host_controller *controller, vmm_device_tree_node_t *node)
{
    int                      rc, bcount;
    struct pci_device       *pdev;
    struct pci_dev_emulator *emu;
    vmm_device_tree_node_t  *tnode;
    vmm_device_tree_node_t  *bus_node, *devs_node, *dev_node;
    uint8_t                  bus_name[32];

    if (!guest || !node || !controller) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&pci_emu_dectrl.emu_lock);

    for (bcount = 0; bcount < controller->nr_buses; bcount++) {
        memset(bus_name, 0, sizeof(bus_name));

        vmm_sprintf((char *)bus_name, "pci_bus%d", bcount);

        bus_node = vmm_device_tree_getchild(node, (const char *)bus_name);

        if (!bus_node) {
            vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
            return VMM_ERR_FAIL;
        }

        devs_node = vmm_device_tree_getchild(bus_node, "devices");
        vmm_device_tree_dref_node(bus_node);

        if (!devs_node) {
            vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
            return VMM_ERR_FAIL;
        }

        vmm_device_tree_for_each_child(tnode, devs_node)
        {
            list_for_each_entry(emu, &pci_emu_dectrl.emu_list, head)
            {
                dev_node = vmm_device_tree_find_matching(tnode, emu->match_table);

                if (!dev_node) {
                    continue;
                }

                pdev = vmm_zalloc(sizeof(struct pci_device));

                if (!pdev) {
                    /* FIXME: more cleanup to do */
                    vmm_device_tree_dref_node(tnode);
                    vmm_device_tree_dref_node(dev_node);
                    vmm_device_tree_dref_node(devs_node);
                    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
                    return VMM_ERR_FAIL;
                }

                INIT_SPIN_LOCK(&pdev->lock);
                pdev->node    = dev_node;
                pdev->private = NULL;
                rc            = vmm_device_tree_read_u32(dev_node, "device_id", &pdev->device_id);

                if (rc) {
                    vmm_printf("%s: error getting device ID information.\n", __func__);
                    vmm_free(pdev);
                    vmm_device_tree_dref_node(tnode);
                    vmm_device_tree_dref_node(dev_node);
                    vmm_device_tree_dref_node(devs_node);
                    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
                    return rc;
                }

                vmm_printf("Probe emulated PCI device %s/%s on PCI Bus %d\n", guest->name, pdev->node->name, bcount);

                if ((rc = emu->probe(pdev, guest, NULL))) {
                    vmm_printf("%s: %s/%s probe error %d\n", __func__, guest->name, pdev->node->name, rc);
                    vmm_free(pdev);
                    vmm_device_tree_dref_node(tnode);
                    vmm_device_tree_dref_node(dev_node);
                    vmm_device_tree_dref_node(devs_node);
                    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
                    return rc;
                }

                if ((rc = emu->reset(pdev))) {
                    vmm_printf("%s: %s/%s reset error %d\n", __func__, guest->name, pdev->node->name, rc);
                    vmm_free(pdev);
                    vmm_device_tree_dref_node(tnode);
                    vmm_device_tree_dref_node(dev_node);
                    vmm_device_tree_dref_node(devs_node);
                    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
                    return rc;
                }

                /* Attach newly found device to its bus */
                if ((rc = pci_emu_attach_pci_device(controller, pdev, bcount))) {
                    vmm_printf("%s: %s/%s couldn't attach PCI device to bus.\n", __func__, guest->name, pdev->node->name);
                    vmm_free(pdev);
                    vmm_device_tree_dref_node(tnode);
                    vmm_device_tree_dref_node(dev_node);
                    vmm_device_tree_dref_node(devs_node);
                    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
                    return rc;
                }

                /* FIXME: Unregister the complete device */
                rc = pci_emu_enumerate_bars(guest, pdev, dev_node);
                vmm_device_tree_dref_node(dev_node);

                if (rc != VMM_OK) {
                    vmm_free(pdev);
                    vmm_device_tree_dref_node(tnode);
                    vmm_device_tree_dref_node(devs_node);
                    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);
                    return rc;
                }
            }
        }

        vmm_device_tree_dref_node(devs_node);
    }

    vmm_mutex_unlock(&pci_emu_dectrl.emu_lock);

    return VMM_OK;
}

int pci_emu_register_controller(vmm_device_tree_node_t *node, struct vmm_guest *guest, struct pci_host_controller *controller)
{
    return pci_emu_probe_devices(guest, controller, node);
}

int pci_emu_attach_new_pci_bus(struct pci_host_controller *controller, uint32_t bus_id)
{
    struct pci_bus *nbus = vmm_zalloc(sizeof(struct pci_bus));
    irq_flags_t     flags;

    if (nbus) {
        INIT_SPIN_LOCK(&nbus->lock);
        nbus->bus_id = bus_id;
        INIT_LIST_HEAD(&nbus->attached_devices);
        nbus->host_controller = controller;

        vmm_spin_lock_irq_save(&controller->lock, flags);
        list_add(&nbus->head, &controller->attached_buses);
        vmm_spin_unlock_irq_restore(&controller->lock, flags);

        return VMM_OK;
    }

    return VMM_ERR_NOMEM;
}

int pci_emu_detach_pci_bus(struct pci_host_controller *controller, uint32_t bus_id)
{
    struct pci_bus *bus;
    irq_flags_t     flags;

    vmm_spin_lock_irq_save(&controller->lock, flags);

    list_for_each_entry(bus, &controller->attached_buses, head)
    {
        if (bus->bus_id == bus_id) {
            list_del(&bus->head);
            vmm_free(bus);
            vmm_spin_unlock_irq_restore(&controller->lock, flags);
            return VMM_OK;
        }
    }

    vmm_spin_unlock_irq_restore(&controller->lock, flags);

    return VMM_ERR_FAIL;
}

int pci_emu_config_space_write(struct pci_class *class, uint32_t reg_offs, uint32_t val)
{
    int         retv = 0;
    irq_flags_t flags;

    vmm_spin_lock_irq_save(&class->lock, flags);

    if (reg_offs > PCI_CONFIG_HEADER_END) {
        if (class->config_write) {
            retv = class->config_write(class, reg_offs, val);
            vmm_spin_unlock_irq_restore(&class->lock, flags);
            return retv;
        } else {
            vmm_printf(
                "%s: Access to register 0x%x but not "
                "implemented outside class.\n",
                __func__, reg_offs);
            vmm_spin_unlock_irq_restore(&class->lock, flags);
            return VMM_ERR_INVALID;
        }
    }

    switch (reg_offs) {
        case PCI_CONFIG_VENDOR_ID_OFFS:
            class->conf_header.vendor_id = val;
            break;

        case PCI_CONFIG_DEVICE_ID_OFFS:
            class->conf_header.device_id = val;
            break;

        case PCI_CONFIG_COMMAND_REG_OFFS:
            class->conf_header.command = val;
            break;

        case PCI_CONFIG_STATUS_REG_OFFS:
            class->conf_header.status = val;
            break;

        case PCI_CONFIG_REVISION_ID_OFFS:
            class->conf_header.revision = val;
            break;

        case PCI_CONFIG_CLASS_CODE_OFFS:
            class->conf_header.class = val;
            break;

        case PCI_CONFIG_SUBCLASS_CODE_OFFS:
            class->conf_header.sub_class = val;
            break;

        case PCI_CONFIG_PROG_IF_OFFS:
            class->conf_header.prog_if = val;
            break;

        case PCI_CONFIG_CACHE_LINE_OFFS:
            class->conf_header.cache_line_sz = val;
            break;

        case PCI_CONFIG_LATENCY_TMR_OFFS:
            class->conf_header.latency_timer = val;
            break;

        case PCI_CONFIG_HEADER_TYPE_OFFS:
            class->conf_header.header_type = val;
            break;

        case PCI_CONFIG_BIST_OFFS:
            class->conf_header.bist = val;
            break;

        case PCI_CONFIG_BAR0_OFFS:
            class->conf_header.bars[0] = val;
            break;

        case PCI_CONFIG_BAR1_OFFS:
            class->conf_header.bars[1] = val;
            break;

        case PCI_CONFIG_BAR2_OFFS:
            class->conf_header.bars[2] = val;
            break;

        case PCI_CONFIG_BAR3_OFFS:
            class->conf_header.bars[3] = val;
            break;

        case PCI_CONFIG_BAR4_OFFS:
            class->conf_header.bars[4] = val;
            break;

        case PCI_CONFIG_BAR5_OFFS:
            class->conf_header.bars[5] = val;
            break;

        case PCI_CONFIG_CARD_BUS_PTR_OFFS:
            class->conf_header.card_bus_ptr = val;
            break;

        case PCI_CONFIG_SUBSYS_VID:
            class->conf_header.subsystem_vendor_id = val;
            break;

        case PCI_CONFIG_SUBSYS_DID:
            class->conf_header.subsystem_device_id = val;
            break;

        case PCI_CONFIG_EROM_OFFS:
            class->conf_header.expansion_rom_base = val;
            break;

        case PCI_CONFIG_CAP_PTR_OFFS:
            class->conf_header.cap_pointer = val;
            break;

        case PCI_CONFIG_INT_LINE_OFFS:
            class->conf_header.int_line = val;
            break;

        case PCI_CONFIG_INT_PIN_OFFS:
            class->conf_header.int_pin = val;
            break;

        case PCI_CONFIG_MIN_GNT_OFFS:
            class->conf_header.min_gnt = val;
            break;

        case PCI_CONFIG_MAX_LAT_OFFS:
            class->conf_header.max_lat = val;
            break;
    }

    vmm_spin_unlock_irq_restore(&class->lock, flags);

    return VMM_OK;
}

uint32_t pci_emu_config_space_read(struct pci_class *class, uint32_t reg_offs, uint32_t size)
{
    uint32_t    ret = 0;
    irq_flags_t flags;

    vmm_spin_lock_irq_save(&class->lock, flags);

    if (reg_offs > PCI_CONFIG_HEADER_END) {
        if (class->config_read) {
            ret = class->config_read(class, reg_offs);
            vmm_spin_unlock_irq_restore(&class->lock, flags);
            return ret;
        } else {
            vmm_printf(
                "%s: Access to register 0x%x but not "
                "implemented outside class.\n",
                __func__, reg_offs);
            vmm_spin_unlock_irq_restore(&class->lock, flags);
            return VMM_ERR_INVALID;
        }
    }

    memcpy(&ret, (const void *)(((uint8_t *)(&class->conf_header)) + reg_offs), size);

    vmm_spin_unlock_irq_restore(&class->lock, flags);

    return ret;
}

static int __init pci_emulator_core_init(void)
{
    memset(&pci_emu_dectrl, 0, sizeof(pci_emu_dectrl));

    INIT_MUTEX(&pci_emu_dectrl.emu_lock);
    INIT_LIST_HEAD(&pci_emu_dectrl.emu_list);

    return VMM_OK;
}

static void __exit pci_emulator_core_exit(void) {}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
