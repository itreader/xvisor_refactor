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
 * @file ext4_control.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for Ext4 control functions
 */
#ifndef _EXT4_CONTROL_H__
#define _EXT4_CONTROL_H__

#include <block/vmm_block_device.h>
#include <vmm_host_io.h>
#include <vmm_mutex.h>

#include "ext4_common.h"

#define __le32(x) vmm_le32_to_cpu(x)
#define __le16(x) vmm_le16_to_cpu(x)

/* Information for accessing block groups. */
struct ext4fs_group {
    /* lock to protect group */
    vmm_mutex_t             grp_lock;
    struct ext2_block_group grp;

    uint8_t *block_bmap;
    uint8_t *inode_bmap;

    bool grp_dirty;
};

/* Information about a "mounted" ext filesystem. */
struct ext4fs_control {
    vmm_block_device_t *block_device;

    /* lock to protect:
     * sblock.free_blocks,
     * sblock.free_inodes,
     * sblock.mtime,
     * sblock.utime,
     * sblock_dirty
     */
    vmm_mutex_t        sblock_lock;
    struct ext2_sblock sblock;

    /* flag to show whether sblock,
     * groups, or bitmaps are updated.
     */
    bool sblock_dirty;

    uint32_t log2_block_size;
    uint32_t block_size;
    uint32_t dir_blocklast;
    uint32_t indir_blocklast;
    uint32_t dindir_blocklast;

    uint32_t inode_size;
    uint32_t inodes_per_block;

    uint32_t             group_count;
    uint32_t             group_table_blockno;
    struct ext4fs_group *groups;
};

uint32_t ext4fs_current_timestamp(void);

int ext4fs_devread(struct ext4fs_control *ctrl, uint32_t blkno, uint32_t blkoff, uint32_t buf_len, char *buf);

int ext4fs_devwrite(struct ext4fs_control *ctrl, uint32_t blkno, uint32_t blkoff, uint32_t buf_len, char *buf);

int ext4fs_control_read_inode(struct ext4fs_control *ctrl, uint32_t inode_no, struct ext2_inode *inode);

int ext4fs_control_write_inode(struct ext4fs_control *ctrl, uint32_t inode_no, struct ext2_inode *inode);

int ext4fs_control_alloc_block(struct ext4fs_control *ctrl, uint32_t inode_no, uint32_t *blkno);

int ext4fs_control_free_block(struct ext4fs_control *ctrl, uint32_t blkno);

int ext4fs_control_alloc_inode(struct ext4fs_control *ctrl, uint32_t parent_inode_no, uint32_t *inode_no);

int ext4fs_control_free_inode(struct ext4fs_control *ctrl, uint32_t inode_no);

int ext4fs_control_sync(struct ext4fs_control *ctrl);

int ext4fs_control_init(struct ext4fs_control *ctrl, vmm_block_device_t *block_device);

int ext4fs_control_exit(struct ext4fs_control *ctrl);

#endif
