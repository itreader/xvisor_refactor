/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file virtio_block.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO based block device Emulator.
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_block.h>
#include <vio/vmm_virtual_disk.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC             "VirtIO Block Emulator"
#define MODULE_AUTHOR           "Anup Patel"
#define MODULE_LICENSE          "GPL"
#define MODULE_IPRIORITY        (VMM_VIRTIO_IPRIORITY + 1)
#define MODULE_INIT             virtio_block_init
#define MODULE_EXIT             virtio_block_exit

#define VIRTIO_BLK_QUEUE_SIZE   128
#define VIRTIO_BLK_IO_QUEUE     0
#define VIRTIO_BLK_NUM_QUEUES   1
#define VIRTIO_BLK_SECTOR_SIZE  512
#define VIRTIO_BLK_DISK_SEG_MAX (VIRTIO_BLK_QUEUE_SIZE - 2)

struct virtio_block_dev_req {
    struct vmm_virtio_queue        *vq;
    uint16_t                        head;
    struct vmm_virtio_iovec        *read_iov;
    uint32_t                        read_iov_cnt;
    uint32_t                        len;
    struct vmm_virtio_iovec         status_iov;
    void                           *data;
    struct vmm_virtual_disk_request r;
};

struct virtio_block_dev {
    struct vmm_virtio_device *vdev;

    struct vmm_virtio_queue     vqs[VIRTIO_BLK_NUM_QUEUES];
    struct vmm_virtio_iovec     iov[VIRTIO_BLK_QUEUE_SIZE];
    struct virtio_block_dev_req reqs[VIRTIO_BLK_QUEUE_SIZE];
    uint64_t                    features;

    struct vmm_virtio_block_config config;
    struct vmm_virtual_disk       *virtual_disk;
};

static uint64_t virtio_block_get_host_features(struct vmm_virtio_device *dev)
{
    return 1UL << VMM_VIRTIO_BLK_F_SEG_MAX | 1UL << VMM_VIRTIO_BLK_F_BLK_SIZE | 1UL << VMM_VIRTIO_BLK_F_FLUSH | 1UL << VMM_VIRTIO_RING_F_EVENT_IDX;
#if 0
    | 1UL << VMM_VIRTIO_RING_F_INDIRECT_DESC;
#endif
}

static void virtio_block_set_guest_features(struct vmm_virtio_device *dev, uint32_t select, uint32_t features)
{
    struct virtio_block_dev *vbdev = dev->emu_data;

    if (1 < select) {
        return;
    }

    vbdev->features &= ~((uint64_t)UINT_MAX << (select * 32));
    vbdev->features |= ((uint64_t)features << (select * 32));
}

static int virtio_block_init_vq(struct vmm_virtio_device *dev, uint32_t vq, uint32_t page_size, uint32_t align, uint32_t pfn)
{
    int                      rc;
    struct virtio_block_dev *vbdev = dev->emu_data;

    switch (vq) {
        case VIRTIO_BLK_IO_QUEUE:
            rc = vmm_virtio_queue_setup(&vbdev->vqs[vq], dev->guest, pfn, page_size, VIRTIO_BLK_QUEUE_SIZE, align);
            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    return rc;
}

static int virtio_block_get_pfn_vq(struct vmm_virtio_device *dev, uint32_t vq)
{
    int                      rc;
    struct virtio_block_dev *vbdev = dev->emu_data;

    switch (vq) {
        case VIRTIO_BLK_IO_QUEUE:
            rc = vmm_virtio_queue_guest_pfn(&vbdev->vqs[vq]);
            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    return rc;
}

static int virtio_block_get_size_vq(struct vmm_virtio_device *dev, uint32_t vq)
{
    int rc;

    switch (vq) {
        case VIRTIO_BLK_IO_QUEUE:
            rc = VIRTIO_BLK_QUEUE_SIZE;
            break;

        default:
            rc = 0;
            break;
    };

    return rc;
}

static int virtio_block_set_size_vq(struct vmm_virtio_device *dev, uint32_t vq, int size)
{
    /* FIXME: dynamic */
    return size;
}

static void virtio_block_req_done(struct virtio_block_dev *vbdev, struct virtio_block_dev_req *req, uint8_t status)
{
    struct vmm_virtio_device *dev     = vbdev->vdev;
    int                       queueid = req->vq - vbdev->vqs;

    if (req->read_iov && req->len && req->data && (status == VMM_VIRTIO_BLK_S_OK) &&
        (vmm_virtual_disk_get_request_type(&req->r) == VMM_VIRTUAL_DISK_REQUEST_READ)) {
        vmm_virtio_buf_to_iovec_write(dev, req->read_iov, req->read_iov_cnt, req->data, req->len);
    }

    if (req->read_iov) {
        vmm_free(req->read_iov);
        req->read_iov     = NULL;
        req->read_iov_cnt = 0;
    }

    vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_UNKNOWN);

    if (req->data) {
        vmm_free(req->data);
        req->data = NULL;
    }

    vmm_virtio_buf_to_iovec_write(dev, &req->status_iov, 1, &status, 1);

    vmm_virtio_queue_set_used_elem(req->vq, req->head, req->len);

    if (vmm_virtio_queue_should_signal(req->vq)) {
        dev->tra->notify(dev, queueid);
    }
}

static void virtio_block_attached(struct vmm_virtual_disk *virtual_disk)
{
    struct virtio_block_dev *vbdev = vmm_virtual_disk_private(virtual_disk);

    DPRINTF("%s: virtual_disk=%s\n", __func__, vmm_virtual_disk_name(virtual_disk));

    vbdev->config.capacity = vmm_virtual_disk_capacity(vbdev->virtual_disk);
    vbdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX, vbdev->config.block_size = vmm_virtual_disk_block_size(vbdev->virtual_disk);
}

static void virtio_block_detached(struct vmm_virtual_disk *virtual_disk)
{
    struct virtio_block_dev *vbdev = vmm_virtual_disk_private(virtual_disk);

    DPRINTF("%s: virtual_disk=%s\n", __func__, vmm_virtual_disk_name(virtual_disk));

    vbdev->config.capacity = 0;
    vbdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX, vbdev->config.block_size = VIRTIO_BLK_SECTOR_SIZE;
}

static void virtio_block_req_completed(struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq)
{
    DPRINTF("%s: virtual_disk=%s\n", __func__, vmm_virtual_disk_name(virtual_disk));

    virtio_block_req_done(vmm_virtual_disk_private(virtual_disk), container_of(vreq, struct virtio_block_dev_req, r), VMM_VIRTIO_BLK_S_OK);
}

static void virtio_block_req_failed(struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq)
{
    DPRINTF("%s: virtual_disk=%s\n", __func__, vmm_virtual_disk_name(virtual_disk));

    virtio_block_req_done(vmm_virtual_disk_private(virtual_disk), container_of(vreq, struct virtio_block_dev_req, r), VMM_VIRTIO_BLK_S_IOERR);
}

static void virtio_block_do_io(struct vmm_virtio_device *dev, struct virtio_block_dev *vbdev)
{
    int                            rc;
    uint16_t                       head, thead;
    uint32_t                       i, iov_cnt, len;
    struct virtio_block_dev_req   *req;
    struct vmm_virtio_queue       *vq = &vbdev->vqs[VIRTIO_BLK_IO_QUEUE];
    struct vmm_virtio_block_outhdr hdr;

    while (vmm_virtio_queue_available(vq)) {
        thead = vmm_virtio_queue_pop(vq);
        req   = &vbdev->reqs[thead];
        rc    = vmm_virtio_queue_get_head_iovec(vq, thead, vbdev->iov, &iov_cnt, &len, &head);

        if (rc) {
            vmm_printf("%s: failed to get iovec (error %d)\n", __func__, rc);
            continue;
        }

        req->vq           = vq;
        req->head         = head;
        req->read_iov     = NULL;
        req->read_iov_cnt = 0;
        req->len          = 0;

        for (i = 1; i < (iov_cnt - 1); i++) {
            req->len += vbdev->iov[i].len;
        }

        req->status_iov.addr = vbdev->iov[iov_cnt - 1].addr;
        req->status_iov.len  = vbdev->iov[iov_cnt - 1].len;
        vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_UNKNOWN);

        len = vmm_virtio_iovec_to_buf_read(dev, &vbdev->iov[0], 1, &hdr, sizeof(hdr));

        if (len < sizeof(hdr)) {
            vmm_virtio_queue_set_used_elem(req->vq, req->head, 0);
            continue;
        }

        switch (hdr.type) {
            case VMM_VIRTIO_BLK_T_IN:
                vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_READ);
                req->data = vmm_malloc(req->len);

                if (!req->data) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                    continue;
                }

                len           = sizeof(struct vmm_virtio_iovec) * (iov_cnt - 2);
                req->read_iov = vmm_malloc(len);

                if (!req->read_iov) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                    continue;
                }

                req->read_iov_cnt = iov_cnt - 2;

                for (i = 0; i < req->read_iov_cnt; i++) {
                    req->read_iov[i].addr = vbdev->iov[i + 1].addr;
                    req->read_iov[i].len  = vbdev->iov[i + 1].len;
                }

                DPRINTF(
                    "%s: VIRTIO_BLK_T_IN dev=%s "
                    "hdr.sector=%" PRIu64 " req->len=%d\n",
                    __func__, dev->name, (uint64_t)hdr.sector, req->len);
                /* Note: We will get failed() or complete() callback
                 * even when no block device attached to virtual disk
                 */
                vmm_virtual_disk_submit_request(vbdev->virtual_disk, &req->r, VMM_VIRTUAL_DISK_REQUEST_READ, hdr.sector, req->data, req->len);
                break;

            case VMM_VIRTIO_BLK_T_OUT:
                vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_WRITE);
                req->data = vmm_malloc(req->len);

                if (!req->data) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                    continue;
                } else {
                    vmm_virtio_iovec_to_buf_read(dev, &vbdev->iov[1], iov_cnt - 2, req->data, req->len);
                }

                DPRINTF(
                    "%s: VIRTIO_BLK_T_OUT dev=%s "
                    "hdr.sector=%" PRIu64 " req->len=%d\n",
                    __func__, dev->name, (uint64_t)hdr.sector, req->len);
                /* Note: We will get failed() or complete() callback
                 * even when no block device attached to virtual disk
                 */
                vmm_virtual_disk_submit_request(vbdev->virtual_disk, &req->r, VMM_VIRTUAL_DISK_REQUEST_WRITE, hdr.sector, req->data, req->len);
                break;

            case VMM_VIRTIO_BLK_T_FLUSH:
                vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_WRITE);
                DPRINTF("%s: VIRTIO_BLK_T_FLUSH dev=%s\n", __func__, dev->name);

                if (vmm_virtual_disk_flush_cache(vbdev->virtual_disk)) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                } else {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_OK);
                }

                break;

            case VMM_VIRTIO_BLK_T_GET_ID:
                vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_READ);
                req->len  = VMM_VIRTIO_BLK_ID_BYTES;
                req->data = vmm_zalloc(req->len);

                if (!req->data) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                    continue;
                }

                req->read_iov = vmm_malloc(sizeof(struct vmm_virtio_iovec));

                if (!req->read_iov) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                    continue;
                }

                req->read_iov_cnt     = 1;
                req->read_iov[0].addr = vbdev->iov[1].addr;
                req->read_iov[0].len  = vbdev->iov[1].len;
                DPRINTF("%s: VIRTIO_BLK_T_GET_ID dev=%s req->len=%d\n", __func__, dev->name, req->len);

                if (vmm_virtual_disk_current_block_device(vbdev->virtual_disk, req->data, req->len)) {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_IOERR);
                } else {
                    virtio_block_req_done(vbdev, req, VMM_VIRTIO_BLK_S_OK);
                }

                break;

            default:
                vmm_printf("%s: unhandled hdr.type=%d\n", __func__, hdr.type);
                break;
        };
    }
}

static int virtio_block_notify_vq(struct vmm_virtio_device *dev, uint32_t vq)
{
    int                      rc    = VMM_OK;
    struct virtio_block_dev *vbdev = dev->emu_data;

    DPRINTF("%s: dev=%s vq=%d\n", __func__, dev->name, vq);

    switch (vq) {
        case VIRTIO_BLK_IO_QUEUE:
            virtio_block_do_io(dev, vbdev);
            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    return rc;
}

static void virtio_block_status_changed(struct vmm_virtio_device *dev, uint32_t new_status)
{
    /* Nothing to do here. */
}

static int virtio_block_read_config(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len)
{
    uint32_t                 i;
    struct virtio_block_dev *vbdev = dev->emu_data;
    uint8_t                 *src   = (uint8_t *)&vbdev->config;

    DPRINTF("%s: dev=%s offset=%d dst=%p dst_len=%d\n", __func__, dev->name, offset, dst, dst_len);

    for (i = 0; (i < dst_len) && ((offset + i) < sizeof(vbdev->config)); i++) {
        ((uint8_t *)dst)[i] = src[offset + i];
    }

    return VMM_OK;
}

static int virtio_block_write_config(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len)
{
    uint32_t                 i;
    struct virtio_block_dev *vbdev = dev->emu_data;
    uint8_t                 *dst   = (uint8_t *)&vbdev->config;

    DPRINTF("%s: dev=%s offset=%d src=%p src_len=%d\n", __func__, dev->name, offset, src, src_len);

    for (i = 0; (i < src_len) && ((offset + i) < sizeof(vbdev->config)); i++) {
        dst[offset + i] = ((uint8_t *)src)[i];
    }

    return VMM_OK;
}

static int virtio_block_reset(struct vmm_virtio_device *dev)
{
    int                          i, rc;
    struct virtio_block_dev_req *req;
    struct virtio_block_dev     *vbdev = dev->emu_data;

    DPRINTF("%s: dev=%s\n", __func__, dev->name);

    for (i = 0; i < VIRTIO_BLK_QUEUE_SIZE; i++) {
        req = &vbdev->reqs[i];

        if (vmm_virtual_disk_get_request_type(&req->r) != VMM_VIRTUAL_DISK_REQUEST_UNKNOWN) {
            vmm_virtual_disk_abort_request(vbdev->virtual_disk, &req->r);
        }

        memset(req, 0, sizeof(*req));
        vmm_virtual_disk_set_request_type(&req->r, VMM_VIRTUAL_DISK_REQUEST_UNKNOWN);
    }

    rc = vmm_virtio_queue_cleanup(&vbdev->vqs[VIRTIO_BLK_IO_QUEUE]);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}

static int virtio_block_connect(struct vmm_virtio_device *dev, struct vmm_virtio_emulator *emu)
{
    const char              *attr;
    struct virtio_block_dev *vbdev;

    DPRINTF("%s: dev=%s emu=%s\n", __func__, dev->name, emu->name);

    vbdev = vmm_zalloc(sizeof(struct virtio_block_dev));

    if (!vbdev) {
        vmm_printf("Failed to allocate virtio block device....\n");
        return VMM_ERR_NOMEM;
    }

    vbdev->vdev            = dev;

    vbdev->config.capacity = 0;
    vbdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX, vbdev->config.block_size = VIRTIO_BLK_SECTOR_SIZE;

    vbdev->virtual_disk = vmm_virtual_disk_create(
        dev->name, VIRTIO_BLK_SECTOR_SIZE, virtio_block_attached, virtio_block_detached, virtio_block_req_completed, virtio_block_req_failed, vbdev);

    if (!vbdev->virtual_disk) {
        vmm_free(vbdev);
        return VMM_ERR_FAIL;
    }

    /* Attach block device */
    if (vmm_device_tree_read_string(dev->edev->node, "blkdev", &attr) == VMM_OK) {
        vmm_virtual_disk_attach_block_device(vbdev->virtual_disk, attr);
    }

    dev->emu_data = vbdev;

    return VMM_OK;
}

static void virtio_block_disconnect(struct vmm_virtio_device *dev)
{
    struct virtio_block_dev *vbdev = dev->emu_data;

    DPRINTF("%s: dev=%s\n", __func__, dev->name);

    vmm_virtual_disk_destroy(vbdev->virtual_disk);
    vmm_free(vbdev);
}

struct vmm_virtio_device_id virtio_block_emu_id[] = {
    {.type = VMM_VIRTIO_ID_BLOCK},
    {},
};

struct vmm_virtio_emulator virtio_block = {
    .name               = "virtio_block",
    .id_table           = virtio_block_emu_id,

    /* VirtIO operations */
    .get_host_features  = virtio_block_get_host_features,
    .set_guest_features = virtio_block_set_guest_features,
    .init_vq            = virtio_block_init_vq,
    .get_pfn_vq         = virtio_block_get_pfn_vq,
    .get_size_vq        = virtio_block_get_size_vq,
    .set_size_vq        = virtio_block_set_size_vq,
    .notify_vq          = virtio_block_notify_vq,
    .status_changed     = virtio_block_status_changed,

    /* Emulator operations */
    .read_config        = virtio_block_read_config,
    .write_config       = virtio_block_write_config,
    .reset              = virtio_block_reset,
    .connect            = virtio_block_connect,
    .disconnect         = virtio_block_disconnect,
};

static int __init virtio_block_init(void)
{
    return vmm_virtio_register_emulator(&virtio_block);
}

static void __exit virtio_block_exit(void)
{
    vmm_virtio_unregister_emulator(&virtio_block);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
