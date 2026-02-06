/**
 * Copyright (c) 2015 Pranavkumar Sawargaonkar.
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
 * @file gpex.c
 * @author Pranavkumar Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Generic PCIe Host Controller Emulator
 */

#include <emu/pci/pci_emu_core.h>
#include <emu/pci/pci_ids.h>
#include <libs/stringlib.h>
#include <vio/vmm_vserial.h>
#include <vmm_compiler.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_timer.h>
#include <vmm_types.h>

#define GPEX_EMU_IPRIORITY (PCI_EMU_CORE_IPRIORITY + 1)

#define MODULE_DESC        "Generic PCIe Host Emulator"
#define MODULE_AUTHOR      "Pranavkumar Sawargaonkar"
#define MODULE_LICENSE     "GPL"
#define MODULE_IPRIORITY   GPEX_EMU_IPRIORITY
#define MODULE_INIT        gpex_emulator_init
#define MODULE_EXIT        gpex_emulator_exit

enum {
    GPEX_LOG_LVL_ERR,
    GPEX_LOG_LVL_INFO,
    GPEX_LOG_LVL_DEBUG,
    GPEX_LOG_LVL_VERBOSE
};

static int gpex_default_log_lvl = GPEX_LOG_LVL_VERBOSE;

#define GPEX_LOG(lvl, fmt, args...)                                 \
    do {                                                            \
        if (GPEX_LOG_##lvl <= gpex_default_log_lvl) {               \
            vmm_printf("(%s:%d) " fmt, __func__, __LINE__, ##args); \
        }                                                           \
    } while (0);

struct gpex_state {
    vmm_mutex_t                 lock;
    struct vmm_guest           *guest;
    vmm_device_tree_node_t     *node;
    struct pci_host_controller *controller;
};

static uint32_t gpex_config_read(struct pci_class *pci_class, uint16_t reg_offset)
{
    /* TBD: Handle MSI/MSI-X */
    return 0;
}

static int gpex_config_write(struct pci_class *pci_class, uint16_t reg_offset, uint32_t data)
{
    return VMM_OK;
}

static int gpex_reg_write(struct gpex_state *s, uint32_t addr, uint32_t src_mask, uint32_t val)
{
    struct pci_device *pdev;
    uint32_t           config_addr;
    int                ret = VMM_OK;

    pdev                   = pci_emu_pci_dev_find_by_addr(s->controller, addr);

    if (!pdev) {
        ret = VMM_EFAIL;
        goto exit;
    }

    config_addr = addr & (PCI_CONFIG_SPACE_SIZE - 1);

    ret         = pci_emu_config_space_write(PCI_DEVICE_TO_CLASS(pdev), config_addr, val);

exit:
    return ret;
}

static int gpex_reg_read(struct gpex_state *s, uint32_t addr, uint32_t *dst, uint32_t size)
{
    struct pci_device *pdev;
    uint32_t           config_addr;
    int                ret = VMM_OK;

    pdev                   = pci_emu_pci_dev_find_by_addr(s->controller, addr);

    if (!pdev) {
        *dst = 0xFFFF;
        ret  = VMM_EFAIL;
        goto exit;
    }

    config_addr = addr & (PCI_CONFIG_SPACE_SIZE - 1);
    *dst        = pci_emu_config_space_read(PCI_DEVICE_TO_CLASS(pdev), config_addr, size);

exit:
    return ret;
}

static int gpex_emulator_reset(vmm_emulate_device_t *edev)
{
    return VMM_OK;
}

static int gpex_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = gpex_reg_read(edev->private, offset, &regval, 1);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int gpex_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = gpex_reg_read(edev->private, offset, &regval, 2);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int gpex_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return gpex_reg_read(edev->private, offset, dst, 4);
}

static int gpex_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return gpex_reg_write(edev->private, offset, 0xFFFFFF00, src);
}

static int gpex_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return gpex_reg_write(edev->private, offset, 0xFFFF0000, src);
}

static int gpex_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return gpex_reg_write(edev->private, offset, 0x00000000, src);
}

static int gpex_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                rc = VMM_OK, i;
    char               name[64];
    struct gpex_state *s;
    struct pci_class *class;

    s = vmm_zalloc(sizeof(struct gpex_state));

    if (!s) {
        GPEX_LOG(LVL_ERR, "Failed to allocate gpex state.\n");
        rc = VMM_EFAIL;
        goto _failed;
    }

    s->node       = edev->node;
    s->guest      = guest;
    s->controller = vmm_zalloc(sizeof(struct pci_host_controller));

    if (!s->controller) {
        GPEX_LOG(
            LVL_ERR, "Failed to allocate pci host contoller"
                     "for gpex.\n");
        goto _failed;
    }

    INIT_MUTEX(&s->lock);
    INIT_LIST_HEAD(&s->controller->head);
    INIT_LIST_HEAD(&s->controller->attached_buses);
    INIT_SPIN_LOCK(&s->controller->lock);

    /* initialize class */
    class = PCI_CONTROLLER_TO_CLASS(s->controller);

    INIT_SPIN_LOCK(&class->lock);
    class->conf_header.vendor_id = PCI_VENDOR_ID_REDHAT;
    class->conf_header.device_id = PCI_DEVICE_ID_REDHAT_PCIE_HOST;
    class->config_read           = gpex_config_read;
    class->config_write          = gpex_config_write;

    rc                           = vmm_device_tree_read_u32(edev->node, "nr_buses", &s->controller->nr_buses);

    if (rc) {
        GPEX_LOG(LVL_ERR, "Failed to get fifo size in guest DTS.\n");
        goto _failed;
    }

    GPEX_LOG(LVL_VERBOSE, "%s: %d busses on this controller.\n", __func__, s->controller->nr_buses);

    for (i = 0; i < s->controller->nr_buses; i++) {
        if ((rc = pci_emu_attach_new_pci_bus(s->controller, i)) != VMM_OK) {
            GPEX_LOG(LVL_ERR, "Failed to attach PCI bus %d\n", i + 1);
            goto _failed;
        }
    }

    strlcpy(name, guest->name, sizeof(name));
    strlcat(name, "/", sizeof(name));

    if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
        rc = VMM_EOVERFLOW;
        goto _failed;
    }

    edev->private = s;

    vmm_mutex_lock(&s->lock);

    if ((rc = pci_emu_register_controller(s->node, s->guest, s->controller)) != VMM_OK) {
        GPEX_LOG(LVL_ERR, "Failed to attach PCI controller.\n");
        goto _controller_failed;
    }

    vmm_mutex_unlock(&s->lock);

    GPEX_LOG(LVL_VERBOSE, "Success.\n");

    goto _done;

_controller_failed:
    vmm_mutex_unlock(&s->lock);

_failed:

    if (s && s->controller) {
        vmm_free(s->controller);
    }

    if (s) {
        vmm_free(s);
    }

_done:
    return rc;
}

static int gpex_emulator_remove(vmm_emulate_device_t *edev)
{
    return VMM_OK;
}

static struct vmm_device_tree_nodeid gpex_emuid_table[] = {
    {
     .type       = "pci-host-controller",
     .compatible = "pci-host-cam-generic",
     },
    {.type = "pci-host-controller", .compatible = "pci-host-ecam-generic"},
    {/* end of list */},
};

static vmm_emulator_t gpex_emulator = {
    .name        = "pci-host-generic",
    .match_table = gpex_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,  // ??
    .probe       = gpex_emulator_probe,
    .read8       = gpex_emulator_read8,
    .write8      = gpex_emulator_write8,
    .read16      = gpex_emulator_read16,
    .write16     = gpex_emulator_write16,
    .read32      = gpex_emulator_read32,
    .write32     = gpex_emulator_write32,
    .reset       = gpex_emulator_reset,
    .remove      = gpex_emulator_remove,
};

static int __init gpex_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&gpex_emulator);
}

static void __exit gpex_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&gpex_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
