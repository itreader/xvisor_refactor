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
 * @file vmm_block_partition_dos.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief IBM PC DOS兼容分区管理源文件
 *
 * This is the default partition style that is always available with
 * block device partition management.
 *
 * Newer partition styles are generally implemented as an extension under
 * IBM PC DOS style primary partitions.
 */

#include <block/vmm_block_partition.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "IBM PC DOS Style Partitions"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_BLOCKPART_IPRIORITY + 1)
#define MODULE_INIT      vmm_block_partition_dos_init
#define MODULE_EXIT      vmm_block_partition_dos_exit

#undef DOS_DEBUG

#ifdef DOS_DEBUG
#define debug(x...) vmm_printf(x)
#else
#define debug(x...)
#endif

#define DOS_MBR_SIGN_OFFSET    0x1FE
#define DOS_MBR_SIGN_VALUE     0xAA55
#define DOS_MBR_PARTTBL_OFFSET 0x1BE

/* Enumeration of MBR partition status */
/**
 * @brief DOS分区状态结构，记录分区的有效性标志
 */
enum dos_partition_status {
    DOS_MBR_PARTITON_NONBOOTABLE = 0x00, /**< 0x00成员 */
    DOS_MBR_PARTITION_BOOTABLE   = 0x80
};

/* Enumeration of MBR partition types */
/**
 * @brief DOS分区类型表，映射分区类型ID到名称
 */
enum dos_partition_types {
    DOS_MBR_PARTITION_EMPTY               = 0x00, /**< 0x00成员 */
    DOS_MBR_PARTITION_FAT12               = 0x01, /**< 0x01成员 */
    DOS_MBR_PARTITION_XENIX_ROOT          = 0x02, /**< 0x02成员 */
    DOS_MBR_PARTITION_XENIX_USR           = 0x03, /**< 0x03成员 */
    DOS_MBR_PARTITION_FAT16_32M           = 0x04, /**< 0x04成员 */
    DOS_MBR_PARTITION_EXTENDED            = 0x05, /**< 0x05成员 */
    DOS_MBR_PARTITION_FAT16               = 0x06, /**< 0x06成员 */
    DOS_MBR_PARTITION_NTFS                = 0x07, /**< 0x07成员 */
    DOS_MBR_PARTITION_AIX                 = 0x08, /**< 0x08成员 */
    DOS_MBR_PARTITION_AIX_BOOTABLE        = 0x09, /**< 0x09成员 */
    DOS_MBR_PARTITION_OS2_BOOT_MANAGER    = 0x0A, /**< 0x0A成员 */
    DOS_MBR_PARTITION_FAT32               = 0x0B, /**< 0x0B成员 */
    DOS_MBR_PARTITION_FAT32_LBA           = 0x0C, /**< 0x0C成员 */
    DOS_MBR_PARTITION_FAT16_LBA           = 0x0E, /**< 0x0E成员 */
    DOS_MBR_PARTITION_FAT16_EXTENDED      = 0x0F, /**< 0x0F成员 */
    DOS_MBR_PARTITION_OPUS                = 0x10, /**< 0x10成员 */
    DOS_MBR_PARTITION_FAT12_HIDDEN        = 0x11, /**< 0x11成员 */
    DOS_MBR_PARTITION_COMPAQ_DIAG         = 0x12, /**< 0x12成员 */
    DOS_MBR_PARTITION_FAT16_HIDDEN        = 0x14, /**< 0x14成员 */
    DOS_MBR_PARTITION_NTFS_HIDDEN         = 0x17, /**< 0x17成员 */
    DOS_MBR_PARTITION_FAT32_HIDDEN        = 0x1B, /**< 0x1B成员 */
    DOS_MBR_PARTITION_FAT32_HIDDEN_LBA    = 0x1C, /**< 0x1C成员 */
    DOS_MBR_PARTITION_FAT16_HIDDEN_LBA    = 0x1D, /**< 0x1D成员 */
    DOS_MBR_PARTITION_XOSL_FS             = 0x78, /**< 0x78成员 */
    DOS_MBR_PARTITION_LINUX_SWAP          = 0x82, /**< 0x82成员 */
    DOS_MBR_PARTITION_LINUX_NATIVE        = 0x83, /**< 0x83成员 */
    DOS_MBR_PARTITION_GNU_LINUX_EXTENDED  = 0x85, /**< 0x85成员 */
    DOS_MBR_PARTITION_LEGACY_FT_FAT16     = 0x86, /**< 0x86成员 */
    DOS_MBR_PARTITION_LEGACY_FT_NTFS      = 0x87, /**< 0x87成员 */
    DOS_MBR_PARTITION_GNU_LINUX_PLAINTEXT = 0x88, /**< 0x88成员 */
    DOS_MBR_PARTITION_GNU_LINUX_LVM       = 0x89, /**< 0x89成员 */
    DOS_MBR_PARTITION_LEGACY_FT_FAT32     = 0x8B, /**< 0x8B成员 */
    DOS_MBR_PARTITION_LEGACY_FT_FAT32_LBA = 0x8C, /**< 0x8C成员 */
    DOS_MBR_PARTITION_UNKNOWN_LINUX_LVM   = 0x8E, /**< 0x8E成员 */
    DOS_MBR_PARTITION_BSD_SLICE           = 0xA5, /**< 0xA5成员 */
    DOS_MBR_PARTITION_RAW                 = 0xDA, /**< 0xDA成员 */
    DOS_MBR_PARTITION_BOOTIT              = 0xDF, /**< 0xDF成员 */
    DOS_MBR_PARTITION_BFS                 = 0xEB, /**< 0xEB成员 */
    DOS_MBR_PARTITION_EFI_GPT             = 0xEE, /**< 0xEE成员 */
    DOS_MBR_PARTITION_INTEL_EFI           = 0xEF, /**< 0xEF成员 */
    DOS_MBR_PARTITION_VMFS                = 0xFB, /**< 0xFB成员 */
    DOS_MBR_PARTITION_VMKCORE             = 0xFC, /**< 0xFC成员 */
    DOS_MBR_PARTITION_LINUX_RAID          = 0xFD
};

/* MBR partition table entry */
/**
 * @brief DOS分区描述结构，保存分区的起始扇区和大小
 */
struct dos_partition {
    uint8_t  status; /**< 状态 */
    uint8_t  chs_first[3]; /**< chs_first成员 */
    uint8_t  type; /**< 类型 */
    uint8_t  chs_last[3]; /**< chs_last成员 */
    uint32_t lba_start; /**< lba_start成员 */
    uint32_t sector_count; /**< sector_count成员 */
} __packed;

/**
 * @brief 处理DOS分区表中的扩展分区
 * @param block_device 块设备结构体指针
 * @param parent 父设备树节点
 */
static void dos_process_extended_part(vmm_block_device_t *block_device, struct dos_partition *parent)
{
    int                  rc;
    uint16_t i;
    uint16_t sign;
    uint64_t read;
    uint64_t addr;
    uint64_t rel = 0;
    struct dos_partition part[2];

    while (1) {
        /* Print debug info */
        debug("%s: extended partition\n", block_device->name); /**< block_device->name)成员 */
        debug("%s: status=0x%02x type=0x%02x\n", block_device->name, part->status, part->type); /**< part->type)成员 */
        debug("%s: lba_start=0x%08x sector_count=0x%08x\n", block_device->name, part->lba_start, part->sector_count); /**< part->sector_count)成员 */

        /* Check for DOS MBR signature */
        addr = (parent->lba_start + rel) * block_device->block_size; /**< block_device->block_size成员 */
        addr += DOS_MBR_SIGN_OFFSET; /**< DOS_MBR_SIGN_OFFSET成员 */
        read = vmm_block_device_read(block_device, (uint8_t *)&sign, addr, sizeof(uint16_t)); /**< sizeof(uint16_t))成员 */

        if (read != sizeof(uint16_t)) {
            break;
        }

        sign = vmm_le16_to_cpu(sign); /**< vmm_le16_to_cpu(sign)成员 */

        if (sign != DOS_MBR_SIGN_VALUE) {
            break;
        }

        /* Retreive MBR partition table */
        addr = (parent->lba_start + rel) * block_device->block_size; /**< block_device->block_size成员 */
        addr += DOS_MBR_PARTTBL_OFFSET; /**< DOS_MBR_PARTTBL_OFFSET成员 */
        read = vmm_block_device_read(block_device, (uint8_t *)&part[0], addr, sizeof(part)); /**< 分区 */

        if (read != sizeof(part)) {
            break;
        }

        for (i = 0; i < 2; i++) {
            part[i].lba_start    = vmm_le32_to_cpu(part[i].lba_start); /**< 分区 */
            part[i].sector_count = vmm_le32_to_cpu(part[i].sector_count); /**< 分区 */
        }

        /* Sanity check on first entry */
        if (part[0].type == DOS_MBR_PARTITION_EMPTY) {
            break;
        }

        addr = parent->lba_start + rel + part[0].lba_start; /**< 分区 */

        if (addr < parent->lba_start) {
            break;
        }

        addr += part[0].sector_count; /**< 分区 */

        if ((parent->lba_start + parent->sector_count) < addr) {
            break;
        }

        /* Process first entry */
        addr = parent->lba_start + rel + part[0].lba_start; /**< 分区 */
        rc   = vmm_block_device_add_child(block_device, addr, part[0].sector_count); /**< 分区 */

        if (rc) {
            vmm_printf(
                "%s: failed to add extended partition "
                "(error %d)\n", /**< %d)\n"成员 */
                block_device->name, rc); /**< rc)成员 */
            return;
        }

        /* Sanity check on second entry */
        if (part[1].type == DOS_MBR_PARTITION_EMPTY) {
            break;
        }

        /* Update rel based on second entry */
        rel = part[1].lba_start; /**< 分区 */
    }
}

/**
 * @brief 处理DOS分区表中的主分区
 * @param block_device 块设备结构体指针
 * @param part 分区结构体指针
 */
static void dos_process_primary_part(vmm_block_device_t *block_device, struct dos_partition *part)
{
    int rc;

    /* Print debug info */
    debug("%s: primary partition\n", block_device->name);
    debug("%s: status=0x%02x type=0x%02x\n", block_device->name, part->status, part->type);
    debug("%s: lba_start=0x%08x sector_count=0x%08x\n", block_device->name, part->lba_start, part->sector_count);

    /* Add primary partition as child block device */
    rc = vmm_block_device_add_child(block_device, part->lba_start, part->sector_count);

    if (rc) {
        vmm_printf("%s: failed to add primary partition (error %d)\n", block_device->name, rc);
    }
}

/**
 * @brief 解析DOS格式的分区表
 * @param block_device 块设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int dos_parse_part(vmm_block_device_t *block_device)
{
    uint64_t             read;
    uint16_t i;
    uint16_t sign;
    uint16_t process_count;
    struct dos_partition part[4];

    /* Check for DOS MBR signature */
    read = vmm_block_device_read(block_device, (uint8_t *)&sign, DOS_MBR_SIGN_OFFSET, sizeof(uint16_t));

    if (read != sizeof(uint16_t)) {
        return VMM_ERR_IO;
    }

    sign = vmm_le16_to_cpu(sign);

    if (sign != DOS_MBR_SIGN_VALUE) {
        return VMM_ERR_NOSYS;
    }

    /* Retreive MBR partition table */
    read = vmm_block_device_read(block_device, (uint8_t *)&part[0], DOS_MBR_PARTTBL_OFFSET, sizeof(part));

    if (read != sizeof(part)) {
        return VMM_ERR_IO;
    }

    for (i = 0; i < 4; i++) {
        part[i].lba_start    = vmm_le32_to_cpu(part[i].lba_start);
        part[i].sector_count = vmm_le32_to_cpu(part[i].sector_count);
    }

    /* Process each entry of MBR partition table */
    process_count = 0;

    for (i = 0; i < 4; i++) {
        /* Skip empty partition */
        if (part[i].type == DOS_MBR_PARTITION_EMPTY) {
            continue;
        }

        /* Skip EFI_GPT and INTEL_EFI partition type because, this
         * partition style are an extension to the IBM PC DOS style.
         */
        if ((part[i].type == DOS_MBR_PARTITION_EFI_GPT) || (part[i].type == DOS_MBR_PARTITION_INTEL_EFI)) {
            continue;
        }

        /* Process primary and extended partitions */
        if ((part[i].type == DOS_MBR_PARTITION_EXTENDED) || (part[i].type == DOS_MBR_PARTITION_FAT16_EXTENDED) ||
            (part[i].type == DOS_MBR_PARTITION_GNU_LINUX_EXTENDED)) {
            dos_process_extended_part(block_device, &part[i]);
        } else {
            dos_process_primary_part(block_device, &part[i]);
        }

        /* Increment process count */
        process_count++;
    }

    /* Failure if we did not process any MBR partition */
    if (!process_count) {
        return VMM_ERR_NOENT;
    }

    return VMM_OK;
}

static struct vmm_block_partition_manager dos = {
    .sign       = 0x1,
    .name       = "DOS Partitions",
    .parse_part = dos_parse_part,
};

/**
 * @brief 初始化DOS分区
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init vmm_block_partition_dos_init(void)
{
    return vmm_block_partition_manager_register(&dos);
}

/**
 * @brief DOS分区探测退出清理
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_block_partition_dos_exit(void)
{
    vmm_block_partition_manager_unregister(&dos);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
