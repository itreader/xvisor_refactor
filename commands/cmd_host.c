/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_host.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of host command
 */

#include <arch_board.h>
#include <arch_cpu.h>
#include <arch_cpu_addr_space.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_cpumask.h>
#include <vmm_delay.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_extend_irq.h>
#include <vmm_host_irq.h>
#include <vmm_host_ram.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_modules.h>
#include <vmm_page_pool.h>
#include <vmm_resource.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>

#define MODULE_DESC      "Command host"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_host_init
#define MODULE_EXIT      cmd_host_exit

static void cmd_host_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   host help\n");
    vmm_cdev_printf(cdev, "   host info\n");
    vmm_cdev_printf(cdev, "   host cpu info\n");
    vmm_cdev_printf(cdev, "   host cpu poke [<host_cpu>]\n");
    vmm_cdev_printf(cdev, "   host cpu stats\n");
    vmm_cdev_printf(cdev, "   host irq stats\n");
    vmm_cdev_printf(cdev, "   host irq set_affinity <hirq> <host_cpu>\n");
    vmm_cdev_printf(cdev, "   host extirq stats\n");
    vmm_cdev_printf(cdev, "   host addr_space info\n");
    vmm_cdev_printf(cdev, "   host ram info\n");
    vmm_cdev_printf(cdev, "   host ram bitmap [<column count>]\n");
    vmm_cdev_printf(cdev, "   host ram reserve <physaddr> <size>\n");
    vmm_cdev_printf(cdev, "   host virtual_address_pool info\n");
    vmm_cdev_printf(cdev, "   host virtual_address_pool state\n");
    vmm_cdev_printf(cdev, "   host virtual_address_pool bitmap [<column count>]\n");
    vmm_cdev_printf(cdev, "   host page_pool info\n");
    vmm_cdev_printf(cdev, "   host page_pool state\n");
    vmm_cdev_printf(cdev, "   host resources\n");
    vmm_cdev_printf(cdev, "   host bus_list\n");
    vmm_cdev_printf(cdev, "   host bus_device_list <bus_name>\n");
    vmm_cdev_printf(cdev, "   host class_list\n");
    vmm_cdev_printf(cdev, "   host class_device_list <class_name>\n");
}

static int cmd_host_info(vmm_char_device_t *cdev)
{
    int                     rc;
    const char             *attr;
    uint64_t                hwid;
    vmm_device_tree_node_t *node;
    uint32_t                total = vmm_host_ram_total_frame_count();

    attr                          = NULL;
    node                          = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING);

    if (node) {
        vmm_device_tree_read_string(node, VMM_DEVICE_TREE_MODEL_ATTR_NAME, &attr);
        vmm_device_tree_dref_node(node);
    }

    if (attr) {
        vmm_cdev_printf(cdev, "%-25s: %s\n", "Host Name", attr);
    } else {
        vmm_cdev_printf(cdev, "%-25s: %s\n", "Host Name", CONFIG_BOARD);
    }

    rc = vmm_smp_map_hwid(vmm_smp_bootcpu_id(), &hwid);

    if (rc) {
        return rc;
    }

    vmm_cdev_printf(cdev, "%-25s: 0x%lx\n", "Boot CPU Hardware ID", hwid);
    vmm_cdev_printf(cdev, "%-25s: %u\n", "Total Online CPUs", vmm_num_online_cpus());
    vmm_cdev_printf(cdev, "%-25s: %lu MB\n", "Total VIRTUAL_ADDR_POOL", vmm_host_virtual_address_pool_size() / (1024UL * 1024UL));
    vmm_cdev_printf(cdev, "%-25s: %lu MB\n", "Total RAM", ((total * VMM_PAGE_SIZE) >> 20));

    arch_board_print_info(cdev);

    return VMM_OK;
}

static int cmd_host_cpu_info(vmm_char_device_t *cdev)
{
    int      rc;
    uint32_t c, khz;
    uint64_t hwid;
    char     name[25];

    vmm_cdev_printf(cdev, "%-25s: %s\n", "CPU Type", CONFIG_CPU);
    vmm_cdev_printf(cdev, "%-25s: %d\n", "CPU Present Count", vmm_num_present_cpus());
    vmm_cdev_printf(cdev, "%-25s: %d\n", "CPU Possible Count", vmm_num_possible_cpus());
    vmm_cdev_printf(cdev, "%-25s: %u\n", "CPU Online Count", vmm_num_online_cpus());
    arch_cpu_print_summary(cdev);
    vmm_cdev_printf(cdev, "\n");

    for_each_online_cpu(c)
    {
        rc = vmm_smp_map_hwid(c, &hwid);

        if (rc) {
            return rc;
        }

        vmm_sprintf(name, "CPU%d Hardware ID", c);
        vmm_cdev_printf(cdev, "%-25s: 0x%lx\n", name, hwid);

        vmm_sprintf(name, "CPU%d Estimated Speed", c);
        khz = vmm_delay_estimate_cpu_khz(c);
        vmm_cdev_printf(cdev, "%-25s: %d.%03d MHz\n", name, udiv32(khz, 1000), umod32(khz, 1000));

        arch_cpu_print(cdev, c);

        vmm_cdev_printf(cdev, "\n");
    }

    return VMM_OK;
}

static void host_cpu_poke_func(void *arg0, void *arg1, void *arg2)
{
    *((bool *)arg0) = TRUE;
}

static int cmd_host_cpu_poke(vmm_char_device_t *cdev, const vmm_cpumask_t *cmask)
{
    uint32_t c;
    uint64_t tstamp;
    bool    *poke;
    bool     free_poke = TRUE;

    poke               = vmm_zalloc(sizeof(*poke));

    if (!poke) {
        return VMM_ERR_NOMEM;
    }

    for_each_cpu(c, cmask)
    {
        vmm_cdev_printf(cdev, "CPU%d: Poke using async IPI ... ", c);

        *poke  = FALSE;
        tstamp = vmm_timer_timestamp() + 1000000000ULL;
        vmm_smp_ipi_async_call(vmm_cpumask_of(c), host_cpu_poke_func, poke, NULL, NULL);

        while (!(*poke)) {
            if (tstamp < vmm_timer_timestamp()) {
                free_poke = FALSE;
                break;
            }

            vmm_scheduler_yield();
        }

        vmm_cdev_printf(cdev, "%s\n", (*poke) ? "Done" : "Timeout");
    }

    if (free_poke) {
        vmm_free(poke);
    }

    return VMM_OK;
}

static int cmd_host_cpu_stats(vmm_char_device_t *cdev)
{
    int      rc;
    char     str[16];
    uint32_t c, p, khz, util;
    uint64_t hwid;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %4s %14s %15s %13s %12s %16s\n", "CPU#", "HWID", "Speed (MHz)", "Util. (%)", "IRQs (%)", "Active VCPUs");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");

    for_each_online_cpu(c)
    {
        vmm_cdev_printf(cdev, " %4d", c);

        rc = vmm_smp_map_hwid(c, &hwid);

        if (rc) {
            return rc;
        }

        vmm_snprintf(str, sizeof(str), "0x%lx", hwid);
        vmm_cdev_printf(cdev, " %14s", str);

        khz = vmm_delay_estimate_cpu_khz(c);
        vmm_cdev_printf(cdev, " %11d.%03d", udiv32(khz, 1000), umod32(khz, 1000));

        util = udiv64(vmm_scheduler_idle_time(c) * 1000, vmm_scheduler_get_sample_period(c));
        util = (util > 1000) ? 1000 : util;
        util = 1000 - util;
        vmm_cdev_printf(cdev, " %11d.%01d", udiv32(util, 10), umod32(util, 10));

        util = udiv64(vmm_scheduler_irq_time(c) * 1000, vmm_scheduler_get_sample_period(c));
        util = (util > 1000) ? 1000 : util;
        vmm_cdev_printf(cdev, " %10d.%01d", udiv32(util, 10), umod32(util, 10));

        util = 1;

        for (p = VMM_VCPU_MIN_PRIORITY; p <= VMM_VCPU_MAX_PRIORITY; p++) {
            util += vmm_scheduler_ready_count(c, p);
        }

        vmm_cdev_printf(cdev, " %15d ", util);

        vmm_cdev_printf(cdev, "\n");
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");

    return VMM_OK;
}

static void irq_stats_print(vmm_char_device_t *cdev, uint32_t irqno)
{
    vmm_host_irq_t      *irq;
    vmm_host_irq_chip_t *chip;
    const char               *irq_name;
    uint32_t                  cpu, stats;

    irq      = vmm_host_irq_get(irqno);
    irq_name = vmm_host_irq_get_name(irq);

    if (!irq || !irq_name || vmm_host_irq_is_disabled(irq) || vmm_host_irq_is_chained(irq)) {
        return;
    }

    chip = vmm_host_irq_get_chip(irq);

    if (!chip || !chip->name) {
        return;
    }

    vmm_cdev_printf(cdev, " %-7d %-7d %-20s %-16s", irqno, irq->hw_irq_num, irq_name, chip->name);
    for_each_online_cpu(cpu)
    {
        stats = vmm_host_irq_get_count(irq, cpu);
        vmm_cdev_printf(cdev, " %-10d", stats);
    }
    vmm_cdev_printf(cdev, "\n");
}

static void cmd_host_irq_stats(vmm_char_device_t *cdev)
{
    uint32_t num, cpu, irq_count, extend_irq_count;

    vmm_cdev_printf(cdev, "------------------------------------------------------");
    for_each_online_cpu(cpu)
    {
        vmm_cdev_printf(cdev, "-----------");
    }
    vmm_cdev_printf(cdev, "\n");
    vmm_cdev_printf(cdev, " %-7s %-7s %-20s %-16s", "IRQ#", "HWIRQ#", "Name", "Chip");
    for_each_online_cpu(cpu)
    {
        vmm_cdev_printf(cdev, " CPU%-7d", cpu);
    }
    vmm_cdev_printf(cdev, "\n");
    vmm_cdev_printf(cdev, "------------------------------------------------------");
    for_each_online_cpu(cpu)
    {
        vmm_cdev_printf(cdev, "-----------");
    }
    vmm_cdev_printf(cdev, "\n");

    irq_count = vmm_host_irq_count();

    for (num = 0; num < irq_count; num++) {
        irq_stats_print(cdev, num);
    }

    extend_irq_count = vmm_host_extend_irq_count();

    for (num = irq_count; num < irq_count + extend_irq_count; num++) {
        irq_stats_print(cdev, num);
    }

    vmm_cdev_printf(cdev, "------------------------------------------------------");
    for_each_online_cpu(cpu)
    {
        vmm_cdev_printf(cdev, "-----------");
    }
    vmm_cdev_printf(cdev, "\n");
}

static int cmd_host_irq_set_affinity(vmm_char_device_t *cdev, uint32_t hirq, uint32_t host_cpu)
{
    if (CONFIG_CPU_COUNT <= host_cpu) {
        vmm_cdev_printf(cdev, "%s: invalid host CPU%d\n", __func__, host_cpu);
        return VMM_ERR_INVALID;
    }

    if (!vmm_cpu_online(host_cpu)) {
        vmm_cdev_printf(cdev, "%s: host CPU%d not online\n", __func__, host_cpu);
        return VMM_ERR_INVALID;
    }

    return vmm_host_irq_set_affinity(hirq, vmm_cpumask_of(host_cpu), TRUE);
}

static void cmd_host_extirq_stats(vmm_char_device_t *cdev)
{
    vmm_host_extend_irq_debug_dump(cdev);
}

static void cmd_host_addr_space_info(vmm_char_device_t *cdev)
{
    uint32_t free  = vmm_host_memory_map_hash_free_count();
    uint32_t total = vmm_host_memory_map_hash_total_count();

    vmm_cdev_printf(cdev, "Memmap Free Entry   : %u (0x%08x)\n", free, free);
    vmm_cdev_printf(cdev, "Memmap Total Entry  : %u (0x%08x)\n", total, total);
    vmm_cdev_printf(cdev, "\n");

    arch_cpu_addr_space_print_info(cdev);
}

static void cmd_host_ram_info(vmm_char_device_t *cdev)
{
    uint32_t        bn, bank_count = vmm_host_ram_bank_count();
    uint32_t        free  = vmm_host_ram_total_free_frames();
    uint32_t        count = vmm_host_ram_total_frame_count();
    physical_addr_t start;
    physical_size_t size;

    vmm_cdev_printf(cdev, "Frame Size        : %lu (0x%08lx)\n", VMM_PAGE_SIZE, VMM_PAGE_SIZE);
    vmm_cdev_printf(cdev, "Color Operations  : %s\n", vmm_host_ram_color_ops_name());
    vmm_cdev_printf(cdev, "Color Order       : %u (0x%08x)\n", vmm_host_ram_color_order(), vmm_host_ram_color_order());
    vmm_cdev_printf(cdev, "Color Count       : %u (0x%08x)\n", vmm_host_ram_color_count(), vmm_host_ram_color_count());
    vmm_cdev_printf(cdev, "Bank Count        : %d (0x%08x)\n", bank_count, bank_count);
    vmm_cdev_printf(cdev, "Total Free Frames : %d (0x%08x)\n", free, free);
    vmm_cdev_printf(cdev, "Total Frame Count : %d (0x%08x)\n", count, count);

    for (bn = 0; bn < bank_count; bn++) {
        start = vmm_host_ram_bank_start(bn);
        size  = vmm_host_ram_bank_size(bn);
        free  = vmm_host_ram_bank_free_frames(bn);
        count = vmm_host_ram_bank_frame_count(bn);
        vmm_cdev_printf(cdev, "\n");
        vmm_cdev_printf(cdev, "Bank%02d Start      : 0x%" PRIPADDR "\n", bn, start);
        vmm_cdev_printf(cdev, "Bank%02d Size       : 0x%" PRIPADDR "\n", bn, size);
        vmm_cdev_printf(cdev, "Bank%02d Free Frames: %d (0x%08x)\n", bn, free, free);
        vmm_cdev_printf(cdev, "Bank%02d Frame Count: %d (0x%08x)\n", bn, count, count);
    }
}

static int cmd_host_ram_reserve(vmm_char_device_t *cdev, physical_addr_t paddr, int size)
{
    return vmm_host_ram_reserve(paddr, size);
}

static void cmd_host_ram_bitmap(vmm_char_device_t *cdev, int colcnt)
{
    uint32_t        ite, count, bn, bank_count = vmm_host_ram_bank_count();
    physical_addr_t start;

    for (bn = 0; bn < bank_count; bn++) {
        if (bn) {
            vmm_cdev_printf(cdev, "\n");
        }

        start = vmm_host_ram_bank_start(bn);
        count = vmm_host_ram_bank_frame_count(bn);
        vmm_cdev_printf(cdev, "Bank%02d\n", bn);
        vmm_cdev_printf(cdev, "0 : free\n");
        vmm_cdev_printf(cdev, "1 : used");

        for (ite = 0; ite < count; ite++) {
            if (umod32(ite, colcnt) == 0) {
                vmm_cdev_printf(cdev, "\n0x%" PRIPADDR ": ", (physical_addr_t)(start + ite * VMM_PAGE_SIZE));
            }

            if (vmm_host_ram_frame_isfree(start + ite * VMM_PAGE_SIZE)) {
                vmm_cdev_printf(cdev, "0");
            } else {
                vmm_cdev_printf(cdev, "1");
            }
        }

        vmm_cdev_printf(cdev, "\n");
    }
}

static void cmd_host_virtual_address_pool_info(vmm_char_device_t *cdev)
{
    uint32_t       free  = vmm_host_virtual_address_pool_free_page_count();
    uint32_t       total = vmm_host_virtual_address_pool_total_page_count();
    virtual_addr_t base  = vmm_host_virtual_address_pool_base();

    vmm_cdev_printf(cdev, "Base Address : 0x%" PRIADDR "\n", base);
    vmm_cdev_printf(cdev, "Page Size    : %lu (0x%08lx)\n", VMM_PAGE_SIZE, VMM_PAGE_SIZE);
    vmm_cdev_printf(cdev, "Free Pages   : %u (0x%08x)\n", free, free);
    vmm_cdev_printf(cdev, "Total Pages  : %u (0x%08x)\n", total, total);
}

static int cmd_host_virtual_address_pool_state(vmm_char_device_t *cdev)
{
    return vmm_host_virtual_address_pool_print_state(cdev);
}

static void cmd_host_virtual_address_pool_bitmap(vmm_char_device_t *cdev, int colcnt)
{
    uint32_t       ite, total = vmm_host_virtual_address_pool_total_page_count();
    virtual_addr_t base = vmm_host_virtual_address_pool_base();

    vmm_cdev_printf(cdev, "0 : free\n");
    vmm_cdev_printf(cdev, "1 : used");

    for (ite = 0; ite < total; ite++) {
        if (umod32(ite, colcnt) == 0) {
            vmm_cdev_printf(cdev, "\n0x%" PRIADDR ": ", (virtual_addr_t)(base + ite * VMM_PAGE_SIZE));
        }

        if (vmm_host_virtual_address_pool_page_isfree(base + ite * VMM_PAGE_SIZE)) {
            vmm_cdev_printf(cdev, "0");
        } else {
            vmm_cdev_printf(cdev, "1");
        }
    }

    vmm_cdev_printf(cdev, "\n");
}

static int cmd_host_page_pool_info(vmm_char_device_t *cdev)
{
    int            i;
    uint32_t       entry_count      = 0;
    uint32_t       huge_page_count   = 0;
    uint32_t       page_count       = 0;
    uint32_t       page_avail_count = 0;
    virtual_size_t space            = 0;
    uint64_t       pre, size;

    for (i = 0; i < VMM_PAGE_POOL_MAX; i++) {
        space += vmm_page_pool_space(i);
        entry_count += vmm_page_pool_entry_count(i);
        huge_page_count += vmm_page_pool_huge_page_count(i);
        page_count += vmm_page_pool_page_count(i);
        page_avail_count += vmm_page_pool_page_avail_count(i);
    }

    vmm_cdev_printf(cdev, "Entry Count      : %d (0x%08x)\n", entry_count, entry_count);
    vmm_cdev_printf(cdev, "huge_page Count   : %d (0x%08x)\n", huge_page_count, huge_page_count);
    vmm_cdev_printf(cdev, "Avail Page Count : %d (0x%08x)\n", page_avail_count, page_avail_count);
    vmm_cdev_printf(cdev, "Total Page Count : %d (0x%08x)\n", page_count, page_count);
    size = space;
    pre  = 1000;
    size = (size * pre) >> 10;
    vmm_cdev_printf(cdev, "Total Space      : %" PRId64 ".%03" PRId64 " KB\n", udiv64(size, pre), umod64(size, pre));

    return VMM_OK;
}

static int cmd_host_page_pool_state(vmm_char_device_t *cdev)
{
    int            i;
    uint32_t       _entry_count, entry_count           = 0;
    uint32_t       _huge_page_count, huge_page_count     = 0;
    uint32_t       _page_count, page_count             = 0;
    uint32_t       _page_avail_count, page_avail_count = 0;
    virtual_size_t _space, space                       = 0;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    vmm_cdev_printf(cdev, " %-20s %-11s %-10s %-10s %-11s %-11s\n", "Name", "Space (KB)", "Entries", "huge_pages", "AvailPages", "TotalPages");

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    for (i = 0; i < VMM_PAGE_POOL_MAX; i++) {
        _space            = vmm_page_pool_space(i);
        _entry_count      = vmm_page_pool_entry_count(i);
        _huge_page_count   = vmm_page_pool_huge_page_count(i);
        _page_count       = vmm_page_pool_page_count(i);
        _page_avail_count = vmm_page_pool_page_avail_count(i);

        vmm_cdev_printf(
            cdev, " %-20s %-11d %-10d %-10d %-11d %-11d\n", vmm_page_pool_name(i), (uint32_t)(_space >> 10), _entry_count, _huge_page_count,
            _page_avail_count, _page_count);

        space += _space;
        entry_count += _entry_count;
        huge_page_count += _huge_page_count;
        page_count += _page_count;
        page_avail_count += _page_avail_count;
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    vmm_cdev_printf(
        cdev, " %-20s %-11d %-10d %-10d %-11d %-11d\n", "TOTAL", (uint32_t)(space >> 10), entry_count, huge_page_count, page_avail_count, page_count);

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    return VMM_OK;
}

static int cmd_host_resources_print(const char *name, uint64_t start, uint64_t end, uint64_t flags, int level, void *arg)
{
    vmm_char_device_t *cdev = arg;

    while (level > 0) {
        vmm_cdev_puts(cdev, "   ");
        level--;
    }

    vmm_cdev_printf(cdev, "[0x%016" PRIX64 "-0x%016" PRIX64 "] (0x%08x) %s\n", start, end, (uint32_t)flags, (name) ? name : "Unknown");

    return VMM_OK;
}

static void cmd_host_resources(vmm_char_device_t *cdev)
{
    vmm_walk_tree_res(&vmm_hostio_resource, cdev, cmd_host_resources_print);

    vmm_walk_tree_res(&vmm_hostmem_resource, cdev, cmd_host_resources_print);
}

struct cmd_host_list_iter {
    uint32_t           num;
    vmm_char_device_t *cdev;
};

static int cmd_host_bus_list_iter(vmm_bus_t *b, void *data)
{
    uint32_t                   dcount;
    struct cmd_host_list_iter *p = data;

    dcount                       = vmm_device_driver_bus_device_count(b);
    vmm_cdev_printf(p->cdev, " %-7d %-15s %-15d\n", p->num++, b->name, dcount);

    return VMM_OK;
}

static void cmd_host_bus_list(vmm_char_device_t *cdev)
{
    struct cmd_host_list_iter p = {.num = 0, .cdev = cdev};

    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-7s %-15s %-15s\n", "Num#", "Bus Name", "Device Count");
    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_device_driver_bus_iterate(NULL, &p, cmd_host_bus_list_iter);
    vmm_cdev_printf(cdev, "----------------------------------------\n");
}

static int cmd_host_bus_device_list_iter(vmm_device_t *d, void *data)
{
    struct cmd_host_list_iter *p = data;

    vmm_cdev_printf(p->cdev, " %-7d %-25s %-25s\n", p->num++, d->name, (d->parent) ? d->parent->name : "---");

    return VMM_OK;
}

static int cmd_host_bus_device_list(vmm_char_device_t *cdev, const char *bus_name)
{
    vmm_bus_t                *b;
    struct cmd_host_list_iter p = {.num = 0, .cdev = cdev};

    b                           = vmm_device_driver_find_bus(bus_name);

    if (!b) {
        vmm_cdev_printf(cdev, "Failed to find %s bus\n", bus_name);
        return VMM_ERR_NOTAVAIL;
    }

    vmm_cdev_printf(cdev, "----------------------------------------");
    vmm_cdev_printf(cdev, "--------------------\n");
    vmm_cdev_printf(cdev, " %-7s %-25s %-25s\n", "Num#", "Device Name", "Parent Name");
    vmm_cdev_printf(cdev, "----------------------------------------");
    vmm_cdev_printf(cdev, "--------------------\n");
    vmm_device_driver_bus_device_iterate(b, NULL, &p, cmd_host_bus_device_list_iter);
    vmm_cdev_printf(cdev, "----------------------------------------");
    vmm_cdev_printf(cdev, "--------------------\n");

    return VMM_OK;
}

static int cmd_host_class_list_iter(vmm_class_t *c, void *data)
{
    uint32_t                   dcount;
    struct cmd_host_list_iter *p = data;

    dcount                       = vmm_device_driver_class_device_count(c);
    vmm_cdev_printf(p->cdev, " %-7d %-15s %-15d\n", p->num++, c->name, dcount);

    return VMM_OK;
}

static void cmd_host_class_list(vmm_char_device_t *cdev)
{
    struct cmd_host_list_iter p = {.num = 0, .cdev = cdev};

    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-7s %-15s %-15s\n", "Num#", "Class Name", "Device Count");
    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_device_driver_class_iterate(NULL, &p, cmd_host_class_list_iter);
    vmm_cdev_printf(cdev, "----------------------------------------\n");
}

static int cmd_host_class_device_list_iter(vmm_device_t *d, void *data)
{
    struct cmd_host_list_iter *p = data;

    vmm_cdev_printf(p->cdev, " %-7d %-25s %-25s\n", p->num++, d->name, (d->parent) ? d->parent->name : "---");

    return VMM_OK;
}

static int cmd_host_class_device_list(vmm_char_device_t *cdev, const char *class_name)
{
    vmm_class_t              *c;
    struct cmd_host_list_iter p = {.num = 0, .cdev = cdev};

    c                           = vmm_device_driver_find_class(class_name);

    if (!c) {
        vmm_cdev_printf(cdev, "Failed to find %s class\n", class_name);
        return VMM_ERR_NOTAVAIL;
    }

    vmm_cdev_printf(cdev, "----------------------------------------");
    vmm_cdev_printf(cdev, "--------------------\n");
    vmm_cdev_printf(cdev, " %-7s %-25s %-25s\n", "Num#", "Device Name", "Parent Name");
    vmm_cdev_printf(cdev, "----------------------------------------");
    vmm_cdev_printf(cdev, "--------------------\n");
    vmm_device_driver_class_device_iterate(c, NULL, &p, cmd_host_class_device_list_iter);
    vmm_cdev_printf(cdev, "----------------------------------------");
    vmm_cdev_printf(cdev, "--------------------\n");

    return VMM_OK;
}

static int cmd_host_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    const vmm_cpumask_t *cmask;
    int                  hirq, host_cpu, colcnt, size;
    physical_addr_t      physaddr;

    if (argc <= 1) {
        goto fail;
    }

    if (strcmp(argv[1], "help") == 0) {
        cmd_host_usage(cdev);
        return VMM_OK;
    } else if (strcmp(argv[1], "info") == 0) {
        return cmd_host_info(cdev);
    } else if ((strcmp(argv[1], "cpu") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "info") == 0) {
            return cmd_host_cpu_info(cdev);
        } else if (strcmp(argv[2], "poke") == 0) {
            host_cpu = (3 < argc) ? atoi(argv[3]) : -1;

            if (host_cpu >= 0 && vmm_cpu_online(host_cpu)) {
                cmask = vmm_cpumask_of(host_cpu);
            } else {
                cmask = cpu_online_mask;
            }

            return cmd_host_cpu_poke(cdev, cmask);
        } else if (strcmp(argv[2], "stats") == 0) {
            return cmd_host_cpu_stats(cdev);
        }
    } else if ((strcmp(argv[1], "irq") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "stats") == 0) {
            cmd_host_irq_stats(cdev);
            return VMM_OK;
        } else if ((strcmp(argv[2], "set_affinity") == 0) && (4 < argc)) {
            hirq     = atoi(argv[3]);
            host_cpu = atoi(argv[4]);
            return cmd_host_irq_set_affinity(cdev, hirq, host_cpu);
        }
    } else if ((strcmp(argv[1], "extirq") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "stats") == 0) {
            cmd_host_extirq_stats(cdev);
            return VMM_OK;
        }
    } else if ((strcmp(argv[1], "addr_space") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "info") == 0) {
            cmd_host_addr_space_info(cdev);
            return VMM_OK;
        }
    } else if ((strcmp(argv[1], "ram") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "info") == 0) {
            cmd_host_ram_info(cdev);
            return VMM_OK;
        } else if (strcmp(argv[2], "bitmap") == 0) {
            if (3 < argc) {
                colcnt = atoi(argv[3]);
            } else {
                colcnt = 64;
            }

            cmd_host_ram_bitmap(cdev, colcnt);
            return VMM_OK;
        } else if (strcmp(argv[2], "reserve") == 0 && 4 < argc) {
            physaddr = strtoul(argv[3], NULL, 16);
            size     = strtoul(argv[4], NULL, 16);
            return cmd_host_ram_reserve(cdev, physaddr, size);
        }
    } else if ((strcmp(argv[1], "virtual_address_pool") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "info") == 0) {
            cmd_host_virtual_address_pool_info(cdev);
            return VMM_OK;
        } else if (strcmp(argv[2], "state") == 0) {
            return cmd_host_virtual_address_pool_state(cdev);
        } else if (strcmp(argv[2], "bitmap") == 0) {
            if (3 < argc) {
                colcnt = atoi(argv[3]);
            } else {
                colcnt = 64;
            }

            cmd_host_virtual_address_pool_bitmap(cdev, colcnt);
            return VMM_OK;
        }
    } else if ((strcmp(argv[1], "page_pool") == 0) && (2 < argc)) {
        if (strcmp(argv[2], "info") == 0) {
            cmd_host_page_pool_info(cdev);
            return VMM_OK;
        } else if (strcmp(argv[2], "state") == 0) {
            return cmd_host_page_pool_state(cdev);
        }
    } else if ((strcmp(argv[1], "resources") == 0) && (2 == argc)) {
        cmd_host_resources(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "bus_list") == 0) && (2 == argc)) {
        cmd_host_bus_list(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "bus_device_list") == 0) && (3 == argc)) {
        cmd_host_bus_device_list(cdev, argv[2]);
        return VMM_OK;
    } else if ((strcmp(argv[1], "class_list") == 0) && (2 == argc)) {
        cmd_host_class_list(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "class_device_list") == 0) && (3 == argc)) {
        cmd_host_class_device_list(cdev, argv[2]);
        return VMM_OK;
    }

fail:
    cmd_host_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_host = {
    .name  = "host",
    .desc  = "host information",
    .usage = cmd_host_usage,
    .exec  = cmd_host_exec,
};

static int __init cmd_host_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_host);
}

static void __exit cmd_host_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_host);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
