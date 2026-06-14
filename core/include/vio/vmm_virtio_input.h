/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file vmm_virtio_input.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO输入设备接口
 *
 * This header has been derived from linux kernel source:
 * <linux_source>/include/uapi/linux/virtio_input.h
 *
 * The original header is BSD licensed.
 */

/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __VMM_VIRTIO_INPUT_H__
#define __VMM_VIRTIO_INPUT_H__

#include <vmm_types.h>

/**
 * @brief VirtIO输入配置选择枚举，指定查询的配置子结构类型
 */
enum vmm_virtio_input_config_select {
    VMM_VIRTIO_INPUT_CFG_UNSET     = 0x00, /**< 0x00成员 */
    VMM_VIRTIO_INPUT_CFG_ID_NAME   = 0x01, /**< 0x01成员 */
    VMM_VIRTIO_INPUT_CFG_ID_SERIAL = 0x02, /**< 0x02成员 */
    VMM_VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03, /**< 0x03成员 */
    VMM_VIRTIO_INPUT_CFG_PROP_BITS = 0x10, /**< 0x10成员 */
    VMM_VIRTIO_INPUT_CFG_EV_BITS   = 0x11, /**< 0x11成员 */
    VMM_VIRTIO_INPUT_CFG_ABS_INFO  = 0x12, /**< 0x12成员 */
};

/**
 * @brief VirtIO输入绝对轴信息，描述轴的最小/最大值和分辨率
 */
struct vmm_virtio_input_absinfo {
    uint32_t min; /**< 最小值 */
    uint32_t max; /**< 最大值 */
    uint32_t fuzz; /**< 模糊值 */
    uint32_t flat; /**< 平坦值 */
    uint32_t res; /**< 保留/结果 */
};

/**
 * @brief VirtIO输入设备ID，包含总线类型和设备产品标识
 */
struct vmm_virtio_input_deviceids {
    uint16_t bustype; /**< bustype成员 */
    uint16_t vendor; /**< 厂商ID */
    uint16_t product; /**< 产品ID */
    uint16_t version; /**< 版本号 */
};

/**
 * @brief VirtIO输入配置结构，保存事件类型和设备属性信息
 */
struct vmm_virtio_input_config {
    uint8_t select; /**< select成员 */
    uint8_t subsel; /**< subsel成员 */
    uint8_t size; /**< 大小 */
    uint8_t reserved[5]; /**< 保留 */

    union {
        char                              string[128]; /**< string成员 */
        uint8_t                           bitmap[128]; /**< bitmap成员 */
        struct vmm_virtio_input_absinfo   abs; /**< 绝对值 */
        struct vmm_virtio_input_deviceids ids; /**< 标识 */
    } u; /**< u */
} __attribute__((packed));

/**
 * @brief VirtIO输入事件结构，封装输入事件类型和值
 */
struct vmm_virtio_input_event {
    uint16_t type; /**< 类型 */
    uint16_t code; /**< 代码 */
    uint32_t value; /**< 值 */
} __attribute__((packed));

#endif /* __VMM_VIRTIO_INPUT_H__ */
