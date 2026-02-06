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
 * @file fat_common.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common structures and defines for FAT12, FAT16, and FAT32
 */
#ifndef _FAT_COMMON_H__
#define _FAT_COMMON_H__

#include <libs/vfs.h>
#include <vmm_types.h>

/* Important offsets */
#define FAT_BOOTSECTOR_OFFSET 0x000

/* Enumeration of possible values for Media Type field in boot sector */
enum fat_media_types {
    FAT_DOUBLE_SIDED_1_44_MB = 0xF0,
    FAT_FIXED_DISK           = 0xF8,
    FAT_DOUBLE_SIDED_720_KB  = 0xF9,
    FAT_SINGLE_SIDED_320_KB  = 0xFA,
    FAT_DOUBLE_SIDED_640_KB  = 0xFB,
    FAT_SINGLE_SIDED_180_KB  = 0xFC,
    FAT_DOUBLE_SIDED_360_KB  = 0xFD,
    FAT_SINGLE_SIDED_160_KB  = 0xFE,
    FAT_DOUBLE_SIDED_320_KB  = 0xFF
};

/* Enumeration of FAT types */
enum fat_types {
    FAT_TYPE_12 = 12,
    FAT_TYPE_16 = 16,
    FAT_TYPE_32 = 32
};

/* Enumeration of types of cluster in FAT12 table */
enum fat12_cluster_types {
    FAT12_FREE_CLUSTER      = 0x000,
    FAT12_RESERVED1_CLUSTER = 0x001,
    FAT12_RESERVED2_CLUSTER = 0xFF0,
    FAT12_BAD_CLUSTER       = 0xFF7,
    FAT12_LAST_CLUSTER      = 0xFF8
};

/* Enumeration of types of cluster in FAT16 table */
enum fat16_cluster_types {
    FAT16_FREE_CLUSTER      = 0x0000,
    FAT16_RESERVED1_CLUSTER = 0x0001,
    FAT16_RESERVED2_CLUSTER = 0xFFF0,
    FAT16_BAD_CLUSTER       = 0xFFF7,
    FAT16_LAST_CLUSTER      = 0xFFF8
};

/* Enumeration of types of cluster in FAT32 table */
enum fat32_cluster_types {
    FAT32_FREE_CLUSTER      = 0x00000000,
    FAT32_RESERVED1_CLUSTER = 0x00000001,
    FAT32_RESERVED2_CLUSTER = 0x0FFFFFF0,
    FAT32_BAD_CLUSTER       = 0x0FFFFFF7,
    FAT32_LAST_CLUSTER      = 0x0FFFFFF8
};

/* Extended boot sector information for FAT12/FAT16 */
struct fat_boot_sector_ext16 {
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  extended_signature;
    uint32_t serial_number;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[448];
    uint16_t boot_sector_signature;
} __packed;

/* Extended boot sector information for FAT32 */
struct fat_boot_sector_ext32 {
    uint32_t sectors_per_fat;
    uint16_t fat_flags;
    uint16_t version;
    uint32_t root_directory_cluster;
    uint16_t fs_info_sector;
    uint16_t boot_sector_copy;
    uint8_t  reserved1[12];
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  extended_signature;
    uint32_t serial_number;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[420];
    uint16_t boot_sector_signature;
} __packed;

/* Boot sector information for FAT12/FAT16/FAT32 */
typedef struct fat_boot_sector {
    uint8_t  jump[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  number_of_fat;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sector_count;
    uint32_t total_sectors_32;

    union {
        struct fat_boot_sector_ext16 e16;
        struct fat_boot_sector_ext32 e32;
    } ext;
} fat_boot_sector_t __packed;

/* Directory entry attributes */
#define FAT_DIRENT_READONLY 0x01
#define FAT_DIRENT_HIDDEN   0x02
#define FAT_DIRENT_SYSTEM   0x04
#define FAT_DIRENT_VOLLABLE 0x08
#define FAT_DIRENT_SUBDIR   0x10
#define FAT_DIRENT_ARCHIVE  0x20
#define FAT_DIRENT_DEVICE   0x40
#define FAT_DIRENT_UNUSED   0x80

/* Directory entry information for FAT12/FAT16/FAT32 */
struct fat_directory_entry {
    uint8_t  dos_file_name[8];
    uint8_t  dos_extension[3];
    uint8_t  file_attributes;
    uint8_t  reserved;
    uint8_t  create_time_millisecs;
    uint32_t create_time_seconds : 5;
    uint32_t create_time_minutes : 6;
    uint32_t create_time_hours   : 5;
    uint32_t create_date_day     : 5;
    uint32_t create_date_month   : 4;
    uint32_t create_date_year    : 7;
    uint32_t laccess_date_day    : 5;
    uint32_t laccess_date_month  : 4;
    uint32_t laccess_date_year   : 7;
    uint16_t first_cluster_hi; /* For FAT16 this is ea_index */
    uint32_t lmodify_time_seconds : 5;
    uint32_t lmodify_time_minutes : 6;
    uint32_t lmodify_time_hours   : 5;
    uint32_t lmodify_date_day     : 5;
    uint32_t lmodify_date_month   : 4;
    uint32_t lmodify_date_year    : 7;
    uint16_t first_cluster_lo; /* For FAT16 first_cluster = first_cluster_lo */
    uint32_t file_size;
} __packed;

#define FAT_LONGNAME_ATTRIBUTE    0x0F
#define FAT_LONGNAME_LASTSEQ_MASK 0x40
#define FAT_LONGNAME_SEQNO(s)     ((s) & ~0x40)
#define FAT_LONGNAME_LASTSEQ(s)   ((s) & 0x40)
#define FAT_LONGNAME_MINSEQ       1
#define FAT_LONGNAME_MAXSEQ       (VFS_MAX_NAME / 13)

/* Directory long filename information for FAT12/FAT16/FAT32 */
struct fat_longname {
    uint8_t  seqno;
    uint16_t name_utf16_1[5];
    uint8_t  file_attributes;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name_utf16_2[6];
    uint16_t first_cluster;
    uint16_t name_utf16_3[2];
} __packed;

#endif
