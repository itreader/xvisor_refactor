/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file ide.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE/ATA maintenance related defines.
 */
#ifndef _IDE_H
#define _IDE_H

#include <vmm_completion.h>
#include <vmm_mutex.h>
#include <vmm_threads.h>

#define MAX_IDE_DRIVES            4
#define MAX_IDE_CHANNELS          2
#define MAX_IDE_DRIVES_PER_CHAN   (MAX_IDE_DRIVES / MAX_IDE_CHANNELS)

#define IDE_ATA                   0x00
#define IDE_ATAPI                 0x01

#define PRIMARY_ATA_CHANNEL_IRQ   14
#define SECONDARY_ATA_CHANNEL_IRQ 15

struct ide_drive;

struct ide_channel {
    uint16_t base;   /* i/o base */
    uint16_t ctrl;   /* control base */
    uint16_t bmide;  /* bus master ide */
    uint8_t  int_en; /* no interrupt */
    uint8_t  id;     /* channel number */
};

struct ide_drive_ops {
    uint32_t (*block_read)(struct ide_drive *drive, uint64_t start_lba, uint32_t blkcnt, void *buffer);
    uint32_t (*block_write)(struct ide_drive *drive, uint64_t start_lba, uint32_t blkcnt, const void *buffer);
    uint32_t (*block_erase)(struct ide_drive *drive, uint64_t start_lba, uint32_t blkcnt);
};

struct ide_drive {
    double_list_t       link;         /* For the list of detected drives */
    vmm_device_t       *dev;
    uint8_t             present;      /* If this drive is present */
    uint8_t             drive;        /* drive number on the channel */
    uint8_t             type;         /* device type ATA/ATAPI*/
    uint16_t            signature;    /* drive signature */
    uint16_t            capabilities; /* drive capabilities */
    uint32_t            cmd_set;      /* command sets supported */
    uint32_t            size;         /* size in sectors */
    uint32_t            block_size;   /* Block size of drive CDROM: 2048 */
    struct ide_channel *channel;      /* channel on which drive is connected. */

    uint8_t lba48_enabled;            /* device can use 48bit addr (ATA/ATAPI v7) */

    uint32_t lba;                     /* number of blocks */
    uint32_t blksz;                   /* block size */
    uint8_t  model[41];               /* IDE model, SCSI Vendor */

    double_list_t  io_list;           /* IO request list */
    vmm_spinlock_t io_list_lock;      /* IO list lock */
    vmm_mutex_t    lock;

    vmm_thread_t       *io_thread;    /* IO thread */
    vmm_completion_t    io_avail;     /* To wake up I/O thread */
    vmm_completion_t    dev_intr;     /* Device reported interrupt */
    vmm_block_device_t *block_device; /* Block device associated to this drive */

    struct ide_drive_ops io_ops;      /* Host operations */
    void *private;                    /* driver private struct pointer */
};

struct ide_host_controller {
    uint32_t           vendor_id;
    uint32_t           device_id;
    uint32_t           class_id;
    uint32_t           subclass_id;
    uint64_t           bar0;
    uint64_t           bar1;
    uint64_t           bar2;
    uint64_t           bar3;
    uint64_t           bar4;
    struct ide_drive   ide_drives[MAX_IDE_DRIVES];
    struct ide_channel ide_channels[MAX_IDE_CHANNELS];
    uint32_t           nr_drives_present;
};

/*
 * Function Prototypes
 */
extern uint32_t ide_write_sectors(struct ide_drive *drive, uint64_t lba, uint32_t numsects, const void *buffer);
extern uint32_t ide_read_sectors(struct ide_drive *drive, uint64_t lba, uint32_t numsects, void *buffer);
extern int      ide_initialize(struct ide_host_controller *controller);
extern int      ide_add_drive(struct ide_drive *drive);

#endif /* _IDE_H */
