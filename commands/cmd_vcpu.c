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
 * @file cmd_vcpu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vcpu command
 */

#include <arch_vcpu.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_delay.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command vcpu"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_vcpu_init
#define MODULE_EXIT      cmd_vcpu_exit

static void cmd_vcpu_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   vcpu help\n");
    vmm_cdev_printf(cdev, "   vcpu list\n");
    vmm_cdev_printf(cdev, "   vcpu orphan_list\n");
    vmm_cdev_printf(cdev, "   vcpu normal_list\n");
    vmm_cdev_printf(cdev, "   vcpu monitor [<output_char_device_name>]\n");
    vmm_cdev_printf(cdev, "   vcpu reset   <vcpu_id>\n");
    vmm_cdev_printf(cdev, "   vcpu kick    <vcpu_id>\n");
    vmm_cdev_printf(cdev, "   vcpu pause   <vcpu_id>\n");
    vmm_cdev_printf(cdev, "   vcpu resume  <vcpu_id>\n");
    vmm_cdev_printf(cdev, "   vcpu halt    <vcpu_id>\n");
    vmm_cdev_printf(cdev, "   vcpu set_hcpu <vcpu_id> <host_cpu>\n");
    vmm_cdev_printf(
        cdev, "   vcpu set_affinity <vcpu_id> "
              "<hcpu0> <hcpu1> <hcpu2> ...\n");
    vmm_cdev_printf(cdev, "   vcpu dumpreg <vcpu_id>\n");
    vmm_cdev_printf(cdev, "   vcpu dumpstat <vcpu_id>\n");
}

static int cmd_vcpu_help(vmm_char_device_t *cdev, int argc, char **argv)
{
    cmd_vcpu_usage(cdev);
    return VMM_OK;
}

struct vcpu_list_priv {
    vmm_char_device_t *cdev;
    bool               normal;
    bool               orphan;
};

static int vcpu_list_iter(vmm_vcpu_t *vcpu, void *private)
{
    uint32_t               host_cpu, afflen;
    char                   state[10];
    const vmm_cpumask_t   *aff;
    struct vcpu_list_priv *p    = private;
    vmm_char_device_t     *cdev = p->cdev;

    if (!(vcpu->is_normal && p->normal) && !(!vcpu->is_normal && p->orphan)) {
        return VMM_OK;
    }

    switch (vmm_manager_vcpu_get_state(vcpu)) {
        case VMM_VCPU_STATE_UNKNOWN:
            strcpy(state, "Unknown");
            break;

        case VMM_VCPU_STATE_RESET:
            strcpy(state, "Reset");
            break;

        case VMM_VCPU_STATE_READY:
            strcpy(state, "Ready");
            break;

        case VMM_VCPU_STATE_RUNNING:
            strcpy(state, "Running");
            break;

        case VMM_VCPU_STATE_PAUSED:
            strcpy(state, "Paused");
            break;

        case VMM_VCPU_STATE_HALTED:
            strcpy(state, "Halted");
            break;

        default:
            strcpy(state, "Invalid");
            break;
    }

    vmm_cdev_printf(cdev, " %-6d", vcpu->id);
#ifdef CONFIG_SMP
    vmm_manager_vcpu_get_hcpu(vcpu, &host_cpu);
    vmm_cdev_printf(cdev, " %-6d", host_cpu);
#endif
    vmm_cdev_printf(cdev, " %-7d %-10s %-17s", vcpu->priority, state, vcpu->name);
    vmm_cdev_printf(cdev, " %s", "{");
    aff    = vmm_manager_vcpu_get_affinity(vcpu);
    afflen = 0;
    for_each_cpu(host_cpu, aff)
    {
        if (afflen) {
            vmm_cdev_printf(cdev, ",");
        }

        vmm_cdev_printf(cdev, "%d", host_cpu);
        afflen++;
    }
    vmm_cdev_printf(cdev, "%s\n", "}");

    return VMM_OK;
}

static int vcpu_list(vmm_char_device_t *cdev, bool normal, bool orphan)
{
    int                   rc;
    struct vcpu_list_priv p;

    p.cdev   = cdev;
    p.normal = normal;
    p.orphan = orphan;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");
    vmm_cdev_printf(cdev, " %-6s", "ID ");
#ifdef CONFIG_SMP
    vmm_cdev_printf(cdev, " %-6s", "CPU ");
#endif
    vmm_cdev_printf(cdev, " %-7s %-10s %-17s %-34s\n", "Prio", "State", "Name", "Affinity");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    rc = vmm_manager_vcpu_iterate(vcpu_list_iter, &p);

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    return rc;
}

static int cmd_vcpu_list(vmm_char_device_t *cdev, int argc, char **argv)
{
    return vcpu_list(cdev, TRUE, TRUE);
}

static int cmd_vcpu_orphan_list(vmm_char_device_t *cdev, int argc, char **argv)
{
    return vcpu_list(cdev, FALSE, TRUE);
}

static int cmd_vcpu_normal_list(vmm_char_device_t *cdev, int argc, char **argv)
{
    return vcpu_list(cdev, TRUE, FALSE);
}

static int cmd_vcpu_monitor(vmm_char_device_t *cdev, int argc, char **argv)
{
    char               ch;
    bool               skip_sleep;
    bool               found_escape_ch;
    uint32_t           i, c, util;
    virtual_size_t     vfree, vtotal;
    physical_size_t    pfree, ptotal;
    vmm_char_device_t *ocdev = NULL;

    if (argc) {
        ocdev = vmm_char_device_find(argv[0]);
    }

    if (!ocdev) {
        ocdev = cdev;
    }

    while (1) {
        /* Reset cursor positon using VT100 command */
        vmm_cdev_puts(ocdev, "\e[H");

        /* Clear entire screen using VT100 command */
        vmm_cdev_puts(ocdev, "\e[J");

        /* Print CPU usage */
        i = 0;
        for_each_online_cpu(c)
        {
            vmm_cdev_printf(ocdev, "CPU%d:", c);
            util = udiv64(vmm_scheduler_idle_time(c) * 1000, vmm_scheduler_get_sample_period(c));
            util = (util > 1000) ? 1000 : util;
            util = 1000 - util;
            vmm_cdev_printf(ocdev, " %d.%01d%%  ", udiv32(util, 10), umod32(util, 10));
            i++;

            if (i % 4 == 0) {
                vmm_cdev_puts(ocdev, "\n");
            }
        }

        if (i % 4) {
            vmm_cdev_puts(ocdev, "\n");
        }

        /* Print VAPOOL usage */
        vfree = vmm_host_virtual_address_pool_free_page_count();
        vfree *= VMM_PAGE_SIZE;
        vtotal = vmm_host_virtual_address_pool_total_page_count();
        vtotal *= VMM_PAGE_SIZE;
        vmm_cdev_printf(
            ocdev,
            "VAPOOL: free %" PRISIZE "KiB  "
            "used %" PRISIZE "KiB  total %" PRISIZE "KiB\n",
            vfree / 1024, (vtotal - vfree) / 1024, vtotal / 1024);
        /* Print RAM usage */
        pfree = vmm_host_ram_total_free_frames();
        pfree *= VMM_PAGE_SIZE;
        ptotal = vmm_host_ram_total_frame_count();
        ptotal *= VMM_PAGE_SIZE;
        vmm_cdev_printf(
            ocdev,
            "RAM: free %" PRIPSIZE "KiB  "
            "used %" PRIPSIZE "KiB  total %" PRIPSIZE "KiB\n",
            pfree / 1024, (ptotal - pfree) / 1024, ptotal / 1024);

        /* Print VCPU list */
        vcpu_list(ocdev, TRUE, TRUE);

        /* Look for escape character 'q' */
        ch              = 0;
        skip_sleep      = FALSE;
        found_escape_ch = FALSE;

        while (!vmm_scanchars(cdev, &ch, 1, FALSE)) {
            skip_sleep = TRUE;

            if (ch == 'q') {
                found_escape_ch = TRUE;
                break;
            }
        }

        if (found_escape_ch) {
            break;
        }

        /* Sleep for 1 seconds */
        if (!skip_sleep) {
            vmm_ssleep(1);
        }
    }

    return VMM_OK;
}

static int cmd_vcpu_reset(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_reset(vcpu))) {
        vmm_cdev_printf(cdev, "%s: Failed to reset\n", vcpu->name);
    } else {
        vmm_cdev_printf(cdev, "%s: Reset\n", vcpu->name);
    }

    return ret;
}

static int cmd_vcpu_kick(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_kick(vcpu))) {
        vmm_cdev_printf(cdev, "%s: Failed to kick\n", vcpu->name);
    } else {
        vmm_cdev_printf(cdev, "%s: Kicked\n", vcpu->name);
    }

    return ret;
}

static int cmd_vcpu_pause(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_pause(vcpu))) {
        vmm_cdev_printf(cdev, "%s: Failed to pause\n", vcpu->name);
    } else {
        vmm_cdev_printf(cdev, "%s: Paused\n", vcpu->name);
    }

    return ret;
}

static int cmd_vcpu_resume(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_resume(vcpu))) {
        vmm_cdev_printf(cdev, "%s: Failed to resume\n", vcpu->name);
    } else {
        vmm_cdev_printf(cdev, "%s: Resumed\n", vcpu->name);
    }

    return ret;
}

static int cmd_vcpu_halt(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_halt(vcpu))) {
        vmm_cdev_printf(cdev, "%s: Failed to halt\n", vcpu->name);
    } else {
        vmm_cdev_printf(cdev, "%s: Halted\n", vcpu->name);
    }

    return ret;
}

static int cmd_vcpu_set_hcpu(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id, host_cpu;
    vmm_vcpu_t *vcpu;

    if (argc != 2) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID and host CPU\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id       = atoi(argv[0]);
    host_cpu = atoi(argv[1]);

    vcpu     = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_set_hcpu(vcpu, (uint32_t)host_cpu))) {
        vmm_cdev_printf(cdev, "%s: Failed to set host CPU%d\n", vcpu->name, host_cpu);
    } else {
        vmm_cdev_printf(cdev, "%s: Host CPU%d set\n", vcpu->name, host_cpu);
    }

    return ret;
}

static int cmd_vcpu_set_affinity(vmm_char_device_t *cdev, int argc, char **argv)
{
    int           ret, i, id, host_cpu;
    vmm_cpumask_t mask;
    vmm_vcpu_t   *vcpu;

    if (argc < 2) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID and host CPUs\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    mask = VMM_CPU_MASK_NONE;

    for (i = 1; i < argc; i++) {
        host_cpu = atoi(argv[i]);

        if (CONFIG_CPU_COUNT <= host_cpu) {
            vmm_cdev_printf(cdev, "Invalid host CPU%d (>= %d)\n", host_cpu, CONFIG_CPU_COUNT);
            return VMM_EINVALID;
        }

        if (!vmm_cpu_online(host_cpu)) {
            vmm_cdev_printf(cdev, "Host CPU%d not online\n", host_cpu);
            return VMM_EINVALID;
        }

        vmm_cpumask_set_cpu(host_cpu, &mask);
    }

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    if ((ret = vmm_manager_vcpu_set_affinity(vcpu, &mask))) {
        vmm_cdev_printf(cdev, "%s: Failed to set affinity\n", vcpu->name);
    } else {
        vmm_cdev_printf(cdev, "%s: Set affinity done\n", vcpu->name);
    }

    return ret;
}

static int cmd_vcpu_dumpreg(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         id;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    /* Architecture specific dumpreg */
    arch_vcpu_regs_dump(cdev, vcpu);

    return VMM_OK;
}

static void nsecs_to_hhmmsstt(uint64_t nsecs, uint32_t *hours, uint32_t *mins, uint32_t *secs, uint32_t *msecs)
{
    nsecs  = udiv64(nsecs, 1000000ULL);

    *msecs = umod64(nsecs, 1000ULL);
    nsecs  = udiv64(nsecs, 1000ULL);

    *secs  = umod64(nsecs, 60ULL);
    nsecs  = udiv64(nsecs, 60ULL);

    *mins  = umod64(nsecs, 60ULL);
    nsecs  = udiv64(nsecs, 60ULL);

    *hours = umod64(nsecs, 60ULL);
    nsecs  = udiv64(nsecs, 60ULL);
}

static int cmd_vcpu_dumpstat(vmm_char_device_t *cdev, int argc, char **argv)
{
    int         ret, id;
    uint8_t     priority;
    uint32_t    h, m, s, ms;
    uint32_t    state, host_cpu, reset_count;
    uint64_t    last_reset_nsecs, total_nsecs;
    uint64_t    ready_nsecs, running_nsecs, paused_nsecs;
    uint64_t    halted_nsecs, system_nsecs;
    vmm_vcpu_t *vcpu;

    if (!argc) {
        vmm_cdev_printf(cdev, "Must provide vcpu ID\n");
        cmd_vcpu_usage(cdev);
        return VMM_EINVALID;
    }

    id   = atoi(argv[0]);

    vcpu = vmm_manager_vcpu(id);

    if (!vcpu) {
        vmm_cdev_printf(cdev, "Failed to find vcpu\n");
        return VMM_EFAIL;
    }

    /* Retrive general statistics*/
    ret = vmm_scheduler_stats(
        vcpu, &state, &priority, &host_cpu, &reset_count, &last_reset_nsecs, &ready_nsecs, &running_nsecs, &paused_nsecs, &halted_nsecs,
        &system_nsecs);

    if (ret) {
        vmm_cdev_printf(cdev, "%s: Failed to get stats\n", vcpu->name);
        return ret;
    }

    /* General statistics */
    vmm_cdev_printf(cdev, "Name             : %s\n", vcpu->name);
    vmm_cdev_printf(cdev, "State            : ");

    switch (state) {
        case VMM_VCPU_STATE_UNKNOWN:
            vmm_cdev_printf(cdev, "Unknown\n");
            break;

        case VMM_VCPU_STATE_RESET:
            vmm_cdev_printf(cdev, "Reset\n");
            break;

        case VMM_VCPU_STATE_READY:
            vmm_cdev_printf(cdev, "Ready\n");
            break;

        case VMM_VCPU_STATE_RUNNING:
            vmm_cdev_printf(cdev, "Running\n");
            break;

        case VMM_VCPU_STATE_PAUSED:
            vmm_cdev_printf(cdev, "Paused\n");
            break;

        case VMM_VCPU_STATE_HALTED:
            vmm_cdev_printf(cdev, "Halted\n");
            break;

        default:
            vmm_cdev_printf(cdev, "Invalid\n");
            break;
    }

    vmm_cdev_printf(cdev, "Priority         : %d\n", priority);
#ifdef CONFIG_SMP
    vmm_cdev_printf(cdev, "Host CPU         : %d\n", host_cpu);
#endif
    vmm_cdev_printf(cdev, "\n");
    nsecs_to_hhmmsstt(ready_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "Ready Time       : %d:%02d:%02d:%03d\n", h, m, s, ms);
    nsecs_to_hhmmsstt(running_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "Running Time     : %d:%02d:%02d:%03d\n", h, m, s, ms);
    nsecs_to_hhmmsstt(paused_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "Paused Time      : %d:%02d:%02d:%03d\n", h, m, s, ms);
    nsecs_to_hhmmsstt(halted_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "Halted Time      : %d:%02d:%02d:%03d\n", h, m, s, ms);
    total_nsecs = ready_nsecs;
    total_nsecs += running_nsecs;
    total_nsecs += paused_nsecs;
    total_nsecs += halted_nsecs;
    nsecs_to_hhmmsstt(total_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "Total Time       : %d:%02d:%02d:%03d\n", h, m, s, ms);
    nsecs_to_hhmmsstt(system_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "System Time      : %d:%02d:%02d:%03d\n", h, m, s, ms);
    vmm_cdev_printf(cdev, "\n");
    vmm_cdev_printf(cdev, "Reset Count      : %d\n", reset_count);
    nsecs_to_hhmmsstt(last_reset_nsecs, &h, &m, &s, &ms);
    vmm_cdev_printf(cdev, "Last Reset Since : %d:%02d:%02d:%03d\n", h, m, s, ms);
    vmm_cdev_printf(cdev, "\n");

    /* Architecture specific dumpstat */
    arch_vcpu_stat_dump(cdev, vcpu);

    return ret;
}

static const struct {
    char *name;
    int (*function)(vmm_char_device_t *, int, char **);
    int argc;
} command[] = {
    {"help",         cmd_vcpu_help,         0},
    {"list",         cmd_vcpu_list,         0},
    {"orphan_list",  cmd_vcpu_orphan_list,  0},
    {"normal_list",  cmd_vcpu_normal_list,  0},
    {"monitor",      cmd_vcpu_monitor,      0},
    {"reset",        cmd_vcpu_reset,        1},
    {"kick",         cmd_vcpu_kick,         1},
    {"pause",        cmd_vcpu_pause,        1},
    {"resume",       cmd_vcpu_resume,       1},
    {"halt",         cmd_vcpu_halt,         1},
    {"set_hcpu",     cmd_vcpu_set_hcpu,     2},
    {"set_affinity", cmd_vcpu_set_affinity, 2},
    {"dumpreg",      cmd_vcpu_dumpreg,      1},
    {"dumpstat",     cmd_vcpu_dumpstat,     1},
    {NULL,           NULL,                  0},
};

static int cmd_vcpu_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    int index = 0;

    if (argc <= 1) {
        cmd_vcpu_usage(cdev);
        return VMM_EFAIL;
    }

    while (command[index].name) {
        if ((strcmp(argv[1], command[index].name) == 0) && ((argc - 2) >= command[index].argc)) {
            return command[index].function(cdev, argc - 2, &argv[2]);
        }

        index++;
    }

    cmd_vcpu_usage(cdev);

    return VMM_EFAIL;
}

static vmm_command_t cmd_vcpu = {
    .name  = "vcpu",
    .desc  = "control commands for vcpu",
    .usage = cmd_vcpu_usage,
    .exec  = cmd_vcpu_exec,
};

static int __init cmd_vcpu_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_vcpu);
}

static void __exit cmd_vcpu_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_vcpu);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
