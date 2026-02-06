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
 * @file virtio_pci.c
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Virtio PCI Transport Layer Emulator
 */

#include <emu/pci/pci_emu_core.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_pci.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define VIRTIO_MAX_DEVICE_ID          10
#define VIRTIO_MIN_DEVICE_ID          1

#define VIRTIO_PCI_VENDOR_ID          0x1af4
#define VIRTIO_PCI_DEVICE_ID_BASE     0x1000

#define GET_VIRTIO_PCI_DEVICE_ID(did) (VIRTIO_PCI_DEVICE_ID_BASE + did)

#define VIRTIO_PCI_EMU_IPRIORITY      (PCI_EMU_CORE_IPRIORITY + VMM_VIRTIO_IPRIORITY + 1)

#define MODULE_DESC                   "Virtio PCI Transport Layer"
#define MODULE_AUTHOR                 "Himanshu Chauhan"
#define MODULE_LICENSE                "GPL"
#define MODULE_IPRIORITY              VIRTIO_PCI_EMU_IPRIORITY
#define MODULE_INIT                   virtio_pci_emulator_init
#define MODULE_EXIT                   virtio_pci_emulator_exit

struct virtio_pci_dev {
    struct vmm_guest            *guest;
    struct vmm_virtio_device     dev;
    struct vmm_virtio_pci_config config;
    uint32_t                     irq;
};

static int virtio_pci_notify(struct vmm_virtio_device *dev, uint32_t vq)
{
    struct virtio_pci_dev *m = dev->tra_data;

    m->config.interrupt_state |= VMM_VIRTIO_PCI_INT_VRING;

    vmm_device_emulate_emulate_irq(m->guest, m->irq, 1);

    return VMM_OK;
}

int virtio_pci_config_read(struct virtio_pci_dev *m, uint32_t offset, void *dst, uint32_t dst_len)
{
    int rc = VMM_OK;

    switch (offset) {
        case VMM_VIRTIO_PCI_HOST_FEATURES:
            *(uint32_t *)dst = (uint32_t)m->dev.emu->get_host_features(&m->dev);
            break;

        case VMM_VIRTIO_PCI_QUEUE_PFN:
            *(uint32_t *)dst = m->dev.emu->get_pfn_vq(&m->dev, m->config.queue_sel);
            break;

        case VMM_VIRTIO_PCI_QUEUE_NUM:
            *(uint32_t *)dst = m->dev.emu->get_size_vq(&m->dev, m->config.queue_sel);
            break;

        case VMM_VIRTIO_PCI_STATUS:
            *(uint32_t *)dst = m->config.status;
            break;

        case VMM_VIRTIO_PCI_ISR:
            /* reading from the ISR also clears it. */
            *(uint32_t *)dst          = m->config.interrupt_state;
            m->config.interrupt_state = 0;
            vmm_device_emulate_emulate_irq(m->guest, m->irq, 0);
            break;

        default:
            vmm_printf("%s: guest=%s invalid offset=0x%x\n", __func__, m->guest->name, offset);
            rc = VMM_EINVALID;
            break;
    }

    return rc;
}

static int virtio_pci_read(struct virtio_pci_dev *m, uint32_t offset, uint32_t *dst, uint32_t dst_len)
{
    /* Device specific config write */
    if (offset >= VMM_VIRTIO_PCI_CONFIG) {
        offset -= VMM_VIRTIO_PCI_CONFIG;
        return vmm_virtio_config_read(&m->dev, offset, dst, dst_len);
    }

    return virtio_pci_config_read(m, offset, dst, dst_len);
}

static int virtio_pci_config_write(struct virtio_pci_dev *m, uint32_t offset, void *src, uint32_t src_len)
{
    int      rc  = VMM_OK;
    uint32_t val = *(uint32_t *)(src);

    switch (offset) {
        case VMM_VIRTIO_PCI_GUEST_FEATURES:
            m->dev.emu->set_guest_features(&m->dev, 0, val);
            break;

        case VMM_VIRTIO_PCI_QUEUE_PFN:
            m->dev.emu->init_vq(&m->dev, m->config.queue_sel, VMM_VIRTIO_PCI_PAGE_SIZE, VMM_VIRTIO_PCI_PAGE_SIZE, val);
            break;

        case VMM_VIRTIO_PCI_QUEUE_SEL:
            if (val < VMM_VIRTIO_PCI_QUEUE_MAX) {
                m->config.queue_sel = (uint16_t)val;
            }

            break;

        case VMM_VIRTIO_PCI_QUEUE_NOTIFY:
            if (val < VMM_VIRTIO_PCI_QUEUE_MAX) {
                m->dev.emu->notify_vq(&m->dev, val);
            }

            break;

        case VMM_VIRTIO_PCI_STATUS:
            if ((uint8_t)val != m->config.status) {
                m->dev.emu->status_changed(&m->dev, val);
            }

            m->config.status = (uint8_t)val;
            break;

        default:
            vmm_printf("%s: guest=%s invalid offset=0x%x\n", __func__, m->guest->name, offset);
            rc = VMM_EINVALID;
            break;
    }

    return rc;
}

static int virtio_pci_write(struct virtio_pci_dev *m, uint32_t offset, uint32_t src_mask, uint32_t src, uint32_t src_len)
{
    src = src & ~src_mask;

    /* Device specific config write */
    if (offset >= VMM_VIRTIO_PCI_CONFIG) {
        offset -= VMM_VIRTIO_PCI_CONFIG;
        return vmm_virtio_config_write(&m->dev, offset, &src, src_len);
    }

    return virtio_pci_config_write(m, offset, &src, src_len);
}

static struct vmm_virtio_transport pci_tra = {
    .name   = "virtio_pci",
    .notify = virtio_pci_notify,
};

static int virtio_pci_emulator_reset(struct pci_device *pdev)
{
    return VMM_OK;
}

static int virtio_pci_emulator_probe(struct pci_device *pdev, struct vmm_guest *guest, const struct vmm_device_tree_nodeid *eid)
{
    struct pci_class *class = PCI_DEVICE_TO_CLASS(pdev);

    /* sanitize device ID */
    if ((pdev->device_id > VIRTIO_MAX_DEVICE_ID) || (pdev->device_id < VIRTIO_MIN_DEVICE_ID)) {
        return VMM_EFAIL;
    }

    /* Virtio device */
    class->conf_header.vendor_id = VIRTIO_PCI_VENDOR_ID;
    /* Block Device */
    class->conf_header.device_id = GET_VIRTIO_PCI_DEVICE_ID(pdev->device_id);

    pdev->private                = NULL;

    return VMM_OK;
}

static int virtio_pci_emulator_remove(struct pci_device *pdev)
{
    return VMM_OK;
}

static int virtio_pci_bar_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = virtio_pci_read(edev->private, offset, &regval, 1);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int virtio_pci_bar_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = virtio_pci_read(edev->private, offset, &regval, 2);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int virtio_pci_bar_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return virtio_pci_read(edev->private, offset, dst, 4);
}

static int virtio_pci_bar_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return virtio_pci_write(edev->private, offset, 0xFFFFFF00, src, 1);
}

static int virtio_pci_bar_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return virtio_pci_write(edev->private, offset, 0xFFFF0000, src, 2);
}

static int virtio_pci_bar_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return virtio_pci_write(edev->private, offset, 0x00000000, src, 4);
}

static int virtio_pci_bar_reset(vmm_emulate_device_t *edev)
{
    struct virtio_pci_dev *m  = edev->private;

    m->config.queue_sel       = 0x0;
    m->config.interrupt_state = 0x0;
    m->config.status          = 0x0;
    vmm_device_emulate_emulate_irq(m->guest, m->irq, 0);

    return vmm_virtio_reset(&m->dev);
}

static int virtio_pci_bar_remove(vmm_emulate_device_t *edev)
{
    struct virtio_pci_dev *vdev = edev->private;

    if (vdev) {
        vmm_virtio_unregister_device(&vdev->dev);
        vmm_free(vdev);
        edev->private = NULL;
    }

    return VMM_OK;
}

static int virtio_pci_bar_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                    rc = VMM_OK;
    struct virtio_pci_dev *vdev;

    vdev = vmm_zalloc(sizeof(struct virtio_pci_dev));

    if (!vdev) {
        rc = VMM_ENOMEM;
        goto virtio_pci_probe_done;
    }

    vdev->guest = guest;

    vmm_snprintf(vdev->dev.name, VMM_VIRTIO_DEVICE_MAX_NAME_LEN, "%s/%s", guest->name, edev->node->name);
    vdev->dev.edev     = edev;
    vdev->dev.tra      = &pci_tra;
    vdev->dev.tra_data = vdev;
    vdev->dev.guest    = guest;

    vdev->config       = (struct vmm_virtio_pci_config){
              .queue_num = 256,
    };

    rc = vmm_device_tree_read_u32(edev->node, "virtio_type", &vdev->dev.id.type);

    if (rc) {
        goto virtio_pci_probe_freestate_fail;
    }

    rc = vmm_device_tree_read_u32_atindex(edev->node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &vdev->irq, 0);

    if (rc) {
        goto virtio_pci_probe_freestate_fail;
    }

    if ((rc = vmm_virtio_register_device(&vdev->dev))) {
        goto virtio_pci_probe_freestate_fail;
    }

    edev->private = vdev;

    goto virtio_pci_probe_done;

virtio_pci_probe_freestate_fail:
    vmm_free(vdev);
virtio_pci_probe_done:
    return rc;
}

static struct vmm_device_tree_nodeid virtio_pci_emuid_table[] = {
    {
     .type       = "virtio",
     .compatible = "virtio,pci",
     },
    {/* end of list */                     },
};

static struct pci_dev_emulator virtio_pci_emulator = {
    .name        = "virtio-pci",
    .match_table = virtio_pci_emuid_table,
    .probe       = virtio_pci_emulator_probe,
    .reset       = virtio_pci_emulator_reset,
    .remove      = virtio_pci_emulator_remove,
};

static struct vmm_device_tree_nodeid virtio_pci_bar_emulator_emuid_table[] = {
    {
     .type       = "virtio",
     .compatible = "virtio,pci,bar",
     },
    {/* end of list */                     },
};

static vmm_emulator_t virtio_bar_emulator = {
    .name        = "virtio-pci-bar",
    .match_table = virtio_pci_bar_emulator_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = virtio_pci_bar_probe,
    .read8       = virtio_pci_bar_read8,
    .write8      = virtio_pci_bar_write8,
    .read16      = virtio_pci_bar_read16,
    .write16     = virtio_pci_bar_write16,
    .read32      = virtio_pci_bar_read32,
    .write32     = virtio_pci_bar_write32,
    .reset       = virtio_pci_bar_reset,
    .remove      = virtio_pci_bar_remove,
};

static int __init virtio_pci_emulator_init(void)
{
    int rc;

    if ((rc = pci_emu_register_device(&virtio_pci_emulator)) != VMM_OK) {
        return rc;
    }

    return vmm_device_emulate_register_emulator(&virtio_bar_emulator);
}

static void __exit virtio_pci_emulator_exit(void)
{
    pci_emu_unregister_device(&virtio_pci_emulator);
    vmm_device_emulate_unregister_emulator(&virtio_bar_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
