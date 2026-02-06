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
 * @file vmm_virtio.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Core Framework Interface
 */
#ifndef __VMM_VIRTIO_H__
#define __VMM_VIRTIO_H__

#include <libs/list.h>
#include <vio/vmm_virtio_config.h>
#include <vio/vmm_virtio_ids.h>
#include <vio/vmm_virtio_ring.h>
#include <vmm_types.h>

/** VirtIO module intialization priority */
#define VMM_VIRTIO_IPRIORITY           1

#define VMM_VIRTIO_DEVICE_MAX_NAME_LEN 64

#define VMM_VIRTIO_IRQ_LOW             0
#define VMM_VIRTIO_IRQ_HIGH            1

struct vmm_guest;
struct vmm_virtio_device;
struct vmm_emulate_device;
typedef struct vmm_emulate_device vmm_emulate_device_t;

struct vmm_virtio_iovec {
    /* Address (guest-physical). */
    uint64_t addr;
    /* Length. */
    uint32_t len;
    /* The flags as indicated above. */
    uint16_t flags;
};

struct vmm_virtio_queue {
    /* The last_avail_idx field is an index to ->ring of struct vring_avail.
       It's where we assume the next request index is at.  */
    uint16_t last_avail_idx;
    uint16_t last_used_signalled;

    struct vmm_vring vring;

    struct vmm_guest *guest;
    uint32_t          desc_count;
    uint32_t          align;
    physical_addr_t   guest_pfn;
    physical_size_t   guest_page_size;
    physical_addr_t   guest_addr;
    physical_addr_t   host_addr;
    physical_size_t   total_size;
};

struct vmm_virtio_device_id {
    uint32_t type;
};

struct vmm_virtio_device {
    char                  name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN];
    vmm_emulate_device_t *edev;

    struct vmm_virtio_device_id id;

    struct vmm_virtio_transport *tra;
    void                        *tra_data;

    struct vmm_virtio_emulator *emu;
    void                       *emu_data;

    double_list_t     node;
    struct vmm_guest *guest;
};

struct vmm_virtio_transport {
    const char *name;

    int (*notify)(struct vmm_virtio_device *, uint32_t vq);
};

struct vmm_virtio_emulator {
    const char                        *name;
    const struct vmm_virtio_device_id *id_table;

    /* VirtIO operations */
    uint64_t (*get_host_features)(struct vmm_virtio_device *dev);
    void (*set_guest_features)(struct vmm_virtio_device *dev, uint32_t select, uint32_t features);
    int (*init_vq)(struct vmm_virtio_device *dev, uint32_t vq, uint32_t page_size, uint32_t align, uint32_t pfn);
    int (*get_pfn_vq)(struct vmm_virtio_device *dev, uint32_t vq);
    int (*get_size_vq)(struct vmm_virtio_device *dev, uint32_t vq);
    int (*set_size_vq)(struct vmm_virtio_device *dev, uint32_t vq, int size);
    int (*notify_vq)(struct vmm_virtio_device *dev, uint32_t vq);
    void (*status_changed)(struct vmm_virtio_device *dev, uint32_t new_status);

    /* Emulator operations */
    int (*read_config)(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len);
    int (*write_config)(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len);
    int (*reset)(struct vmm_virtio_device *dev);
    int (*connect)(struct vmm_virtio_device *dev, struct vmm_virtio_emulator *emu);
    void (*disconnect)(struct vmm_virtio_device *dev);

    double_list_t node;
};

/** Get guest to which the queue belongs
 *  Note: only available after queue setup is done
 */
struct vmm_guest *vmm_virtio_queue_guest(struct vmm_virtio_queue *vq);

/** Get maximum number of descriptors in queue
 *  Note: only available after queue setup is done
 */
uint32_t vmm_virtio_queue_desc_count(struct vmm_virtio_queue *vq);

/** Get queue alignment
 *  Note: only available after queue setup is done
 */
uint32_t vmm_virtio_queue_align(struct vmm_virtio_queue *vq);

/** Get guest page frame number of queue
 *  Note: only available after queue setup is done
 */
physical_addr_t vmm_virtio_queue_guest_pfn(struct vmm_virtio_queue *vq);

/** Get guest page size for this queue
 *  Note: only available after queue setup is done
 */
physical_size_t vmm_virtio_queue_guest_page_size(struct vmm_virtio_queue *vq);

/** Get guest physical address of this queue
 *  Note: only available after queue setup is done
 */
physical_addr_t vmm_virtio_queue_guest_addr(struct vmm_virtio_queue *vq);

/** Get host physical address of this queue
 *  Note: only available after queue setup is done
 */
physical_addr_t vmm_virtio_queue_host_addr(struct vmm_virtio_queue *vq);

/** Get total physical space required by this queue
 *  Note: only available after queue setup is done
 */
physical_size_t virtio_queue_total_size(struct vmm_virtio_queue *vq);

/** Retrive maximum number of vring descriptors
 *  Note: works only after queue setup is done
 */
uint32_t vmm_virtio_queue_max_desc(struct vmm_virtio_queue *vq);

/** Retrive vring descriptor at given index
 *  Note: works only after queue setup is done
 */
int vmm_virtio_queue_get_desc(struct vmm_virtio_queue *vq, uint16_t indx, struct vmm_vring_desc *desc);

/** Pop the index of next available descriptor
 *  Note: works only after queue setup is done
 */
uint16_t vmm_virtio_queue_pop(struct vmm_virtio_queue *vq);

/** Check whether any descriptor is available or not
 *  Note: works only after queue setup is done
 */
bool vmm_virtio_queue_available(struct vmm_virtio_queue *vq);

/** Check whether queue notification is required
 *  Note: works only after queue setup is done
 */
bool vmm_virtio_queue_should_signal(struct vmm_virtio_queue *vq);

/** Update avail_event in vring
 *  Note: works only after queue setup is done
 */
void vmm_virtio_queue_set_avail_event(struct vmm_virtio_queue *vq);

/** Update used element in vring
 *  Note: works only after queue setup is done
 */
void vmm_virtio_queue_set_used_elem(struct vmm_virtio_queue *vq, uint32_t head, uint32_t len);

/** Check whether queue setup is done by guest or not */
bool vmm_virtio_queue_setup_done(struct vmm_virtio_queue *vq);

/** Cleanup or reset the queue
 *  Note: After cleanup we need to setup queue before reusing it.
 */
int vmm_virtio_queue_cleanup(struct vmm_virtio_queue *vq);

/** Setup or initialize the queue
 *  Note: If queue was already setup then it will cleanup first.
 */
int vmm_virtio_queue_setup(
    struct vmm_virtio_queue *vq, struct vmm_guest *guest, physical_addr_t guest_pfn, physical_size_t guest_page_size, uint32_t desc_count,
    uint32_t align);

/** Get guest IO vectors based on given head
 *  Note: works only after queue setup is done
 */
int vmm_virtio_queue_get_head_iovec(
    struct vmm_virtio_queue *vq, uint16_t head, struct vmm_virtio_iovec *iov, uint32_t *ret_iov_cnt, uint32_t *ret_total_len, uint16_t *ret_head);

/** Get guest IO vectors based on current head
 *  Note: works only after queue setup is done
 */
int vmm_virtio_queue_get_iovec(
    struct vmm_virtio_queue *vq, struct vmm_virtio_iovec *iov, uint32_t *ret_iov_cnt, uint32_t *ret_total_len, uint16_t *ret_head);

/** Read contents from guest IO vectors to a buffer */
uint32_t vmm_virtio_iovec_to_buf_read(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt, void *buf, uint32_t buf_len);

/** Write contents to guest IO vectors from a buffer */
uint32_t vmm_virtio_buf_to_iovec_write(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt, void *buf, uint32_t buf_len);

/** Fill guest IO vectors with zeros */
void vmm_virtio_iovec_fill_zeros(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt);

/** Read VirtIO device configuration */
int vmm_virtio_config_read(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len);

/** Write VirtIO device configuration */
int vmm_virtio_config_write(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len);

/** Reset VirtIO device */
int vmm_virtio_reset(struct vmm_virtio_device *dev);

/** Register VirtIO device */
int vmm_virtio_register_device(struct vmm_virtio_device *dev);

/** UnRegister VirtIO device */
void vmm_virtio_unregister_device(struct vmm_virtio_device *dev);

/** Register VirtIO device emulator */
int vmm_virtio_register_emulator(struct vmm_virtio_emulator *emu);

/** UnRegister VirtIO device emulator */
void vmm_virtio_unregister_emulator(struct vmm_virtio_emulator *emu);

#endif /* __VMM_VIRTIO_H__ */
