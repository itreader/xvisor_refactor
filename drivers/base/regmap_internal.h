/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file regmap_internal.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Regmap internal header.
 *
 * The source has been largely adapted from Linux
 * drivers/base/regmap_internal.h
 *
 * Register map access API internal header
 *
 * Copyright 2011 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * The original code is licensed under the GPL.
 */

#ifndef __REGMAP_INTERNAL_H_
#define __REGMAP_INTERNAL_H_

#include <drv/regmap.h>
#include <vmm_mutex.h>
#include <vmm_spinlocks.h>

struct regmap_format {
    size_t buf_size;
    size_t reg_bytes;
    size_t pad_bytes;
    size_t val_bytes;
    void (*format_write)(struct regmap *map, uint32_t reg, uint32_t val);
    void (*format_reg)(void *buf, uint32_t reg, uint32_t shift);
    void (*format_val)(void *buf, uint32_t val, uint32_t shift);
    uint32_t (*parse_val)(const void *buf);
    void (*parse_inplace)(void *buf);
};

struct regmap {
    union {
        vmm_mutex_t mutex;

        struct {
            vmm_spinlock_t spinlock;
            uint64_t       spinlock_flags;
        };
    };

    regmap_lock   lock;
    regmap_unlock unlock;
    void         *lock_arg;            /* This is passed to lock/unlock functions */

    vmm_device_t            *dev;      /* Device we do I/O on */
    void                    *work_buf; /* Scratch buffer used to format I/O */
    struct regmap_format     format;   /* Buffer format */
    const struct regmap_bus *bus;
    void                    *bus_context;
    const char              *name;

    uint32_t max_register;
    bool (*writeable_reg)(vmm_device_t *dev, uint32_t reg);
    bool (*readable_reg)(vmm_device_t *dev, uint32_t reg);
    bool (*volatile_reg)(vmm_device_t *dev, uint32_t reg);
    bool (*precious_reg)(vmm_device_t *dev, uint32_t reg);
    const struct regmap_access_table *wr_table;
    const struct regmap_access_table *rd_table;
    const struct regmap_access_table *volatile_table;
    const struct regmap_access_table *precious_table;

    int (*reg_read)(void *context, uint32_t reg, uint32_t *val);
    int (*reg_write)(void *context, uint32_t reg, uint32_t val);
    int (*reg_update_bits)(void *context, uint32_t reg, uint32_t mask, uint32_t val);

    uint64_t read_flag_mask;
    uint64_t write_flag_mask;

    /* number of bits to (left) shift the reg value when formatting*/
    int reg_shift;
    int reg_stride;
    int reg_stride_order;

    /* if set, converts bulk read to single read */
    bool use_single_read;
    /* if set, converts bulk read to single read */
    bool use_single_write;
    /* if set, the device supports multi write mode */
    bool can_multi_write;

    /* if set, raw reads/writes are limited to this size */
    size_t max_raw_read;
    size_t max_raw_write;
};

bool regmap_writeable(struct regmap *map, uint32_t reg);
bool regmap_readable(struct regmap *map, uint32_t reg);
bool regmap_volatile(struct regmap *map, uint32_t reg);
bool regmap_precious(struct regmap *map, uint32_t reg);

int _regmap_write(struct regmap *map, uint32_t reg, uint32_t val);

enum regmap_endian regmap_get_val_endian(vmm_device_t *dev, const struct regmap_bus *bus, const struct regmap_config *config);

static inline uint32_t regmap_get_offset(const struct regmap *map, uint32_t index)
{
    if (map->reg_stride_order >= 0) {
        return index << map->reg_stride_order;
    } else {
        return index * map->reg_stride;
    }
}

#endif /* __REGMAP_INTERNAL_H_ */
