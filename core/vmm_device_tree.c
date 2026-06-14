/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_device_tree.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备树实现
 */

#include <arch_device_tree.h>
#include <arch_sections.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_stdio.h>

/**
 * @brief 设备树控制结构，维护设备树节点和属性
 */
struct vmm_device_tree_ctrl {
    vmm_device_tree_node_t                *root; /**< 根节点 */
    uint32_t                               nidtable_count; /**< nidtable_count成员 */
    struct vmm_device_tree_nidtable_entry *nidtbl; /**< nidtbl成员 */
};

static struct vmm_device_tree_ctrl dtree_ctrl;

/**
 * @brief 判断设备树属性类型是否为字面量类型
 * @param attrtype 属性类型标识
 * @return 字面量返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_isliteral(uint32_t attrtype)
{
    bool ret = FALSE;

    switch (attrtype) {
        case VMM_DEVICE_TREE_ATTRTYPE_UINT32:
        case VMM_DEVICE_TREE_ATTRTYPE_UINT64:
        case VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR:
        case VMM_DEVICE_TREE_ATTRTYPE_VIRTSIZE:
        case VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR:
        case VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE:
            ret = TRUE;
            break;
    };

    return ret;
}

/**
 * @brief 获取设备树字面量属性类型的数据大小
 * @param attrtype 属性类型标识
 * @return 大小值（字节）
 */
uint32_t vmm_device_tree_literal_size(uint32_t attrtype)
{
    uint32_t ret = 0;

    switch (attrtype) {
        case VMM_DEVICE_TREE_ATTRTYPE_UINT32:
            ret = sizeof(uint32_t);
            break;

        case VMM_DEVICE_TREE_ATTRTYPE_UINT64:
            ret = sizeof(uint64_t);
            break;

        case VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR:
            ret = sizeof(virtual_addr_t);
            break;

        case VMM_DEVICE_TREE_ATTRTYPE_VIRTSIZE:
            ret = sizeof(virtual_size_t);
            break;

        case VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR:
            ret = sizeof(physical_addr_t);
            break;

        case VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE:
            ret = sizeof(physical_size_t);
            break;
    };

    return ret;
}

/**
 * @brief 估算设备树属性类型
 * @param name 目标对象的名称
 * @return 大小值（字节）
 */
uint32_t vmm_device_tree_estimate_attrtype(const char *name)
{
    uint32_t ret = VMM_DEVICE_TREE_ATTRTYPE_BYTEARRAY;

    if (!name) {
        return ret;
    }

    if (!strcmp(name, VMM_DEVICE_TREE_MODEL_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CLOCK_FREQ_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CLOCKS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CLOCK_NAMES_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CLOCK_OUT_NAMES_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_REG_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_RANGES_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_ADDR_CELLS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_SIZE_CELLS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_PHANDLE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_MEMORY_PHYS_ADDR_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_MEMORY_PHYS_SIZE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE;
    } else if (!strcmp(name, VMM_DEVICE_TREE_ENABLE_METHOD_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CPU_RELEASE_ADDR_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CPU_CLEAR_ADDR_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_ENDIANNESS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_START_PC_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_PRIORITY_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_TIME_SLICE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT64;
    } else if (!strcmp(name, VMM_DEVICE_TREE_MANIFEST_TYPE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_HOST_PHYS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_ALIAS_PHYS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR;
    } else if (!strcmp(name, VMM_DEVICE_TREE_PHYS_SIZE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE;
    } else if (!strcmp(name, VMM_DEVICE_TREE_ALIGN_ORDER_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_SWITCH_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_CONSOLE_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_RTCDEV_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_BOOTARGS_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_BLKDEV_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_STRING;
    } else if (!strcmp(name, VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    } else if (!strcmp(name, VMM_DEVICE_TREE_VCPU_POWEROFF_ATTR_NAME)) {
        ret = VMM_DEVICE_TREE_ATTRTYPE_UINT32;
    }

    return ret;
}

/**
 * @brief 检查设备树节点的设备类型和兼容性属性是否匹配
 * @param node 设备树节点指针
 * @param compat 兼容字符串指针
 * @return 检查结果
 */
static int device_tree_node_is_compatible(const vmm_device_tree_node_t *node, const char *compat)
{
    const char *cp;
    int cplen;
    int l;

    cp    = vmm_device_tree_attrval(node, VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME);
    cplen = vmm_device_tree_attrlen(node, VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME);

    if (cp == NULL) {
        return 0;
    }

    while (cplen > 0) {
        if (strcmp(cp, compat) == 0) {
            return 1;
        }

        l = strlen(cp) + 1;
        cp += l;
        cplen -= l;
    }

    return 0;
}

/**
 * @brief 获取设备树节点指定属性的原始值指针
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @return 目标对象指针，不存在返回NULL
 */
const void *vmm_device_tree_attrval(const vmm_device_tree_node_t *node, const char *attrib)
{
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib) {
        return NULL; /**< NULL成员 */
    }

    vmm_device_tree_for_each_attr(attr, node)
    {
        if (strcmp(attr->name, attrib) == 0) {
            return attr->value;
        }
    }

    return NULL;
}

/**
 * @brief 获取设备树节点指定属性的值长度
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @return 成功返回属性长度，失败返回0
 */
uint32_t vmm_device_tree_attrlen(const vmm_device_tree_node_t *node, const char *attrib)
{
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib) {
        return 0; /**< 0 */
    }

    vmm_device_tree_for_each_attr(attr, node)
    {
        if (strcmp(attr->name, attrib) == 0) {
            return attr->len;
        }
    }

    return 0;
}

/**
 * @brief 检查设备树节点是否具有任何属性
 * @param node 设备树节点指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_have_attr(const vmm_device_tree_node_t *node)
{
    bool                    ret;
    irq_flags_t             flags;
    vmm_device_tree_node_t *np = (vmm_device_tree_node_t *)node;

    if (!np) {
        return FALSE;
    }

    vmm_read_lock_irq_save_lite(&np->attr_lock, flags);
    ret = list_empty(&np->attr_list) ? FALSE : TRUE;
    vmm_read_unlock_irq_restore_lite(&np->attr_lock, flags);

    return ret;
}

struct vmm_device_tree_attr *vmm_device_tree_next_attr(const vmm_device_tree_node_t *node, struct vmm_device_tree_attr *current)
{
    irq_flags_t                  flags; /**< 标志位 */
    struct vmm_device_tree_attr *ret = NULL; /**< NULL成员 */
    vmm_device_tree_node_t      *np  = (vmm_device_tree_node_t *)node; /**< )node成员 */

    if (!np) {
        return NULL; /**< NULL成员 */
    }

    vmm_read_lock_irq_save_lite(&np->attr_lock, flags); /**< flags)成员 */

    if (!current) {
        if (!list_empty(&np->attr_list)) {
            ret = list_first_entry(&np->attr_list, struct vmm_device_tree_attr, head); /**< head)成员 */
        }
    } else if (!list_is_last(&current->head, &np->attr_list)) {
        ret = list_first_entry(&current->head, struct vmm_device_tree_attr, head); /**< head)成员 */
    }

    vmm_read_unlock_irq_restore_lite(&np->attr_lock, flags); /**< flags)成员 */

    return ret; /**< 返回值 */
}

/**
 * @brief 设置设备树节点的指定属性，支持大端字节序转换
 * @param node 设备树节点指针
 * @param name 目标对象的名称
 * @param value 属性值数据指针
 * @param type 类型标识值
 * @param len 大小
 * @param value_is_be 值是否为大端字节序
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_setattr(vmm_device_tree_node_t *node, const char *name, void *value, uint32_t type, uint32_t len, bool value_is_be)
{
    uint32_t i;
    uint32_t size;
    uint32_t cnt;
    bool                         found;
    irq_flags_t                  flags;
    struct vmm_device_tree_attr *attr;

    if (!node || !name || (len && !value) || (VMM_DEVICE_TREE_MAX_ATTRTYPE <= type)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    found = FALSE;
    vmm_device_tree_for_each_attr(attr, node)
    {
        if (strcmp(attr->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        attr = vmm_malloc(sizeof(struct vmm_device_tree_attr));

        if (!attr) {
            return VMM_ERR_NOMEM;
        }

        INIT_LIST_HEAD(&attr->head);
        attr->len  = len;
        attr->type = type;
        strncpy(attr->name, name, sizeof(attr->name));

        if (attr->len) {
            attr->value = vmm_malloc(attr->len);

            if (!attr->value) {
                vmm_free(attr);
                return VMM_ERR_NOMEM;
            }

            memcpy(attr->value, value, attr->len);
        } else {
            attr->value = NULL;
        }

        vmm_write_lock_irq_save_lite(&node->attr_lock, flags);
        list_add_tail(&attr->head, &node->attr_list);
        vmm_write_unlock_irq_restore_lite(&node->attr_lock, flags);
    } else {
        attr->type = type;

        if (attr->len != len) {
            if (attr->len) {
                vmm_free(attr->value);
                attr->value = NULL;
                attr->len   = 0;
            }

            attr->len = len;

            if (attr->len) {
                attr->value = vmm_malloc(attr->len);

                if (!attr->value) {
                    attr->len = 0;
                    return VMM_ERR_NOMEM;
                }
            } else {
                attr->value = NULL;
            }
        }

        if (attr->len) {
            memcpy(attr->value, value, attr->len);
        }
    }

    if (attr->value && !value_is_be) {
        cnt  = 0;
        size = 0;

        switch (type) {
            case VMM_DEVICE_TREE_ATTRTYPE_UINT32:
                size = sizeof(uint32_t);
                cnt  = udiv32(len, size);
                break;

            case VMM_DEVICE_TREE_ATTRTYPE_UINT64:
                size = sizeof(uint64_t);
                cnt  = udiv32(len, size);
                break;

            case VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR:
                if (sizeof(virtual_addr_t) == sizeof(uint64_t)) {
                    size = sizeof(uint64_t);
                } else {
                }

                cnt = udiv32(len, size);
                break;

            case VMM_DEVICE_TREE_ATTRTYPE_VIRTSIZE:
                if (sizeof(virtual_size_t) == sizeof(uint64_t)) {
                    size = sizeof(uint64_t);
                } else {
                    size = sizeof(uint32_t);
                }

                cnt = udiv32(len, size);
                break;

            case VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR:
                if (sizeof(physical_addr_t) == sizeof(uint64_t)) {
                    size = sizeof(uint64_t);
                } else {
                    size = sizeof(uint32_t);
                }

                cnt = udiv32(len, size);
                break;

            case VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE:
                if (sizeof(physical_size_t) == sizeof(uint64_t)) {
                    size = sizeof(uint64_t);
                } else {
                    size = sizeof(uint32_t);
                }

                cnt = udiv32(len, size);
                break;

            default:
                break;
        };

        for (i = 0; i < cnt; i++) {
            switch (size) {
                case 4:
                    ((uint32_t *)attr->value)[i] = vmm_cpu_to_be32(((uint32_t *)attr->value)[i]);
                    break;

                case 8:
                    ((uint64_t *)attr->value)[i] = vmm_cpu_to_be64(((uint64_t *)attr->value)[i]);
                    break;

                default:
                    break;
            };
        }
    }

    return VMM_OK;
}

struct vmm_device_tree_attr *vmm_device_tree_getattr(const vmm_device_tree_node_t *node, const char *name)
{
    struct vmm_device_tree_attr *attr; /**< attr成员 */

    if (!node || !name) {
        return NULL; /**< NULL成员 */
    }

    vmm_device_tree_for_each_attr(attr, node)
    {
        if (strcmp(attr->name, name) == 0) {
            return attr; /**< attr成员 */
        }
    }

    return NULL; /**< NULL成员 */
}

/**
 * @brief 删除设备树节点的指定属性
 * @param node 设备树节点指针
 * @param name 目标对象的名称
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_delattr(vmm_device_tree_node_t *node, const char *name)
{
    irq_flags_t                  flags;
    struct vmm_device_tree_attr *attr;

    if (!node || !name) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    attr = vmm_device_tree_getattr(node, name);

    if (!attr) {
        return VMM_ERR_FAIL;
    }

    if (attr->value) {
        vmm_free(attr->value);
    }

    vmm_write_lock_irq_save_lite(&node->attr_lock, flags);
    list_del(&attr->head);
    vmm_write_unlock_irq_restore_lite(&node->attr_lock, flags);

    vmm_free(attr);

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的8位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u8_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out, int index)
{
    uint32_t                     asz;
    const uint8_t               *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (index < 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    asz = vmm_device_tree_attrlen(node, attrib);

    if (asz <= index) {
        return VMM_ERR_NOTAVAIL;
    }

    *out = aval[index];

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取8位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u8_array(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out, size_t size)
{
    uint32_t i;
    uint32_t asz;
    const uint8_t               *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || !size) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    asz = vmm_device_tree_attrlen(node, attrib);

    if (asz < size) {
        return VMM_ERR_NOTAVAIL;
    }

    for (i = 0; i < asz; i++) {
        out[i] = aval[i];
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的16位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u16_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out, int index)
{
    bool                         found;
    uint32_t i;
    uint32_t s;
    uint32_t asz;
    const void                  *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (index < 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    i     = 0;
    found = FALSE;
    asz   = vmm_device_tree_attrlen(node, attrib);

    while (asz) {
        s = (asz < sizeof(uint16_t)) ? asz : sizeof(uint16_t);

        if (i == index) {
            switch (s) {
                case 1:
                    *out = *((const uint8_t *)aval);
                    break;

                case 2:
                    *out = vmm_be16_to_cpu(*((const uint16_t *)aval));
                    break;

                default:
                    return VMM_ERR_FAIL;
            };

            found = TRUE;

            break;
        }

        aval += s;
        asz -= s;
        i++;
    }

    if (!found) {
        return VMM_ERR_NOTAVAIL;
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取16位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u16_array(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out, size_t size)
{
    uint32_t i;
    uint32_t s;
    uint32_t asz;
    const void                  *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (size <= 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    i   = 0;
    asz = vmm_device_tree_attrlen(node, attrib);

    while (asz && (i < size)) {
        s = (asz < sizeof(uint16_t)) ? asz : sizeof(uint16_t);

        switch (s) {
            case 1:
                out[i] = *((const uint8_t *)aval);
                break;

            case 2:
                out[i] = vmm_be16_to_cpu(*((const uint16_t *)aval));
                break;

            default:
                return VMM_ERR_FAIL;
        };

        aval += s;

        asz -= s;

        i++;
    }

    if (i < size) {
        return VMM_ERR_NOTAVAIL;
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的32位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u32_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out, int index)
{
    bool                         found;
    uint32_t i;
    uint32_t s;
    uint32_t asz;
    const void                  *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (index < 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    i     = 0;
    found = FALSE;
    asz   = vmm_device_tree_attrlen(node, attrib);

    while (asz) {
        s = (asz < sizeof(uint32_t)) ? asz : sizeof(uint32_t);

        if (i == index) {
            switch (s) {
                case 1:
                    *out = *((const uint8_t *)aval);
                    break;

                case 2:
                    *out = vmm_be16_to_cpu(*((const uint16_t *)aval));
                    break;

                case 4:
                    *out = vmm_be32_to_cpu(*((const uint32_t *)aval));
                    break;

                default:
                    return VMM_ERR_FAIL;
            };

            found = TRUE;

            break;
        }

        aval += s;
        asz -= s;
        i++;
    }

    if (!found) {
        return VMM_ERR_NOTAVAIL;
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取32位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u32_array(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out, size_t size)
{
    uint32_t i;
    uint32_t s;
    uint32_t asz;
    const void                  *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (size <= 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    i   = 0;
    asz = vmm_device_tree_attrlen(node, attrib);

    while (asz && (i < size)) {
        s = (asz < sizeof(uint32_t)) ? asz : sizeof(uint32_t);

        switch (s) {
            case 1:
                out[i] = *((const uint8_t *)aval);
                break;

            case 2:
                out[i] = vmm_be16_to_cpu(*((const uint16_t *)aval));
                break;

            case 4:
                out[i] = vmm_be32_to_cpu(*((const uint32_t *)aval));
                break;

            default:
                return VMM_ERR_FAIL;
        };

        aval += s;

        asz -= s;

        i++;
    }

    if (i < size) {
        return VMM_ERR_NOTAVAIL;
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的64位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u64_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out, int index)
{
    bool                         found;
    uint32_t i;
    uint32_t s;
    uint32_t asz;
    const void                  *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (index < 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    i     = 0;
    found = FALSE;
    asz   = vmm_device_tree_attrlen(node, attrib);

    while (asz) {
        s = (asz < sizeof(uint64_t)) ? asz : sizeof(uint64_t);

        if (i == index) {
            switch (s) {
                case 1:
                    *out = *((const uint8_t *)aval);
                    break;

                case 2:
                    *out = vmm_be16_to_cpu(*((const uint16_t *)aval));
                    break;

                case 4:
                    *out = vmm_be32_to_cpu(*((const uint32_t *)aval));
                    break;

                case 8:
                    *out = vmm_be64_to_cpu(*((const uint64_t *)aval));
                    break;

                default:
                    return VMM_ERR_FAIL;
            };

            found = TRUE;

            break;
        }

        aval += s;
        asz -= s;
        i++;
    }

    if (!found) {
        return VMM_ERR_NOTAVAIL;
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取64位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u64_array(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out, size_t size)
{
    uint32_t i;
    uint32_t s;
    uint32_t asz;
    const void                  *aval;
    struct vmm_device_tree_attr *attr;

    if (!node || !attrib || !out || (size <= 0)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID;
    }

    aval = attr->value;

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    i   = 0;
    asz = vmm_device_tree_attrlen(node, attrib);

    while (asz && (i < size)) {
        s = (asz < sizeof(uint64_t)) ? asz : sizeof(uint64_t);

        switch (s) {
            case 1:
                out[i] = *((const uint8_t *)aval);
                break;

            case 2:
                out[i] = vmm_be16_to_cpu(*((const uint16_t *)aval));
                break;

            case 4:
                out[i] = vmm_be32_to_cpu(*((const uint32_t *)aval));
                break;

            case 8:
                out[i] = vmm_be64_to_cpu(*((const uint64_t *)aval));
                break;

            default:
                break;
        };

        aval += s;

        asz -= s;

        i++;
    }

    if (i < size) {
        return VMM_ERR_NOTAVAIL;
    }

    return VMM_OK;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的物理地址值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physaddr_atindex(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out, int index)
{
    if (sizeof(physical_addr_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_atindex(node, attrib, (uint32_t *)out, index);
    } else if (sizeof(physical_addr_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_atindex(node, attrib, (uint64_t *)out, index);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取物理地址数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physaddr_array(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out, size_t size)
{
    if (sizeof(physical_addr_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_array(node, attrib, (uint32_t *)out, size);
    } else if (sizeof(physical_addr_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_array(node, attrib, (uint64_t *)out, size);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的物理大小值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physsize_atindex(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out, int index)
{
    if (sizeof(physical_size_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_atindex(node, attrib, (uint32_t *)out, index);
    } else if (sizeof(physical_size_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_atindex(node, attrib, (uint64_t *)out, index);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取物理大少数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physsize_array(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out, size_t size)
{
    if (sizeof(physical_size_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_array(node, attrib, (uint32_t *)out, size);
    } else if (sizeof(physical_size_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_array(node, attrib, (uint64_t *)out, size);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的虚拟地址值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtaddr_atindex(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out, int index)
{
    if (sizeof(virtual_addr_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_atindex(node, attrib, (uint32_t *)out, index);
    } else if (sizeof(virtual_addr_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_atindex(node, attrib, (uint64_t *)out, index);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取虚拟地址数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtaddr_array(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out, size_t size)
{
    if (sizeof(virtual_addr_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_array(node, attrib, (uint32_t *)out, size);
    } else if (sizeof(virtual_addr_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_array(node, attrib, (uint64_t *)out, size);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的虚拟大小值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtsize_atindex(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out, int index)
{
    if (sizeof(virtual_size_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_atindex(node, attrib, (uint32_t *)out, index);
    } else if (sizeof(virtual_size_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_atindex(node, attrib, (uint64_t *)out, index);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取虚拟大少数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtsize_array(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out, size_t size)
{
    if (sizeof(virtual_size_t) == sizeof(uint32_t)) {
        return vmm_device_tree_read_u32_array(node, attrib, (uint32_t *)out, size);
    } else if (sizeof(virtual_size_t) == sizeof(uint64_t)) {
        return vmm_device_tree_read_u64_array(node, attrib, (uint64_t *)out, size);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 从设备树节点的指定属性中读取字符串值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_string(const vmm_device_tree_node_t *node, const char *attrib, const char **out)
{
    const char *aval;

    if (!node || !attrib || !out) {
        return VMM_ERR_INVALID;
    }

    aval = vmm_device_tree_attrval(node, attrib);

    if (!aval) {
        return VMM_ERR_NOTAVAIL;
    }

    *out = aval;

    return VMM_OK;
}

/**
 * @brief 检查设备树节点的字符串属性是否与指定字符串匹配
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param string 待匹配的字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_match_string(vmm_device_tree_node_t *node, const char *attrib, const char *string)
{
    int                          i;
    size_t                       l;
    const char *p = NULL;
    const char *end = NULL;
    struct vmm_device_tree_attr *attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (!attr->value) {
        return VMM_ERR_NODATA;
    }

    p   = attr->value;
    end = p + attr->len;

    for (i = 0; p < end; i++, p += l) {
        l = strlen(p) + 1;

        if (p + l > end) {
            return VMM_ERR_ILSEQ;
        }

        if (strcmp(string, p) == 0) {
            return i; /* Found it; return index */
        }
    }

    return VMM_ERR_NODATA;
}

/**
 * @brief 统计设备树节点指定字符串属性中的字符串数量
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_count_strings(vmm_device_tree_node_t *node, const char *attrib)
{
    int                          i = 0;
    const char                  *p;
    size_t l = 0;
    size_t total = 0;
    struct vmm_device_tree_attr *attr = vmm_device_tree_getattr(node, attrib);

    if (!attr) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (!attr->value) {
        return VMM_ERR_NODATA;
    }

    if (strnlen(attr->value, attr->len) >= attr->len) {
        return VMM_ERR_ILSEQ;
    }

    p = attr->value;

    for (i = 0; total < attr->len; total += l, p += l, i++) {
        l = strlen(p) + 1;
    }

    return i;
}

/**
 * @brief 从设备树节点的字符串属性中获取指定索引处的字符串
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param index 数组中的索引位置
 * @param out 用于返回读取结果的输出指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_string_index(vmm_device_tree_node_t *node, const char *attrib, int index, const char **out)
{
    int                          i;
    size_t                       l;
    const char *p = NULL;
    const char *end = NULL;
    struct vmm_device_tree_attr *attr = vmm_device_tree_getattr(node, attrib);

    if (!attr || !out) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (!attr->value) {
        return VMM_ERR_NODATA;
    }

    if (index < 0) {
        return VMM_ERR_ILSEQ;
    }

    p   = attr->value;
    end = p + attr->len;

    for (i = 0; p < end; i++, p += l) {
        l = strlen(p) + 1;

        if (p + l > end) {
            return VMM_ERR_ILSEQ;
        }

        if (i == index) {
            *out = p;
            return l - 1;
        }
    }

    return VMM_ERR_NODATA;
}

/**
 * @brief 遍历设备树属性中的下一个32位无符号整数值
 * @param attr 属性结构体指针
 * @param cur 当前遍历位置指针
 * @param val 待写入的值
 * @return 索引值，未找到返回负数错误码
 */
const uint32_t *vmm_device_tree_next_u32(struct vmm_device_tree_attr *attr, const uint32_t *cur, uint32_t *val)
{
    const uint32_t *ret;

    if (!attr || !attr->value) {
        return NULL;
    }

    if (!cur) {
        ret = (const uint32_t *)attr->value;
    } else if (((const uint32_t *)attr->value <= cur) && (cur < ((const uint32_t *)attr->value + attr->len / sizeof(uint32_t)))) {
        ret = cur++;
    } else {
        ret = NULL;
    }

    if (val && ret) {
        *val = *ret;
    }

    return ret;
}

/**
 * @brief 遍历设备树属性中的下一个字符串值
 * @param attr 属性结构体指针
 * @param cur 当前遍历位置指针
 * @return 下一个元素指针，遍历结束返回NULL
 */
const char *vmm_device_tree_next_string(struct vmm_device_tree_attr *attr, const char *cur)
{
    const char *first = NULL;
    const char *last = NULL;

    if (!attr || !attr->value) {
        return NULL;
    }

    first = (const char *)attr->value;
    last  = first + attr->len;

    if (!cur) {
        return attr->value;
    } else if ((first <= cur) && (cur < last)) {
        cur = cur + strlen(cur) + 1;
        return (cur < last) ? cur : NULL;
    }

    return NULL;
}

/**
 * @brief 递归获取设备树节点路径
 * @param out 用于返回读取结果的输出指针
 * @param out_len 输出缓冲区的最大长度
 * @param node 设备树节点指针
 * @return 设备树操作结果
 */
static int recursive_getpath(char **out, size_t *out_len, const vmm_device_tree_node_t *node)
{
    int    rc;
    size_t len;

    if (!node) {
        return VMM_ERR_INVALID;
    }

    if (node->parent) {
        rc = recursive_getpath(out, out_len, node->parent);

        if (rc) {
            return rc;
        }

        if (*out_len < 2) {
            return VMM_ERR_NOSPC;
        }

        **out = VMM_DEVICE_TREE_PATH_SEPARATOR;
        (*out) += 1;
        **out = '\0';
        *out_len -= 1;
    }

    len = strlen(node->name);

    if (*out_len < (len + 1)) {
        return VMM_ERR_NOSPC;
    }

    strncpy(*out, node->name, len);
    (*out) += len;
    **out = '\0';
    *out_len -= len;

    return VMM_OK;
}

/**
 * @brief 获取设备树节点的完整路径字符串
 * @param out 用于返回读取结果的输出指针
 * @param out_len 输出缓冲区的最大长度
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_getpath(char *out, size_t out_len, const vmm_device_tree_node_t *node)
{
    int    rc;
    char  *out_ptr     = out;
    size_t out_ptr_len = out_len;

    if (!node || !out || (out_len < 2)) {
        return VMM_ERR_FAIL;
    }

    out[0] = 0;

    rc     = recursive_getpath(&out_ptr, &out_ptr_len, node);

    if (rc) {
        return rc;
    }

    if (strcmp(out, "") == 0) {
        out[0] = VMM_DEVICE_TREE_PATH_SEPARATOR;
        out[1] = '\0';
    }

    return VMM_OK;
}

/**
 * @brief 根据路径获取设备树节点的子节点
 * @param node 设备树节点指针
 * @param path 路径字符串
 * @return 目标对象指针，不存在返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_getchild(vmm_device_tree_node_t *node, const char *path)
{
    uint32_t                len;
    bool                    found;
    vmm_device_tree_node_t *np = NULL;
    vmm_device_tree_node_t *tp = NULL;
    vmm_device_tree_node_t *child = NULL;

    if (!path || !node) {
        return NULL;
    }

    while (*path == VMM_DEVICE_TREE_PATH_SEPARATOR) {
        path++;
    }

    np = node;

    while (*path) {
        found = FALSE;
        vmm_device_tree_for_each_child(child, np)
        {
            len = strlen(child->name);

            if (strncmp(child->name, path, len) == 0) {
                found = TRUE;
                path += len;

                if (*path) {
                    if (*path != VMM_DEVICE_TREE_PATH_SEPARATOR && *(path + 1) != '\0') {
                        path -= len;
                        continue;
                    }

                    if (*path == VMM_DEVICE_TREE_PATH_SEPARATOR) {
                        path++;
                    }
                }

                break;
            }
        }

        if (!found) {
            for (tp = np; tp && (tp != node); tp = tp->parent) {
                vmm_device_tree_dref_node(tp);
            }

            return NULL;
        }

        np = child;
    };

    if (np) {
        vmm_device_tree_ref_node(np);
    }

    for (tp = np; tp && (tp != node); tp = tp->parent) {
        vmm_device_tree_dref_node(tp);
    }

    return np;
}

/**
 * @brief 根据绝对路径查找设备树节点
 * @param path 路径字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_getnode(const char *path)
{
    vmm_device_tree_node_t *node = dtree_ctrl.root;

    if (!node) {
        return NULL;
    }

    if (!path) {
        vmm_device_tree_ref_node(node);
        return node;
    }

    if (strncmp(node->name, path, strlen(node->name)) != 0) {
        return NULL;
    }

    path += strlen(node->name);

    if (*path) {
        if (*path != VMM_DEVICE_TREE_PATH_SEPARATOR && *(path + 1) != '\0') {
            return NULL;
        }

        if (*path == VMM_DEVICE_TREE_PATH_SEPARATOR) {
            path++;
        }
    }

    return vmm_device_tree_getchild(node, path);
}

/**
 * @brief 将设备树节点与节点ID匹配表进行比较，返回匹配项
 * @param matches 节点ID匹配表指针
 * @param node 设备树节点指针
 * @return 目标对象指针，不存在返回NULL
 */
const struct vmm_device_tree_nodeid *vmm_device_tree_match_node(const struct vmm_device_tree_nodeid *matches, const vmm_device_tree_node_t *node)
{
    const char *type;

    if (!matches || !node) {
        return NULL;
    }

    type = vmm_device_tree_attrval(node, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME);

    while (matches->name[0] || matches->type[0] || matches->compatible[0]) {
        int match = 1;

        if (matches->name[0]) {
            match &= !strcmp(matches->name, node->name);
        }

        if (matches->type[0]) {
            match &= type && !strcmp(matches->type, type);
        }

        if (matches->compatible[0]) {
            match &= device_tree_node_is_compatible(node, matches->compatible);
        }

        if (match) {
            return matches;
        }

        matches++;
    }

    return NULL;
}

/**
 * @brief 在设备树子节点中查找与匹配表匹配的第一个节点
 * @param node 设备树节点指针
 * @param matches 节点ID匹配表指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_find_matching(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *matches)
{
    vmm_device_tree_node_t *child = NULL;
    vmm_device_tree_node_t *ret = NULL;

    if (!matches) {
        return NULL;
    }

    if (!node) {
        node = dtree_ctrl.root;
    }

    if (vmm_device_tree_match_node(matches, node)) {
        vmm_device_tree_ref_node(node);
        return node;
    }

    vmm_device_tree_for_each_child(child, node)
    {
        ret = vmm_device_tree_find_matching(child, matches);

        if (ret) {
            vmm_device_tree_dref_node(child);
            return ret;
        }
    }

    return NULL;
}

/**
 * @brief 递归遍历设备树，查找所有匹配指定节点ID表的节点
 */
void vmm_device_tree_iterate_matching(
    vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid                                         *matches,
    void (*found)(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data), void *found_data)
{
    vmm_device_tree_node_t              *child;
    const struct vmm_device_tree_nodeid *mid;

    if (!found || !matches) {
        return;
    }

    if (!node) {
        node = dtree_ctrl.root;
    }

    vmm_device_tree_ref_node(node);

    mid = vmm_device_tree_match_node(matches, node);

    if (mid) {
        found(node, mid, found_data);
    }

    vmm_device_tree_for_each_child(child, node)
    {
        vmm_device_tree_iterate_matching(child, matches, found, found_data);
    }

    vmm_device_tree_dref_node(node);
}

/**
 * @brief 在设备树子节点中查找具有指定设备类型和兼容字符串的节点
 * @param node 设备树节点指针
 * @param device_type 设备类型字符串
 * @param compatible 兼容字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_find_compatible(vmm_device_tree_node_t *node, const char *device_type, const char *compatible)
{
    struct vmm_device_tree_nodeid id[2];

    if (!compatible) {
        return NULL; /**< NULL成员 */
    }

    memset(id, 0, sizeof(id));

    if (device_type) {
        if (strlcpy(id[0].type, device_type, sizeof(id[0].type)) >= sizeof(id[0].type)) {
            return NULL;
        }
    }

    if (strlcpy(id[0].compatible, compatible, sizeof(id[0].compatible)) >= sizeof(id[0].compatible)) {
        return NULL;
    }

    return vmm_device_tree_find_matching(node, id);
}

/**
 * @brief 检查设备树节点是否具有指定的兼容字符串
 * @param node 设备树节点指针
 * @param compatible 兼容字符串
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_compatible(const vmm_device_tree_node_t *node, const char *compatible)
{
    struct vmm_device_tree_nodeid id[2];

    if (!node || !compatible) {
        return FALSE; /**< FALSE成员 */
    }

    memset(id, 0, sizeof(id));

    if (strlcpy(id[0].compatible, compatible, sizeof(id[0].compatible)) >= sizeof(id[0].compatible)) {
        return FALSE;
    }

    return vmm_device_tree_match_node(id, node) ? TRUE : FALSE;
}

/**
 * @brief 递归查找指定phandle值对应的设备树节点
 * @param node 设备树节点指针
 * @param phandle phandle句柄值
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static vmm_device_tree_node_t *recursive_find_node_by_phandle(vmm_device_tree_node_t *node, uint32_t phandle)
{
    int                     rc;
    uint32_t                phnd;
    vmm_device_tree_node_t *child = NULL;
    vmm_device_tree_node_t *ret = NULL;

    if (!node) {
        return NULL;
    }

    rc = vmm_device_tree_read_u32(node, VMM_DEVICE_TREE_PHANDLE_ATTR_NAME, &phnd);

    if ((rc == VMM_OK) && (phnd == phandle)) {
        ret = node;
        goto done;
    }

    ret = NULL;
    vmm_device_tree_for_each_child(child, node)
    {
        ret = recursive_find_node_by_phandle(child, phandle);

        if (ret) {
            vmm_device_tree_dref_node(child);
            break;
        }
    }

done:

    if (ret) {
        vmm_device_tree_ref_node(ret);
    }

    return ret;
}

/**
 * @brief 根据phandle值查找设备树节点
 * @param phandle phandle句柄值
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_find_node_by_phandle(uint32_t phandle)
{
    if (!dtree_ctrl.root) {
        return NULL;
    }

    return recursive_find_node_by_phandle(dtree_ctrl.root, phandle);
}

/**
 * @brief 解析设备树节点中phandle列表属性及其参数
 * @return 查找结果，失败返回错误码
 */
static int device_tree_parse_phandle_with_args(
    const vmm_device_tree_node_t *np, const char *list_name, const char *cells_name, int cell_count, int index,
    struct vmm_device_tree_phandle_args *out)
{
    uint32_t                count = 0, phandle; /**< phandle成员 */
    int                     rc = 0, size, cur_index = 0; /**< 0 */
    const uint32_t         *list, *list_end, *cells_val; /**< cells_val成员 */
    vmm_device_tree_node_t *node = NULL; /**< NULL成员 */

    /* Retrieve the phandle list property */
    list                         = vmm_device_tree_attrval(np, list_name); /**< list_name)成员 */

    if (!list) {
        return VMM_ERR_NOENT; /**< VMM_ERR_NOENT成员 */
    }

    size     = vmm_device_tree_attrlen(np, list_name); /**< list_name)成员 */
    list_end = list + size / sizeof(*list); /**< 链表 */

    /* Loop over the phandles until all the requested entry is found */
    while (list < list_end) {
        rc      = VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
        count   = 0; /**< 0 */

        /*
         * If phandle is 0, then it is an empty entry with no
         * arguments.  Skip forward to the next entry.
         */
        phandle = vmm_be32_to_cpu(*list); /**< 链表 */
        list++;

        if (phandle) {
            /*
             * Find the provider node and parse the #*-cells
             * property to determine the argument length.
             *
             * This is not needed if the cell count is hard-coded
             * (i.e. cells_name not set, but cell_count is set),
             * except when we're going to return the found node
             * below.
             */
            if (cells_name || cur_index == index) {
                node = vmm_device_tree_find_node_by_phandle(phandle); /**< vmm_device_tree_find_node_by_phandle(phandle)成员 */

                if (!node) {
                    vmm_printf("%s: phandle not found\n", np->name); /**< np->name)成员 */
                    goto err; /**< 错误码 */
                }
            }

            if (cells_name) {
                cells_val = vmm_device_tree_attrval(node, cells_name); /**< cells_name)成员 */

                if (!cells_val) {
                    vmm_printf(
                        "%s: could not get "
                        "%s for %s\n", /**< %s\n"成员 */
                        np->name, cells_name, node->name); /**< node->name)成员 */
                    goto err; /**< 错误码 */
                }

                count = vmm_be32_to_cpu(*cells_val); /**< cells_val成员 */
            } else {
                count = cell_count; /**< cell_count成员 */
            }

            /*
             * Make sure that the arguments actually fit in the
             * remaining property data length
             */
            if (list + count > list_end) {
                vmm_printf("%s: args longer than attribute\n", np->name); /**< np->name)成员 */
                goto err; /**< 错误码 */
            }
        }

        /*
         * All of the error cases above bail out of the loop, so at
         * this point, the parsing is successful. If the requested
         * index matches, then fill the out_args structure and return,
         * or return VMM_ERR_NOENT for an empty entry.
         */
        rc = VMM_ERR_NOENT; /**< VMM_ERR_NOENT成员 */

        if (cur_index == index) {
            if (!phandle) {
                goto err; /**< 错误码 */
            }

            if (out) {
                int i; /**< i */

                if (WARN_ON(count > VMM_MAX_PHANDLE_ARGS)) {
                    count = VMM_MAX_PHANDLE_ARGS; /**< VMM_MAX_PHANDLE_ARGS成员 */
                }

                vmm_device_tree_ref_node(node);
                out->np         = node; /**< 节点 */
                out->args_count = count; /**< 计数 */

                for (i = 0; i < count; i++) {
                    out->args[i] = vmm_be32_to_cpu(*list); /**< 链表 */
                    list++;
                }
            }

            /* Found it! return success */
            return VMM_OK; /**< VMM_OK成员 */
        }

        node = NULL; /**< NULL成员 */
        list += count; /**< 计数 */
        cur_index++;
    }

    /*
     * Unlock node before returning result; will be one of:
     * VMM_ERR_NOENT : index is for empty phandle
     * VMM_ERR_INVAL : parsing error on data
     * [1..n]  : Number of phandle (count mode; when index = -1)
     */
    rc = index < 0 ? cur_index : VMM_ERR_NOENT; /**< VMM_ERR_NOENT成员 */
err:
    return rc; /**< rc */
}

/**
 * @brief 解析设备树节点中phandle属性引用的节点
 * @param node 设备树节点指针
 * @param phandle_name phandle属性名称
 * @param index 数组中的索引位置
 * @return 成功返回解析得到的节点指针，失败返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_parse_phandle(const vmm_device_tree_node_t *node, const char *phandle_name, int index)
{
    struct vmm_device_tree_phandle_args args;

    if (index < 0) {
        return NULL; /**< NULL成员 */
    }

    if (device_tree_parse_phandle_with_args(node, phandle_name, NULL, 0, index, &args)) {
        return NULL;
    }

    return args.np;
}

/**
 * @brief 解析设备树节点中phandle列表属性及其参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_parse_phandle_with_args(
    const vmm_device_tree_node_t *node, const char *list_name, const char *cells_name, int index, struct vmm_device_tree_phandle_args *out)
{
    if (index < 0) {
        return VMM_ERR_INVALID;
    }

    return device_tree_parse_phandle_with_args(node, list_name, cells_name, 0, index, out);
}

/**
 * @brief 解析设备树节点中phandle列表属性及固定数量参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_parse_phandle_with_fixed_args(
    const vmm_device_tree_node_t *node, const char *list_name, int cells_count, int index, struct vmm_device_tree_phandle_args *out)
{
    if (index < 0) {
        return VMM_ERR_INVALID;
    }

    return device_tree_parse_phandle_with_args(node, list_name, NULL, cells_count, index, out);
}

/**
 * @brief 统计设备树节点中phandle列表属性中的条目数量
 * @param node 设备树节点指针
 * @param list_name 列表属性名称
 * @param cells_name 单元数量属性名称
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_count_phandle_with_args(const vmm_device_tree_node_t *node, const char *list_name, const char *cells_name)
{
    return device_tree_parse_phandle_with_args(node, list_name, cells_name, 0, -1, NULL);
}

/**
 * @brief 增加设备树节点的引用计数
 * @param node 设备树节点指针
 * @return 增加引用计数后返回的节点指针
 */
vmm_device_tree_node_t *vmm_device_tree_ref_node(vmm_device_tree_node_t *node)
{
    if (!node) {
        return NULL;
    }

    xref_get(&node->ref_count);

    return node;
}

/**
 * @brief 释放设备树节点及其关联的内存资源
 * @param ref 引用计数结构体指针
 */
static void __device_tree_node_free(struct xref *ref)
{
    int                          rc;
    irq_flags_t                  flags;
    struct vmm_device_tree_attr *attr = NULL;
    struct vmm_device_tree_attr *attr_next = NULL;
    vmm_device_tree_node_t      *parent;
    vmm_device_tree_node_t      *node = container_of(ref, vmm_device_tree_node_t, ref_count);

    if (dtree_ctrl.root == node) {
        dtree_ctrl.root = NULL; /**< NULL成员 */
    }

    vmm_read_lock_irq_save_lite(&node->attr_lock, flags);
    list_for_each_entry_safe(attr, attr_next, &node->attr_list, head)
    {
        vmm_read_unlock_irq_restore_lite(&node->attr_lock, flags);

        if ((rc = vmm_device_tree_delattr(node, attr->name))) {
            vmm_printf(
                "%s: Failed to delete attibute=%s "
                "from node=%s (error %d)\n",
                __func__, attr->name, node->name, rc);
        }

        vmm_read_lock_irq_save_lite(&node->attr_lock, flags);
    }
    vmm_read_unlock_irq_restore_lite(&node->attr_lock, flags);

    if (node->parent) {
        parent = node->parent;
        vmm_write_lock_irq_save_lite(&parent->child_lock, flags);
        list_del(&node->head);
        vmm_write_unlock_irq_restore_lite(&parent->child_lock, flags);
        node->parent = NULL;
        vmm_device_tree_dref_node(parent);
    }

    vmm_free(node);
}

/**
 * @brief 减少设备树节点的引用计数，计数归零时释放节点
 * @param node 设备树节点指针
 */
void vmm_device_tree_dref_node(vmm_device_tree_node_t *node)
{
    if (node) {
        xref_put(&node->ref_count, __device_tree_node_free);
    }
}

/**
 * @brief 检查设备树节点是否拥有子节点
 * @param node 设备树节点指针
 * @return 子节点返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_have_child(const vmm_device_tree_node_t *node)
{
    bool                    ret;
    irq_flags_t             flags;
    vmm_device_tree_node_t *np = (vmm_device_tree_node_t *)node;

    if (!np) {
        return FALSE;
    }

    vmm_read_lock_irq_save_lite(&np->child_lock, flags);
    ret = list_empty(&np->child_list) ? FALSE : TRUE;
    vmm_read_unlock_irq_restore_lite(&np->child_lock, flags);

    return ret;
}

/**
 * @brief 获取设备树节点的下一个子节点
 * @param node 设备树节点指针
 * @param current 当前遍历位置（NULL表示获取第一个）
 * @return 目标对象指针，不存在返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_next_child(const vmm_device_tree_node_t *node, vmm_device_tree_node_t *current)
{
    irq_flags_t             flags;
    vmm_device_tree_node_t *ret = NULL;
    vmm_device_tree_node_t *np  = (vmm_device_tree_node_t *)node;

    if (!np) {
        return NULL;
    }

    vmm_read_lock_irq_save_lite(&np->child_lock, flags);

    if (!current) {
        if (!list_empty(&np->child_list)) {
            ret = list_first_entry(&np->child_list, vmm_device_tree_node_t, head);
        }
    } else if ((current->parent == np) && !list_is_last(&current->head, &np->child_list)) {
        ret = list_first_entry(&current->head, vmm_device_tree_node_t, head);
    }

    if (ret) {
        vmm_device_tree_ref_node(ret);
    }

    vmm_read_unlock_irq_restore_lite(&np->child_lock, flags);

    if (current) {
        vmm_device_tree_dref_node(current);
    }

    return ret;
}

/**
 * @brief 获取设备树的按名称查找子节点
 * @param node 设备树节点指针
 * @param name 目标对象的名称
 * @return 目标对象指针，不存在返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_get_child_by_name(vmm_device_tree_node_t *node, const char *name)
{
    vmm_device_tree_node_t *ret = NULL;
    vmm_device_tree_node_t *child = NULL;

    vmm_device_tree_for_each_child(child, node)
    {
        if (strcasecmp(child->name, name) == 0) {
            ret = child;
            break;
        }
    }

    return ret;
}

/**
 * @brief 在设备树中创建新的子节点
 * @param parent 父设备树节点
 * @param name 目标对象的名称
 * @return 成功返回新创建的节点指针，失败返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_addnode(vmm_device_tree_node_t *parent, const char *name)
{
    irq_flags_t             flags;
    vmm_device_tree_node_t *node = NULL;

    if (!name) {
        return NULL;
    }

    if (!parent) {
        parent = dtree_ctrl.root;
    }

    if (parent) {
        vmm_device_tree_for_each_child(node, parent)
        {
            if (strcmp(node->name, name) == 0) {
                vmm_device_tree_dref_node(node);
                return NULL;
            }
        }
    }

    node = vmm_zalloc(sizeof(vmm_device_tree_node_t));

    if (!node) {
        return NULL;
    }

    INIT_LIST_HEAD(&node->head);
    INIT_RW_LOCK(&node->attr_lock);
    INIT_LIST_HEAD(&node->attr_list);
    INIT_RW_LOCK(&node->child_lock);
    INIT_LIST_HEAD(&node->child_list);
    xref_init(&node->ref_count);
    strncpy(node->name, name, sizeof(node->name));
    node->parent      = NULL;
    node->system_data = NULL;
    node->private     = NULL;

    if (parent) {
        vmm_device_tree_ref_node(parent);
        node->parent = parent;
        vmm_write_lock_irq_save_lite(&parent->child_lock, flags);
        list_add_tail(&node->head, &parent->child_list);
        vmm_write_unlock_irq_restore_lite(&parent->child_lock, flags);
    }

    return node;
}

/**
 * @brief 递归复制设备树节点
 * @param dst 目标缓冲区指针
 * @param src 源设备树节点
 * @return 设备树操作结果
 */
static int device_tree_copynode_recursive(vmm_device_tree_node_t *dst, vmm_device_tree_node_t *src)
{
    int                          rc;
    struct vmm_device_tree_attr *attr   = NULL;
    vmm_device_tree_node_t      *child  = NULL;
    vmm_device_tree_node_t      *schild = NULL;

    vmm_device_tree_for_each_attr(attr, src)
    {
        if ((rc = vmm_device_tree_setattr(dst, attr->name, attr->value, attr->type, attr->len, TRUE))) {
            return rc;
        }
    }

    vmm_device_tree_for_each_child(schild, src)
    {
        child = vmm_device_tree_addnode(dst, schild->name);

        if (!child) {
            vmm_device_tree_dref_node(schild);
            return VMM_ERR_FAIL;
        }

        if ((rc = device_tree_copynode_recursive(child, schild))) {
            vmm_device_tree_dref_node(schild);
            return rc;
        }
    }

    return VMM_OK;
}

/**
 * @brief 将源节点及其属性复制到目标父节点下
 * @param parent 父设备树节点
 * @param name 目标对象的名称
 * @param src 源设备树节点
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_copynode(vmm_device_tree_node_t *parent, const char *name, vmm_device_tree_node_t *src)
{
    vmm_device_tree_node_t *node = NULL;

    if (!parent || !name || !src) {
        return VMM_ERR_FAIL;
    }

    node = parent;

    while (node && src != node) {
        node = node->parent;
    }

    if (src == node) {
        return VMM_ERR_FAIL;
    }

    node = NULL;

    node = vmm_device_tree_addnode(parent, name);

    if (!node) {
        return VMM_ERR_FAIL;
    }

    return device_tree_copynode_recursive(node, src);
}

/**
 * @brief 从设备树中删除指定节点
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_delnode(vmm_device_tree_node_t *node)
{
    int                     rc;
    irq_flags_t             flags;
    vmm_device_tree_node_t *child = NULL;
    vmm_device_tree_node_t *child_next = NULL;

    if (!node) {
        return VMM_ERR_FAIL;
    }

    vmm_read_lock_irq_save_lite(&node->child_lock, flags);
    list_for_each_entry_safe(child, child_next, &node->child_list, head)
    {
        vmm_read_unlock_irq_restore_lite(&node->child_lock, flags);

        if ((rc = vmm_device_tree_delnode(child))) {
            return rc;
        }

        vmm_read_lock_irq_save_lite(&node->child_lock, flags);
    }
    vmm_read_unlock_irq_restore_lite(&node->child_lock, flags);

    vmm_device_tree_dref_node(node);

    return VMM_OK;
}

/**
 * @brief 获取设备树节点的时钟频率属性值
 * @param node 设备树节点指针
 * @param clock_freq 用于返回时钟频率值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_clock_frequency(vmm_device_tree_node_t *node, uint32_t *clock_freq)
{
    if (!node || !clock_freq) {
        return VMM_ERR_FAIL;
    }

    return vmm_device_tree_read_u32(node, VMM_DEVICE_TREE_CLOCK_FREQ_ATTR_NAME, clock_freq);
}

/**
 * @brief 检查设备树节点是否可用（status属性不为disabled）
 * @param node 设备树节点指针
 * @return 可用返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_available(const vmm_device_tree_node_t *node)
{
    const char *stat;
    uint32_t    statlen;

    if (!node) {
        return FALSE;
    }

    stat = vmm_device_tree_attrval(node, "status");

    if (stat == NULL) {
        return TRUE;
    }

    statlen = vmm_device_tree_attrlen(node, "status");

    if (statlen > 0) {
        if (!strcmp(stat, "okay") || !strcmp(stat, "ok")) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief 获取设备树别名的ID
 * @param node 设备树节点指针
 * @param stem 别名前缀字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_alias_get_id(vmm_device_tree_node_t *node, const char *stem)
{
    int                          id;
    struct vmm_device_tree_attr *attr;
    vmm_device_tree_node_t      *aliases;

    aliases = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_ALIASES_NODE_NAME);

    if (!aliases) {
        return VMM_ERR_NODEV;
    }

    id = VMM_ERR_NODEV;
    vmm_device_tree_for_each_attr(attr, aliases)
    {
        const char             *start = attr->name;
        const char             *end   = start + strlen(start);
        vmm_device_tree_node_t *np;
        int                     len;

        /* Skip those we do not want to proceed */
        if (!strcmp(attr->name, "name") || !strcmp(attr->name, "phandle") || !strcmp(attr->name, "linux,phandle")) {
            continue;
        }

        /* Find the node by path */
        np = vmm_device_tree_getnode(attr->value);

        if (!np) {
            continue;
        }

        /* Found node should be same as given node */
        if (node != np) {
            vmm_device_tree_dref_node(np);
            continue;
        }

        vmm_device_tree_dref_node(np);

        /* Walk the alias backwards to extract the id and
         * work out the 'stem' string
         */
        while (isdigit(*(end - 1)) && end > start) {
            end--;
        }

        len = end - start;

        /* Compare stem to alias attribute name */
        if (strncmp(stem, start, len)) {
            continue;
        }

        id = atoi(end);

        /* If id found then we are done */
        if (id >= 0) {
            break;
        }
    }

    return id;
}

/**
 * @brief 获取设备树节点ID表的数量
 * @return 数量值
 */
uint32_t vmm_device_tree_nidtable_count(void)
{
    return dtree_ctrl.nidtable_count;
}

struct vmm_device_tree_nidtable_entry *vmm_device_tree_nidtable_get(int index)
{
    if ((index < 0) || (dtree_ctrl.nidtable_count <= index)) {
        return NULL; /**< NULL成员 */
    }

    return &dtree_ctrl.nidtbl[index]; /**< nidtbl成员 */
}

/**
 * @brief 将设备树节点ID与匹配表进行比较
 * @param subsys 子系统名称字符串
 * @param nide IDE设备指针
 * @return 数量值
 */
static bool device_tree_compare_nid_for_matches(const char *subsys, struct vmm_device_tree_nidtable_entry *nide)
{
    if (!subsys) {
        return TRUE;
    }

    return (strcmp(nide->subsys, subsys) == 0) ? TRUE : FALSE;
}

/**
 * @brief 创建设备树节点ID匹配表
 * @param subsys 子系统名称字符串
 * @return 成功返回目标指针，失败返回NULL
 */
const struct vmm_device_tree_nodeid *vmm_device_tree_nidtable_create_matches(const char *subsys)
{
    uint32_t i;
    uint32_t idx;
    uint32_t count;
    struct vmm_device_tree_nodeid *nodeid = NULL;
    struct vmm_device_tree_nodeid *matches = NULL;
    struct vmm_device_tree_nidtable_entry *nide;

    /* Count number of enteries to be put in matches table */
    count = 0;

    for (i = 0; i < dtree_ctrl.nidtable_count; i++) {
        nide = &dtree_ctrl.nidtbl[i];

        if (device_tree_compare_nid_for_matches(subsys, nide)) {
            count++;
        }
    }

    if (!count) {
        return NULL;
    }

    /* Alloc matches table with extra zero entry at the end */
    matches = vmm_zalloc((count + 1) * sizeof(struct vmm_device_tree_nodeid));

    if (!matches) {
        return NULL;
    }

    /* Prepare matches table */
    idx = 0;

    for (i = 0; i < dtree_ctrl.nidtable_count; i++) {
        if (count <= idx) {
            break;
        }

        nide = &dtree_ctrl.nidtbl[i];
        nodeid  = &nide->nodeid;

        if (device_tree_compare_nid_for_matches(subsys, nide)) {
            memcpy(&matches[idx], nodeid, sizeof(*nodeid));
            idx++;
        }
    }

    return matches;
}

/**
 * @brief 销毁设备树节点ID匹配表
 * @param matches 节点ID匹配表指针
 */
void vmm_device_tree_nidtable_destroy_matches(const struct vmm_device_tree_nodeid *matches)
{
    if (matches) {
        vmm_free((void *)matches);
    }
}

/**
 * @brief 初始化设备树
 * @return 编号值
 */
int __init vmm_device_tree_init(void)
{
    int                                    rc;
    uint32_t                               nidtable_cnt;
    virtual_addr_t ca;
    virtual_addr_t nidtable_va;
    virtual_size_t                         nidtable_sz;
    struct vmm_device_tree_nidtable_entry *nide = NULL;
    struct vmm_device_tree_nidtable_entry *tnide = NULL;

    /* Reset the control structure */
    memset(&dtree_ctrl, 0, sizeof(dtree_ctrl));

    /* Populate Board Specific Device Tree */
    rc = arch_device_tree_populate(&dtree_ctrl.root);

    if (rc) {
        return rc;
    }

    /* Populate nodeid table */
    nidtable_va = arch_nidtable_vaddr();
    nidtable_sz = arch_nidtable_size();

    if (!nidtable_sz) {
        return VMM_OK;
    }

    nidtable_cnt      = udiv64(nidtable_sz, sizeof(*tnide));
    dtree_ctrl.nidtbl = vmm_zalloc(nidtable_cnt * sizeof(*tnide));

    if (!dtree_ctrl.nidtbl) {
        return VMM_ERR_NOMEM;
    }

    dtree_ctrl.nidtable_count = 0;

    for (ca = nidtable_va; ca < (nidtable_va + nidtable_sz);) {
        if (*(uint32_t *)ca != VMM_DEVICE_TREE_NIDTBL_SIGNATURE) {
            ca += sizeof(uint32_t);
            continue;
        }

        nide  = (struct vmm_device_tree_nidtable_entry *)ca;
        tnide = &dtree_ctrl.nidtbl[dtree_ctrl.nidtable_count];

        memcpy(tnide, nide, sizeof(*tnide));

        dtree_ctrl.nidtable_count++;
        ca += sizeof(*nide);
    }

    return VMM_OK;
}
