/**
 * Copyright (c) 2013 Pranav Sawargaonkar.
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
 * @file virtio_mmio.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO MMIO Transport Device
 */

#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_mmio.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "VirtIO MMIO Transport"
#define MODULE_AUTHOR    "Pranav Sawargaonkar"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VIRTIO_IPRIORITY + 1)
#define MODULE_INIT      virtio_mmio_init
#define MODULE_EXIT      virtio_mmio_exit

struct virtio_mmio_dev {
    struct vmm_guest             *guest;
    struct vmm_virtio_device      dev;
    struct vmm_virtio_mmio_config config;
    uint32_t                      irq;
};

static int virtio_mmio_notify(struct vmm_virtio_device *dev, uint32_t vq)
{
    struct virtio_mmio_dev *m = dev->tra_data;

    m->config.interrupt_state |= VMM_VIRTIO_MMIO_INT_VRING;

    vmm_device_emulate_emulate_irq(m->guest, m->irq, 1);

    return VMM_OK;
}

int virtio_mmio_config_read(struct virtio_mmio_dev *m, uint32_t offset, void *dst, uint32_t dst_len)
{
    int rc = VMM_OK;

    if (dst_len != 4) {
        vmm_printf("%s: guest=%s invalid length=%d\n", __func__, m->guest->name, dst_len);
        return VMM_ERR_INVALID;
    }

    switch (offset) {
        case VMM_VIRTIO_MMIO_MAGIC_VALUE:
            *(uint32_t *)dst = *((uint32_t *)((void *)&m->config.magic[0]));
            break;

        case VMM_VIRTIO_MMIO_VERSION:
            *(uint32_t *)dst = *((uint32_t *)((void *)&m->config.version));
            break;

        case VMM_VIRTIO_MMIO_DEVICE_ID:
            *(uint32_t *)dst = *((uint32_t *)((void *)&m->config.device_id));
            break;

        case VMM_VIRTIO_MMIO_VENDOR_ID:
            *(uint32_t *)dst = *((uint32_t *)((void *)&m->config.vendor_id));
            break;

        case VMM_VIRTIO_MMIO_INTERRUPT_STATUS:
            *(uint32_t *)dst = *((uint32_t *)((void *)&m->config.interrupt_state));
            break;

        case VMM_VIRTIO_MMIO_HOST_FEATURES:
            if (m->config.host_features_sel == 0) {
                *(uint32_t *)dst = (uint32_t)m->dev.emu->get_host_features(&m->dev);
            } else {
                *(uint32_t *)dst = (uint32_t)(m->dev.emu->get_host_features(&m->dev) >> 32);
            }

            break;

        case VMM_VIRTIO_MMIO_QUEUE_PFN:
            *(uint32_t *)dst = m->dev.emu->get_pfn_vq(&m->dev, m->config.queue_sel);
            break;

        case VMM_VIRTIO_MMIO_QUEUE_NUM_MAX:
            *(uint32_t *)dst = m->dev.emu->get_size_vq(&m->dev, m->config.queue_sel);
            break;

        case VMM_VIRTIO_MMIO_STATUS:
            *(uint32_t *)dst = *((uint32_t *)((void *)&m->config.status));
            break;

        default:
            vmm_printf("%s: guest=%s invalid offset=0x%x\n", __func__, m->guest->name, offset);
            rc = VMM_ERR_INVALID;
            break;
    }

    return rc;
}

static int virtio_mmio_read(struct virtio_mmio_dev *m, uint32_t offset, uint32_t *dst, uint32_t dst_len)
{
    /* Device specific config write */
    if (offset >= VMM_VIRTIO_MMIO_CONFIG) {
        offset -= VMM_VIRTIO_MMIO_CONFIG;
        return vmm_virtio_config_read(&m->dev, offset, dst, dst_len);
    }

    return virtio_mmio_config_read(m, offset, dst, dst_len);
}

static int virtio_mmio_config_write(struct virtio_mmio_dev *m, uint32_t offset, void *src, uint32_t src_len)
{
    int      rc  = VMM_OK;
    uint32_t val = *(uint32_t *)(src);

    if (src_len != 4) {
        vmm_printf("%s: guest=%s invalid length=%d\n", __func__, m->guest->name, src_len);
        return VMM_ERR_INVALID;
    }

    switch (offset) {
        case VMM_VIRTIO_MMIO_HOST_FEATURES_SEL:
            m->config.host_features_sel = val;
            break;

        case VMM_VIRTIO_MMIO_GUEST_FEATURES_SEL:
            m->config.guest_features_sel = val;
            break;

        case VMM_VIRTIO_MMIO_GUEST_FEATURES:
            m->dev.emu->set_guest_features(&m->dev, m->config.guest_features_sel, val);
            break;

        case VMM_VIRTIO_MMIO_GUEST_PAGE_SIZE:
            m->config.guest_page_size = val;
            break;

        case VMM_VIRTIO_MMIO_QUEUE_SEL:
            m->config.queue_sel = val;
            break;

        case VMM_VIRTIO_MMIO_QUEUE_NUM:
            m->config.queue_num = val;
            m->dev.emu->set_size_vq(&m->dev, m->config.queue_sel, m->config.queue_num);
            break;

        case VMM_VIRTIO_MMIO_QUEUE_ALIGN:
            m->config.queue_align = val;
            break;

        case VMM_VIRTIO_MMIO_QUEUE_PFN:
            m->dev.emu->init_vq(&m->dev, m->config.queue_sel, m->config.guest_page_size, m->config.queue_align, val);
            break;

        case VMM_VIRTIO_MMIO_QUEUE_NOTIFY:
            m->dev.emu->notify_vq(&m->dev, val);
            break;

        case VMM_VIRTIO_MMIO_INTERRUPT_ACK:
            m->config.interrupt_state &= ~val;
            vmm_device_emulate_emulate_irq(m->guest, m->irq, 0);
            break;

        case VMM_VIRTIO_MMIO_STATUS:
            if (val != m->config.status) {
                m->dev.emu->status_changed(&m->dev, val);
            }

            m->config.status = val;
            break;

        default:
            vmm_printf("%s: guest=%s invalid offset=0x%x\n", __func__, m->guest->name, offset);
            rc = VMM_ERR_INVALID;
            break;
    };

    return rc;
}

static int virtio_mmio_write(struct virtio_mmio_dev *m, uint32_t offset, uint32_t src_mask, uint32_t src, uint32_t src_len)
{
    src = src & ~src_mask;

    /* Device specific config write */
    if (offset >= VMM_VIRTIO_MMIO_CONFIG) {
        offset -= VMM_VIRTIO_MMIO_CONFIG;
        return vmm_virtio_config_write(&m->dev, offset, &src, src_len);
    }

    return virtio_mmio_config_write(m, offset, &src, src_len);
}

static int virtio_mmio_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = virtio_mmio_read(edev->private, offset, &regval, 1);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int virtio_mmio_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = virtio_mmio_read(edev->private, offset, &regval, 2);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int virtio_mmio_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return virtio_mmio_read(edev->private, offset, dst, 4);
}

static int virtio_mmio_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return virtio_mmio_write(edev->private, offset, 0xFFFFFF00, src, 1);
}

static int virtio_mmio_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return virtio_mmio_write(edev->private, offset, 0xFFFF0000, src, 2);
}

static int virtio_mmio_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return virtio_mmio_write(edev->private, offset, 0x00000000, src, 4);
}

static int virtio_mmio_reset(vmm_emulate_device_t *edev)
{
    struct virtio_mmio_dev *m    = edev->private;

    m->config.host_features_sel  = 0x0;
    m->config.guest_features_sel = 0x0;
    m->config.queue_sel          = 0x0;
    m->config.interrupt_state    = 0x0;
    m->config.status             = 0x0;
    vmm_device_emulate_emulate_irq(m->guest, m->irq, 0);

    return vmm_virtio_reset(&m->dev);
}

static struct vmm_virtio_transport mmio_tra = {
    .name   = "virtio_mmio",
    .notify = virtio_mmio_notify,
};

static int virtio_mmio_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    uint32_t                val = 0;
    int                     rc  = VMM_OK;
    struct virtio_mmio_dev *m;

    m = vmm_zalloc(sizeof(struct virtio_mmio_dev));

    if (!m) {
        rc = VMM_ERR_NOMEM;
        goto virtio_mmio_probe_done;
    }

    m->guest = guest;

    vmm_snprintf(m->dev.name, VMM_VIRTIO_DEVICE_MAX_NAME_LEN, "%s/%s", guest->name, edev->node->name);
    m->dev.edev     = edev;
    m->dev.tra      = &mmio_tra;
    m->dev.tra_data = m;
    m->dev.guest    = guest;

    m->config       = (struct vmm_virtio_mmio_config){
              .magic         = {'v', 'i', 'r', 't'},
              .version       = 1,
              .vendor_id     = 0x52535658, /* XVSR */
              .queue_num_max = 256,
    };

    rc = vmm_device_tree_read_u32(edev->node, "virtio_type", &val);

    if (rc) {
        goto virtio_mmio_probe_freestate_fail;
    }

    m->config.device_id = val;
    m->dev.id.type      = m->config.device_id;

    rc                  = vmm_device_tree_read_u32_atindex(edev->node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &m->irq, 0);

    if (rc) {
        goto virtio_mmio_probe_freestate_fail;
    }

    if ((rc = vmm_virtio_register_device(&m->dev))) {
        goto virtio_mmio_probe_freestate_fail;
    }

    edev->private = m;

    goto virtio_mmio_probe_done;

virtio_mmio_probe_freestate_fail:
    vmm_free(m);
virtio_mmio_probe_done:
    return rc;
}

static int virtio_mmio_remove(vmm_emulate_device_t *edev)
{
    struct virtio_mmio_dev *m = edev->private;

    if (m) {
        vmm_virtio_unregister_device(&m->dev);
        vmm_free(m);
        edev->private = NULL;
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid virtio_mmio_emuid_table[] = {
    {
     .type       = "virtio",
     .compatible = "virtio,mmio",
     },
    {/* end of list */                     },
};

static vmm_emulator_t virtio_mmio = {
    .name        = "virtio_mmio",
    .match_table = virtio_mmio_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = virtio_mmio_probe,
    .read8       = virtio_mmio_read8,
    .write8      = virtio_mmio_write8,
    .read16      = virtio_mmio_read16,
    .write16     = virtio_mmio_write16,
    .read32      = virtio_mmio_read32,
    .write32     = virtio_mmio_write32,
    .reset       = virtio_mmio_reset,
    .remove      = virtio_mmio_remove,
};

static int __init virtio_mmio_init(void)
{
    return vmm_device_emulate_register_emulator(&virtio_mmio);
}

static void __exit virtio_mmio_exit(void)
{
    vmm_device_emulate_unregister_emulator(&virtio_mmio);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
