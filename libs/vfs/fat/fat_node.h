/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file fat_node.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for FAT node functions
 */
#ifndef _FAT_NODE_H__
#define _FAT_NODE_H__

#include <vmm_types.h>

#include "fat_common.h"

#define FAT_NODE_LOOKUP_SIZE 4

/* Information for accessing a FAT file/directory. */
struct fatfs_node {
    /* Parent FAT control */
    fatfs_control_t *ctrl;

    /* Parent directory entry */
    struct fatfs_node         *parent;
    uint32_t                   parent_dent_off;
    uint32_t                   parent_dent_len;
    struct fat_directory_entry parent_dent;
    bool                       parent_dent_dirty;

    /* First cluster */
    uint32_t first_cluster;

    /* Cached clusters */
    uint8_t *cached_data;
    uint32_t cached_clust;
    bool     cached_dirty;

    /* Child directory entry lookup table */
    uint32_t                   lookup_victim;
    char                       lookup_name[FAT_NODE_LOOKUP_SIZE][VFS_MAX_NAME];
    uint32_t                   lookup_off[FAT_NODE_LOOKUP_SIZE];
    uint32_t                   lookup_len[FAT_NODE_LOOKUP_SIZE];
    struct fat_directory_entry lookup_dent[FAT_NODE_LOOKUP_SIZE];
};

uint32_t fatfs_node_get_size(struct fatfs_node *node);

uint32_t fatfs_node_read(struct fatfs_node *node, uint32_t pos, uint32_t len, uint8_t *buf);

uint32_t fatfs_node_write(struct fatfs_node *node, uint32_t pos, uint32_t len, uint8_t *buf);

int fatfs_node_truncate(struct fatfs_node *node, uint32_t pos);

int fatfs_node_sync(struct fatfs_node *node);

int fatfs_node_init(fatfs_control_t *ctrl, struct fatfs_node *node);

int fatfs_node_exit(struct fatfs_node *node);

int fatfs_node_read_dirent(struct fatfs_node *dnode, loff_t off, struct dirent *d);

int fatfs_node_find_dirent(struct fatfs_node *dnode, const char *name, struct fat_directory_entry *dent, uint32_t *dent_off, uint32_t *dent_len);

int fatfs_node_add_dirent(struct fatfs_node *dnode, const char *name, struct fat_directory_entry *ndent);

int fatfs_node_del_dirent(struct fatfs_node *dnode, const char *name, uint32_t dent_off, uint32_t dent_len);

#endif
