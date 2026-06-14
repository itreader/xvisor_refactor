/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vmm_device_tree_reg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 注册主机相关的设备树函数
 */

#include <libs/stringlib.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_resource.h>
#include <vmm_stdio.h>

/**
 * @brief 获取设备树节点的地址单元数和大小单元数
 * @param node 设备树节点指针
 * @param addr_cells_p 地址单元数输出指针
 * @param size_cells_p 大小
 * @return 大小值（字节）
 */
static int device_tree_get_regcells(vmm_device_tree_node_t *node, uint32_t *addr_cells_p, uint32_t *size_cells_p)
{
    uint32_t addr_cells;
    uint32_t size_cells;
    vmm_device_tree_node_t *np;

    addr_cells = sizeof(physical_addr_t) / sizeof(uint32_t);
    size_cells = sizeof(physical_size_t) / sizeof(uint32_t);

    np         = node->parent;

    while (np && vmm_device_tree_read_u32(np, VMM_DEVICE_TREE_ADDR_CELLS_ATTR_NAME, &addr_cells)) {
        np = node->parent;
    }

    np = node->parent;

    while (np && vmm_device_tree_read_u32(np, VMM_DEVICE_TREE_SIZE_CELLS_ATTR_NAME, &size_cells)) {
        np = node->parent;
    }

    if ((2 < addr_cells) || (2 < size_cells)) {
        return VMM_ERR_INVALID;
    }

    if (addr_cells_p) {
        *addr_cells_p = addr_cells;
    }

    if (size_cells_p) {
        *size_cells_p = size_cells;
    }

    return VMM_OK;
}

/**
 * @brief 将设备树节点的物理地址映射为虚拟地址
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param map_addr 映射的物理地址
 */
static void device_tree_map_regaddr(vmm_device_tree_node_t *node, physical_addr_t addr, physical_addr_t *map_addr)
{
    int                     rc;
    uint32_t start;
    uint32_t end;
    uint32_t c[2] = {0,0};
    uint32_t addr_cells;
    uint32_t size_cells;
    uint32_t n_addr_cells;
    uint32_t n_size_cells;
    physical_addr_t in_addr;
    physical_addr_t out_addr;
    physical_size_t         in_size;
    vmm_device_tree_node_t *np;

    if (!node) {
        goto done;
    }

    np = node->parent;

    while (np) {
        if (!vmm_device_tree_getattr(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME)) {
            goto skip;
        }

        rc = vmm_device_tree_read_u32(np, VMM_DEVICE_TREE_ADDR_CELLS_ATTR_NAME, &addr_cells);

        if (rc) {
            goto skip;
        }

        rc = vmm_device_tree_read_u32(np, VMM_DEVICE_TREE_SIZE_CELLS_ATTR_NAME, &size_cells);

        if (rc) {
            goto skip;
        }

        if ((addr_cells < 1) || (size_cells < 1)) {
            goto done;
        }

        rc = device_tree_get_regcells(np, &n_addr_cells, &n_size_cells);

        if (rc) {
            goto skip;
        }

        if ((n_addr_cells < 1) || (n_size_cells < 1)) {
            goto done;
        }

        start = 0;
        end   = vmm_device_tree_attrlen(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME);
        end   = end / sizeof(uint32_t);

        while (start < end) {
            rc = vmm_device_tree_read_u32_atindex(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME, &c[0], start);
            start++;

            if (rc) {
                continue;
            }

            if (addr_cells == 2) {
                rc = vmm_device_tree_read_u32_atindex(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME, &c[1], start);
                start++;

                if (rc) {
                    continue;
                }

                in_addr = ((uint64_t)c[0] << 32) | (uint64_t)c[1];
            } else {
                in_addr = c[0];
            }

            rc = vmm_device_tree_read_u32_atindex(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME, &c[0], start);
            start++;

            if (rc) {
                continue;
            }

            if (n_addr_cells == 2) {
                rc = vmm_device_tree_read_u32_atindex(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME, &c[1], start);
                start++;

                if (rc) {
                    continue;
                }

                out_addr = ((uint64_t)c[0] << 32) | (uint64_t)c[1];
            } else {
                out_addr = c[0];
            }

            rc = vmm_device_tree_read_u32_atindex(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME, &c[0], start);
            start++;

            if (rc) {
                continue;
            }

            if (size_cells == 2) {
                rc = vmm_device_tree_read_u32_atindex(np, VMM_DEVICE_TREE_RANGES_ATTR_NAME, &c[1], start);
                start++;

                if (rc) {
                    continue;
                }

                in_size = ((uint64_t)c[0] << 32) | (uint64_t)c[1];
            } else {
                in_size = c[0];
            }

            if (in_addr <= addr && addr < (in_addr + in_size)) {
                addr = out_addr + (addr - in_addr);
            }
        }

    skip:
        np = np->parent;
    }

done:

    if (map_addr) {
        *map_addr = addr;
    }
}

/**
 * @brief 获取设备树节点指定寄存器集的大小
 * @param node 设备树节点指针
 * @param size 数据大小（字节数）
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regsize(vmm_device_tree_node_t *node, physical_size_t *size, int regset)
{
    int      rc;
    uint32_t start;
    uint32_t addr_cells;
    uint32_t size_cells;
    uint32_t cells[2] = {0,0};

    if (!node || !size || regset < 0) {
        return VMM_ERR_FAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_ERR_NOTAVAIL;
    }

    rc = device_tree_get_regcells(node, &addr_cells, &size_cells);

    if (rc) {
        return rc;
    }

    if (size_cells < 1) {
        return VMM_ERR_INVALID;
    }

    start = regset * (addr_cells + size_cells) + addr_cells;

    rc    = vmm_device_tree_read_u32_atindex(node, VMM_DEVICE_TREE_REG_ATTR_NAME, &cells[0], start);

    if (rc) {
        return rc;
    }

    if (size_cells == 2) {
        rc = vmm_device_tree_read_u32_atindex(node, VMM_DEVICE_TREE_REG_ATTR_NAME, &cells[1], start + 1);

        if (rc) {
            return rc;
        }
    }

    if (size_cells == 2) {
        *size = ((uint64_t)cells[0] << 32) | (uint64_t)cells[1];
    } else {
        *size = cells[0];
    }

    return VMM_OK;
}

/**
 * @brief 获取设备树节点指定寄存器集的物理地址
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regaddr(vmm_device_tree_node_t *node, physical_addr_t *addr, int regset)
{
    int      rc;
    uint32_t start;
    uint32_t addr_cells;
    uint32_t size_cells;
    uint32_t cells[2] = {0,0};  

    if (!node || !addr || regset < 0) {
        return VMM_ERR_FAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_ERR_NOTAVAIL;
    }

    rc = device_tree_get_regcells(node, &addr_cells, &size_cells);

    if (rc) {
        return rc;
    }

    if (addr_cells < 1) {
        return VMM_ERR_INVALID;
    }

    start = regset * (addr_cells + size_cells);

    rc    = vmm_device_tree_read_u32_atindex(node, VMM_DEVICE_TREE_REG_ATTR_NAME, &cells[0], start);

    if (rc) {
        return rc;
    }

    if (addr_cells == 2) {
        rc = vmm_device_tree_read_u32_atindex(node, VMM_DEVICE_TREE_REG_ATTR_NAME, &cells[1], start + 1);

        if (rc) {
            return rc;
        }
    }

    if (addr_cells == 2) {
        *addr = ((uint64_t)cells[0] << 32) | (uint64_t)cells[1];
    } else {
        *addr = cells[0];
    }

    device_tree_map_regaddr(node, *addr, addr);

    return VMM_OK;
}

/**
 * @brief 将设备树节点的寄存器映射到虚拟地址空间
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;

    if (!node || !addr || regset < 0) {
        return VMM_ERR_FAIL;
    }

    rc = vmm_device_tree_read_virtaddr_atindex(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME, addr, regset);

    if (!rc) {
        return VMM_OK;
    }

    rc = vmm_device_tree_regsize(node, &size, regset);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_regaddr(node, &pa, regset);

    if (rc) {
        return rc;
    }

    if (!size) {
        return VMM_ERR_INVALID;
    }

    *addr = vmm_host_iomap(pa, size);

    return VMM_OK;
}

/**
 * @brief 取消设备树节点寄存器的虚拟地址映射
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regunmap(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset)
{
    int             rc;
    physical_size_t size;
    virtual_addr_t  vva;
    virtual_size_t  vsz;

    if (!node || regset < 0) {
        return VMM_ERR_FAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_OK;
    }

    rc = vmm_device_tree_regsize(node, &size, regset);

    if (rc) {
        return rc;
    }

    rc = vmm_host_virtual_address_pool_find(addr, &vva, &vsz);

    if (rc) {
        return rc;
    }

    if (size != vsz) {
        return VMM_ERR_INVALID;
    }

    return vmm_host_iounmap(addr);
}

/**
 * @brief 根据寄存器名称查找对应的寄存器集索引
 * @param node 设备树节点指针
 * @param regname 寄存器名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regname_to_regset(vmm_device_tree_node_t *node, const char *regname)
{
    if (!node || !regname) {
        return VMM_ERR_FAIL;
    }

    return vmm_device_tree_match_string(node, VMM_DEVICE_TREE_REG_NAMES_ATTR_NAME, regname);
}

/**
 * @brief 按名称映射设备树寄存器区域
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regname 寄存器名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regmap_byname(vmm_device_tree_node_t *node, virtual_addr_t *addr, const char *regname)
{
    int regset;

    if (!node || !addr || !regname) {
        return VMM_ERR_FAIL;
    }

    regset = vmm_device_tree_regname_to_regset(node, regname);

    if (regset < 0) {
        return regset;
    }

    return vmm_device_tree_regmap(node, addr, regset);
}

/**
 * @brief 按名称取消映射设备树寄存器区域
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regname 寄存器名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regunmap_byname(vmm_device_tree_node_t *node, virtual_addr_t addr, const char *regname)
{
    int regset;

    if (!node || !regname) {
        return VMM_ERR_FAIL;
    }

    regset = vmm_device_tree_regname_to_regset(node, regname);

    if (regset < 0) {
        return regset;
    }

    return vmm_device_tree_regunmap(node, addr, regset);
}

/**
 * @brief 检查设备树节点的寄存器是否使用大端字节序
 * @param node 设备树节点指针
 * @return 大端返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_reg_big_endian(vmm_device_tree_node_t *node)
{
    if (!node) {
        return FALSE;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_BIG_ENDIAN_ATTR_NAME)) {
        return TRUE;
    }

    if (IS_ENABLED(CONFIG_CPU_BE) && vmm_device_tree_getattr(node, VMM_DEVICE_TREE_NATIVE_ENDIAN_ATTR_NAME)) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief 检查设备树节点是否标记为DMA一致性设备
 * @param node 设备树节点指针
 * @return DMA一致性返回TRUE，否则返回FALSE
 */
bool vmm_device_tree_is_dma_coherent(vmm_device_tree_node_t *node)
{
    if (node && vmm_device_tree_getattr(node, VMM_DEVICE_TREE_DMA_COHERENT_ATTR_NAME)) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief 请求并映射设备树节点的寄存器资源到虚拟地址
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @param resname 资源名称字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_request_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset, const char *resname)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;

    if (!node || !addr || (regset < 0) || !resname) {
        return VMM_ERR_FAIL;
    }

    rc = vmm_device_tree_read_virtaddr_atindex(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME, addr, regset);

    if (!rc) {
        return VMM_ERR_INVALID;
    }

    rc = vmm_device_tree_regsize(node, &size, regset);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_regaddr(node, &pa, regset);

    if (rc) {
        return rc;
    }

    if (!size) {
        return VMM_ERR_INVALID;
    }

    vmm_request_mem_region(pa, size, resname);

    *addr = vmm_host_iomap(pa, size);

    return VMM_OK;
}

/**
 * @brief 释放设备树寄存器映射
 * @param node 设备树节点指针
 * @param addr 地址值
 * @param regset 寄存器集索引号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_regunmap_release(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;
    virtual_addr_t  vva;
    virtual_size_t  vsz;

    if (!node || regset < 0) {
        return VMM_ERR_FAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_ERR_INVALID;
    }

    rc = vmm_device_tree_regsize(node, &size, regset);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_regaddr(node, &pa, regset);

    if (rc) {
        return rc;
    }

    rc = vmm_host_virtual_address_pool_find(addr, &vva, &vsz);

    if (rc) {
        return rc;
    }

    if (size != vsz) {
        return VMM_ERR_INVALID;
    }

    rc = vmm_host_iounmap(addr);

    if (rc) {
        return rc;
    }

    vmm_release_mem_region(pa, size);

    return VMM_OK;
}

/**
 * @brief 初始化设备树预留内存
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_device_tree_reserved_memory_init(void)
{
    int pos;
    int ret;
    physical_addr_t         pa;
    physical_size_t         size;
    vmm_device_tree_node_t *child = NULL;
    vmm_device_tree_node_t *node = NULL;

    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_RESERVED_MEMORY_NODE_NAME);

    if (!node) {
        return VMM_OK;
    }

    vmm_device_tree_for_each_child(child, node)
    {
        pos = 0;

        while (1) {
            if (vmm_device_tree_regaddr(child, &pa, pos) != VMM_OK) {
                break;
            }

            if (vmm_device_tree_regsize(child, &size, pos) != VMM_OK) {
                break;
            }

            pos++;
            ret = vmm_host_ram_reserve(pa, size);
            vmm_init_printf("ram_reserve: phys=0x%" PRIPADDR " size=%" PRIPSIZE "%s\n", pa, size, (ret) ? " (ignored)" : "");
        }
    }

    vmm_device_tree_dref_node(node);

    return VMM_OK;
}
