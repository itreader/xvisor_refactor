/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file acpi.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief ACPI parser structure definition.
 */

#ifndef __ACPI_H__
#define __ACPI_H__

#include <vmm_device_tree.h>
#include <vmm_types.h>

#define VMM_DEVICE_TREE_MOTHERBOARD_NODE_NAME      "motherboard"
#define VMM_DEVICE_TREE_LAPIC_NODE_PARENT_NAME     "apic"
#define VMM_DEVICE_TREE_LAPIC_PCPU_NODE_FMT        "lapic@%d"
#define VMM_DEVICE_TREE_LAPIC_CPU_ID_ATTR_NAME     "acpi_cpu_id"
#define VMM_DEVICE_TREE_LAPIC_LAPIC_ID_ATTR_NAME   "lapic_id"
#define VMM_DEVICE_TREE_IOAPIC_NODE_FMT            "ioapic@%d"
#define VMM_DEVICE_TREE_IOAPIC_PADDR_ATTR_NAME     "phys_addr"
#define VMM_DEVICE_TREE_IOAPIC_GINT_BASE_ATTR_NAME "gint_base"
#define VMM_DEVICE_TREE_NR_IOAPIC_ATTR_NAME        "nr_ioapic"
#define VMM_DEVICE_TREE_NR_LAPIC_ATTR_NAME         "nr_lapic"
#define VMM_DEVICE_TREE_NR_HPET_ATTR_NAME          "nr_hpet"
#define VMM_DEVICE_TREE_HPET_NODE_FMT              "hpet@%d"
#define VMM_DEVICE_TREE_HPET_PADDR_ATTR_NAME       "phys_addr"
#define VMM_DEVICE_TREE_HPET_ID_ATTR_NAME          "id"

struct acpi_search_area {
    char           *area_name;
    physical_addr_t phys_start;
    physical_addr_t phys_end;
};

extern struct acpi_search_area acpi_areas[];

#define RSDP_SIGN_LEN        8
#define OEM_ID_LEN           6
#define SDT_SIGN_LEN         4

#define RSDP_SIGNATURE       "RSD PTR "
#define RSDT_SIGNATURE       "RSDT"
#define HPET_SIGNATURE       "HPET"
#define APIC_SIGNATURE       "APIC"

#define NR_HPET_TIMER_BLOCKS 8

/* Root system description pointer */
struct acpi_rsdp {
    uint8_t  signature[RSDP_SIGN_LEN];
    uint8_t  checksum;
    uint8_t  oem_id[OEM_ID_LEN];
    uint8_t  rev;
    uint32_t rsdt_addr;
    uint32_t rsdt_len;
    uint64_t xsdt_addr;
    uint8_t  xchecksum;
    uint8_t  reserved[3];
} __packed;

struct acpi_sdt_hdr {
    uint8_t  signature[SDT_SIGN_LEN];
    uint32_t len;
    uint8_t  rev;
    uint8_t  checksum;
    uint8_t  oem_id[OEM_ID_LEN];
    uint64_t oem_table_id;
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __packed;

#define MAX_RSDT 35 /* ACPI defines 35 signatures */

struct acpi_rsdt {
    struct acpi_sdt_hdr hdr;
    uint32_t            data[MAX_RSDT];
};

struct acpi_madt_hdr {
    struct acpi_sdt_hdr hdr;
    uint32_t            local_apic_address;
    uint32_t            flags;
};

#define ACPI_MADT_TYPE_LAPIC            0
#define ACPI_MADT_TYPE_IOAPIC           1
#define ACPI_MADT_TYPE_INT_SRC          2
#define ACPI_MADT_TYPE_NMI_SRC          3
#define ACPI_MADT_TYPE_LAPIC_NMI        4
#define ACPI_MADT_TYPE_LAPIC_ADRESS     5
#define ACPI_MADT_TYPE_IOSAPIC          6
#define ACPI_MADT_TYPE_LSAPIC           7
#define ACPI_MADT_TYPE_PLATFORM_INT_SRC 8
#define ACPI_MADT_TYPE_Lx2APIC          9
#define ACPI_MADT_TYPE_Lx2APIC_NMI      10

struct acpi_madt_item_hdr {
    uint8_t type;
    uint8_t length;
} __packed;

struct acpi_madt_lapic {
    struct acpi_madt_item_hdr hdr;
    uint8_t                   acpi_cpu_id;
    uint8_t                   apic_id;
    uint32_t                  flags;
} __packed;

struct acpi_madt_ioapic {
    struct acpi_madt_item_hdr hdr;
    uint8_t                   id;
    uint8_t                   __reserved;
    uint32_t                  address;
    uint32_t                  global_int_base;
} __packed;

struct acpi_madt_int_src {
    struct acpi_madt_item_hdr hdr;
    uint8_t                   bus;
    uint8_t                   bus_int;
    uint32_t                  global_int;
    uint16_t                  mps_flags;
} __packed;

struct acpi_madt_nmi {
    struct acpi_madt_item_hdr hdr;
    uint16_t                  flags;
    uint32_t                  global_int;
} __packed;

struct acpi_timer_blocks {
    uint32_t blkid;
    uint8_t  asid;
    uint8_t  rbw;
    uint8_t  rbo;
    uint8_t  resvd;
    uint64_t base;
    uint8_t  id;
    uint16_t min_clock_tick;
    uint8_t  pg_prot;
} __packed;

struct acpi_hpet {
    struct acpi_sdt_hdr      hdr;
    struct acpi_timer_blocks tmr_blocks[NR_HPET_TIMER_BLOCKS];
} __packed;

int acpi_init(void);

#endif /* __ACPI_H__ */
