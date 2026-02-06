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
 * @file fat_control.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for FAT control functions
 */
#ifndef _FAT_CONTROL_H__
#define _FAT_CONTROL_H__

#include <block/vmm_block_device.h>
#include <vmm_host_io.h>
#include <vmm_mutex.h>

#include "fat_common.h"

#define __le32(x)            vmm_le32_to_cpu(x)
#define __le16(x)            vmm_le16_to_cpu(x)

#define FAT_TABLE_CACHE_SIZE 32

/* Information about a "mounted" FAT filesystem. */
typedef struct fatfs_control {
    /* FAT boot sector */
    fat_boot_sector_t bsec;

    /* Underlying block device */
    vmm_block_device_t *block_device;

    /* Frequently required boot sector info */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint8_t  number_of_fat;
    uint32_t bytes_per_cluster;
    uint32_t total_sectors;

    /* Derived FAT info */
    uint32_t first_fat_sector;
    uint32_t sectors_per_fat;
    uint32_t fat_sectors;

    uint32_t first_root_sector;
    uint32_t root_sectors;
    uint32_t first_root_cluster;

    uint32_t first_data_sector;
    uint32_t data_sectors;
    uint32_t data_clusters;

    /* FAT type (i.e. FAT12/FAT16/FAT32) */
    enum fat_types type;

    /* FAT sector cache */
    vmm_mutex_t fat_cache_lock;
    uint32_t    fat_cache_victim;
    bool        fat_cache_dirty[FAT_TABLE_CACHE_SIZE];
    uint32_t    fat_cache_num[FAT_TABLE_CACHE_SIZE];
    uint8_t    *fat_cache_buf;
} fatfs_control_t;

uint32_t fatfs_pack_timestamp(uint32_t year, uint32_t mon, uint32_t day, uint32_t hour, uint32_t min, uint32_t sec);

void fatfs_current_timestamp(uint32_t *year, uint32_t *mon, uint32_t *day, uint32_t *hour, uint32_t *min, uint32_t *sec);

bool fatfs_control_valid_cluster(fatfs_control_t *ctrl, uint32_t clust);

int fatfs_control_nth_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t pos, uint32_t *next);

int fatfs_control_set_last_cluster(fatfs_control_t *ctrl, uint32_t clust);

int fatfs_control_alloc_first_cluster(fatfs_control_t *ctrl, uint32_t *newclust);

int fatfs_control_append_free_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t *newclust);

int fatfs_control_truncate_clusters(fatfs_control_t *ctrl, uint32_t clust);

int fatfs_control_sync(fatfs_control_t *ctrl);

int fatfs_control_init(fatfs_control_t *ctrl, vmm_block_device_t *block_device);

int fatfs_control_exit(fatfs_control_t *ctrl);

#endif
