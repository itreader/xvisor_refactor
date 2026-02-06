/*
 * Copyright © 1999-2010 David Woodhouse <dwmw2@infradead.org> et al.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 *
 * Modified by Jimmy Durand Wesolowski for Xvisor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __MTD_MTD_H__
#define __MTD_MTD_H__

#include <asm/div64.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <uapi/mtd/mtd-abi.h>

/* Xvisor module init priority level */
#define MTD_IPRIORITY         1

#define MTD_ERASE_PENDING     0x01
#define MTD_ERASING           0x02
#define MTD_ERASE_SUSPEND     0x04
#define MTD_ERASE_DONE        0x08
#define MTD_ERASE_FAILED      0x10

#define MTD_FAIL_ADDR_UNKNOWN -1LL

/*
 * If the erase fails, fail_addr might indicate exactly which block failed. If
 * fail_addr = MTD_FAIL_ADDR_UNKNOWN, the failure was not at the device level
 * or was not specific to any particular block.
 */
struct erase_info {
    struct mtd_info *mtd;
    uint64_t         addr;
    uint64_t         len;
    uint64_t         fail_addr;
    uint64_t         time;
    uint64_t         retries;
    uint32_t         dev;
    uint32_t         cell;
    void (*callback)(struct erase_info *self);
    uint64_t private;
    uint8_t            state;
    struct erase_info *next;
};

struct mtd_erase_region_info {
    uint64_t  offset;     /* At which this region starts, from the beginning of the MTD */
    uint32_t  erase_size; /* For this region */
    uint32_t  num_blocks; /* Number of blocks of erase_size in this region */
    uint64_t *lock_map;   /* If keeping bitmap of locks */
};

/**
 * struct mtd_oob_ops - oob operation operands
 * @mode:   operation mode
 *
 * @len:    number of data bytes to write/read
 *
 * @retlen: number of data bytes written/read
 *
 * @oob_len:    number of oob bytes to write/read
 * @oob_ret_len:    number of oob bytes written/read
 * @oob_offset: offset of oob data in the oob area (only relevant when
 *      mode = MTD_OPS_PLACE_OOB or MTD_OPS_RAW)
 * @data_buffer:    data buffer - if NULL only oob data are read/written
 * @oob_buffer: oob data buffer
 *
 * Note, it is allowed to read more than one OOB area at one go, but not write.
 * The interface assumes that the OOB write requests program only one page's
 * OOB area.
 */
struct mtd_oob_ops {
    uint32_t mode;
    size_t   len;
    size_t   retlen;
    size_t   oob_len;
    size_t   oob_ret_len;
    uint32_t oob_offset;
    uint8_t *data_buffer;
    uint8_t *oob_buffer;
};

#define MTD_MAX_OOBFREE_ENTRIES_LARGE 32
#define MTD_MAX_ECCPOS_ENTRIES_LARGE  640

/*
 * Internal ECC layout control structure. For historical reasons, there is a
 * similar, smaller struct nand_ecc_layout_user (in mtd-abi.h) that is retained
 * for export to user-space via the ECCGETLAYOUT ioctl.
 * nand_ecc_layout should be expandable in the future simply by the above macros.
 */
struct nand_ecc_layout {
    uint32_t             ecc_bytes;
    uint32_t             ecc_pos[MTD_MAX_ECCPOS_ENTRIES_LARGE];
    uint32_t             oob_avail;
    struct nand_oob_free oob_free[MTD_MAX_OOBFREE_ENTRIES_LARGE];
};

struct module; /* only needed for owner field in mtd_info */

struct mtd_info {
    uint8_t  type;
    uint32_t flags;
    uint64_t size;  // Total size of the MTD

    /* "Major" erase size for the device. Naïve users may take this
     * to be the only erase size available, or may use the more detailed
     * information below if they desire
     */
    uint32_t erase_size;
    /* Minimal writable flash unit size. In case of NOR flash it is 1 (even
     * though individual bits can be cleared), in case of NAND flash it is
     * one NAND page (or half, or one-fourths of it), in case of ECC-ed NOR
     * it is of ECC block size, etc. It is illegal to have write_size = 0.
     * Any driver registering a struct mtd_info must ensure a write_size of
     * 1 or larger.
     */
    uint32_t write_size;

    /*
     * Size of the write buffer used by the MTD. MTD devices having a write
     * buffer can write multiple write_size chunks at a time. E.g. while
     * writing 4 * write_size bytes to a device with 2 * write_size bytes
     * buffer the MTD driver can (but doesn't have to) do 2 write_size
     * operations, but not 4. Currently, all NANDs have write_buffer_size
     * equivalent to write_size (NAND page size). Some NOR flashes do have
     * write_buffer_size greater than write_size.
     */
    uint32_t write_buffer_size;

    uint32_t oob_size;   // Amount of OOB data per block (e.g. 16)
    uint32_t oob_avail;  // Available OOB bytes per block

    /*
     * If erase_size is a power of 2 then the shift is stored in
     * erase_size_shift otherwise erase_size_shift is zero. Ditto write_size.
     */
    uint32_t erase_size_shift;
    uint32_t write_size_shift;
    /* Masks based on erase_size_shift and write_size_shift */
    uint32_t erase_size_mask;
    uint32_t write_size_mask;

    /*
     * read ops return -EUCLEAN if max number of bitflips corrected on any
     * one region comprising an ecc step equals or exceeds this value.
     * Settable by driver, else defaults to ecc_strength.  User can override
     * in sysfs.  N.B. The meaning of the -EUCLEAN return code has changed;
     * see Documentation/ABI/testing/sysfs-class-mtd for more detail.
     */
    uint32_t bitflip_threshold;

    // Kernel-only stuff starts here.
    char *name;
    int   index;

    /* ECC layout structure pointer - read only! */
    struct nand_ecc_layout *ecc_layout;

    /* the ecc step size. */
    uint32_t ecc_step_size;

    /* max number of correctible bit errors per ecc step */
    uint32_t ecc_strength;

    /* Data for variable erase regions. If num_erase_regions is zero,
     * it means that the whole device has erase_size as given above.
     */
    int                           num_erase_regions;
    struct mtd_erase_region_info *erase_regions;

    /*
     * Do not call via these pointers, use corresponding mtd_*()
     * wrappers instead.
     */
    int (*_erase)(struct mtd_info *mtd, struct erase_info *instr);
    int (*_point)(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, void **virt, resource_size_t *phys);
    int (*_unpoint)(struct mtd_info *mtd, loff_t from, size_t len);
#if 0
    uint64_t (*_get_unmapped_area) (struct mtd_info *mtd,
                                    uint64_t len,
                                    uint64_t offset,
                                    uint64_t flags);
#endif
    int (*_read)(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, uint8_t *buf);
    int (*_write)(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf);
    int (*_panic_write)(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf);
    int (*_read_oob)(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);
    int (*_write_oob)(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);
#if 0
    int (*_get_fact_prot_info) (struct mtd_info *mtd, struct otp_info *buf,
                                size_t len);
    int (*_read_fact_prot_reg) (struct mtd_info *mtd, loff_t from,
                                size_t len, size_t *retlen, uint8_t *buf);
    int (*_get_user_prot_info) (struct mtd_info *mtd, struct otp_info *buf,
                                size_t len);
    int (*_read_user_prot_reg) (struct mtd_info *mtd, loff_t from,
                                size_t len, size_t *retlen, uint8_t *buf);
    int (*_write_user_prot_reg) (struct mtd_info *mtd, loff_t to,
                                 size_t len, size_t *retlen, uint8_t *buf);
    int (*_lock_user_prot_reg) (struct mtd_info *mtd, loff_t from,
                                size_t len);
    int (*_writev) (struct mtd_info *mtd, const struct kvec *vecs,
                    uint64_t count, loff_t to, size_t *retlen);
#endif
    void (*_sync)(struct mtd_info *mtd);
    int (*_lock)(struct mtd_info *mtd, loff_t ofs, uint64_t len);
    int (*_unlock)(struct mtd_info *mtd, loff_t ofs, uint64_t len);
    int (*_is_locked)(struct mtd_info *mtd, loff_t ofs, uint64_t len);
    int (*_block_isbad)(struct mtd_info *mtd, loff_t ofs);
    int (*_block_markbad)(struct mtd_info *mtd, loff_t ofs);
    int (*_suspend)(struct mtd_info *mtd);
    void (*_resume)(struct mtd_info *mtd);
    /*
     * If the driver is something smart, like UBI, it may need to maintain
     * its own reference counting. The below functions are only for driver.
     */
    int (*_get_device)(struct mtd_info *mtd);
    void (*_put_device)(struct mtd_info *mtd);

#if 0
    /* Backing device capabilities for this device
     * - provides mmap capabilities
     */
    struct backing_dev_info *backing_dev_info;
#endif

#if 0
    struct notifier_block reboot_notifier;  /* default mode before reboot */
#endif

    /* ECC status information */
    struct mtd_ecc_status ecc_status;
    /* Subpage shift (NAND) */
    int                   subpage_shift;

    void *private;

    struct module *owner;
    struct device  dev;
    int            usecount;
};

int mtd_erase(struct mtd_info *mtd, struct erase_info *instr);
#if 0
int mtd_point(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen,
              void **virt, resource_size_t *phys);
int mtd_unpoint(struct mtd_info *mtd, loff_t from, size_t len);
uint64_t mtd_get_unmapped_area(struct mtd_info *mtd, uint64_t len,
                               uint64_t offset, uint64_t flags);
#endif
int mtd_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen, uint8_t *buf);
int mtd_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf);
int mtd_panic_write(struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const uint8_t *buf);

int mtd_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);

static inline int mtd_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
    ops->retlen = ops->oob_ret_len = 0;

    if (!mtd->_write_oob) {
        return -EOPNOTSUPP;
    }

    if (!(mtd->flags & MTD_WRITEABLE)) {
        return -EROFS;
    }

    return mtd->_write_oob(mtd, to, ops);
}

#if 0
int mtd_get_fact_prot_info(struct mtd_info *mtd, struct otp_info *buf,
                           size_t len);
int mtd_read_fact_prot_reg(struct mtd_info *mtd, loff_t from, size_t len,
                           size_t *retlen, uint8_t *buf);
int mtd_get_user_prot_info(struct mtd_info *mtd, struct otp_info *buf,
                           size_t len);
int mtd_read_user_prot_reg(struct mtd_info *mtd, loff_t from, size_t len,
                           size_t *retlen, uint8_t *buf);
int mtd_write_user_prot_reg(struct mtd_info *mtd, loff_t to, size_t len,
                            size_t *retlen, uint8_t *buf);
int mtd_lock_user_prot_reg(struct mtd_info *mtd, loff_t from, size_t len);
#endif

int mtd_writev(struct mtd_info *mtd, const struct kvec *vecs, uint64_t count, loff_t to, size_t *retlen);

static inline void mtd_sync(struct mtd_info *mtd)
{
    if (mtd->_sync) {
        mtd->_sync(mtd);
    }
}

int mtd_lock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
int mtd_unlock(struct mtd_info *mtd, loff_t ofs, uint64_t len);
int mtd_is_locked(struct mtd_info *mtd, loff_t ofs, uint64_t len);
int mtd_block_isbad(struct mtd_info *mtd, loff_t ofs);
int mtd_block_markbad(struct mtd_info *mtd, loff_t ofs);

static inline int mtd_suspend(struct mtd_info *mtd)
{
    return mtd->_suspend ? mtd->_suspend(mtd) : 0;
}

static inline void mtd_resume(struct mtd_info *mtd)
{
    if (mtd->_resume) {
        mtd->_resume(mtd);
    }
}

static inline uint32_t mtd_div_by_eb(uint64_t size, struct mtd_info *mtd)
{
    if (mtd->erase_size_shift) {
        return size >> mtd->erase_size_shift;
    }

    do_div(size, mtd->erase_size);
    return size;
}

static inline uint32_t mtd_mod_by_eb(uint64_t size, struct mtd_info *mtd)
{
    if (mtd->erase_size_shift) {
        return size & mtd->erase_size_mask;
    }

    return do_div(size, mtd->erase_size);
}

static inline uint32_t mtd_div_by_ws(uint64_t size, struct mtd_info *mtd)
{
    if (mtd->write_size_shift) {
        return size >> mtd->write_size_shift;
    }

    do_div(size, mtd->write_size);
    return size;
}

#if 0
static inline uint32_t mtd_mod_by_ws(uint64_t size, struct mtd_info *mtd)
{
    if (mtd->write_size_shift)
        return size & mtd->write_size_mask;

    return do_div(size, mtd->write_size);
}
#endif

static inline int mtd_has_oob(const struct mtd_info *mtd)
{
    return mtd->_read_oob && mtd->_write_oob;
}

static inline int mtd_type_is_nand(const struct mtd_info *mtd)
{
    return mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;
}

static inline int mtd_can_have_bb(const struct mtd_info *mtd)
{
    return !!mtd->_block_isbad;
}

/* Kernel-side ioctl definitions */

struct mtd_partition;
struct mtd_part_parser_data;

extern int mtd_device_parse_register(
    struct mtd_info *mtd, const char *const *part_probe_types, struct mtd_part_parser_data *parser_data, const struct mtd_partition *defparts,
    int defnr_parts);
#define mtd_device_register(master, parts, nr_parts) mtd_device_parse_register(master, NULL, NULL, parts, nr_parts)
extern int              mtd_device_unregister(struct mtd_info *master);
extern struct mtd_info *get_mtd_device(struct mtd_info *mtd, int num);
extern int              __get_mtd_device(struct mtd_info *mtd);
extern void             __put_mtd_device(struct mtd_info *mtd);
extern struct mtd_info *get_mtd_device_nm(const char *name);
extern void             put_mtd_device(struct mtd_info *mtd);

struct mtd_notifier {
    void (*add)(struct mtd_info *mtd);
    void (*remove)(struct mtd_info *mtd);
    list_head_t list;
};

extern void register_mtd_user(struct mtd_notifier *new);
extern int  unregister_mtd_user(struct mtd_notifier *old);
void       *mtd_kmalloc_up_to(const struct mtd_info *mtd, size_t *size);

void mtd_erase_callback(struct erase_info *instr);

static inline int mtd_is_bitflip(int err)
{
    return err == -EUCLEAN;
}

static inline int mtd_is_eccerr(int err)
{
    return err == -EBADMSG;
}

extern struct mtd_info *__mtd_next_device(int i);

static inline struct mtd_info *mtd_get_device(int i)
{
    return __mtd_next_device(i);
}

#if 0
static inline int mtd_is_bitflip_or_eccerr(int err) {
    return mtd_is_bitflip(err) || mtd_is_eccerr(err);
}
#endif

#endif /* __MTD_MTD_H__ */
