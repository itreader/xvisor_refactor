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
 * @file vmm_virtio_ring.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Ring Interface
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * include/uapi/linux/virtio_ring.h
 *
 * Copyright Rusty Russell IBM Corporation 2007.
 *
 * The original code is licensed under the BSD.
 */

#ifndef __VMM_VIRTIO_RING_H__
#define __VMM_VIRTIO_RING_H__

#include <vmm_macros.h>
#include <vmm_types.h>

/* This marks a buffer as continuing via the next field. */
#define VMM_VRING_DESC_F_NEXT           1
/* This marks a buffer as write-only (otherwise read-only). */
#define VMM_VRING_DESC_F_WRITE          2
/* This means the buffer contains a list of buffer descriptors. */
#define VMM_VRING_DESC_F_INDIRECT       4

/* The Host uses this in used->flags to advise the Guest: don't kick me when
 * you add a buffer.  It's unreliable, so it's simply an optimization.  Guest
 * will still kick if it's out of buffers. */
#define VMM_VRING_USED_F_NO_NOTIFY      1
/* The Guest uses this in avail->flags to advise the Host: don't interrupt me
 * when you consume a buffer.  It's unreliable, so it's simply an
 * optimization.  */
#define VMM_VRING_AVAIL_F_NO_INTERRUPT  1

/* We support indirect buffer descriptors */
#define VMM_VIRTIO_RING_F_INDIRECT_DESC 28

/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field.
 */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field.
 */
#define VMM_VIRTIO_RING_F_EVENT_IDX     29

/* Virtio ring descriptors: 16 bytes.  These can chain together via "next". */
struct vmm_vring_desc {
    /* Address (guest-physical). */
    uint64_t addr;
    /* Length. */
    uint32_t len;
    /* The flags as indicated above. */
    uint16_t flags;
    /* We chain unused descriptors via this, too */
    uint16_t next;
};

struct vmm_vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};

/* uint32_t is used here for ids for padding reasons. */
struct vmm_vring_used_elem {
    /* Index of start of used descriptor chain. */
    uint32_t id;
    /* Total length of the descriptor chain which was used (written to) */
    uint32_t len;
};

struct vmm_vring_used {
    uint16_t                   flags;
    uint16_t                   idx;
    struct vmm_vring_used_elem ring[];
};

struct vmm_vring {
    uint32_t num;

    struct vmm_vring_desc *desc;
    physical_addr_t        desc_pa;

    struct vmm_vring_avail *avail;
    physical_addr_t         avail_pa;

    struct vmm_vring_used *used;
    physical_addr_t        used_pa;
};

/* The standard layout for the ring is a continuous chunk of memory which looks
 * like this.  We assume num is a power of 2.
 *
 * struct vmm_vring {
 *  // The actual descriptors (16 bytes each)
 *  struct vring_desc desc[num];
 *
 *  // A ring of available descriptor heads with free-running index.
 *  uint16_t avail_flags;
 *  uint16_t avail_idx;
 *  uint16_t available[num];
 *
 *  // Padding to the next align boundary.
 *  char pad[];
 *
 *  // A ring of used descriptor heads with free-running index.
 *  uint16_t used_flags;
 *  uint16_t used_idx;
 *  struct vmm_vring_used_elem used[num];
 * };
 */
static inline void vmm_vring_init(struct vmm_vring *vr, uint32_t num, void *base, physical_addr_t base_pa, uint64_t align)
{
    vr->num      = num;

    vr->desc     = base;
    vr->desc_pa  = base_pa;

    vr->avail    = base + num * sizeof(struct vmm_vring_desc);
    vr->avail_pa = base_pa + num * sizeof(struct vmm_vring_desc);

    vr->used     = (void *)&vr->avail->ring[num];
    vr->used     = (void *)(((uint64_t)vr->used + align - 1) & ~(align - 1));
    vr->used_pa  = vr->avail_pa + offsetof(struct vmm_vring_avail, ring[num]);
    vr->used_pa  = (vr->used_pa + align - 1) & ~(align - 1);
}

static inline unsigned vmm_vring_size(uint32_t num, uint64_t align)
{
    return ((sizeof(struct vmm_vring_desc) * num + sizeof(uint16_t) * (2 + num) + align - 1) & ~(align - 1)) + sizeof(uint16_t) * 2 +
           sizeof(struct vmm_vring_used_elem) * num;
}

static inline int vmm_vring_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
    return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old);
}

#endif /* __VMM_VIRTIO_RING_H__ */
