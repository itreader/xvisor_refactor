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
 * @file vmm_virtio.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO半虚拟化框架实现
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vio/vmm_virtio.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "VirtIO Para-virtualization Framework"
#define MODULE_AUTHOR    "Pranav Sawargaonkar"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY VMM_VIRTIO_IPRIORITY
#define MODULE_INIT      vmm_virtio_core_init
#define MODULE_EXIT      vmm_virtio_core_exit

/*
 * virtio_mutex protects entire virtio subsystem and is taken every time
 * virtio device or emulator is registered or unregistered.
 */

static DEFINE_MUTEX(virtio_mutex);

static LIST_HEAD(virtio_dev_list);

static LIST_HEAD(virtio_emu_list);

/* ========== VirtIO queue implementations ========== */

struct vmm_guest *vmm_virtio_queue_guest(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->guest : NULL; /**< NULL成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_guest);

/**
 * @brief 获取VirtIO队列描述符的数量
 * @param vq VirtIO队列指针
 * @return 数量值
 */
uint32_t vmm_virtio_queue_desc_count(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->desc_count : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_desc_count);

/**
 * @brief VirtIO队列对齐设置
 * @param vq VirtIO队列指针
 * @return 数量值
 */
uint32_t vmm_virtio_queue_align(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->align : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_align);

/**
 * @brief 获取VirtIO队列的客户机页帧号
 * @param vq VirtIO队列指针
 * @return 获取到的值，失败返回错误码
 */
physical_addr_t vmm_virtio_queue_guest_pfn(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->guest_pfn : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_guest_pfn);

/**
 * @brief 获取VirtIO队列的客户机页大小
 * @param vq VirtIO队列指针
 * @return 大小值（字节）
 */
physical_size_t vmm_virtio_queue_guest_page_size(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->guest_page_size : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_guest_page_size);

/**
 * @brief 获取VirtIO队列的客户机物理地址
 * @param vq VirtIO队列指针
 * @return 大小值（字节）
 */
physical_addr_t vmm_virtio_queue_guest_addr(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->guest_addr : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_guest_addr);

/**
 * @brief 获取VirtIO队列的主机虚拟地址
 * @param vq VirtIO队列指针
 * @return 获取到的值，失败返回错误码
 */
physical_addr_t vmm_virtio_queue_host_addr(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->host_addr : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_host_addr);

/**
 * @brief 获取VirtIO队列的总大小
 * @param vq VirtIO队列指针
 * @return 大小值（字节）
 */
physical_size_t vmm_virtio_queue_total_size(struct vmm_virtio_queue *vq)
{
    return (vq) ? vq->total_size : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_total_size);

/**
 * @brief 获取VirtIO队列的最大描述符的数量
 * @param vq VirtIO队列指针
 * @return 数量值
 */
uint32_t vmm_virtio_queue_max_desc(struct vmm_virtio_queue *vq)
{
    if (!vq || !vq->guest) {
        return 0;
    }

    return vq->desc_count;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_max_desc);

/**
 * @brief 获取VirtIO队列的desc
 * @param vq VirtIO队列指针
 * @param indx 索引值
* @param desc MSI描述符结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_get_desc(struct vmm_virtio_queue *vq, uint16_t indx, struct vmm_vring_desc *desc)
{
    uint32_t        ret;
    physical_addr_t desc_pa;

    if (!vq || !vq->guest || !desc) {
        return VMM_ERR_INVALID;
    }

    desc_pa = vq->vring.desc_pa + indx * sizeof(*desc);
    ret     = vmm_guest_memory_read(vq->guest, desc_pa, desc, sizeof(*desc), TRUE);

    if (ret != sizeof(*desc)) {
        return VMM_ERR_IO;
    }

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_get_desc);

/**
 * @brief 从VirtIO可用环中弹出一个描述符
 * @param vq VirtIO队列指针
 * @return 获取到的值，失败返回错误码
 */
uint16_t vmm_virtio_queue_pop(struct vmm_virtio_queue *vq)
{
    uint16_t        val;
    uint32_t        ret;
    physical_addr_t avail_pa;

    if (!vq || !vq->guest) {
        return 0;
    }

    ret      = umod32(vq->last_avail_idx++, vq->desc_count);

    avail_pa = vq->vring.avail_pa + offsetof(struct vmm_vring_avail, ring[ret]);
    ret      = vmm_guest_memory_read(vq->guest, avail_pa, &val, sizeof(val), TRUE);

    if (ret != sizeof(val)) {
        vmm_printf("%s: read failed at avail_pa=0x%" PRIPADDR "\n", __func__, avail_pa);
        return 0;
    }

    return val;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_pop);

/**
 * @brief 检查VirtIO队列是否可用
 * @param vq VirtIO队列指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_virtio_queue_available(struct vmm_virtio_queue *vq)
{
    uint16_t        val;
    uint32_t        ret;
    physical_addr_t avail_pa;

    if (!vq || !vq->guest) {
        return FALSE;
    }

    avail_pa = vq->vring.avail_pa + offsetof(struct vmm_vring_avail, idx);
    ret      = vmm_guest_memory_read(vq->guest, avail_pa, &val, sizeof(val), TRUE);

    if (ret != sizeof(val)) {
        vmm_printf("%s: read failed at avail_pa=0x%" PRIPADDR "\n", __func__, avail_pa);
        return FALSE;
    }

    return val != vq->last_avail_idx;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_available);

/**
 * @brief 判断VirtIO队列是否需要通知
 * @param vq VirtIO队列指针
 * @return 可用返回TRUE，不可用返回FALSE
 */
bool vmm_virtio_queue_should_signal(struct vmm_virtio_queue *vq)
{
    uint32_t        ret;
    uint16_t old_idx;
    uint16_t new_idx;
    uint16_t event_idx;
    physical_addr_t used_pa;
    physical_addr_t avail_pa;

    if (!vq || !vq->guest) {
        return FALSE;
    }

    old_idx = vq->last_used_signalled;

    used_pa = vq->vring.used_pa + offsetof(struct vmm_vring_used, idx);
    ret     = vmm_guest_memory_read(vq->guest, used_pa, &new_idx, sizeof(new_idx), TRUE);

    if (ret != sizeof(new_idx)) {
        vmm_printf("%s: read failed at used_pa=0x%" PRIPADDR "\n", __func__, used_pa);
        return FALSE;
    }

    avail_pa = vq->vring.avail_pa + offsetof(struct vmm_vring_avail, ring[vq->vring.num]);
    ret      = vmm_guest_memory_read(vq->guest, avail_pa, &event_idx, sizeof(event_idx), TRUE);

    if (ret != sizeof(event_idx)) {
        vmm_printf("%s: read failed at avail_pa=0x%" PRIPADDR "\n", __func__, avail_pa);
        return FALSE;
    }

    if (vmm_vring_need_event(event_idx, new_idx, old_idx)) {
        vq->last_used_signalled = new_idx;
        return TRUE;
    }

    return FALSE;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_should_signal);

/**
 * @brief 设置VirtIO队列的可用事件索引
 * @param vq VirtIO队列指针
 */
void vmm_virtio_queue_set_avail_event(struct vmm_virtio_queue *vq)
{
    uint16_t        val;
    uint32_t        ret;
    physical_addr_t avail_evt_pa;

    if (!vq || !vq->guest) {
        return;
    }

    val          = vq->last_avail_idx;
    avail_evt_pa = vq->vring.used_pa + offsetof(struct vmm_vring_used, ring[vq->vring.num]);
    ret          = vmm_guest_memory_write(vq->guest, avail_evt_pa, &val, sizeof(val), TRUE);

    if (ret != sizeof(val)) {
        vmm_printf("%s: write failed at avail_evt_pa=0x%" PRIPADDR "\n", __func__, avail_evt_pa);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_set_avail_event);

/**
 * @brief 设置VirtIO队列的已用元素
 * @param vq VirtIO队列指针
 * @param head 头部索引
 * @param len 大小
 */
void vmm_virtio_queue_set_used_elem(struct vmm_virtio_queue *vq, uint32_t head, uint32_t len)
{
    uint32_t                   ret;
    uint16_t                   used_idx;
    struct vmm_vring_used_elem used_elem;
    physical_addr_t used_idx_pa;
    physical_addr_t used_elem_pa;

    if (!vq || !vq->guest) {
        return;
    }

    used_idx_pa = vq->vring.used_pa + offsetof(struct vmm_vring_used, idx);
    ret         = vmm_guest_memory_read(vq->guest, used_idx_pa, &used_idx, sizeof(used_idx), TRUE);

    if (ret != sizeof(used_idx)) {
        vmm_printf("%s: read failed at used_idx_pa=0x%" PRIPADDR "\n", __func__, used_idx_pa);
    }

    used_elem.id  = head;
    used_elem.len = len;
    ret           = umod32(used_idx, vq->vring.num);
    used_elem_pa  = vq->vring.used_pa + offsetof(struct vmm_vring_used, ring[ret]);
    ret           = vmm_guest_memory_write(vq->guest, used_elem_pa, &used_elem, sizeof(used_elem), TRUE);

    if (ret != sizeof(used_elem)) {
        vmm_printf("%s: write failed at used_elem_pa=0x%" PRIPADDR "\n", __func__, used_elem_pa);
    }

    used_idx++;
    ret = vmm_guest_memory_write(vq->guest, used_idx_pa, &used_idx, sizeof(used_idx), TRUE);

    if (ret != sizeof(used_idx)) {
        vmm_printf("%s: write failed at used_idx_pa=0x%" PRIPADDR "\n", __func__, used_idx_pa);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_set_used_elem);

/**
 * @brief VirtIO队列设置完成检查
 * @param vq VirtIO队列指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_virtio_queue_setup_done(struct vmm_virtio_queue *vq)
{
    return (vq) ? ((vq->guest) ? TRUE : FALSE) : FALSE;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_setup_done);

/**
 * @brief 清理VirtIO队列中的已处理描述符
 * @param vq VirtIO队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_cleanup(struct vmm_virtio_queue *vq)
{
    if (!vq || !vq->guest) {
        goto done;
    }

    vq->last_avail_idx      = 0;
    vq->last_used_signalled = 0;

    vq->guest               = NULL;

    vq->desc_count          = 0;
    vq->align               = 0;
    vq->guest_pfn           = 0;
    vq->guest_page_size     = 0;

    vq->guest_addr          = 0;
    vq->host_addr           = 0;
    vq->total_size          = 0;

done:
    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_cleanup);

/**
 * @brief 设置VirtIO队列
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_setup(
    struct vmm_virtio_queue *vq, struct vmm_guest *guest, physical_addr_t guest_pfn, physical_size_t guest_page_size, uint32_t desc_count,
    uint32_t align)
{
    int             rc = VMM_OK; /**< VMM_OK成员 */
    uint32_t        reg_flags; /**< reg_flags成员 */
    physical_addr_t guest_physical_addr, hphys_addr; /**< 主机物理地址 */
    physical_size_t gphys_size, avail_size; /**< avail_size成员 */

    if (!vq || !guest) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    if ((rc = vmm_virtio_queue_cleanup(vq))) {
        vmm_printf("%s: cleanup failed\n", __func__); /**< __func__)成员 */
        return rc; /**< rc */
    }

    guest_physical_addr = guest_pfn * guest_page_size; /**< guest_page_size成员 */
    gphys_size          = vmm_vring_size(desc_count, align); /**< align)成员 */

    if ((rc = vmm_guest_physical_map(guest, guest_physical_addr, gphys_size, &hphys_addr, &avail_size, &reg_flags))) {
        vmm_printf("%s: vmm_guest_physical_map() failed\n", __func__); /**< __func__)成员 */
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    if (!(reg_flags & VMM_REGION_IS_RAM)) {
        vmm_printf("%s: region is not backed by RAM\n", __func__); /**< __func__)成员 */
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (avail_size < gphys_size) {
        vmm_printf("%s: available size less than required size\n", __func__); /**< __func__)成员 */
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_vring_init(&vq->vring, desc_count, NULL, guest_physical_addr, align); /**< align)成员 */

    vq->guest           = guest; /**< 客户机 */
    vq->desc_count      = desc_count; /**< desc_count成员 */
    vq->align           = align; /**< align成员 */
    vq->guest_pfn       = guest_pfn; /**< guest_pfn成员 */
    vq->guest_page_size = guest_page_size; /**< guest_page_size成员 */

    vq->guest_addr      = guest_physical_addr; /**< 客户机物理地址 */
    vq->host_addr       = hphys_addr; /**< 主机物理地址 */
    vq->total_size      = gphys_size; /**< gphys_size成员 */

    return VMM_OK; /**< VMM_OK成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_setup);

/*
 * Each buffer in the virtqueues is actually a chain of descriptors.  This
 * function returns the next descriptor in the chain, max descriptor count
 * if we're at the end.
 */
/**
 * @brief 获取描述符链中的下一个描述符索引
 * @param vq VirtIO队列指针
 * @param desc 虚拟环形队列描述符数组
 * @param i 当前描述符索引
 * @param max 最大描述符数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
static unsigned next_desc(struct vmm_virtio_queue *vq, struct vmm_vring_desc *desc, uint32_t i, uint32_t max)
{
    int      rc;
    uint32_t next;

    if (!(desc->flags & VMM_VRING_DESC_F_NEXT)) {
        return max;
    }

    next = desc->next;

    rc   = vmm_virtio_queue_get_desc(vq, next, desc);

    if (rc) {
        vmm_printf("%s: failed to get descriptor next=%d error=%d\n", __func__, next, rc);
        return max;
    }

    return next;
}

/**
 * @brief 获取VirtIO队列的头部IO向量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_get_head_iovec(
    struct vmm_virtio_queue *vq, uint16_t head, struct vmm_virtio_iovec *iov, uint32_t *ret_iov_cnt, uint32_t *ret_total_len, uint16_t *ret_head)
{
    int                   i, rc = VMM_OK; /**< VMM_OK成员 */
    uint16_t              idx, max; /**< 最大值 */
    struct vmm_vring_desc desc; /**< 描述 */

    if (!vq || !vq->guest || !iov) {
        goto fail; /**< fail成员 */
    }

    idx = head; /**< 链表头 */

    if (ret_iov_cnt) {
        *ret_iov_cnt = 0;
    }

    if (ret_total_len) {
        *ret_total_len = 0;
    }

    if (ret_head) {
        *ret_head = 0;
    }

    max = vmm_virtio_queue_max_desc(vq); /**< vmm_virtio_queue_max_desc(vq)成员 */

    rc  = vmm_virtio_queue_get_desc(vq, idx, &desc); /**< &desc)成员 */

    if (rc) {
        vmm_printf("%s: failed to get descriptor idx=%d error=%d\n", __func__, idx, rc); /**< rc)成员 */
        goto fail; /**< fail成员 */
    }

    if (desc.flags & VMM_VRING_DESC_F_INDIRECT) {
#if 0
        max = desc[idx].len / sizeof(struct vring_desc); /**< 描述 */
        desc = guest_flat_to_host(kvm, desc[idx].addr); /**< 描述 */
        idx = 0; /**< 0 */
#endif
        vmm_printf("%s: indirect descriptor not supported idx=%d\n", __func__, idx); /**< idx)成员 */
        rc = VMM_ERR_NOTSUPP; /**< VMM_ERR_NOTSUPP成员 */
        goto fail; /**< fail成员 */
    }

    i = 0; /**< 0 */

    do {
        iov[i].addr = desc.addr; /**< iov成员 */
        iov[i].len  = desc.len; /**< iov成员 */

        if (ret_total_len) {
            *ret_total_len += desc.len;
        }

        if (desc.flags & VMM_VRING_DESC_F_WRITE) {
            iov[i].flags = 1; /* Write */
        } else {
            iov[i].flags = 0; /* Read */
        }

        i++;
    } while ((idx = next_desc(vq, &desc, idx, max)) != max); /**< max)成员 */

    if (ret_iov_cnt) {
        *ret_iov_cnt = i;
    }

    vmm_virtio_queue_set_avail_event(vq);

    if (ret_head) {
        *ret_head = head;
    }

    return VMM_OK; /**< VMM_OK成员 */

fail:

    if (ret_iov_cnt) {
        *ret_iov_cnt = 0;
    }

    if (ret_total_len) {
        *ret_total_len = 0;
    }

    return rc; /**< rc */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_get_head_iovec);

/**
 * @brief 获取VirtIO队列的iovec
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_get_iovec(
    struct vmm_virtio_queue *vq, struct vmm_virtio_iovec *iov, uint32_t *ret_iov_cnt, uint32_t *ret_total_len, uint16_t *ret_head)
{
    uint16_t head = vmm_virtio_queue_pop(vq); /**< vmm_virtio_queue_pop(vq)成员 */

    return vmm_virtio_queue_get_head_iovec(vq, head, iov, ret_iov_cnt, ret_total_len, ret_head); /**< ret_head)成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_queue_get_iovec);

/**
 * @brief 从VirtIO的iovec向量读取数据到缓冲区
 * @param dev 设备结构体指针
 * @param iov IO向量数组
 * @param iov_cnt IO向量数量
 * @param buf 数据缓冲区指针
 * @param buf_len 大小
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_virtio_iovec_to_buf_read(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt, void *buf, uint32_t buf_len)
{
    uint32_t i = 0;
    uint32_t pos = 0;
    uint32_t len = 0;

    for (i = 0; i < iov_cnt && pos < buf_len; i++) {
        len = ((buf_len - pos) < iov[i].len) ? (buf_len - pos) : iov[i].len;

        len = vmm_guest_memory_read(dev->guest, iov[i].addr, buf + pos, len, TRUE);

        if (!len) {
            break;
        }

        pos += len;
    }

    return pos;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_iovec_to_buf_read);

/**
 * @brief 将缓冲区数据写入VirtIO的iovec向量
 * @param dev 设备结构体指针
 * @param iov IO向量数组
 * @param iov_cnt IO向量数量
 * @param buf 数据缓冲区指针
 * @param buf_len 大小
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_virtio_buf_to_iovec_write(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt, void *buf, uint32_t buf_len)
{
    uint32_t i = 0;
    uint32_t pos = 0;
    uint32_t len = 0;

    for (i = 0; i < iov_cnt && pos < buf_len; i++) {
        len = ((buf_len - pos) < iov[i].len) ? (buf_len - pos) : iov[i].len;

        len = vmm_guest_memory_write(dev->guest, iov[i].addr, buf + pos, len, TRUE);

        if (!len) {
            break;
        }

        pos += len;
    }

    return pos;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_buf_to_iovec_write);

/**
 * @brief VirtIO IO向量填充零
 * @param dev 设备结构体指针
 * @param iov IO向量数组
 * @param iov_cnt IO向量数量
 */
void vmm_virtio_iovec_fill_zeros(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt)
{
    uint32_t i = 0;
    uint32_t pos = 0;
    uint32_t len = 0;
    uint8_t  zeros[16];

    memset(zeros, 0, sizeof(zeros));

    while (i < iov_cnt) {
        len = (iov[i].len < 16) ? iov[i].len : 16;
        len = vmm_guest_memory_write(dev->guest, iov[i].addr + pos, zeros, len, TRUE);

        if (!len) {
            break;
        }

        pos += len;

        if (pos == iov[i].len) {
            pos = 0;
            i++;
        }
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_iovec_fill_zeros);

/* ========== VirtIO device and emulator implementations ========== */

/**
 * @brief   复位VirtIO设备模拟器
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __virtio_reset_emulator(struct vmm_virtio_device *dev)
{
    if (dev && dev->emu && dev->emu->reset) {
        return dev->emu->reset(dev);
    }

    return VMM_OK;
}

/**
 * @brief   连接VirtIO设备模拟器
 * @param dev 设备结构体指针
 * @param emu 模拟设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __virtio_connect_emulator(struct vmm_virtio_device *dev, struct vmm_virtio_emulator *emu)
{
    if (dev && emu && emu->connect) {
        return emu->connect(dev, emu);
    }

    return VMM_OK;
}

/**
 * @brief   断开VirtIO设备模拟器
 * @param dev 设备结构体指针
 */
static void __virtio_disconnect_emulator(struct vmm_virtio_device *dev)
{
    if (dev && dev->emu && dev->emu->disconnect) {
        dev->emu->disconnect(dev);
    }
}

/**
 * @brief   从VirtIO模拟器读取配置
 * @param dev 设备结构体指针
 * @param offset 偏移量（字节）
 * @param dst 目标缓冲区指针
 * @param dst_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __virtio_config_read_emulator(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len)
{
    if (dev && dev->emu && dev->emu->read_config) {
        return dev->emu->read_config(dev, offset, dst, dst_len);
    }

    return VMM_OK;
}

/**
 * @brief   向VirtIO模拟器写入配置
 * @param dev 设备结构体指针
 * @param offset 偏移量（字节）
 * @param src 源设备树节点
 * @param src_len 大小
 * @return 成功读取的字节数，失败返回错误码
 */
static int __virtio_config_write_emulator(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len)
{
    if (dev && dev->emu && dev->emu->write_config) {
        return dev->emu->write_config(dev, offset, src, src_len);
    }

    return VMM_OK;
}

/**
 * @brief VirtIO设备匹配函数
 * @param ids ID数组指针
 * @param dev 设备结构体指针
 * @return 成功写入的字节数，失败返回错误码
 */
static bool __virtio_match_device(const struct vmm_virtio_device_id *ids, struct vmm_virtio_device *dev)
{
    while (ids->type) {
        if (ids->type == dev->id.type) {
            return TRUE;
        }

        ids++;
    }

    return FALSE;
}

/**
 * @brief   绑定VirtIO设备模拟器
 * @param dev 设备结构体指针
 * @param emu 模拟设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __virtio_bind_emulator(struct vmm_virtio_device *dev, struct vmm_virtio_emulator *emu)
{
    int rc = VMM_ERR_INVALID;

    if (__virtio_match_device(emu->id_table, dev)) {
        dev->emu = emu;

        if ((rc = __virtio_connect_emulator(dev, emu))) {
            dev->emu = NULL;
        }
    }

    return rc;
}

/**
 * @brief   查找VirtIO设备模拟器
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __virtio_find_emulator(struct vmm_virtio_device *dev)
{
    struct vmm_virtio_emulator *emu;

    if (!dev || dev->emu) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    list_for_each_entry(emu, &virtio_emu_list, node)
    {
        if (!__virtio_bind_emulator(dev, emu)) {
            return VMM_OK;
        }
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief   附加VirtIO设备模拟器
 * @param emu 模拟设备指针
 */
static void __virtio_attach_emulator(struct vmm_virtio_emulator *emu)
{
    struct vmm_virtio_device *dev;

    if (!emu) {
        return;
    }

    list_for_each_entry(dev, &virtio_dev_list, node)
    {
        if (!dev->emu) {
            __virtio_bind_emulator(dev, emu);
        }
    }
}

/**
 * @brief 读取VirtIO设备的配置空间数据
 * @param dev 设备结构体指针
 * @param offset 偏移量（字节）
 * @param dst 目标缓冲区指针
 * @param dst_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_config_read(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len)
{
    return __virtio_config_read_emulator(dev, offset, dst, dst_len);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_config_read);

/**
 * @brief 写入VirtIO设备的配置空间数据
 * @param dev 设备结构体指针
 * @param offset 偏移量（字节）
 * @param src 源设备树节点
 * @param src_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_config_write(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len)
{
    return __virtio_config_write_emulator(dev, offset, src, src_len);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_config_write);

/**
 * @brief 复位virtio
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_reset(struct vmm_virtio_device *dev)
{
    return __virtio_reset_emulator(dev);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_reset);

/**
 * @brief 注册VirtIO设备
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_register_device(struct vmm_virtio_device *dev)
{
    int rc = VMM_OK;

    if (!dev || !dev->tra) {
        return VMM_ERR_FAIL;
    }

    INIT_LIST_HEAD(&dev->node);
    dev->emu      = NULL;
    dev->emu_data = NULL;

    vmm_mutex_lock(&virtio_mutex);

    list_add_tail(&dev->node, &virtio_dev_list);
    rc = __virtio_find_emulator(dev);

    vmm_mutex_unlock(&virtio_mutex);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_register_device);

/**
 * @brief 注销VirtIO设备
 * @param dev 设备结构体指针
 */
void vmm_virtio_unregister_device(struct vmm_virtio_device *dev)
{
    if (!dev) {
        return;
    }

    vmm_mutex_lock(&virtio_mutex);

    __virtio_disconnect_emulator(dev);
    list_del(&dev->node);

    vmm_mutex_unlock(&virtio_mutex);
}

VMM_ERR_XPORT_SYMBOL(virtio_unregister_device);

/**
 * @brief 注册VirtIO模拟器
 * @param emu 模拟设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_register_emulator(struct vmm_virtio_emulator *emu)
{
    bool                        found;
    struct vmm_virtio_emulator *vemu;

    if (!emu) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    vemu  = NULL;
    found = FALSE;

    vmm_mutex_lock(&virtio_mutex);

    list_for_each_entry(vemu, &virtio_emu_list, node)
    {
        if (strcmp(vemu->name, emu->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&virtio_mutex);
        return VMM_ERR_FAIL;
    }

    INIT_LIST_HEAD(&emu->node);
    list_add_tail(&emu->node, &virtio_emu_list);

    __virtio_attach_emulator(emu);

    vmm_mutex_unlock(&virtio_mutex);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_register_emulator);

/**
 * @brief 注销VirtIO模拟器
 * @param emu 模拟设备指针
 */
void vmm_virtio_unregister_emulator(struct vmm_virtio_emulator *emu)
{
    struct vmm_virtio_device *dev;

    vmm_mutex_lock(&virtio_mutex);

    list_del(&emu->node);

    list_for_each_entry(dev, &virtio_dev_list, node)
    {
        if (dev->emu == emu) {
            __virtio_disconnect_emulator(dev);
            __virtio_find_emulator(dev);
        }
    }

    vmm_mutex_unlock(&virtio_mutex);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtio_unregister_emulator);

/**
 * @brief 初始化VirtIO核心
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init vmm_virtio_core_init(void)
{
    /* Nothing to be done */
    return VMM_OK;
}

/**
 * @brief VirtIO核心子系统退出清理
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_virtio_core_exit(void)
{
    /* Nothing to be done */
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
