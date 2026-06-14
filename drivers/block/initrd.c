/**
 * Copyright (c) 2015 Jean-Christophe Dubois.
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
 * @file initrd.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief initrd block device driver.
 */

#include <drv/initrd.h>
#include <drv/ram_backed_device.h>
#include <vmm_device_driver.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "INITRD Driver"
#define MODULE_AUTHOR    "Jean-Christophe Dubois"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY INITRD_IPRIORITY
#define MODULE_INIT      initrd_driver_init
#define MODULE_EXIT      initrd_driver_exit

static struct ram_backed_device *initrd_rbd;

void initrd_ram_backed_device_destroy(void)
{
    if (initrd_rbd) {
        ram_backed_device_destroy(initrd_rbd);
        initrd_rbd = NULL;
    }
}

VMM_ERR_XPORT_SYMBOL(initrd_ram_backed_device_destroy);

struct ram_backed_device *initrd_ram_backed_device_get(void)
{
    return initrd_rbd;
}

VMM_ERR_XPORT_SYMBOL(initrd_ram_backed_device_get);

int initrd_device_tree_update(uint64_t start, uint64_t end)
{
    int                     rc = VMM_OK;
    vmm_device_tree_node_t *node;

    /* Sanity checks */
    if (start >= end) {
        return VMM_ERR_INVALID;
    }

    if (initrd_rbd) {
        return VMM_ERR_BUSY;
    }

    /* There should be a /chosen node */
    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_CHOSEN_NODE_NAME);

    if (!node) {
        return VMM_ERR_NODEV;
    }

    /* Update start attribute in /chosen node */
    rc = vmm_device_tree_setattr(node, INITRD_START_ATTR2_NAME, &start, VMM_DEVICE_TREE_ATTRTYPE_UINT64, sizeof(start), FALSE);

    if (rc) {
        goto done;
    }

    /* Update end attribute in /chosen node */
    rc = vmm_device_tree_setattr(node, INITRD_END_ATTR2_NAME, &end, VMM_DEVICE_TREE_ATTRTYPE_UINT64, sizeof(end), FALSE);

    if (rc) {
        goto done;
    }

done:
    vmm_device_tree_dref_node(node);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(initrd_device_tree_update);

static int __init initrd_driver_init(void)
{
    vmm_device_tree_node_t *node;
    uint64_t                initrd_start, initrd_end;

    /* There should be a /chosen node */
    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_CHOSEN_NODE_NAME);

    if (!node) {
        vmm_printf("initrd: No chosen node\n");
        return VMM_ERR_NODEV;
    }

    /* Is there a start attribute */
    if (vmm_device_tree_read_u64(node, INITRD_START_ATTR_NAME, &initrd_start) != VMM_OK) {
        if (vmm_device_tree_read_u64(node, INITRD_START_ATTR2_NAME, &initrd_start) != VMM_OK) {
            vmm_printf("initrd: %s/%s attribute not found\n", INITRD_START_ATTR_NAME, INITRD_START_ATTR2_NAME);
            goto error;
        }
    }

    /* If so is there also a end attribte */
    if (vmm_device_tree_read_u64(node, INITRD_END_ATTR_NAME, &initrd_end) != VMM_OK) {
        if (vmm_device_tree_read_u64(node, INITRD_END_ATTR2_NAME, &initrd_end) != VMM_OK) {
            vmm_printf("initrd: %s/%s attribute not found\n", INITRD_END_ATTR_NAME, INITRD_END_ATTR2_NAME);
            goto error;
        }
    }

    /* Let's do a little bit os sanity check */
    if (initrd_end <= initrd_start) {
        vmm_printf("initrd: error: initrd_start > initrd_end\n");
        goto error;
    }

    /* OK, we know where the initrd device is located */
    if ((initrd_rbd = ram_backed_device_create("initrd", (physical_addr_t)initrd_start, (physical_size_t)(initrd_end - initrd_start), true)) ==
        NULL) {
        vmm_printf("initrd: ram_backed_device_create() failed\n");
        goto error;
    }

    vmm_printf("initrd: RBD created at 0x%" PRIx64 " - 0x%" PRIx64 "\n", initrd_start, initrd_end);

error:
    vmm_device_tree_dref_node(node);

    return VMM_OK;
}

static void __exit initrd_driver_exit(void)
{
    initrd_ram_backed_device_destroy();
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
