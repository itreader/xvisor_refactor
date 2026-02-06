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
 * @brief Host registers related device tree functions
 */

#include <libs/stringlib.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_resource.h>
#include <vmm_stdio.h>

static int device_tree_get_regcells(vmm_device_tree_node_t *node, uint32_t *addr_cells_p, uint32_t *size_cells_p)
{
    uint32_t                addr_cells, size_cells;
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
        return VMM_EINVALID;
    }

    if (addr_cells_p) {
        *addr_cells_p = addr_cells;
    }

    if (size_cells_p) {
        *size_cells_p = size_cells;
    }

    return VMM_OK;
}

static void device_tree_map_regaddr(vmm_device_tree_node_t *node, physical_addr_t addr, physical_addr_t *map_addr)
{
    int                     rc;
    uint32_t                start, end, c[2] = {0, 0};
    uint32_t                addr_cells, size_cells;
    uint32_t                n_addr_cells, n_size_cells;
    physical_addr_t         in_addr, out_addr;
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

int vmm_device_tree_regsize(vmm_device_tree_node_t *node, physical_size_t *size, int regset)
{
    int      rc;
    uint32_t start, addr_cells, size_cells, cells[2] = {0, 0};

    if (!node || !size || regset < 0) {
        return VMM_EFAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_ENOTAVAIL;
    }

    rc = device_tree_get_regcells(node, &addr_cells, &size_cells);

    if (rc) {
        return rc;
    }

    if (size_cells < 1) {
        return VMM_EINVALID;
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

int vmm_device_tree_regaddr(vmm_device_tree_node_t *node, physical_addr_t *addr, int regset)
{
    int      rc;
    uint32_t start, addr_cells, size_cells, cells[2] = {0, 0};

    if (!node || !addr || regset < 0) {
        return VMM_EFAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_ENOTAVAIL;
    }

    rc = device_tree_get_regcells(node, &addr_cells, &size_cells);

    if (rc) {
        return rc;
    }

    if (addr_cells < 1) {
        return VMM_EINVALID;
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

int vmm_device_tree_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;

    if (!node || !addr || regset < 0) {
        return VMM_EFAIL;
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
        return VMM_EINVALID;
    }

    *addr = vmm_host_iomap(pa, size);

    return VMM_OK;
}

int vmm_device_tree_regunmap(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset)
{
    int             rc;
    physical_size_t size;
    virtual_addr_t  vva;
    virtual_size_t  vsz;

    if (!node || regset < 0) {
        return VMM_EFAIL;
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
        return VMM_EINVALID;
    }

    return vmm_host_iounmap(addr);
}

int vmm_device_tree_regname_to_regset(vmm_device_tree_node_t *node, const char *regname)
{
    if (!node || !regname) {
        return VMM_EFAIL;
    }

    return vmm_device_tree_match_string(node, VMM_DEVICE_TREE_REG_NAMES_ATTR_NAME, regname);
}

int vmm_device_tree_regmap_byname(vmm_device_tree_node_t *node, virtual_addr_t *addr, const char *regname)
{
    int regset;

    if (!node || !addr || !regname) {
        return VMM_EFAIL;
    }

    regset = vmm_device_tree_regname_to_regset(node, regname);

    if (regset < 0) {
        return regset;
    }

    return vmm_device_tree_regmap(node, addr, regset);
}

int vmm_device_tree_regunmap_byname(vmm_device_tree_node_t *node, virtual_addr_t addr, const char *regname)
{
    int regset;

    if (!node || !regname) {
        return VMM_EFAIL;
    }

    regset = vmm_device_tree_regname_to_regset(node, regname);

    if (regset < 0) {
        return regset;
    }

    return vmm_device_tree_regunmap(node, addr, regset);
}

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

bool vmm_device_tree_is_dma_coherent(vmm_device_tree_node_t *node)
{
    if (node && vmm_device_tree_getattr(node, VMM_DEVICE_TREE_DMA_COHERENT_ATTR_NAME)) {
        return TRUE;
    }

    return FALSE;
}

int vmm_device_tree_request_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset, const char *resname)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;

    if (!node || !addr || (regset < 0) || !resname) {
        return VMM_EFAIL;
    }

    rc = vmm_device_tree_read_virtaddr_atindex(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME, addr, regset);

    if (!rc) {
        return VMM_EINVALID;
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
        return VMM_EINVALID;
    }

    vmm_request_mem_region(pa, size, resname);

    *addr = vmm_host_iomap(pa, size);

    return VMM_OK;
}

int vmm_device_tree_regunmap_release(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;
    virtual_addr_t  vva;
    virtual_size_t  vsz;

    if (!node || regset < 0) {
        return VMM_EFAIL;
    }

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME)) {
        return VMM_EINVALID;
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
        return VMM_EINVALID;
    }

    rc = vmm_host_iounmap(addr);

    if (rc) {
        return rc;
    }

    vmm_release_mem_region(pa, size);

    return VMM_OK;
}

int __init vmm_device_tree_reserved_memory_init(void)
{
    int                     pos, ret;
    physical_addr_t         pa;
    physical_size_t         size;
    vmm_device_tree_node_t *child, *node;

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
