/**
 * Copyright (c) 2018 Anup Patel.
 * All rights reserved.
 *
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for board information implementation.
 *
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file brd_main.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic board support
 */

#include <arch_board.h>
#include <libs/video_terminal_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_platform.h>
#include <vmm_stdio.h>

#include <generic_board.h>

#include <linux/clk-provider.h>

/*
 * Global board context
 */

#if defined(CONFIG_VIDEO_TERMINAL_EMULATE)
struct video_terminal_emulate *generic_vt;
#endif

static const struct vmm_device_tree_nodeid *generic_board_matches;

/*
 * Print board information
 */

static void generic_board_print_info(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    const struct generic_board *brd  = match->data;
    vmm_char_device_t          *cdev = data;

    if (!brd || !brd->print_info) {
        return;
    }

    brd->print_info(cdev);
}

void arch_board_print_info(vmm_char_device_t *cdev)
{
    if (generic_board_matches) {
        vmm_device_tree_iterate_matching(NULL, generic_board_matches, generic_board_print_info, cdev);
    }
}

/*
 * Initialization functions
 */

int __init arch_board_nascent_init(void)
{
    /* Host addr_space, Heap, and Device tree available. */

    /* Nothing to do here. */

    return 0;
}

static void __init generic_board_early(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int                         err;
    const struct generic_board *brd = match->data;

    if (!brd || !brd->early_init) {
        return;
    }

    err = brd->early_init(node);

    if (err) {
        vmm_printf("%s: Early init %s node failed (error %d)\n", __func__, node->name, err);
    }
}

int __init arch_board_early_init(void)
{
    /* Host addr_space, Heap, Device tree, and Host IRQ available.
     *
     * Do necessary early stuff like:
     * iomapping devices,
     * SOC clocking init,
     * Setting-up system data in device tree nodes,
     * ....
     */

    /* Determine generic board matches from nodeid table */
    generic_board_matches = vmm_device_tree_nidtable_create_matches("generic_board");

    /* Early init of generic boards with
     * matching nodeid table enteries.
     */
    if (generic_board_matches) {
        vmm_device_tree_iterate_matching(NULL, generic_board_matches, generic_board_early, NULL);
    }

    /* Initialize clocking framework */
    of_clock_init(NULL);

    return VMM_OK;
}

static void __init generic_board_final(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int                         err;
    const struct generic_board *brd = match->data;

    if (!brd || !brd->final_init) {
        return;
    }

    err = brd->final_init(node);

    if (err) {
        vmm_printf("%s: Final init %s node failed (error %d)\n", __func__, node->name, err);
    }
}

int __init arch_board_final_init(void)
{
    int                     rc;
    vmm_device_tree_node_t *node;
    vmm_device_tree_node_t *root;
#if defined(CONFIG_VIDEO_TERMINAL_EMULATE)
    struct frame_buffer_info *info;
#endif

    /* All VMM API's are available here */
    /* We can register a Board specific resource here */

    root = vmm_device_tree_getnode("/");

    vmm_device_tree_for_each_child(node, root)
    {
        /* check if node has compatible attribute */
        if (!vmm_device_tree_getattr(node, VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME)) {
            continue;
        }

        /* Do platform device probing using device driver framework */
        rc = vmm_platform_probe(node);

        if (rc) {
            vmm_device_tree_dref_node(node);
            return rc;
        }
    }

    vmm_device_tree_dref_node(root);

    /* Create VIDEO_TERMINAL_EMULATE instace if available */
#if defined(CONFIG_VIDEO_TERMINAL_EMULATE)
    info = fb_find("fb0");

    if (info) {
        generic_vt = video_terminal_emulate_create(info->name, info, NULL);
    }

#endif

    /* Final init of generic boards with
     * matching nodeid table enteries.
     */
    if (generic_board_matches) {
        vmm_device_tree_iterate_matching(NULL, generic_board_matches, generic_board_final, NULL);
    }

    return VMM_OK;
}
