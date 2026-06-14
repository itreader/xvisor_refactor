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
 * @file fat_control.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief source file for FAT control functions
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_wall_clock.h>

#include "fat_control.h"

static int __fatfs_control_flush_fat_cache(fatfs_control_t *ctrl, uint32_t index)
{
    uint32_t i, sect_num;
    uint64_t fat_base, len;

    if (!ctrl->fat_cache_dirty[index]) {
        return VMM_OK;
    }

    for (i = 0; i < ctrl->number_of_fat; i++) {
        fat_base = ((uint64_t)ctrl->first_fat_sector + (i * ctrl->sectors_per_fat)) * ctrl->bytes_per_sector;
        sect_num = ctrl->fat_cache_num[index];
        len      = vmm_block_device_write(
            ctrl->block_device, &ctrl->fat_cache_buf[index * ctrl->bytes_per_sector], fat_base + sect_num * ctrl->bytes_per_sector,
            ctrl->bytes_per_sector);

        if (len != ctrl->bytes_per_sector) {
            return VMM_ERR_IO;
        }
    }

    ctrl->fat_cache_dirty[index] = FALSE;

    return VMM_OK;
}

static int __fatfs_control_find_fat_cache(fatfs_control_t *ctrl, uint32_t sect_num)
{
    int index;

    for (index = 0; index < FAT_TABLE_CACHE_SIZE; index++) {
        if (ctrl->fat_cache_num[index] == sect_num) {
            return index;
        }
    }

    return -1;
}

static int __fatfs_control_load_fat_cache(fatfs_control_t *ctrl, uint32_t sect_num)
{
    int      rc;
    uint32_t index;
    uint64_t fat_base, len;

    if (-1 < __fatfs_control_find_fat_cache(ctrl, sect_num)) {
        return VMM_OK;
    }

    index = ctrl->fat_cache_victim;
    ctrl->fat_cache_victim++;

    if (ctrl->fat_cache_victim == FAT_TABLE_CACHE_SIZE) {
        ctrl->fat_cache_victim = 0;
    }

    rc = __fatfs_control_flush_fat_cache(ctrl, index);

    if (rc) {
        return rc;
    }

    fat_base = (uint64_t)ctrl->first_fat_sector * ctrl->bytes_per_sector;
    len      = vmm_block_device_read(
        ctrl->block_device, &ctrl->fat_cache_buf[index * ctrl->bytes_per_sector], fat_base + sect_num * ctrl->bytes_per_sector,
        ctrl->bytes_per_sector);

    if (len != ctrl->bytes_per_sector) {
        return VMM_ERR_IO;
    }

    ctrl->fat_cache_num[index] = sect_num;

    return VMM_OK;
}

static uint32_t __fatfs_control_read_fat_cache(fatfs_control_t *ctrl, uint8_t *buf, uint32_t pos)
{
    int      rc, index;
    uint32_t ret, sect_num, sect_off;

    if ((ctrl->sectors_per_fat * ctrl->bytes_per_sector) <= pos) {
        return 0;
    }

    sect_num = udiv32(pos, ctrl->bytes_per_sector);
    sect_off = pos - (sect_num * ctrl->bytes_per_sector);

    rc       = __fatfs_control_load_fat_cache(ctrl, sect_num);

    if (rc) {
        return 0;
    }

    index = __fatfs_control_find_fat_cache(ctrl, sect_num);

    if (index < 0) {
        return 0;
    }

    index = (index * ctrl->bytes_per_sector) + sect_off;

    switch (ctrl->type) {
        case FAT_TYPE_12:
        case FAT_TYPE_16:
            ret    = 2;
            buf[0] = ctrl->fat_cache_buf[index + 0];
            buf[1] = ctrl->fat_cache_buf[index + 1];
            break;

        case FAT_TYPE_32:
            ret    = 4;
            buf[0] = ctrl->fat_cache_buf[index + 0];
            buf[1] = ctrl->fat_cache_buf[index + 1];
            buf[2] = ctrl->fat_cache_buf[index + 2];
            buf[3] = ctrl->fat_cache_buf[index + 3];
            break;

        default:
            ret = 0;
            break;
    };

    return ret;
}

static uint32_t __fatfs_control_write_fat_cache(fatfs_control_t *ctrl, uint8_t *buf, uint32_t pos)
{
    int      rc, index, cache_buf_index;
    uint32_t ret, sect_num, sect_off;

    if ((ctrl->sectors_per_fat * ctrl->bytes_per_sector) <= pos) {
        return 0;
    }

    sect_num = udiv32(pos, ctrl->bytes_per_sector);
    sect_off = pos - (sect_num * ctrl->bytes_per_sector);

    rc       = __fatfs_control_load_fat_cache(ctrl, sect_num);

    if (rc) {
        return 0;
    }

    index = __fatfs_control_find_fat_cache(ctrl, sect_num);

    if (index < 0) {
        return 0;
    }

    cache_buf_index = (index * ctrl->bytes_per_sector) + sect_off;

    switch (ctrl->type) {
        case FAT_TYPE_12:
        case FAT_TYPE_16:
            ret                                      = 2;
            ctrl->fat_cache_buf[cache_buf_index + 0] = buf[0];
            ctrl->fat_cache_buf[cache_buf_index + 1] = buf[1];
            break;

        case FAT_TYPE_32:
            ret                                      = 4;
            ctrl->fat_cache_buf[cache_buf_index + 0] = buf[0];
            ctrl->fat_cache_buf[cache_buf_index + 1] = buf[1];
            ctrl->fat_cache_buf[cache_buf_index + 2] = buf[2];
            ctrl->fat_cache_buf[cache_buf_index + 3] = buf[3];
            break;

        default:
            ret = 0;
            break;
    };

    ctrl->fat_cache_dirty[index] = TRUE;

    return ret;
}

static uint32_t __fatfs_control_first_valid_cluster(fatfs_control_t *ctrl)
{
    switch (ctrl->type) {
        case FAT_TYPE_12:
            return FAT12_RESERVED1_CLUSTER + 1;
            break;

        case FAT_TYPE_16:
            return FAT16_RESERVED1_CLUSTER + 1;
            break;

        case FAT_TYPE_32:
            return FAT32_RESERVED1_CLUSTER + 1;
            break;
    };

    return 0x0;
}

static uint32_t __fatfs_control_last_valid_cluster(fatfs_control_t *ctrl)
{
    switch (ctrl->type) {
        case FAT_TYPE_12:
            return FAT12_RESERVED2_CLUSTER - 1;
            break;

        case FAT_TYPE_16:
            return FAT16_RESERVED2_CLUSTER - 1;
            break;

        case FAT_TYPE_32:
            return FAT32_RESERVED2_CLUSTER - 1;
            break;
    };

    return 0x0;
}

static bool __fatfs_control_valid_cluster(fatfs_control_t *ctrl, uint32_t cl)
{
    switch (ctrl->type) {
        case FAT_TYPE_12:
            if ((cl <= FAT12_RESERVED1_CLUSTER) || (FAT12_RESERVED2_CLUSTER <= cl)) {
                return FALSE;
            }

            break;

        case FAT_TYPE_16:
            if ((cl <= FAT16_RESERVED1_CLUSTER) || (FAT16_RESERVED2_CLUSTER <= cl)) {
                return FALSE;
            }

            break;

        case FAT_TYPE_32:
            if ((cl <= FAT32_RESERVED1_CLUSTER) || (FAT32_RESERVED2_CLUSTER <= cl)) {
                return FALSE;
            }

            break;
    };

    return TRUE;
}

static int __fatfs_control_get_next_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t *next)
{
    uint8_t  fat_entry_b[4];
    uint32_t fat_entry, fat_off, fat_len, len;

    if (!__fatfs_control_valid_cluster(ctrl, clust)) {
        return VMM_ERR_INVALID;
    }

    switch (ctrl->type) {
        case FAT_TYPE_12:
            if (clust % 2) {
                fat_off = (clust - 1) * 12 / 8 + 1;
            } else {
                fat_off = clust * 12 / 8;
            }

            fat_len = 2;
            break;

        case FAT_TYPE_16:
            fat_off = clust * 2;
            fat_len = 2;
            break;

        case FAT_TYPE_32:
            fat_off = clust * 4;
            fat_len = 4;
            break;

        default:
            return VMM_ERR_NOENT;
    };

    len = __fatfs_control_read_fat_cache(ctrl, &fat_entry_b[0], fat_off);

    if (len != fat_len) {
        return VMM_ERR_IO;
    }

    if (fat_len == 2) {
        fat_entry = ((uint32_t)fat_entry_b[1] << 8) | ((uint32_t)fat_entry_b[0]);
    } else {
        fat_entry =
            ((uint32_t)fat_entry_b[3] << 24) | ((uint32_t)fat_entry_b[2] << 16) | ((uint32_t)fat_entry_b[1] << 8) | ((uint32_t)fat_entry_b[0]);
    }

    if (ctrl->type == FAT_TYPE_12) {
        if (clust % 2) {
            fat_entry >>= 4;
        } else {
            fat_entry &= 0xFFF;
        }
    }

    if (next) {
        *next = fat_entry;
    }

    return VMM_OK;
}

static int __fatfs_control_set_next_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t next)
{
    uint8_t  fat_entry_b[4];
    uint32_t fat_entry, fat_off, fat_len, len;

    if (!__fatfs_control_valid_cluster(ctrl, clust)) {
        return VMM_ERR_INVALID;
    }

    switch (ctrl->type) {
        case FAT_TYPE_12:
            if (clust % 2) {
                fat_off = (clust - 1) * 12 / 8 + 1;
            } else {
                fat_off = clust * 12 / 8;
            }

            fat_len = 2;
            len     = __fatfs_control_read_fat_cache(ctrl, &fat_entry_b[0], fat_off);

            if (len != fat_len) {
                return VMM_ERR_IO;
            }

            fat_entry = ((uint32_t)fat_entry_b[1] << 8) | fat_entry_b[0];

            if (clust % 2) {
                fat_entry &= ~0xFFF0;
                fat_entry |= (next & 0xFFF) << 4;
            } else {
                fat_entry &= ~0xFFF;
                fat_entry |= (next & 0xFFF);
            }

            fat_entry_b[1] = (fat_entry >> 8) & 0xFF;
            fat_entry_b[0] = (fat_entry) & 0xFF;
            break;

        case FAT_TYPE_16:
            fat_off        = clust * 2;
            fat_len        = 2;
            fat_entry      = next & 0xFFFF;
            fat_entry_b[1] = (fat_entry >> 8) & 0xFF;
            fat_entry_b[0] = (fat_entry) & 0xFF;
            break;

        case FAT_TYPE_32:
            fat_off        = clust * 4;
            fat_len        = 4;
            fat_entry      = next & 0xFFFFFFFF;
            fat_entry_b[3] = (fat_entry >> 24) & 0xFF;
            fat_entry_b[2] = (fat_entry >> 16) & 0xFF;
            fat_entry_b[1] = (fat_entry >> 8) & 0xFF;
            fat_entry_b[0] = (fat_entry) & 0xFF;
            break;

        default:
            return VMM_ERR_NOENT;
    };

    len = __fatfs_control_write_fat_cache(ctrl, &fat_entry_b[0], fat_off);

    if (len != fat_len) {
        return VMM_ERR_IO;
    }

    return VMM_OK;
}

static int __fatfs_control_set_last_cluster(fatfs_control_t *ctrl, uint32_t clust)
{
    int rc;

    if (!__fatfs_control_valid_cluster(ctrl, clust)) {
        return VMM_ERR_INVALID;
    }

    switch (ctrl->type) {
        case FAT_TYPE_12:
            rc = __fatfs_control_set_next_cluster(ctrl, clust, FAT12_LAST_CLUSTER);
            break;

        case FAT_TYPE_16:
            rc = __fatfs_control_set_next_cluster(ctrl, clust, FAT16_LAST_CLUSTER);
            break;

        case FAT_TYPE_32:
            rc = __fatfs_control_set_next_cluster(ctrl, clust, FAT32_LAST_CLUSTER);
            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    return rc;
}

static int __fatfs_control_nth_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t pos, uint32_t *next)
{
    int      rc;
    uint32_t i;

    if (next) {
        *next = clust;
    }

    if (!__fatfs_control_valid_cluster(ctrl, clust)) {
        return VMM_ERR_INVALID;
    }

    for (i = 0; i < pos; i++) {
        rc = __fatfs_control_get_next_cluster(ctrl, clust, &clust);

        if (rc) {
            return rc;
        }

        if (next) {
            *next = clust;
        }

        if (!__fatfs_control_valid_cluster(ctrl, clust)) {
            return VMM_ERR_INVALID;
        }
    }

    return VMM_OK;
}

static int __fatfs_control_alloc_first_cluster(fatfs_control_t *ctrl, uint32_t *newclust)
{
    int      rc;
    bool     found;
    uint32_t current, next, first, last;

    found = FALSE;
    first = __fatfs_control_first_valid_cluster(ctrl);
    last  = __fatfs_control_last_valid_cluster(ctrl);

    for (current = first; current <= last; current++) {
        rc = __fatfs_control_get_next_cluster(ctrl, current, &next);

        if (rc) {
            return rc;
        }

        if (next == 0x0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        return VMM_ERR_NOTAVAIL;
    }

    rc = __fatfs_control_set_last_cluster(ctrl, current);

    if (rc) {
        return rc;
    }

    if (newclust) {
        *newclust = current;
    }

    return VMM_OK;
}

static int __fatfs_control_append_free_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t *newclust)
{
    int      rc;
    bool     found;
    uint32_t current, next, first, last;

    if (!__fatfs_control_valid_cluster(ctrl, clust)) {
        return VMM_ERR_INVALID;
    }

    rc = __fatfs_control_get_next_cluster(ctrl, clust, &next);

    if (rc) {
        return rc;
    }

    while (__fatfs_control_valid_cluster(ctrl, next)) {
        clust = next;

        rc    = __fatfs_control_get_next_cluster(ctrl, clust, &next);

        if (rc) {
            return rc;
        }
    }

    found   = FALSE;
    first   = __fatfs_control_first_valid_cluster(ctrl);
    last    = __fatfs_control_last_valid_cluster(ctrl);
    current = clust + 1;

    while (1) {
        if (clust == current) {
            break;
        }

        rc = __fatfs_control_get_next_cluster(ctrl, current, &next);

        if (rc) {
            return rc;
        }

        if (next == 0x0) {
            found = TRUE;
            break;
        }

        current++;

        if (current > last) {
            current = first;
        }
    }

    if (!found) {
        return VMM_ERR_NOTAVAIL;
    }

    rc = __fatfs_control_set_last_cluster(ctrl, current);

    if (rc) {
        return rc;
    }

    rc = __fatfs_control_set_next_cluster(ctrl, clust, current);

    if (rc) {
        return rc;
    }

    if (newclust) {
        *newclust = clust;
    }

    return VMM_OK;
}

static int __fatfs_control_truncate_clusters(fatfs_control_t *ctrl, uint32_t clust)
{
    int      rc;
    uint32_t current, next = clust;

    while (__fatfs_control_valid_cluster(ctrl, next)) {
        current = next;

        rc      = __fatfs_control_get_next_cluster(ctrl, current, &next);

        if (rc) {
            return rc;
        }

        rc = __fatfs_control_set_next_cluster(ctrl, current, 0x0);

        if (rc) {
            return rc;
        }
    }

    return VMM_OK;
}

uint32_t fatfs_pack_timestamp(uint32_t year, uint32_t mon, uint32_t day, uint32_t hour, uint32_t min, uint32_t sec)
{
    return (uint32_t)vmm_wall_clock_mktime(1980 + year, mon, day, hour, min, sec);
}

void fatfs_current_timestamp(uint32_t *year, uint32_t *mon, uint32_t *day, uint32_t *hour, uint32_t *min, uint32_t *sec)

{
    vmm_time_value_t tv;
    vmm_time_info_t  ti;

    vmm_wall_clock_get_local_time(&tv);
    vmm_wall_clock_mkinfo(tv.tv_sec, 0, &ti);

    if (year) {
        *year = ti.tm_year + 1900 - 1980;
    }

    if (mon) {
        *mon = ti.tm_month;
    }

    if (day) {
        *day = ti.tm_month_of_day;
    }

    if (hour) {
        *hour = ti.tm_hour;
    }

    if (min) {
        *min = ti.tm_minute;
    }

    if (sec) {
        *sec = ti.tm_second;
    }
}

bool fatfs_control_valid_cluster(fatfs_control_t *ctrl, uint32_t clust)
{
    return __fatfs_control_valid_cluster(ctrl, clust);
}

int fatfs_control_nth_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t pos, uint32_t *next)
{
    int rc;

    vmm_mutex_lock(&ctrl->fat_cache_lock);
    rc = __fatfs_control_nth_cluster(ctrl, clust, pos, next);
    vmm_mutex_unlock(&ctrl->fat_cache_lock);

    return rc;
}

int fatfs_control_set_last_cluster(fatfs_control_t *ctrl, uint32_t clust)
{
    int rc;

    vmm_mutex_lock(&ctrl->fat_cache_lock);
    rc = __fatfs_control_set_last_cluster(ctrl, clust);
    vmm_mutex_unlock(&ctrl->fat_cache_lock);

    return rc;
}

int fatfs_control_alloc_first_cluster(fatfs_control_t *ctrl, uint32_t *newclust)
{
    int rc;

    vmm_mutex_lock(&ctrl->fat_cache_lock);
    rc = __fatfs_control_alloc_first_cluster(ctrl, newclust);
    vmm_mutex_unlock(&ctrl->fat_cache_lock);

    return rc;
}

int fatfs_control_append_free_cluster(fatfs_control_t *ctrl, uint32_t clust, uint32_t *newclust)
{
    int rc;

    vmm_mutex_lock(&ctrl->fat_cache_lock);
    rc = __fatfs_control_append_free_cluster(ctrl, clust, newclust);
    vmm_mutex_unlock(&ctrl->fat_cache_lock);

    return rc;
}

int fatfs_control_truncate_clusters(fatfs_control_t *ctrl, uint32_t clust)
{
    int rc;

    vmm_mutex_lock(&ctrl->fat_cache_lock);
    rc = __fatfs_control_truncate_clusters(ctrl, clust);
    vmm_mutex_unlock(&ctrl->fat_cache_lock);

    return rc;
}

int fatfs_control_sync(fatfs_control_t *ctrl)
{
    int rc, index;

    /* Flush entire FAT sector cache */
    vmm_mutex_lock(&ctrl->fat_cache_lock);

    for (index = 0; index < FAT_TABLE_CACHE_SIZE; index++) {
        rc = __fatfs_control_flush_fat_cache(ctrl, index);

        if (rc) {
            vmm_mutex_unlock(&ctrl->fat_cache_lock);
            return rc;
        }
    }

    vmm_mutex_unlock(&ctrl->fat_cache_lock);

    /* Flush cached data in device request queue */
    rc = vmm_block_device_flush_cache(ctrl->block_device);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}

int fatfs_control_init(fatfs_control_t *ctrl, vmm_block_device_t *block_device)
{
    uint32_t           i;
    uint64_t           rlen;
    fat_boot_sector_t *bsec = &ctrl->bsec;

    /* Read boot sector from block device */
    rlen                    = vmm_block_device_read(block_device, (uint8_t *)bsec, FAT_BOOTSECTOR_OFFSET, sizeof(fat_boot_sector_t));

    if (rlen != sizeof(fat_boot_sector_t)) {
        return VMM_ERR_IO;
    }

    /* Save underlying block device pointer */
    ctrl->block_device        = block_device;

    /* Get bytes_per_sector and sector_per_cluster */
    ctrl->bytes_per_sector    = __le16(bsec->bytes_per_sector);
    ctrl->sectors_per_cluster = bsec->sectors_per_cluster;

    /* Sanity check bytes_per_sector and sector_per_cluster */
    if (!ctrl->bytes_per_sector || !ctrl->sectors_per_cluster) {
        return VMM_ERR_NOSYS;
    }

    /* Frequently required info */
    ctrl->number_of_fat     = bsec->number_of_fat;
    ctrl->bytes_per_cluster = ctrl->sectors_per_cluster * ctrl->bytes_per_sector;
    ctrl->total_sectors     = __le16(bsec->total_sectors_16);

    if (!ctrl->total_sectors) {
        ctrl->total_sectors = __le32(bsec->total_sectors_32);
    }

    /* Calculate derived info assuming FAT12/FAT16 */
    ctrl->first_fat_sector   = __le16(bsec->reserved_sector_count);
    ctrl->sectors_per_fat    = __le16(bsec->sectors_per_fat);
    ctrl->fat_sectors        = ctrl->number_of_fat * ctrl->sectors_per_fat;

    ctrl->first_root_sector  = ctrl->first_fat_sector + ctrl->fat_sectors;
    ctrl->root_sectors       = (__le16(bsec->root_entry_count) * 32) + (ctrl->bytes_per_sector - 1);
    ctrl->root_sectors       = udiv32(ctrl->root_sectors, ctrl->bytes_per_sector);
    ctrl->first_root_cluster = 0;

    ctrl->first_data_sector  = ctrl->first_root_sector + ctrl->root_sectors;
    ctrl->data_sectors       = ctrl->total_sectors - ctrl->first_data_sector;
    ctrl->data_clusters      = udiv32(ctrl->data_sectors, ctrl->sectors_per_cluster);

    /* Determine FAT type */
    if (ctrl->data_clusters < 4085) {
        ctrl->type = FAT_TYPE_12;
    } else if (ctrl->data_clusters < 65525) {
        ctrl->type = FAT_TYPE_16;
    } else {
        ctrl->type = FAT_TYPE_32;
    }

    /* FAT type sanity check */
    switch (ctrl->type) {
        case FAT_TYPE_12:
            if (memcmp(bsec->ext.e16.fs_type, "FAT12", 5)) {
                return VMM_ERR_NOSYS;
            }

            break;

        case FAT_TYPE_16:
            if (memcmp(bsec->ext.e16.fs_type, "FAT16", 5)) {
                return VMM_ERR_NOSYS;
            }

            break;

        case FAT_TYPE_32:
            if (memcmp(bsec->ext.e32.fs_type, "FAT32", 5)) {
                return VMM_ERR_NOSYS;
            }

            break;

        default:
            return VMM_ERR_NOSYS;
    }

    /* For FAT32, recompute derived info */
    if (ctrl->type == FAT_TYPE_32) {
        ctrl->first_fat_sector   = __le16(bsec->reserved_sector_count);
        ctrl->sectors_per_fat    = __le32(bsec->ext.e32.sectors_per_fat);
        ctrl->fat_sectors        = ctrl->number_of_fat * ctrl->sectors_per_fat;

        ctrl->first_root_sector  = 0;
        ctrl->root_sectors       = 0;
        ctrl->first_root_cluster = __le32(bsec->ext.e32.root_directory_cluster);

        ctrl->first_data_sector  = ctrl->first_fat_sector + ctrl->fat_sectors;
        ctrl->data_sectors       = ctrl->total_sectors - ctrl->first_data_sector;
        ctrl->data_clusters      = udiv32(ctrl->data_sectors, ctrl->sectors_per_cluster);
    }

    /* Initialize fat cache */
    INIT_MUTEX(&ctrl->fat_cache_lock);
    ctrl->fat_cache_victim = 0;

    for (i = 0; i < FAT_TABLE_CACHE_SIZE; i++) {
        ctrl->fat_cache_dirty[i] = FALSE;
        ctrl->fat_cache_num[i]   = i;
    }

    ctrl->fat_cache_buf = vmm_zalloc(FAT_TABLE_CACHE_SIZE * ctrl->bytes_per_sector);

    if (!ctrl->fat_cache_buf) {
        return VMM_ERR_NOMEM;
    }

    /* Load fat cache */
    rlen = vmm_block_device_read(
        ctrl->block_device, ctrl->fat_cache_buf, ctrl->first_fat_sector * ctrl->bytes_per_sector, FAT_TABLE_CACHE_SIZE * ctrl->bytes_per_sector);

    if (rlen != (FAT_TABLE_CACHE_SIZE * ctrl->bytes_per_sector)) {
        vmm_free(ctrl->fat_cache_buf);
        return VMM_ERR_IO;
    }

    return VMM_OK;
}

int fatfs_control_exit(fatfs_control_t *ctrl)
{
    vmm_free(ctrl->fat_cache_buf);

    return VMM_OK;
}
