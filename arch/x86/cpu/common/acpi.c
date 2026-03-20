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
 * @file acpi.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief ACPI parser.
 * Some part of the code for MADT and other SDT parsing
 * is taken, with some modifications, from MINIX3.
 *
 * My sincere thanks for MINIX3 developers.
 */

#include <acpi.h>
#include <arch_cpu.h>
#include <cpu_apic.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <libs/stringlib.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

struct acpi_search_area acpi_areas[] = {
    {"Extended BIOS Data Area (EBDA)", 0x0009FC00, 0x0009FFFF},
    {"BIOS Read-Only Memory",          0xE0000,    0xFFFFF   },
    {NULL,                             0,          0         },
};

static uint64_t __init locate_rsdp_in_area(virtual_addr_t vaddr, uint32_t size)
{
    virtual_addr_t addr;

    for (addr = vaddr; addr < (vaddr + size); addr += 16) {
        if (!memcmp((const void *)addr, RSDP_SIGNATURE, RSDP_SIGN_LEN)) {
            return addr;
        }
    }

    return 0;
}

static int __init acpi_check_csum(struct acpi_sdt_hdr *tb, size_t size)
{
    uint8_t total = 0;
    int     i;

    for (i = 0; i < size; i++) {
        total += ((unsigned char *)tb)[i];
    }

    return total == 0 ? 0 : -1;
}

static int __init acpi_check_signature(const char *orig, const char *match)
{
    return strncmp(orig, match, SDT_SIGN_LEN);
}

static int __init acpi_read_sdt_at(void *sdt_va, struct acpi_sdt_hdr *tb, size_t size, const char *name)
{
    struct acpi_sdt_hdr hdr;

    /* if NULL is supplied, we only return the size of the table */
    if (tb == NULL) {
        memcpy(&hdr, sdt_va, sizeof(struct acpi_sdt_hdr));
        return hdr.len;
    }

    memcpy(tb, sdt_va, sizeof(struct acpi_sdt_hdr));

    if (acpi_check_signature((const char *)tb->signature, (const char *)name)) {
        vmm_printf("ACPI ERROR: acpi %s signature does not match\n", name);
        return VMM_EFAIL;
    }

    if (size < tb->len) {
        vmm_printf("ACPI ERROR: acpi buffer too small for %s\n", name);
        return VMM_EFAIL;
    }

    memcpy(tb, sdt_va, tb->len);

    if (acpi_check_csum(tb, tb->len)) {
        vmm_printf("ACPI ERROR: acpi %s checksum does not match\n", name);
        return VMM_EFAIL;
    }

    return tb->len;
}

static void *__init acpi_madt_get_typed_item(struct acpi_madt_hdr *hdr, unsigned char type, unsigned idx)
{
    uint8_t *t, *end;
    int      i;

    t   = (uint8_t *)hdr + sizeof(struct acpi_madt_hdr);
    end = (uint8_t *)hdr + hdr->hdr.len;

    i   = 0;

    while (t < end) {
        if (type == ((struct acpi_madt_item_hdr *)t)->type) {
            if (i == idx) {
                return t;
            } else {
                i++;
            }
        }

        t += ((struct acpi_madt_item_hdr *)t)->length;
    }

    return NULL;
}

static virtual_addr_t __init find_root_system_descriptor(void)
{
    struct acpi_search_area *carea = &acpi_areas[0];
    virtual_addr_t           area_map;
    virtual_addr_t           rsdp_base = 0;
    virtual_size_t           size      = 0;

    while (carea->area_name) {
        vmm_printf("Search for RSDP in %s... ", carea->area_name);

        size     = carea->phys_end - carea->phys_start;

        area_map = vmm_host_memmap(carea->phys_start, size, VMM_MEMORY_FLAGS_NORMAL_NOCACHE);

        BUG_ON((void *)area_map == NULL);

        if ((rsdp_base = locate_rsdp_in_area(area_map, size)) != 0) {
            vmm_printf("found.\n");
            break;
        }

        rsdp_base = 0;
        carea++;
        vmm_host_memunmap(area_map);
        vmm_printf("not found.\n");
    }

    if (likely(rsdp_base)) {
        vmm_printf("RSDP Base: 0x%" PRIADDR "\n", rsdp_base);
    }

    return rsdp_base;
}

static int __init acpi_populate_ioapic_device_tree(struct acpi_madt_hdr *madt_hdr, vmm_device_tree_node_t *cnode)
{
    uint32_t                 idx = 0;
    int                      ret = VMM_OK;
    struct acpi_madt_ioapic *ioapic;
    char                     ioapic_nm[256];

    for (;;) {
        ioapic = (struct acpi_madt_ioapic *)acpi_madt_get_typed_item(madt_hdr, ACPI_MADT_TYPE_IOAPIC, idx);

        if (!ioapic) {
            break;
        }

        memset(ioapic_nm, 0, sizeof(ioapic_nm));
        vmm_sprintf(ioapic_nm, VMM_DEVICE_TREE_IOAPIC_NODE_FMT, idx);
        vmm_device_tree_node_t *nnode = vmm_device_tree_addnode(cnode, ioapic_nm);

        if (vmm_device_tree_setattr(
                nnode, VMM_DEVICE_TREE_IOAPIC_PADDR_ATTR_NAME, &ioapic->address, VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR, sizeof(physical_addr_t), FALSE) !=
            VMM_OK) {
            ret = VMM_EFAIL;
            break;
        }

        if (vmm_device_tree_setattr(
                nnode, VMM_DEVICE_TREE_IOAPIC_GINT_BASE_ATTR_NAME, &ioapic->global_int_base, VMM_DEVICE_TREE_ATTRTYPE_UINT32,
                sizeof(ioapic->global_int_base), FALSE) != VMM_OK) {
            ret = VMM_EFAIL;
            break;
        }

        idx++;
    }

    vmm_device_tree_setattr(cnode, VMM_DEVICE_TREE_NR_IOAPIC_ATTR_NAME, &idx, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(idx), FALSE);

    return ret;
}

static int __init acpi_populate_lapic_device_tree(struct acpi_madt_hdr *madt_hdr, vmm_device_tree_node_t *cnode)
{
    uint32_t                idx = 0;
    int                     ret = VMM_OK;
    struct acpi_madt_lapic *lapic;
    char                    lapic_nm[256];

    for (;;) {
        lapic = (struct acpi_madt_lapic *)acpi_madt_get_typed_item(madt_hdr, ACPI_MADT_TYPE_LAPIC, idx);

        if (!lapic) {
            break;
        }

        memset(lapic_nm, 0, sizeof(lapic_nm));
        vmm_sprintf(lapic_nm, VMM_DEVICE_TREE_LAPIC_PCPU_NODE_FMT, idx);
        vmm_device_tree_node_t *nnode = vmm_device_tree_addnode(cnode, lapic_nm);

        if (vmm_device_tree_setattr(
                nnode, VMM_DEVICE_TREE_LAPIC_CPU_ID_ATTR_NAME, &lapic->acpi_cpu_id, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(lapic->acpi_cpu_id),
                FALSE) != VMM_OK) {
            ret = VMM_EFAIL;
            break;
        }

        if (vmm_device_tree_setattr(
                nnode, VMM_DEVICE_TREE_LAPIC_LAPIC_ID_ATTR_NAME, &lapic->apic_id, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(lapic->apic_id), FALSE) !=
            VMM_OK) {
            ret = VMM_EFAIL;
            break;
        }

        idx++;
    }

    vmm_device_tree_setattr(cnode, VMM_DEVICE_TREE_NR_LAPIC_ATTR_NAME, &idx, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(idx), FALSE);

    return ret;
}

static int __init process_acpi_sdt_table(char *tab_sign, void *tab_data)
{
    vmm_device_tree_node_t *node  = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_MOTHERBOARD_NODE_NAME);
    /* FIXME: First find if tab_size already exists. */
    vmm_device_tree_node_t *cnode = vmm_device_tree_addnode(node, tab_sign);

    vmm_device_tree_dref_node(node);

    if (!strncmp(tab_sign, APIC_SIGNATURE, strlen(APIC_SIGNATURE))) {
        struct acpi_madt_hdr *madt_hdr;
        madt_hdr = (struct acpi_madt_hdr *)tab_data;

        if (acpi_populate_ioapic_device_tree(madt_hdr, cnode) != VMM_OK) {
            return VMM_EFAIL;
        }

        if (acpi_populate_lapic_device_tree(madt_hdr, cnode) != VMM_OK) {
            return VMM_EFAIL;
        }
    } else if (!strncmp(tab_sign, HPET_SIGNATURE, strlen(HPET_SIGNATURE))) {
        struct acpi_hpet hpet_chip, *hpet;
        int              nr_hpet_blocks, i;
        char             hpet_nm[256];

        if (acpi_read_sdt_at(tab_data, (struct acpi_sdt_hdr *)&hpet_chip, sizeof(struct acpi_hpet), HPET_SIGNATURE) < 0) {
            return VMM_EFAIL;
        }

        hpet           = (struct acpi_hpet *)tab_data;
        nr_hpet_blocks = (hpet->hdr.len - sizeof(struct acpi_sdt_hdr)) / sizeof(struct acpi_timer_blocks);

        vmm_device_tree_setattr(
            cnode, VMM_DEVICE_TREE_NR_HPET_ATTR_NAME, &nr_hpet_blocks, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(nr_hpet_blocks), FALSE);

        for (i = 0; i < nr_hpet_blocks; i++) {
            memset(hpet_nm, 0, sizeof(hpet_nm));
            vmm_sprintf(hpet_nm, VMM_DEVICE_TREE_HPET_NODE_FMT, i);
            vmm_device_tree_node_t *nnode = vmm_device_tree_addnode(cnode, hpet_nm);

            BUG_ON(nnode == NULL);

            if (vmm_device_tree_setattr(
                    nnode, VMM_DEVICE_TREE_HPET_ID_ATTR_NAME, &hpet->tmr_blocks[i].asid, VMM_DEVICE_TREE_ATTRTYPE_UINT32,
                    sizeof(hpet->tmr_blocks[i].asid), FALSE) != VMM_OK) {
                return VMM_EFAIL;
            }

            if (vmm_device_tree_setattr(
                    nnode, VMM_DEVICE_TREE_HPET_PADDR_ATTR_NAME, &hpet->tmr_blocks[i].base, VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR,
                    sizeof(physical_addr_t), FALSE) != VMM_OK) {
                return VMM_EFAIL;
            }
        }
    }

    return VMM_OK;
}

int __init acpi_init(void)
{
    int               i, nr_sys_hdr, ret = VMM_EFAIL;
    struct acpi_rsdp *root_desc = NULL;
    struct acpi_rsdt  rsdt, *prsdt;

    vmm_printf("Starting to parse ACPI tables...\n");
    root_desc = (struct acpi_rsdp *)find_root_system_descriptor();

    if (root_desc == NULL) {
        vmm_printf("ACPI ERROR: No root system descriptor"
                   " table found!\n");
        goto rdesc_fail;
    }

    if (root_desc->rsdt_addr == 0) {
        vmm_printf("ACPI ERROR: No root descriptor found"
                   " in RSD Pointer!\n");
        goto rsdt_fail;
    }

    prsdt = (struct acpi_rsdt *)vmm_host_iomap(root_desc->rsdt_addr, PAGE_SIZE);

    if (unlikely(!prsdt)) {
        vmm_printf("ACPI ERROR: Failed to map physical address 0x%x.\n", root_desc->rsdt_addr);
        goto rsdt_fail;
    }

    if (acpi_read_sdt_at(prsdt, (struct acpi_sdt_hdr *)&rsdt, sizeof(struct acpi_rsdt), RSDT_SIGNATURE) < 0) {
        goto sdt_fail;
    }

    nr_sys_hdr = (rsdt.hdr.len - sizeof(struct acpi_sdt_hdr)) / sizeof(uint32_t);

    for (i = 0; i < nr_sys_hdr; i++) {
        struct acpi_sdt_hdr *hdr;
        char                 sign[32];

        memset(sign, 0, sizeof(sign));
        hdr = (struct acpi_sdt_hdr *)vmm_host_iomap(rsdt.data[i], PAGE_SIZE);

        if (hdr == NULL) {
            vmm_printf("ACPI ERROR: Cannot read header at 0x%x\n", rsdt.data[i]);
            goto sdt_fail;
        }

        memcpy(sign, hdr->signature, SDT_SIGN_LEN);
        sign[SDT_SIGN_LEN] = 0;

        if (process_acpi_sdt_table((char *)sign, (void *)hdr) != VMM_OK) {
            vmm_host_iounmap((virtual_addr_t)hdr);
            goto sdt_fail;
        }

        vmm_host_iounmap((virtual_addr_t)hdr);
    }

    ret = VMM_OK;

sdt_fail:
    vmm_host_iounmap((virtual_addr_t)prsdt);

rsdt_fail:

    vmm_host_iounmap((virtual_addr_t)root_desc);
rdesc_fail:

    return ret;
}
