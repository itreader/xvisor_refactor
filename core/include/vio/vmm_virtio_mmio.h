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
 * @file vmm_virtio_mmio.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO MMIO传输层接口
 */

#ifndef __VMM_VIRTIO_MMIO_H__
#define __VMM_VIRTIO_MMIO_H__

/*
 * Control registers --> Copied from linux's virtio_mmio.h
 */

/* Magic value ("virt" string) - Read Only */
#define VMM_VIRTIO_MMIO_MAGIC_VALUE         0x000

/* Virtio device version - Read Only */
#define VMM_VIRTIO_MMIO_VERSION             0x004

/* Virtio device ID - Read Only */
#define VMM_VIRTIO_MMIO_DEVICE_ID           0x008

/* Virtio vendor ID - Read Only */
#define VMM_VIRTIO_MMIO_VENDOR_ID           0x00c

/* Bitmask of the features supported by the host (device)
 * (32 bits per set) - Read Only */
#define VMM_VIRTIO_MMIO_HOST_FEATURES       0x010
#define VMM_VIRTIO_MMIO_DEVICE_FEATURES     VMM_VIRTIO_MMIO_HOST_FEATURES

/* Host (device) features set selector - Write Only */
#define VMM_VIRTIO_MMIO_HOST_FEATURES_SEL   0x014
#define VMM_VIRTIO_MMIO_DEVICE_FEATURES_SEL VMM_VIRTIO_MMIO_HOST_FEATURES_SEL

/* Bitmask of features activated by the guest (driver)
 * (32 bits per set) - Write Only */
#define VMM_VIRTIO_MMIO_GUEST_FEATURES      0x020
#define VMM_VIRTIO_MMIO_DRIVER_FEATURES     VMM_VIRTIO_MMIO_GUEST_FEATURES

/* Activated features set selector by the guest (driver) - Write Only */
#define VMM_VIRTIO_MMIO_GUEST_FEATURES_SEL  0x024
#define VMM_VIRTIO_MMIO_DRIVER_FEATURES_SEL VMM_VIRTIO_MMIO_GUEST_FEATURES_SEL

/* Guest's memory page size in bytes - Write Only */
#define VMM_VIRTIO_MMIO_GUEST_PAGE_SIZE     0x028

/* Queue selector - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_SEL           0x030

/* Maximum size of the currently selected queue - Read Only */
#define VMM_VIRTIO_MMIO_QUEUE_NUM_MAX       0x034

/* Queue size for the currently selected queue - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_NUM           0x038

/* Used Ring alignment for the currently selected queue - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_ALIGN         0x03c

/* PFN for the currently selected queue - Read Write */
#define VMM_VIRTIO_MMIO_QUEUE_PFN           0x040

/* Ready bit for the currently selected queue - Read Write */
#define VMM_VIRTIO_MMIO_QUEUE_READY         0x044

/* Queue notifier - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_NOTIFY        0x050

/* Interrupt status - Read Only */
#define VMM_VIRTIO_MMIO_INTERRUPT_STATUS    0x060

/* Interrupt acknowledge - Write Only */
#define VMM_VIRTIO_MMIO_INTERRUPT_ACK       0x064

/* Device status register - Read Write */
#define VMM_VIRTIO_MMIO_STATUS              0x070

/* Selected queue's Descriptor Table address, 64 bits in two halves */
#define VMM_VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VMM_VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084

/* Selected queue's Available Ring address, 64 bits in two halves */
#define VMM_VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090
#define VMM_VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094

/* Selected queue's Used Ring address, 64 bits in two halves */
#define VMM_VIRTIO_MMIO_QUEUE_USED_LOW      0x0a0
#define VMM_VIRTIO_MMIO_QUEUE_USED_HIGH     0x0a4

/* Configuration atomicity value */
#define VMM_VIRTIO_MMIO_CONFIG_GENERATION   0x0fc

/* The config space is defined by each driver as
 * the per-driver configuration space - Read Write */
#define VMM_VIRTIO_MMIO_CONFIG              0x100

/*
 * Interrupt flags (re: interrupt status & acknowledge registers)
 */

#define VMM_VIRTIO_MMIO_INT_VRING           (1 << 0)
#define VMM_VIRTIO_MMIO_INT_CONFIG          (1 << 1)

#define VMM_VIRTIO_MMIO_MAX_VQ              3
#define VMM_VIRTIO_MMIO_MAX_CONFIG          1
#define VMM_VIRTIO_MMIO_IO_SIZE             0x200

/**
 * @brief VirtIO MMIO配置空间，定义MMIO设备的寄存器和特性
 */
struct vmm_virtio_mmio_config {
    char     magic[4]; /**< 魔术值 */
    uint32_t version; /**< 版本号 */
    uint32_t device_id; /**< 设备ID */
    uint32_t vendor_id; /**< 厂商ID */
    uint32_t host_features; /**< 主机特性 */
    uint32_t host_features_sel; /**< host_features_sel成员 */
    uint32_t reserved_1[2]; /**< reserved_1成员 */
    uint32_t guest_features; /**< 客户机特性 */
    uint32_t guest_features_sel; /**< guest_features_sel成员 */
    uint32_t guest_page_size; /**< guest_page_size成员 */
    uint32_t reserved_2; /**< reserved_2成员 */
    uint32_t queue_sel; /**< 队列选择 */
    uint32_t queue_num_max; /**< 队列最大数量 */
    uint32_t queue_num; /**< 队列数量 */
    uint32_t queue_align; /**< 队列对齐 */
    uint32_t queue_pfn; /**< 队列页帧号 */
    uint32_t reserved_3[3]; /**< reserved_3成员 */
    uint32_t queue_notify; /**< queue_notify成员 */
    uint32_t reserved_4[3]; /**< reserved_4成员 */
    uint32_t interrupt_state; /**< interrupt_state成员 */
    uint32_t interrupt_ack; /**< 中断应答 */
    uint32_t reserved_5[2]; /**< reserved_5成员 */
    uint32_t status; /**< 状态 */
} __attribute__((packed));

#endif /* __VMM_VIRTIO_MMIO_H__ */
