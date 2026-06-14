/**
 * Copyright (c) 2010 Anup Patel.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_device_tree.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief 设备树头文件
 */
#ifndef __VMM_DEVICE_TREE_H_
#define __VMM_DEVICE_TREE_H_

#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_compiler.h>
#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_DEVICE_TREE_PATH_SEPARATOR              '/'
#define VMM_DEVICE_TREE_PATH_SEPARATOR_STRING       "/"

#define VMM_DEVICE_TREE_MODEL_ATTR_NAME             "model"
#define VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME       "device_type"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_CPU         "cpu"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_GUEST       "guest"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_VCPU        "vcpu"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_RAM         "ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_RAM "alloced_ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_RAM "colored_ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_RAM  "shared_ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ROM         "rom"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_ROM "alloced_rom"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_ROM "colored_rom"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_ROM  "shared_rom"
#define VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME        "compatible"
#define VMM_DEVICE_TREE_DMA_COHERENT_ATTR_NAME      "dma-coherent"
#define VMM_DEVICE_TREE_CLOCK_FREQ_ATTR_NAME        "clock-frequency"
#define VMM_DEVICE_TREE_CLOCKS_ATTR_NAME            "clocks"
#define VMM_DEVICE_TREE_CLOCK_NAMES_ATTR_NAME       "clock-names"
#define VMM_DEVICE_TREE_CLOCK_OUT_NAMES_ATTR_NAME   "clock-output-names"
#define VMM_DEVICE_TREE_REG_ATTR_NAME               "reg"
#define VMM_DEVICE_TREE_REG_NAMES_ATTR_NAME         "reg-names"
#define VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME       "virtual-reg"
#define VMM_DEVICE_TREE_RANGES_ATTR_NAME            "ranges"
#define VMM_DEVICE_TREE_BIG_ENDIAN_ATTR_NAME        "big-endian"
#define VMM_DEVICE_TREE_NATIVE_ENDIAN_ATTR_NAME     "native-endian"
#define VMM_DEVICE_TREE_ADDR_CELLS_ATTR_NAME        "#address-cells"
#define VMM_DEVICE_TREE_SIZE_CELLS_ATTR_NAME        "#size-cells"
#define VMM_DEVICE_TREE_PHANDLE_ATTR_NAME           "phandle"

#define VMM_DEVICE_TREE_DEBUG_ATTR_NAME             "debug"

#define VMM_DEVICE_TREE_CHOSEN_NODE_NAME            "chosen"
#define VMM_DEVICE_TREE_CONSOLE_ATTR_NAME           "console"
#define VMM_DEVICE_TREE_STDOUT_ATTR_NAME            "stdout-path"
#define VMM_DEVICE_TREE_RTCDEV_ATTR_NAME            "rtcdev"
#define VMM_DEVICE_TREE_BOOTARGS_ATTR_NAME          "bootargs"
#define VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME           "bootcmd"

#define VMM_DEVICE_TREE_ALIASES_NODE_NAME           "aliases"

#define VMM_DEVICE_TREE_VMMINFO_NODE_NAME           "vmm"
#define VMM_DEVICE_TREE_VMMNET_NODE_NAME            "net"
#define VMM_DEVICE_TREE_NETSTACK_NODE_NAME          "hoststack"

#define VMM_DEVICE_TREE_MEMORY_NODE_NAME            "memory"
#define VMM_DEVICE_TREE_MEMORY_PHYS_ADDR_ATTR_NAME  "physical_addr"
#define VMM_DEVICE_TREE_MEMORY_PHYS_SIZE_ATTR_NAME  "physical_size"

#define VMM_DEVICE_TREE_RESERVED_MEMORY_NODE_NAME   "reserved-memory"

#define VMM_DEVICE_TREE_CPUS_NODE_NAME              "cpus"
#define VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME        "interrupts"
#define VMM_DEVICE_TREE_INTERRUPT_CNTRL_ATTR_NAME   "interrupt-controller"
#define VMM_DEVICE_TREE_ENABLE_METHOD_ATTR_NAME     "enable-method"
#define VMM_DEVICE_TREE_CPU_CLEAR_ADDR_ATTR_NAME    "cpu-clear-addr"
#define VMM_DEVICE_TREE_CPU_RELEASE_ADDR_ATTR_NAME  "cpu-release-addr"

#define VMM_DEVICE_TREE_GUESTINFO_NODE_NAME         "guests"
#define VMM_DEVICE_TREE_VCPUS_NODE_NAME             "vcpus"
#define VMM_DEVICE_TREE_VCPU_TEMPLATE_NODE_NAME     "vcpu_template"
#define VMM_DEVICE_TREE_ENDIANNESS_ATTR_NAME        "endianness"
#define VMM_DEVICE_TREE_ENDIANNESS_VAL_BIG          "big"
#define VMM_DEVICE_TREE_ENDIANNESS_VAL_LITTLE       "little"
#define VMM_DEVICE_TREE_START_PC_ATTR_NAME          "start_pc"
#define VMM_DEVICE_TREE_PRIORITY_ATTR_NAME          "priority"
#define VMM_DEVICE_TREE_TIME_SLICE_ATTR_NAME        "time_slice"
#define VMM_DEVICE_TREE_DEADLINE_ATTR_NAME          "deadline"
#define VMM_DEVICE_TREE_PERIODICITY_ATTR_NAME       "periodicity"
#define VMM_DEVICE_TREE_ADDRSPACE_NODE_NAME         "addr_space"
#define VMM_DEVICE_TREE_GUESTIRQCNT_ATTR_NAME       "guest_irq_count"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_ATTR_NAME     "manifest_type"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_REAL      "real"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_VIRTUAL   "virtual"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_ALIAS     "alias"
#define VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME      "address_type"
#define VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_MEMORY     "memory"
#define VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_IO         "io"
#define VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME        "guest_physical_addr"
#define VMM_DEVICE_TREE_HOST_PHYS_ATTR_NAME         "host_physical_addr"
#define VMM_DEVICE_TREE_ALIAS_PHYS_ATTR_NAME        "alias_physical_addr"
#define VMM_DEVICE_TREE_PHYS_SIZE_ATTR_NAME         "physical_size"
#define VMM_DEVICE_TREE_ALIGN_ORDER_ATTR_NAME       "align_order"
#define VMM_DEVICE_TREE_FIRST_COLOR_ATTR_NAME       "first_color"
#define VMM_DEVICE_TREE_NUM_COLORS_ATTR_NAME        "num_colors"
#define VMM_DEVICE_TREE_SHARED_MEM_ATTR_NAME        "shared_mem"
#define VMM_DEVICE_TREE_MAP_ORDER_ATTR_NAME         "map_order"
#define VMM_DEVICE_TREE_SWITCH_ATTR_NAME            "switch"
#define VMM_DEVICE_TREE_DOMAIN_ATTR_NAME            "domain"
#define VMM_DEVICE_TREE_NODE_ADDR_ATTR_NAME         "node_addr"
#define VMM_DEVICE_TREE_NODE_NS_NAME_ATTR_NAME      "node_ns_name"
#define VMM_DEVICE_TREE_BLKDEV_ATTR_NAME            "blkdev"
#define VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME     "affinity"
#define VMM_DEVICE_TREE_VCPU_POWEROFF_ATTR_NAME     "poweroff"
#define VMM_DEVICE_TREE_NO_CHILD_PROBE_ATTR_NAME    "no-child-probe"
#define VMM_DEVICE_TREE_THREADS_AFFINITY_ATTR_NAME  "threads_affinity"

/**
 * @brief 设备树属性类型枚举，定义字符串、整数等属性值类型
 */
enum vmm_device_tree_attrypes {
    VMM_DEVICE_TREE_ATTRTYPE_UINT32    = 0, /**< 0 */
    VMM_DEVICE_TREE_ATTRTYPE_UINT64    = 1, /**< 1 */
    VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR  = 2, /**< 2 */
    VMM_DEVICE_TREE_ATTRTYPE_VIRTSIZE  = 3, /**< 3 */
    VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR  = 4, /**< 4 */
    VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE  = 5, /**< 5 */
    VMM_DEVICE_TREE_ATTRTYPE_STRING    = 6, /**< 6 */
    VMM_DEVICE_TREE_ATTRTYPE_BYTEARRAY = 7, /**< 7 */
    VMM_DEVICE_TREE_MAX_ATTRTYPE       = 8
};

/**
 * @brief 设备树属性结构，保存属性的名称、类型和数据指针
 */
struct vmm_device_tree_attr {
    double_list_t head; /**< 链表头 */
    char          name[VMM_FIELD_SHORT_NAME_SIZE]; /**< 名称 */
    uint32_t      type; /**< 类型 */
    void         *value; /**< 值 */
    uint32_t      len; /**< 长度 */
};

/**
 * @brief 设备树节点ID结构，通过路径或phandle标识节点
 */
typedef struct vmm_device_tree_nodeid {
    char        name[VMM_FIELD_SHORT_NAME_SIZE]; /**< 名称 */
    char        type[VMM_FIELD_TYPE_SIZE]; /**< 类型 */
    char        compatible[VMM_FIELD_COMPAT_SIZE]; /**< compatible成员 */
    const void *data; /**< 数据 */
} vmm_device_tree_nodeid_t;

#define VMM_DEVICE_TREE_NIDTBL_SIGNATURE 0xDEADF001

/**
 * @brief 设备树节点ID表条目，缓存节点查找结果
 */
struct vmm_device_tree_nidtable_entry {
    uint32_t                      signature; /**< 签名标识 */
    char                          subsys[VMM_FIELD_SHORT_NAME_SIZE]; /**< subsys成员 */
    vmm_device_tree_nodeid_t nodeid; /**< nodeid成员 */
};

#ifndef __VMM_MODULES__

#define VMM_DEVICE_TREE_NIDTBL_ENTRY(nid, _subsys, _name, _type, _compat, _data)                                                                     \
    __nidtbl struct vmm_device_tree_nidtable_entry __##nid = {                                                                                       \
        .signature         = VMM_DEVICE_TREE_NIDTBL_SIGNATURE,                                                                                       \
        .subsys            = (_subsys),                                                                                                              \
        .nodeid.name       = (_name),                                                                                                                \
        .nodeid.type       = (_type),                                                                                                                \
        .nodeid.compatible = (_compat),                                                                                                              \
        .nodeid.data       = (_data),                                                                                                                \
    }

#else

/**
 * TODO: NodeID table enteries cannot be created from runtime pluggable
 * modules. This will be added in future because vmm_modules needs to be
 * updated to support it.
 */
#define VMM_DEVICE_TREE_NIDTBL_ENTRY(nodeid, _subsys, _name, _type, _compat, _data)

#endif

/**
 * @brief 设备树节点结构，维护节点层级关系和属性列表
 */
struct vmm_device_tree_node {
    /* Private fields */
    double_list_t                head; /**< 链表头 */
    vmm_rwlock_t                 attr_lock; /**< attr_lock成员 */
    double_list_t                attr_list; /**< attr_list成员 */
    vmm_rwlock_t                 child_lock; /**< child_lock成员 */
    double_list_t                child_list; /**< 子节点链表 */
    struct xref                  ref_count; /**< 引用计数 */
    /* Public fields */
    char                         name[VMM_FIELD_SHORT_NAME_SIZE]; /**< 名称 */
    struct vmm_device_tree_node *parent; /**< 父节点 */
    void                        *system_data; /* System data pointer
                                         (Arch. specific code can use this to
                                          pass inforation to device driver) */
    void *private;                            /* Generic Private pointer */
};

typedef struct vmm_device_tree_node vmm_device_tree_node_t;

#define VMM_MAX_PHANDLE_ARGS 8

/**
 * @brief 设备树phandle参数结构，保存引用节点的附加参数
 */
struct vmm_device_tree_phandle_args {
    vmm_device_tree_node_t *np; /**< 网络端口/无奇偶校验 */
    int                     args_count; /**< args_count成员 */
    uint32_t                args[VMM_MAX_PHANDLE_ARGS]; /**< 参数 */
};

/**
 * @brief 判断设备树属性类型是否为字面量类型
 * @param attrtype 属性类型标识
 * @return 字面量返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_isliteral(uint32_t attrtype);

/**
 * @brief 获取设备树字面量属性类型的数据大小
 * @param attrtype 属性类型标识
 * @return 大小值（字节）
 */
uint32_t vmm_device_tree_literal_size(uint32_t attrtype);

/**
 * @brief 估算设备树属性类型
 * @param name 目标对象的名称
 * @return 大小值（字节）
 */
uint32_t vmm_device_tree_estimate_attrtype(const char *name);

/**
 * @brief 获取设备树节点指定属性的原始值指针
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @return 目标对象指针，不存在返回NULL
 */
const void *vmm_device_tree_attrval(const vmm_device_tree_node_t *node, const char *attrib);

/**
 * @brief 获取设备树节点指定属性的值长度
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @return 成功返回属性长度，失败返回0
 */
uint32_t vmm_device_tree_attrlen(const vmm_device_tree_node_t *node, const char *attrib);

/**
 * @brief 检查设备树节点是否具有任何属性
 * @param node 设备树节点指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_have_attr(const vmm_device_tree_node_t *node);

/** Get next attribute of a device tree node */
struct vmm_device_tree_attr *vmm_device_tree_next_attr(const vmm_device_tree_node_t *node, struct vmm_device_tree_attr *current);

/** Itreate over each attribute of a device tree node */
#define vmm_device_tree_for_each_attr(attr, node)                                                                                                    \
    for (attr = vmm_device_tree_next_attr(node, NULL); attr; attr = vmm_device_tree_next_attr(node, attr))

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
int vmm_device_tree_setattr(vmm_device_tree_node_t *node, const char *name, void *value, uint32_t type, uint32_t len, bool value_is_be);

/** Get an attribute from a device tree node */
struct vmm_device_tree_attr *vmm_device_tree_getattr(const vmm_device_tree_node_t *node, const char *name);

/**
 * @brief 删除设备树节点的指定属性
 * @param node 设备树节点指针
 * @param name 目标对象的名称
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_delattr(vmm_device_tree_node_t *node, const char *name);

/**
 * @brief 从设备树节点的指定属性中读取索引处的8位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u8_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取8位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u8_array(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out, size_t size);

/**
 * @brief 从属性中读取8位无符号整数值
 */
static inline int vmm_device_tree_read_u8(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out)
{
/**
 * @brief 从设备树节点的指定属性中读取8位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param 1 参数1
 * @return 成功读取的字节数，失败返回错误码
 */
    return vmm_device_tree_read_u8_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的16位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u16_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取16位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u16_array(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out, size_t size);

/**
 * @brief 从设备树属性中读取16位无符号整数
 */
static inline int vmm_device_tree_read_u16(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out)
{
/**
 * @brief 从设备树节点的指定属性中读取16位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param 1 参数1
 * @return 成功读取的字节数，失败返回错误码
 */
    return vmm_device_tree_read_u16_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的32位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u32_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取32位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u32_array(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out, size_t size);

/**
 * @brief 从设备树属性中读取32位无符号整数
 */
static inline int vmm_device_tree_read_u32(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out)
{
/**
 * @brief 从设备树节点的指定属性中读取32位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param 1 参数1
 * @return 成功读取的字节数，失败返回错误码
 */
    return vmm_device_tree_read_u32_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的64位无符号整数值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u64_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取64位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_u64_array(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out, size_t size);

/**
 * @brief 从设备树属性中读取64位无符号整数
 */
static inline int vmm_device_tree_read_u64(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out)
{
/**
 * @brief 从设备树节点的指定属性中读取64位无符号整数数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param 1 参数1
 * @return 成功读取的字节数，失败返回错误码
 */
    return vmm_device_tree_read_u64_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的物理地址值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physaddr_atindex(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取物理地址数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physaddr_array(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out, size_t size);

/**
 * @brief 从设备树节点的指定属性中读取物理地址数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @return 成功读取的字节数，失败返回错误码
 */
static inline int vmm_device_tree_read_physaddr(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out)
{

    return vmm_device_tree_read_physaddr_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的物理大小值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physsize_atindex(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取物理大少数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_physsize_array(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out, size_t size);

/**
 * @brief 从设备树节点的指定属性中读取物理大少数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @return 大小值（字节）
 */
static inline int vmm_device_tree_read_physsize(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out)
{

    return vmm_device_tree_read_physsize_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的虚拟地址值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtaddr_atindex(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取虚拟地址数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtaddr_array(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out, size_t size);

/**
 * @brief 从设备树节点的指定属性中读取虚拟地址数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @return 成功读取的字节数，失败返回错误码
 */
static inline int vmm_device_tree_read_virtaddr(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out)
{

    return vmm_device_tree_read_virtaddr_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取索引处的虚拟大小值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param index 数组中的索引位置
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtsize_atindex(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out, int index);

/**
 * @brief 从设备树节点的指定属性中读取虚拟大少数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_virtsize_array(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out, size_t size);

/**
 * @brief 从设备树节点的指定属性中读取虚拟大少数组
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @return 大小值（字节）
 */
static inline int vmm_device_tree_read_virtsize(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out)
{
    return vmm_device_tree_read_virtsize_array(node, attrib, out, 1);
}

/**
 * @brief 从设备树节点的指定属性中读取字符串值
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param out 用于返回读取结果的输出指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_read_string(const vmm_device_tree_node_t *node, const char *attrib, const char **out);

/**
 * @brief 检查设备树节点的字符串属性是否与指定字符串匹配
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param string 待匹配的字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_match_string(vmm_device_tree_node_t *node, const char *attrib, const char *string);

/**
 * @brief 统计设备树节点指定字符串属性中的字符串数量
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_count_strings(vmm_device_tree_node_t *node, const char *attrib);

/**
 * @brief 从设备树节点的字符串属性中获取指定索引处的字符串
 * @param node 设备树节点指针
 * @param attrib 属性名称
 * @param index 数组中的索引位置
 * @param out 用于返回读取结果的输出指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_string_index(vmm_device_tree_node_t *node, const char *attrib, int index, const char **out);

/**
 * @brief 遍历设备树属性中的下一个32位无符号整数值
 * @param attr 属性结构体指针
 * @param cur 当前遍历位置指针
 * @param val 待写入的值
 * @return 索引值，未找到返回负数错误码
 */
const uint32_t *vmm_device_tree_next_u32(struct vmm_device_tree_attr *attr, const uint32_t *cur, uint32_t *val);

/**
 * @brief 遍历设备树属性中的下一个字符串值
 * @param attr 属性结构体指针
 * @param cur 当前遍历位置指针
 * @return 下一个元素指针，遍历结束返回NULL
 */
const char *vmm_device_tree_next_string(struct vmm_device_tree_attr *attr, const char *cur);

#define vmm_device_tree_for_each_u32(np, attrname, attr, p, u)                                                                                       \
    for (attr = vmm_device_tree_getattr(np, attrname), p = vmm_device_tree_next_u32(attr, NULL, &u); p; p = vmm_device_tree_next_u32(attr, p, &u))

#define vmm_device_tree_for_each_string(np, attrname, attr, s)                                                                                       \
    for (attr = vmm_device_tree_getattr(np, attrname), s = vmm_device_tree_next_string(attr, NULL); s; s = vmm_device_tree_next_string(attr, s))

/**
 * @brief 获取设备树节点的完整路径字符串
 * @param out 用于返回读取结果的输出指针
 * @param out_len 输出缓冲区的最大长度
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_getpath(char *out, size_t out_len, const vmm_device_tree_node_t *node);

/**
 * @brief 根据路径获取设备树节点的子节点
 * @param node 设备树节点指针
 * @param path 路径字符串
 * @return 目标对象指针，不存在返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_getchild(vmm_device_tree_node_t *node, const char *path);

/**
 * @brief 根据绝对路径查找设备树节点
 * @param path 路径字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_getnode(const char *path);

/**
 * @brief 将设备树节点与节点ID匹配表进行比较，返回匹配项
 * @param matches 节点ID匹配表指针
 * @param node 设备树节点指针
 * @return 目标对象指针，不存在返回NULL
 */
const vmm_device_tree_nodeid_t *vmm_device_tree_match_node(const vmm_device_tree_nodeid_t *matches, const vmm_device_tree_node_t *node);

/**
 * @brief 在设备树子节点中查找与匹配表匹配的第一个节点
 * @param node 设备树节点指针
 * @param matches 节点ID匹配表指针
 */
vmm_device_tree_node_t *vmm_device_tree_find_matching(vmm_device_tree_node_t *node, const vmm_device_tree_nodeid_t *matches);

/**
 * @brief 遍历所有匹配的节点，若node为NULL则从根节点开始
 */
void vmm_device_tree_iterate_matching(
    vmm_device_tree_node_t *node, const vmm_device_tree_nodeid_t                                         *matches,
    void (*found)(vmm_device_tree_node_t *node, const vmm_device_tree_nodeid_t *match, void *data), void *found_data);

/**
 * @brief 在设备树子节点中查找具有指定设备类型和兼容字符串的节点
 * @param node 设备树节点指针
 * @param device_type 设备类型字符串
 * @param compatible 兼容字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_find_compatible(vmm_device_tree_node_t *node, const char *device_type, const char *compatible);

/**
 * @brief 检查设备树节点是否具有指定的兼容字符串
 * @param node 设备树节点指针
 * @param compatible 兼容字符串
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_compatible(const vmm_device_tree_node_t *node, const char *compatible);

/**
 * @brief 根据phandle值查找设备树节点
 * @param phandle phandle句柄值
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_find_node_by_phandle(uint32_t phandle);

/**
 * @brief 解析设备树节点中phandle属性引用的节点
 * @param node 设备树节点指针
 * @param phandle_name phandle属性名称
 * @param index 数组中的索引位置
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_parse_phandle(const vmm_device_tree_node_t *node, const char *phandle_name, int index);

/** Find a node pointed by phandle in a list
 *
 *  This function is useful to parse lists of phandles and their arguments.
 *  Returns VMM_OK on success and fills out (i.e. args), on error returns
 *  appropriate errno value.
 *
 *  Example:
 *
 *  phandle1: node1 {
 *      #list-cells = <2>;
 *  }
 *
 *  phandle2: node2 {
 *  #list-cells = <1>;
 *  }
 *
 *  node3 {
 *  list = <&phandle1 1 2 &phandle2 3>;
 *  }
 *
 *  To get a device_node of the `node2' node you may call this:
 *  vmm_device_tree_parse_phandle_with_args(node3, "list", "#list-cells", 1, &out);
 *
 *  NOTE: The returned nodes will have increased refrence count
 */
int vmm_device_tree_parse_phandle_with_args(
    const vmm_device_tree_node_t *node, const char *list_name, const char *cells_name, int index, struct vmm_device_tree_phandle_args *out);

/**
 * Find a node pointed by phandle in a list
 *
 * This function is useful to parse lists of phandles and their arguments.
 * Returns 0 on success and fills out_args, on error returns appropriate
 * errno value.
 *
 * Example:
 *
 * phandle1: node1 {
 * }
 *
 * phandle2: node2 {
 * }
 *
 * node3 {
 *  list = <&phandle1 0 2 &phandle2 2 3>;
 * }
 *
 * To get a device_node of the `node2' node you may call this:
 * vmm_device_tree_parse_phandle_with_fixed_args(node3, "list", 2, 1, &args);
 *
 *  NOTE: The returned nodes will have increased refrence count
 */
int vmm_device_tree_parse_phandle_with_fixed_args(
    const vmm_device_tree_node_t *node, const char *list_name, int cells_count, int index, struct vmm_device_tree_phandle_args *out);

/**
 * @brief 统计设备树节点中phandle列表属性中的条目数量
 * @param node 设备树节点指针
 * @param list_name 列表属性名称
 * @param cells_name 单元数量属性名称
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_count_phandle_with_args(const vmm_device_tree_node_t *node, const char *list_name, const char *cells_name);

/**
 * @brief 增加设备树节点的引用计数
 * @param node 设备树节点指针
 */
vmm_device_tree_node_t *vmm_device_tree_ref_node(vmm_device_tree_node_t *node);

/**
 * @brief 减少设备树节点的引用计数，计数归零时释放节点
 * @param node 设备树节点指针
 */
void vmm_device_tree_dref_node(vmm_device_tree_node_t *node);

/**
 * @brief 检查设备树节点是否拥有子节点
 * @param node 设备树节点指针
 * @return 子节点返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_have_child(const vmm_device_tree_node_t *node);

/**
 * @brief 获取设备树节点的下一个子节点
 * @param node 设备树节点指针
 * @param current 当前遍历位置（NULL表示获取第一个）
 * @return 目标对象指针，不存在返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_next_child(const vmm_device_tree_node_t *node, vmm_device_tree_node_t *current);

/** Itreate over each child node of a device tree node
 *  NOTE: If we need to break-out of the loop in-between then
 *  we will need use vmm_device_tree_dref_node() on child node of
 *  current iteration just before breaking-out loop.
 */
#define vmm_device_tree_for_each_child(child, node)                                                                                                  \
    for (child = vmm_device_tree_next_child(node, NULL); child; child = vmm_device_tree_next_child(node, child))

/**
 * @brief 获取设备树的按名称查找子节点
 * @param node 设备树节点指针
 * @param name 目标对象的名称
 * @return 目标对象指针，不存在返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_get_child_by_name(vmm_device_tree_node_t *node, const char *name);

/**
 * @brief 在设备树中创建新的子节点
 * @param parent 父设备树节点
 * @param name 目标对象的名称
 * @return 成功返回新创建的节点指针，失败返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_addnode(vmm_device_tree_node_t *parent, const char *name);

/**
 * @brief 将源节点及其属性复制到目标父节点下
 * @param parent 父设备树节点
 * @param name 目标对象的名称
 * @param src 源设备树节点
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_copynode(vmm_device_tree_node_t *parent, const char *name, vmm_device_tree_node_t *src);

/**
 * @brief 从设备树中删除指定节点
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_delnode(vmm_device_tree_node_t *node);

/**
 * @brief 获取设备树节点的时钟频率属性值
 * @param node 设备树节点指针
 * @param clock_freq 用于返回时钟频率值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_clock_frequency(vmm_device_tree_node_t *node, uint32_t *clock_freq);

/**
 * @brief 获取设备树中断的数量
 * @param node 设备树节点指针
 * @return 数量值
 */
uint32_t vmm_device_tree_irq_count(vmm_device_tree_node_t *node);

/**
 * @brief 查找设备树节点的中断控制器父节点
 * @param child 子设备树节点
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_irq_find_parent(vmm_device_tree_node_t *child);

/**
 * @brief 解析设备树节点的第index个中断描述
 * @param device 设备结构体指针
 * @param index 数组中的索引位置
 * @param out_irq 用于返回解析后的中断描述
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_irq_parse_one(vmm_device_tree_node_t *device, int index, struct vmm_device_tree_phandle_args *out_irq);

/**
 * Find host irq_domain for a device node
 * @dev: Device node of the device whose interrupt is to be mapped
 *
 * Returns a pointer to the host irq_domain , or NULL if not found.
 */
struct vmm_host_irq_domain *vmm_device_tree_irq_domain_find(vmm_device_tree_node_t *dev);

/**
 * @brief 解析设备树节点的中断并映射到主机中断号
 * @param dev 设备结构体指针
 * @param index 数组中的索引位置
 * @return 中断号
 */
uint32_t vmm_device_tree_irq_parse_map(vmm_device_tree_node_t *dev, int index);

/**
 * @brief 检查设备树节点是否可用（status属性不为disabled）
 * @param node 设备树节点指针
 * @return 可用返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_available(const vmm_device_tree_node_t *node);

/**
 * @brief 获取设备树别名的ID
 * @param node 设备树节点指针
 * @param stem 别名前缀字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_alias_get_id(vmm_device_tree_node_t *node, const char *stem);

/**
 * @brief 获取设备树节点指定寄存器集的大小
 * @param node 设备树节点指针
 * @param size 数据大小（字节数）
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regsize(vmm_device_tree_node_t *node, physical_size_t *size, int regset);

/**
 * @brief 获取设备树节点指定寄存器集的物理地址
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regaddr(vmm_device_tree_node_t *node, physical_addr_t *addr, int regset);

/**
 * @brief 将设备树节点的寄存器映射到虚拟地址空间
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset);

/**
 * @brief 取消设备树节点寄存器的虚拟地址映射
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regunmap(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset);

/**
 * @brief 根据寄存器名称查找对应的寄存器集索引
 * @param node 设备树节点指针
 * @param regname 寄存器名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regname_to_regset(vmm_device_tree_node_t *node, const char *regname);

/**
 * @brief 按名称映射设备树寄存器区域
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regname 寄存器名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regmap_byname(vmm_device_tree_node_t *node, virtual_addr_t *addr, const char *regname);

/**
 * @brief 按名称取消映射设备树寄存器区域
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regname 寄存器名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regunmap_byname(vmm_device_tree_node_t *node, virtual_addr_t addr, const char *regname);

/**
 * @brief 请求并映射设备树节点的寄存器资源到虚拟地址
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @param resname 资源名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_request_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset, const char *resname);

/**
 * @brief 释放设备树寄存器映射
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regunmap_release(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset);

/**
 * @brief 检查设备树节点的寄存器是否使用大端字节序
 * @param node 设备树节点指针
 * @return 大端返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_reg_big_endian(vmm_device_tree_node_t *node);

/**
 * @brief 检查设备树节点是否标记为DMA一致性设备
 * @param node 设备树节点指针
 * @return DMA一致性返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_dma_coherent(vmm_device_tree_node_t *node);

/**
 * @brief 初始化设备树预留内存
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_reserved_memory_init(void);

/**
 * @brief 获取设备树节点ID表的数量
 * @return 数量值
 */
uint32_t vmm_device_tree_nidtable_count(void);

/** Get nodeid table entry at given index */
struct vmm_device_tree_nidtable_entry *vmm_device_tree_nidtable_get(int index);

/**
 * @brief 创建设备树节点ID匹配表
 * @param subsys 子系统名称字符串
 */
const vmm_device_tree_nodeid_t *vmm_device_tree_nidtable_create_matches(const char *subsys);

/**
 * @brief 销毁设备树节点ID匹配表
 * @param matches 节点ID匹配表指针
 */
void vmm_device_tree_nidtable_destroy_matches(const vmm_device_tree_nodeid_t *matches);

/**
 * @brief 初始化设备树
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_init(void);

#endif /* __VMM_DEVICE_TREE_H_ */
