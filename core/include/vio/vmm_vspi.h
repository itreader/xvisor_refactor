/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_vspi.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟SPI框架头文件
 */
#ifndef _VMM_VSPI_H__
#define _VMM_VSPI_H__

#include <libs/list.h>
#include <vmm_completion.h>
#include <vmm_mutex.h>
#include <vmm_types.h>

#define VMM_VSPI_IPRIORITY 0

struct vmm_thread;
struct vmm_emulate_device;
typedef struct vmm_emulate_device vmm_emulate_device_t;
struct vmm_virtual_spi_host;
struct vmm_virtual_spi_slave;

typedef struct vmm_thread            vmm_thread_t;
typedef struct vmm_virtual_spi_host  vmm_virtual_spi_host_t;
typedef struct vmm_virtual_spi_slave vmm_virtual_spi_slave_t;

/** Representation of a virtual spi slave */
/**
 * @brief 虚拟SPI从设备，定义片选和数据传输回调
 */
struct vmm_virtual_spi_slave {
    vmm_emulate_device_t   *edev; /**< 仿真设备 */
    vmm_virtual_spi_host_t *vsh; /**< 虚拟串口头 */
    char                    name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t                chip_select; /**< chip_select成员 */
    uint32_t (*xfer)(struct vmm_virtual_spi_slave *vss, uint32_t data, void *private); /**< 传输 */
    void *private; /**< 私有数据 */
};

/** Representation of a virtual spi host */
/**
 * @brief 虚拟SPI主控制器，管理从设备列表和总线操作
 */
struct vmm_virtual_spi_host {
    double_list_t         head; /**< 链表头 */
    vmm_emulate_device_t *edev; /**< 仿真设备 */
    char                  name[VMM_FIELD_NAME_SIZE]; /**< 名称 */

    void (*xfer)(struct vmm_virtual_spi_host *vsh, void *private); /**< 传输 */
    vmm_completion_t xfer_avail; /**< xfer_avail成员 */
    vmm_thread_t    *xfer_worker; /**< xfer_worker成员 */

    uint32_t chip_select_count; /**< chip_select_count成员 */

    vmm_mutex_t               slaves_lock; /**< slaves_lock成员 */
    vmm_virtual_spi_slave_t **slaves; /**< slaves成员 */

    void *private; /**< 私有数据 */
};

/**
 * @brief 获取SPI从设备的主机
 * @param vss 虚拟屏幕表面指针
 * @return 目标对象指针，不存在返回NULL
 */
vmm_virtual_spi_host_t *vmm_vspislave_get_host(vmm_virtual_spi_slave_t *vss);

/**
 * @brief 获取SPI从设备的名称
 * @param vss 虚拟屏幕表面指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_vspislave_get_name(vmm_virtual_spi_slave_t *vss);

/**
 * @brief 获取SPI从设备的片选信号
 * @param vss 虚拟屏幕表面指针
 * @return SPI片选编号，失败返回U32_MAX
 */
uint32_t vmm_vspislave_get_chip_select(vmm_virtual_spi_slave_t *vss);

/** Create a virtual spi slave */
vmm_virtual_spi_slave_t *vmm_vspislave_create(
    vmm_emulate_device_t *edev, uint32_t chip_select, uint32_t (*xfer)(vmm_virtual_spi_slave_t *, uint32_t, void *), void *private);

/**
 * @brief 销毁vspislave
 * @param vss 虚拟屏幕表面指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vspislave_destroy(vmm_virtual_spi_slave_t *vss);

/**
 * @brief 执行虚拟SPI主机的数据传输
 * @param vsh 虚拟串口句柄
 * @param chip_select 片选信号值
 * @param data 用户自定义数据指针
 * @return 成功返回传输的字节数，失败返回错误码
 */
uint32_t vmm_vspihost_xfer_data(vmm_virtual_spi_host_t *vsh, uint32_t chip_select, uint32_t data);

/**
 * @brief 调度虚拟SPI主机的传输请求
 * @param vsh 虚拟串口句柄
 */
void vmm_vspihost_schedule_xfer(vmm_virtual_spi_host_t *vsh);

/**
 * @brief 获取SPI主控制器的名称
 * @param vsh 虚拟串口句柄
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_vspihost_get_name(vmm_virtual_spi_host_t *vsh);

/**
 * @brief 获取SPI主控制器片选获取的数量
 * @param vsh 虚拟串口句柄
 * @return 数量值
 */
uint32_t vmm_vspihost_get_chip_select_count(vmm_virtual_spi_host_t *vsh);

/**
 * @brief 遍历虚拟SPI主机从设备
 * @param vsh 虚拟串口句柄
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vspihost_iterate_slaves(vmm_virtual_spi_host_t *vsh, void *data, int (*fn)(vmm_virtual_spi_host_t *, vmm_virtual_spi_slave_t *, void *));

/** Create a virtual spi host */
vmm_virtual_spi_host_t *vmm_vspihost_create(
    const char *name_prefix, vmm_emulate_device_t *edev, void (*xfer)(vmm_virtual_spi_host_t *, void *), uint32_t chip_select_count, void *private);

/**
 * @brief 销毁vspihost
 * @param vsh 虚拟串口句柄
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vspihost_destroy(vmm_virtual_spi_host_t *vsh);

/**
 * @brief 查找vspihost
 * @param edev 模拟设备实例指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_virtual_spi_host_t *vmm_vspihost_find(vmm_emulate_device_t *edev);

/**
 * @brief 遍历虚拟SPI主机实例
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vspihost_iterate(vmm_virtual_spi_host_t *start, void *data, int (*fn)(vmm_virtual_spi_host_t *, void *));

/**
 * @brief 获取SPI主控制器的数量
 * @return 数量值
 */
uint32_t vmm_vspihost_count(void);

#endif
