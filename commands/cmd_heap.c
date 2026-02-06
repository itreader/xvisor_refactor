/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_heap.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief command for heap status.
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command heap"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_heap_init
#define MODULE_EXIT      cmd_heap_exit

static void cmd_heap_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   heap help\n");
    vmm_cdev_printf(cdev, "   heap info\n");
    vmm_cdev_printf(cdev, "   heap state\n");
    vmm_cdev_printf(cdev, "   heap dma_info\n");
    vmm_cdev_printf(cdev, "   heap dma_state\n");
}

static int heap_info(vmm_char_device_t *cdev, bool is_normal, virtual_addr_t heap_va, uint64_t heap_sz, uint64_t heap_hksz, uint64_t heap_freesz)
{
    int             rc;
    physical_addr_t heap_pa;
    uint64_t        pre, heap_usesz;

    if (is_normal) {
        heap_usesz = heap_sz - heap_hksz - heap_freesz;
    } else {
        heap_usesz = heap_sz - heap_freesz;
    }

    if ((rc = vmm_host_va2pa(heap_va, &heap_pa))) {
        vmm_cdev_printf(cdev, "Error: Failed to get heap base PA\n");
        return rc;
    }

    vmm_cdev_printf(cdev, "Base Virtual Addr  : ");
    vmm_cdev_printf(cdev, "0x%" PRIADDR "\n", heap_va);

    vmm_cdev_printf(cdev, "Base Physical Addr : ");
    vmm_cdev_printf(cdev, "0x%" PRIPADDR "\n", heap_pa);

    pre = 1000; /* Division correct upto 3 decimal points */

    vmm_cdev_printf(cdev, "House-Keeping Size : ");
    heap_hksz = (heap_hksz * pre) >> 10;
    vmm_cdev_printf(cdev, "%" PRId64 ".%03" PRId64 " KB\n", udiv64(heap_hksz, pre), umod64(heap_hksz, pre));

    vmm_cdev_printf(cdev, "Used Space Size    : ");
    heap_usesz = (heap_usesz * pre) >> 10;
    vmm_cdev_printf(cdev, "%" PRId64 ".%03" PRId64 " KB\n", udiv64(heap_usesz, pre), umod64(heap_usesz, pre));

    vmm_cdev_printf(cdev, "Free Space Size    : ");
    heap_freesz = (heap_freesz * pre) >> 10;
    vmm_cdev_printf(cdev, "%" PRId64 ".%03" PRId64 " KB\n", udiv64(heap_freesz, pre), umod64(heap_freesz, pre));

    vmm_cdev_printf(cdev, "Total Size         : ");
    heap_sz = (heap_sz * pre) >> 10;
    vmm_cdev_printf(cdev, "%" PRId64 ".%03" PRId64 " KB\n", udiv64(heap_sz, pre), umod64(heap_sz, pre));

    return VMM_OK;
}

static int cmd_heap_info(vmm_char_device_t *cdev)
{
    return heap_info(cdev, TRUE, vmm_normal_heap_start_va(), vmm_normal_heap_size(), vmm_normal_heap_hksize(), vmm_normal_heap_free_size());
}

static int cmd_heap_state(vmm_char_device_t *cdev)
{
    return vmm_normal_heap_print_state(cdev);
}

static int cmd_heap_dma_info(vmm_char_device_t *cdev)
{
    return heap_info(cdev, FALSE, vmm_dma_heap_start_va(), vmm_dma_heap_size(), vmm_dma_heap_hksize(), vmm_dma_heap_free_size());
}

static int cmd_heap_dma_state(vmm_char_device_t *cdev)
{
    return vmm_dma_heap_print_state(cdev);
}

static int cmd_heap_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_heap_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "info") == 0) {
            return cmd_heap_info(cdev);
        } else if (strcmp(argv[1], "state") == 0) {
            return cmd_heap_state(cdev);
        } else if (strcmp(argv[1], "dma_info") == 0) {
            return cmd_heap_dma_info(cdev);
        } else if (strcmp(argv[1], "dma_state") == 0) {
            return cmd_heap_dma_state(cdev);
        }
    }

    cmd_heap_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_heap = {
    .name  = "heap",
    .desc  = "show heap status",
    .usage = cmd_heap_usage,
    .exec  = cmd_heap_exec,
};

static int __init cmd_heap_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_heap);
}

static void __exit cmd_heap_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_heap);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
