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
 * @brief VirtIO核心框架接口
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

/**
 * @brief VirtIO IO向量，描述一段客户内存缓冲区的地址和长度
 */
struct vmm_virtio_iovec {
    /* Address (guest-physical). */
    uint64_t addr; /**< 地址 */
    /* Length. */
    uint32_t len; /**< 长度 */
    /* The flags as indicated above. */
    uint16_t flags; /**< 标志位 */
};

/**
 * @brief VirtIO虚拟队列，封装vring和通知机制的运行时状态
 */
struct vmm_virtio_queue {
    /* The last_avail_idx field is an index to ->ring of struct vring_avail.
       It's where we assume the next request index is at.  */
    uint16_t last_avail_idx; /**< last_avail_idx成员 */
    uint16_t last_used_signalled; /**< last_used_signalled成员 */

    struct vmm_vring vring; /**< 虚拟环 */

    struct vmm_guest *guest; /**< 客户机 */
    uint32_t          desc_count; /**< desc_count成员 */
    uint32_t          align; /**< 对齐 */
    physical_addr_t   guest_pfn; /**< guest_pfn成员 */
    physical_size_t   guest_page_size; /**< guest_page_size成员 */
    physical_addr_t   guest_addr; /**< guest_addr成员 */
    physical_addr_t   host_addr; /**< host_addr成员 */
    physical_size_t   total_size; /**< total_size成员 */
};

/**
 * @brief VirtIO设备ID枚举，定义各类VirtIO设备的标识编号
 */
struct vmm_virtio_device_id {
    uint32_t type; /**< 类型 */
};

/**
 * @brief VirtIO设备结构，维护队列数组、传输层和状态信息
 */
struct vmm_virtio_device {
    char                  name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN]; /**< 名称 */
    vmm_emulate_device_t *edev; /**< 仿真设备 */

    struct vmm_virtio_device_id id; /**< 标识符 */

    struct vmm_virtio_transport *tra; /**< 传输 */
    void                        *tra_data; /**< tra_data成员 */

    struct vmm_virtio_emulator *emu; /**< 仿真 */
    void                       *emu_data; /**< emu_data成员 */

    double_list_t     node; /**< 节点 */
    struct vmm_guest *guest; /**< 客户机 */
};

/**
 * @brief VirtIO传输层接口，定义设备配置空间的读写回调
 */
struct vmm_virtio_transport {
    const char *name; /**< 名称 */

    int (*notify)(struct vmm_virtio_device *, uint32_t vq); /**< 通知回调 */
};

/**
 * @brief VirtIO模拟器接口，定义队列通知和设备复位回调
 */
struct vmm_virtio_emulator {
    const char                        *name; /**< 名称 */
    const struct vmm_virtio_device_id *id_table; /**< ID表 */

    /* VirtIO operations */
    uint64_t (*get_host_features)(struct vmm_virtio_device *dev); /**< get_host_features成员 */
    void (*set_guest_features)(struct vmm_virtio_device *dev, uint32_t select, uint32_t features); /**< set_guest_features成员 */
    int (*init_vq)(struct vmm_virtio_device *dev, uint32_t vq, uint32_t page_size, uint32_t align, uint32_t pfn); /**< init_vq成员 */
    int (*get_pfn_vq)(struct vmm_virtio_device *dev, uint32_t vq); /**< get_pfn_vq成员 */
    int (*get_size_vq)(struct vmm_virtio_device *dev, uint32_t vq); /**< get_size_vq成员 */
    int (*set_size_vq)(struct vmm_virtio_device *dev, uint32_t vq, int size); /**< set_size_vq成员 */
    int (*notify_vq)(struct vmm_virtio_device *dev, uint32_t vq); /**< notify_vq成员 */
    void (*status_changed)(struct vmm_virtio_device *dev, uint32_t new_status); /**< status_changed成员 */

    /* Emulator operations */
    int (*read_config)(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len); /**< read_config成员 */
    int (*write_config)(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len); /**< write_config成员 */
    int (*reset)(struct vmm_virtio_device *dev); /**< 复位 */
    int (*connect)(struct vmm_virtio_device *dev, struct vmm_virtio_emulator *emu); /**< connect成员 */
    void (*disconnect)(struct vmm_virtio_device *dev); /**< disconnect成员 */

    double_list_t node; /**< 节点 */
};

/** Get guest to which the queue belongs
 *  Note: only available after queue setup is done
 */
struct vmm_guest *vmm_virtio_queue_guest(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列描述符的数量
 * @param vq VirtIO队列指针
 * @return 数量值
 */
uint32_t vmm_virtio_queue_desc_count(struct vmm_virtio_queue *vq);

/**
 * @brief VirtIO队列对齐设置
 * @param vq VirtIO队列指针
 * @return 数量值
 */
uint32_t vmm_virtio_queue_align(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的客户机页帧号
 * @param vq VirtIO队列指针
 * @return 获取到的值，失败返回错误码
 */
physical_addr_t vmm_virtio_queue_guest_pfn(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的客户机页大小
 * @param vq VirtIO队列指针
 * @return 大小值（字节）
 */
physical_size_t vmm_virtio_queue_guest_page_size(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的客户机物理地址
 * @param vq VirtIO队列指针
 * @return 大小值（字节）
 */
physical_addr_t vmm_virtio_queue_guest_addr(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的主机虚拟地址
 * @param vq VirtIO队列指针
 * @return 获取到的值，失败返回错误码
 */
physical_addr_t vmm_virtio_queue_host_addr(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的总大小
 * @param vq VirtIO队列指针
 * @return 大小值（字节）
 */
physical_size_t virtio_queue_total_size(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的最大描述符的数量
 * @param vq VirtIO队列指针
 * @return 数量值
 */
uint32_t vmm_virtio_queue_max_desc(struct vmm_virtio_queue *vq);

/**
 * @brief 获取VirtIO队列的desc
 * @param vq VirtIO队列指针
 * @param indx 索引值
* @param desc MSI描述符结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_get_desc(struct vmm_virtio_queue *vq, uint16_t indx, struct vmm_vring_desc *desc);

/**
 * @brief 从VirtIO可用环中弹出一个描述符
 * @param vq VirtIO队列指针
 * @return 获取到的值，失败返回错误码
 */
uint16_t vmm_virtio_queue_pop(struct vmm_virtio_queue *vq);

/**
 * @brief 检查VirtIO队列是否可用
 * @param vq VirtIO队列指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_virtio_queue_available(struct vmm_virtio_queue *vq);

/**
 * @brief 判断VirtIO队列是否需要通知
 * @param vq VirtIO队列指针
 * @return 可用返回TRUE，不可用返回FALSE
 */
bool vmm_virtio_queue_should_signal(struct vmm_virtio_queue *vq);

/**
 * @brief 设置VirtIO队列的可用事件索引
 * @param vq VirtIO队列指针
 */
void vmm_virtio_queue_set_avail_event(struct vmm_virtio_queue *vq);

/**
 * @brief 设置VirtIO队列的已用元素
 * @param vq VirtIO队列指针
 * @param head 头部索引
 * @param len 大小
 */
void vmm_virtio_queue_set_used_elem(struct vmm_virtio_queue *vq, uint32_t head, uint32_t len);

/**
 * @brief VirtIO队列设置完成检查
 * @param vq VirtIO队列指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_virtio_queue_setup_done(struct vmm_virtio_queue *vq);

/**
 * @brief 清理VirtIO队列中的已处理描述符
 * @param vq VirtIO队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_queue_cleanup(struct vmm_virtio_queue *vq);

/**
 * @brief 设置或初始化队列，若已设置则先清理
 */
int vmm_virtio_queue_setup(
    struct vmm_virtio_queue *vq, struct vmm_guest *guest, physical_addr_t guest_pfn, physical_size_t guest_page_size, uint32_t desc_count,
    uint32_t align);

/**
 * @brief 根据给定头部获取客户IO向量，仅在队列设置后可用
 */
int vmm_virtio_queue_get_head_iovec(
    struct vmm_virtio_queue *vq, uint16_t head, struct vmm_virtio_iovec *iov, uint32_t *ret_iov_cnt, uint32_t *ret_total_len, uint16_t *ret_head);

/**
 * @brief 根据当前头部获取客户IO向量，仅在队列设置后可用
 */
int vmm_virtio_queue_get_iovec(
    struct vmm_virtio_queue *vq, struct vmm_virtio_iovec *iov, uint32_t *ret_iov_cnt, uint32_t *ret_total_len, uint16_t *ret_head);

/**
 * @brief 从VirtIO的iovec向量读取数据到缓冲区
 * @param dev 设备结构体指针
 * @param iov IO向量数组
 * @param iov_cnt IO向量数量
 * @param buf 数据缓冲区指针
 * @param buf_len 大小
 * @return 从IO向量读取并复制到缓冲区的字节数
 */
uint32_t vmm_virtio_iovec_to_buf_read(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt, void *buf, uint32_t buf_len);

/**
 * @brief 将缓冲区数据写入VirtIO的iovec向量
 * @param dev 设备结构体指针
 * @param iov IO向量数组
 * @param iov_cnt IO向量数量
 * @param buf 数据缓冲区指针
 * @param buf_len 大小
 * @return 从缓冲区写入IO向量的字节数
 */
uint32_t vmm_virtio_buf_to_iovec_write(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt, void *buf, uint32_t buf_len);

/**
 * @brief VirtIO IO向量填充零
 * @param dev 设备结构体指针
 * @param iov IO向量数组
 * @param iov_cnt IO向量数量
 */
void vmm_virtio_iovec_fill_zeros(struct vmm_virtio_device *dev, struct vmm_virtio_iovec *iov, uint32_t iov_cnt);

/**
 * @brief 读取VirtIO设备的配置空间数据
 * @param dev 设备结构体指针
 * @param offset 偏移量（字节）
 * @param dst 目标缓冲区指针
 * @param dst_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_config_read(struct vmm_virtio_device *dev, uint32_t offset, void *dst, uint32_t dst_len);

/**
 * @brief 写入VirtIO设备的配置空间数据
 * @param dev 设备结构体指针
 * @param offset 偏移量（字节）
 * @param src 源设备树节点
 * @param src_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_config_write(struct vmm_virtio_device *dev, uint32_t offset, void *src, uint32_t src_len);

/**
 * @brief 复位virtio
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_reset(struct vmm_virtio_device *dev);

/**
 * @brief 注册VirtIO设备
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_register_device(struct vmm_virtio_device *dev);

/**
 * @brief 注销VirtIO设备
 * @param dev 设备结构体指针
 */
void vmm_virtio_unregister_device(struct vmm_virtio_device *dev);

/**
 * @brief 注册VirtIO模拟器
 * @param emu 模拟设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtio_register_emulator(struct vmm_virtio_emulator *emu);

/**
 * @brief 注销VirtIO模拟器
 * @param emu 模拟设备指针
 */
void vmm_virtio_unregister_emulator(struct vmm_virtio_emulator *emu);

#endif /* __VMM_VIRTIO_H__ */
