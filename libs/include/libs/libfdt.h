/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file libfdt.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Flattend device tree library header
 *
 * The FDT structures for parsing FDT blob are taken from:
 * libfdt/fdt.h of DTC soure code.
 * Refer, https://git.kernel.org/cgit/utils/dtc/dtc.git
 *
 * libfdt - Flat Device Tree manipulation
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 * Copyright 2012 Kim Phillips, Freescale Semiconductor.
 *
 * libfdt is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 */

#ifndef __LIBFDT_H_
#define __LIBFDT_H_

#include <vmm_device_tree.h>
#include <vmm_types.h>

#define FDT_MAGIC      0xd00dfeed /* 4: version, 4: total size */
#define FDT_TAGSIZE    sizeof(uint32_t)

#define FDT_BEGIN_NODE 0x1        /* Start node: full name */
#define FDT_END_NODE   0x2        /* End node */
#define FDT_PROP                                         \
    0x3                           /* Property: name off, \
                             size, content */
#define FDT_NOP      0x4          /* nop */
#define FDT_END      0x9

#define FDT_V1_SIZE  (7 * sizeof(uint32_t))
#define FDT_V2_SIZE  (FDT_V1_SIZE + sizeof(uint32_t))
#define FDT_V3_SIZE  (FDT_V2_SIZE + sizeof(uint32_t))
#define FDT_V16_SIZE FDT_V3_SIZE
#define FDT_V17_SIZE (FDT_V16_SIZE + sizeof(uint32_t))

/* Memory unit in FDT is called a cell (assumed to be uint32_t) */
typedef uint32_t fdt_cell_t;

struct fdt_header {
    uint32_t magic;                   /* magic word FDT_MAGIC */
    uint32_t totalsize;               /* total size of DT block */
    uint32_t off_device_tree_struct;  /* offset to structure */
    uint32_t off_device_tree_strings; /* offset to strings */
    uint32_t off_mem_rsvmap;          /* offset to memory reserve map */
    uint32_t version;                 /* format version */
    uint32_t last_comp_version;       /* last compatible version */

    /* version 2 fields below */
    uint32_t boot_cpuid_phys; /* Which physical CPU id we're
                 booting on */
    /* version 3 fields below */
    uint32_t size_device_tree_strings; /* size of the strings block */

    /* version 17 fields below */
    uint32_t size_device_tree_struct; /* size of the structure block */
};

struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

struct fdt_node_header {
    uint32_t tag;
    char     name[0];
};

struct fdt_property {
    uint32_t tag;
    uint32_t len;
    uint32_t nameoff;
    char     data[0];
};

struct fdt_fileinfo {
    struct fdt_header header;
    char             *data;
    size_t            data_size;
    char             *str;
    size_t            str_size;
    char             *mem_rsvmap;
    uint32_t          mem_rsvcnt;
};

int libfdt_parse_fileinfo(virtual_addr_t fdt_addr, struct fdt_fileinfo *fdt);

int libfdt_parse_device_tree(struct fdt_fileinfo *fdt, vmm_device_tree_node_t **root, const char *root_name, vmm_device_tree_node_t *root_parent);

uint32_t libfdt_reserve_count(struct fdt_fileinfo *fdt);

int libfdt_reserve_address(struct fdt_fileinfo *fdt, uint32_t index, uint64_t *addr);

int libfdt_reserve_size(struct fdt_fileinfo *fdt, uint32_t index, uint64_t *size);

struct fdt_node_header *libfdt_find_matching_node(struct fdt_fileinfo *fdt, int (*match)(struct fdt_node_header *, int, void *), void *private);

struct fdt_node_header *libfdt_find_node(struct fdt_fileinfo *fdt, const char *node_path);

int libfdt_get_property(
    struct fdt_fileinfo *fdt, struct fdt_node_header *fdt_node, uint32_t address_cells, uint32_t size_cells, const char *property,
    void *property_value, uint32_t property_len);

#endif /* __LIBFDT_H_ */
