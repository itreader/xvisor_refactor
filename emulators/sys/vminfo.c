/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vminfo.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Guest/VM Info Emulator.
 */

#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_timer.h>

#define MODULE_DESC        "Guest/VM Info Emulator"
#define MODULE_AUTHOR      "Anup Patel"
#define MODULE_LICENSE     "GPL"
#define MODULE_IPRIORITY   0
#define MODULE_INIT        vminfo_emulator_init
#define MODULE_EXIT        vminfo_emulator_exit

#define VMINFO_MAGIC       0xa4a297a6 /* virt */
#define VMINFO_VENDOR      0x52535658 /* XVSR */
#define VMINFO_VERSION_0_1 0x00000001

struct vminfo_state {
    vmm_emulate_device_t *edev;
    struct vmm_guest     *guest;
    vmm_notifier_block_t  nb;
    vmm_spinlock_t        lock;
    uint32_t              magic;
    uint32_t              vendor;
    uint32_t              version;
    uint32_t              vcpu_count;
    uint32_t              boot_delay;
    uint32_t              reserved1;
    uint32_t              reserved2[6];
    uint32_t              ram0_base_ms;
    uint32_t              ram0_base_ls;
    uint32_t              ram0_size_ms;
    uint32_t              ram0_size_ls;
    uint32_t              ram1_base_ms;
    uint32_t              ram1_base_ls;
    uint32_t              ram1_size_ms;
    uint32_t              ram1_size_ls;
    uint32_t              ram2_base_ms;
    uint32_t              ram2_base_ls;
    uint32_t              ram2_size_ms;
    uint32_t              ram2_size_ls;
    uint32_t              ram3_base_ms;
    uint32_t              ram3_base_ls;
    uint32_t              ram3_size_ms;
    uint32_t              ram3_size_ls;
};

static int vminfo_emulator_read(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst, uint32_t size)
{
    int                  rc = VMM_OK;
    struct vminfo_state *s  = edev->private;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case 0x00: /* MAGIC */
            *dst = s->magic;
            break;

        case 0x04: /* VENDOR */
            *dst = s->vendor;
            break;

        case 0x08: /* VERSION */
            *dst = s->version;
            break;

        case 0x0c: /* VCPU_COUNT */
            *dst = s->vcpu_count;
            break;

        case 0x10: /* BOOT_DELAY */
            *dst = s->boot_delay;
            break;

        case 0x14: /* RESERVED */
            *dst = s->reserved1;
            break;

        case 0x18: /* CLOCKSOURCE_FREQ_MS */
            *dst = 0;
            break;

        case 0x1c: /* CLOCKSOURCE_FREQ_LS */
            *dst = vmm_timer_clocksource_frequency();
            break;

        case 0x20: /* CLOCKCHIP_FREQ_MS */
            *dst = 0;
            break;

        case 0x24: /* CLOCKCHIP_FREQ_LS */
            *dst = vmm_timer_clock_chip_frequency();
            break;

        case 0x28: /* RESERVED */
        case 0x2c: /* RESERVED */
        case 0x30: /* RESERVED */
        case 0x34: /* RESERVED */
        case 0x38: /* RESERVED */
        case 0x3c: /* RESERVED */
            *dst = s->reserved2[(offset - 0x28) >> 2];
            break;

        case 0x40: /* RAM0_BASE_MS */
            *dst = s->ram0_base_ms;
            break;

        case 0x44: /* RAM0_BASE_LS */
            *dst = s->ram0_base_ls;
            break;

        case 0x48: /* RAM0_SIZE_MS */
            *dst = s->ram0_size_ms;
            break;

        case 0x4c: /* RAM0_SIZE_LS */
            *dst = s->ram0_size_ls;
            break;

        case 0x50: /* RAM1_BASE_MS */
            *dst = s->ram1_base_ms;
            break;

        case 0x54: /* RAM1_BASE_LS */
            *dst = s->ram1_base_ls;
            break;

        case 0x58: /* RAM1_SIZE_MS */
            *dst = s->ram1_size_ms;
            break;

        case 0x5c: /* RAM1_SIZE_LS */
            *dst = s->ram1_size_ls;
            break;

        case 0x60: /* RAM2_BASE_MS */
            *dst = s->ram2_base_ms;
            break;

        case 0x64: /* RAM2_BASE_LS */
            *dst = s->ram2_base_ls;
            break;

        case 0x68: /* RAM2_SIZE_MS */
            *dst = s->ram2_size_ms;
            break;

        case 0x6c: /* RAM2_SIZE_LS */
            *dst = s->ram2_size_ls;
            break;

        case 0x70: /* RAM3_BASE_MS */
            *dst = s->ram3_base_ms;
            break;

        case 0x74: /* RAM3_BASE_LS */
            *dst = s->ram3_base_ls;
            break;

        case 0x78: /* RAM3_SIZE_MS */
            *dst = s->ram3_size_ms;
            break;

        case 0x7c: /* RAM3_SIZE_LS */
            *dst = s->ram3_size_ls;
            break;

        default:
            rc = VMM_ERR_FAIL;
            break;
    };

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int vminfo_emulator_write(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t regmask, uint32_t regval, uint32_t size)
{
    /* We don't allow writes */
    return VMM_ERR_NOTSUPP;
}

static int vminfo_emulator_reset(vmm_emulate_device_t *edev)
{
    /* Nothing to do here. */
    return VMM_OK;
}

static int vminfo_guest_address_space_notification(vmm_notifier_block_t *nb, uint64_t evt, void *data)
{
    physical_addr_t                       paddr;
    physical_size_t                       psize;
    struct vmm_region                    *reg;
    struct vmm_guest_address_space_event *edata = data;
    struct vminfo_state                  *s     = container_of(nb, struct vminfo_state, nb);

    if (evt != VMM_GUEST_ADDRESS_SPACE_EVENT_INIT) {
        /* We are only interested in unregister events so,
         * don't care about this event.
         */
        return NOTIFY_DONE;
    }

    if (s->guest != edata->guest) {
        /* We are only interested in events for our guest */
        return NOTIFY_DONE;
    }

    if (!vmm_device_tree_read_physaddr(s->edev->node, "ram0_base", &paddr)) {
        reg = vmm_guest_find_region(s->guest, paddr, VMM_REGION_MEMORY, FALSE);

        if (reg) {
            psize           = VMM_REGION_GPHYS_END(reg) - paddr;
            s->ram0_base_ms = ((uint64_t)paddr >> 32) & 0xffffffff;
            s->ram0_base_ls = paddr & 0xffffffff;
            s->ram0_size_ms = ((uint64_t)psize >> 32) & 0xffffffff;
            s->ram0_size_ls = psize & 0xffffffff;
        }
    }

    if (!vmm_device_tree_read_physaddr(s->edev->node, "ram1_base", &paddr)) {
        reg = vmm_guest_find_region(s->guest, paddr, VMM_REGION_MEMORY, FALSE);

        if (reg) {
            psize           = VMM_REGION_GPHYS_END(reg) - paddr;
            s->ram1_base_ms = ((uint64_t)paddr >> 32) & 0xffffffff;
            s->ram1_base_ls = paddr & 0xffffffff;
            s->ram1_size_ms = ((uint64_t)psize >> 32) & 0xffffffff;
            s->ram1_size_ls = psize & 0xffffffff;
        }
    }

    if (!vmm_device_tree_read_physaddr(s->edev->node, "ram2_base", &paddr)) {
        reg = vmm_guest_find_region(s->guest, paddr, VMM_REGION_MEMORY, FALSE);

        if (reg) {
            psize           = VMM_REGION_GPHYS_END(reg) - paddr;
            s->ram2_base_ms = ((uint64_t)paddr >> 32) & 0xffffffff;
            s->ram2_base_ls = paddr & 0xffffffff;
            s->ram2_size_ms = ((uint64_t)psize >> 32) & 0xffffffff;
            s->ram2_size_ls = psize & 0xffffffff;
        }
    }

    if (!vmm_device_tree_read_physaddr(s->edev->node, "ram3_base", &paddr)) {
        reg = vmm_guest_find_region(s->guest, paddr, VMM_REGION_MEMORY, FALSE);

        if (reg) {
            psize           = VMM_REGION_GPHYS_END(reg) - paddr;
            s->ram3_base_ms = ((uint64_t)paddr >> 32) & 0xffffffff;
            s->ram3_base_ls = paddr & 0xffffffff;
            s->ram3_size_ms = ((uint64_t)psize >> 32) & 0xffffffff;
            s->ram3_size_ls = psize & 0xffffffff;
        }
    }

    return NOTIFY_OK;
}

static int vminfo_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                  rc;
    struct vminfo_state *s;

    s = vmm_zalloc(sizeof(struct vminfo_state));

    if (!s) {
        return VMM_ERR_NOMEM;
    }

    s->edev  = edev;
    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    s->magic      = VMINFO_MAGIC;
    s->vendor     = VMINFO_VENDOR;
    s->version    = (uint32_t)((uint64_t)eid->data);
    s->vcpu_count = guest->vcpu_count;

    if (vmm_device_tree_read_u32(s->edev->node, "boot_delay", &s->boot_delay)) {
        s->boot_delay = 0xffffffff;
    }

    s->nb.notifier_call = &vminfo_guest_address_space_notification;
    s->nb.priority      = 0;
    rc                  = vmm_guest_address_space_register_client(&s->nb);

    if (rc) {
        vmm_free(s);
        return rc;
    }

    edev->private = s;

    return VMM_OK;
}

static int vminfo_emulator_remove(vmm_emulate_device_t *edev)
{
    struct vminfo_state *s = edev->private;

    if (s) {
        vmm_guest_address_space_unregister_client(&s->nb);
        vmm_free(s);
        edev->private = NULL;
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid vminfo_emuid_table[] = {
    {
     .type       = "sys",
     .compatible = "vminfo-0.1",
     .data       = (void *)VMINFO_VERSION_0_1,
     },
    {/* end of list */},
};

VMM_DECLARE_EMULATOR_SIMPLE(
    vminfo_emulator, "vminfo", vminfo_emuid_table, VMM_DEVICE_EMULATE_LITTLE_ENDIAN, vminfo_emulator_probe, vminfo_emulator_remove,
    vminfo_emulator_reset, NULL, vminfo_emulator_read, vminfo_emulator_write);

static int __init vminfo_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&vminfo_emulator);
}

static void __exit vminfo_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&vminfo_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
