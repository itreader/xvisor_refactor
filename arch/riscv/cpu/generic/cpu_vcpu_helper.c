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
 * @file cpu_vcpu_helper.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU helper functions
 */

#include <arch_barrier.h>
#include <arch_guest.h>
#include <arch_vcpu.h>
#include <generic_mmu.h>
#include <vio/vmm_vserial.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_page_pool.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>

#include <cpu_guest_serial.h>
#include <cpu_hwcap.h>
#include <cpu_sbi.h>
#include <cpu_tlb.h>
#include <cpu_vcpu_fp.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_trap.h>
#include <riscv_csr.h>
#include <riscv_lrsc.h>
#include <riscv_timex.h>

#define RISCV_ISA_ALLOWED                                                                                                                            \
    (riscv_isa_extension_mask(a) | riscv_isa_extension_mask(c) | riscv_isa_extension_mask(d) | riscv_isa_extension_mask(f) |                         \
     riscv_isa_extension_mask(i) | riscv_isa_extension_mask(m) | riscv_isa_extension_mask(h) | riscv_isa_extension_mask(SSTC))

static char *guest_fdt_find_serial_node(char *guest_name)
{
    char                   *serial = NULL;
    char                    chosen_node_path[VMM_FIELD_NAME_SIZE];
    vmm_device_tree_node_t *chosen_node;

    /* Process attributes in chosen node */
    vmm_snprintf(
        chosen_node_path, sizeof(chosen_node_path), "/%s/%s/%s", VMM_DEVICE_TREE_GUESTINFO_NODE_NAME, guest_name, VMM_DEVICE_TREE_CHOSEN_NODE_NAME);
    chosen_node = vmm_device_tree_getnode(chosen_node_path);

    if (chosen_node) {
        /* Process console device passed via chosen node */
        vmm_device_tree_read_string(chosen_node, VMM_DEVICE_TREE_STDOUT_ATTR_NAME, (const char **)&serial);
    }

    vmm_device_tree_dref_node(chosen_node);

    return serial;
}

static int guest_vserial_notification(vmm_notifier_block_t *nb, uint64_t evt, void *data)
{
    int                        ret     = NOTIFY_OK;
    struct vmm_vserial_event  *e       = data;
    struct riscv_guest_serial *gserial = container_of(nb, struct riscv_guest_serial, vser_client);

    if (evt == VMM_VSERIAL_EVENT_CREATE) {
        if (!strcmp(e->vser->name, gserial->name)) {
            gserial->vserial = e->vser;
        }
    } else if (evt == VMM_VSERIAL_EVENT_DESTROY) {
        if (!strcmp(e->vser->name, gserial->name)) {
            gserial->vserial = NULL;
        }
    } else {
        ret = NOTIFY_DONE;
    }

    return ret;
}

int arch_guest_init(struct vmm_guest *guest)
{
    struct riscv_guest_priv *private;
    struct riscv_guest_serial *gserial;
    uint32_t                   page_table_attr, page_table_hw_tag;
    char                      *sname;
    int                        rc;

    if (!guest->reset_count) {
        if (!riscv_isa_extension_available(NULL, h) || !sbi_has_0_2_rfence()) {
            return VMM_EINVALID;
        }

        guest->arch_private = vmm_malloc(sizeof(struct riscv_guest_private));

        if (!guest->arch_private) {
            return VMM_ENOMEM;
        }

        private             = riscv_guest_private(guest);

        private->time_delta = -get_cycles64();

        page_table_hw_tag   = 0;
        page_table_attr     = MMU_ATTR_REMOTE_TLB_FLUSH;

        if (riscv_stage2_vmid_available()) {
            page_table_hw_tag = guest->id;
            page_table_attr |= MMU_ATTR_HW_TAG_VALID;
        }

        private->page_table = mmu_page_table_alloc(MMU_STAGE2, -1, page_table_attr, page_table_hw_tag);

        if (!private->page_table) {
            vmm_free(guest->arch_private);
            guest->arch_private = NULL;
            return VMM_ENOMEM;
        }

        private->guest_serial = vmm_malloc(sizeof(struct riscv_guest_serial));

        if (!private->guest_serial) {
            mmu_page_table_free(riscv_guest_private(guest)->page_table);
            vmm_free(guest->arch_private);
            guest->arch_private = NULL;
            return VMM_ENOMEM;
        }

        gserial = riscv_guest_serial(guest);
        sname   = guest_fdt_find_serial_node(guest->name);

        if (sname) {
            strlcpy(gserial->name, guest->name, sizeof(gserial->name));
            strlcat(gserial->name, "/", sizeof(gserial->name));
            strlcat(gserial->name, sname, sizeof(gserial->name));
        }

        gserial->vser_client.notifier_call = &guest_vserial_notification;
        gserial->vser_client.priority      = 0;
        rc                                 = vmm_vserial_register_client(&gserial->vser_client);

        if (rc) {
            vmm_free(gserial);
            mmu_page_table_free(riscv_guest_private(guest)->page_table);
            vmm_free(guest->arch_private);
            guest->arch_private = NULL;
            return rc;
        }
    }

    return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
    int                        rc;
    struct riscv_guest_serial *gs;

    if (guest->arch_private) {
        gs = riscv_guest_serial(guest);

        if ((rc = mmu_page_table_free(riscv_guest_private(guest)->page_table))) {
            return rc;
        }

        if (gs) {
            vmm_vserial_unregister_client(&gs->vser_client);
            vmm_free(gs);
        }

        vmm_free(guest->arch_private);
    }

    return VMM_OK;
}

int arch_guest_add_region(struct vmm_guest *guest, struct vmm_region *region)
{
    return VMM_OK;
}

int arch_guest_del_region(struct vmm_guest *guest, struct vmm_region *region)
{
    return VMM_OK;
}

int arch_vcpu_init(vmm_vcpu_t *vcpu)
{
    int            rc = VMM_OK;
    const char    *attr;
    virtual_addr_t sp, sp_exec;

    /* Determine stack location */
    if (vcpu->is_normal) {
        sp      = 0;
        sp_exec = vcpu->stack_virtual_address + vcpu->stack_size;
        sp_exec = sp_exec & ~(__SIZEOF_POINTER__ - 1);
    } else {
        sp = vcpu->stack_virtual_address + vcpu->stack_size;
        sp = sp & ~(__SIZEOF_POINTER__ - 1);

        if (!vcpu->reset_count) {
            /* First time allocate exception stack */
            sp_exec = vmm_page_pool_alloc(VMM_PAGE_POOL_NORMAL, VMM_SIZE_TO_PAGE(CONFIG_IRQ_STACK_SIZE));

            if (!sp_exec) {
                return VMM_ENOMEM;
            }

            sp_exec += CONFIG_IRQ_STACK_SIZE;
        } else {
            sp_exec = riscv_regs(vcpu)->sp_exec;
        }
    }

    /* For both Orphan & Normal VCPUs */
    memset(riscv_regs(vcpu), 0, sizeof(arch_regs_t));
    riscv_regs(vcpu)->sepc    = vcpu->start_pc;
    riscv_regs(vcpu)->sstatus = SSTATUS_SPP | SSTATUS_SPIE;
    riscv_regs(vcpu)->sp      = sp;
    riscv_regs(vcpu)->sp_exec = sp_exec;
    riscv_regs(vcpu)->hstatus = 0;

    /* For Orphan VCPUs we are done */
    if (!vcpu->is_normal) {
        return VMM_OK;
    }

    /* Following initialization for normal VCPUs only */

    /* First time initialization of private context */
    if (!vcpu->reset_count) {
        /* Check allowed compatible string */
        rc = vmm_device_tree_read_string(vcpu->node, VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME, &attr);

        if (rc) {
            goto fail;
        }

        if (strcmp(attr, "riscv,generic") != 0) {
            rc = VMM_EINVALID;
            goto fail;
        }

        /* Alloc private context */
        vcpu->arch_private = vmm_zalloc(sizeof(struct riscv_private));

        if (!vcpu->arch_private) {
            rc = VMM_ENOMEM;
            goto fail;
        }

        /* Set register width */
        riscv_private(vcpu)->xlen = riscv_xlen;

        /* Allocate ISA feature bitmap */
        riscv_private(vcpu)->isa  = vmm_zalloc(bitmap_estimate_size(RISCV_ISA_EXT_MAX));

        if (!riscv_private(vcpu)->isa) {
            rc = VMM_ENOMEM;
            goto fail_free_private;
        }

        /* Parse VCPU ISA string */
        attr = NULL;
        rc   = vmm_device_tree_read_string(vcpu->node, "riscv,isa", &attr);

        if (rc || !attr) {
            rc = VMM_EINVALID;
            goto fail_free_isa;
        }

        rc = riscv_isa_parse_string(attr, &riscv_private(vcpu)->xlen, riscv_private(vcpu)->isa, RISCV_ISA_EXT_MAX);

        if (rc) {
            goto fail_free_isa;
        }

        if (riscv_private(vcpu)->xlen > riscv_xlen) {
            rc = VMM_EINVALID;
            goto fail_free_isa;
        }

        riscv_private(vcpu)->isa[0] &= RISCV_ISA_ALLOWED;

        /* VCPU ISA bitmap should be ANDed with Host ISA bitmap */
        bitmap_and(riscv_private(vcpu)->isa, riscv_private(vcpu)->isa, riscv_isa_extension_host(), RISCV_ISA_EXT_MAX);

        /* H-extension only available when AIA CSRs are available */
        if (!riscv_isa_extension_available(NULL, SxAIA)) {
            riscv_private(vcpu)->isa[0] &= ~riscv_isa_extension_mask(h);
        }

        /* Initialize nested state */
        rc = cpu_vcpu_nested_init(vcpu);

        if (rc) {
            goto fail_free_isa;
        }
    }

    /* Set a0 to VCPU sub-id (i.e. virtual HARTID) */
    riscv_regs(vcpu)->a0 = vcpu->subid;

    /* Update HSTATUS */
    riscv_regs(vcpu)->hstatus |= HSTATUS_VTW;
    riscv_regs(vcpu)->hstatus |= HSTATUS_SPVP;
    riscv_regs(vcpu)->hstatus |= HSTATUS_SPV;

    /* TODO: Update HSTATUS.VSXL for 32bit Guest on 64-bit Host */

    /* TODO: Update HSTATUS.VSBE for big-endian Guest */

    /* Reset stats gathering */
    memset(riscv_stats_private(vcpu), 0, sizeof(struct riscv_priv_stats));

    /* Update VCPU CSRs */
    riscv_private(vcpu)->hie        = 0;
    riscv_private(vcpu)->hip        = 0;
    riscv_private(vcpu)->hvip       = 0;
    riscv_private(vcpu)->henvcfg    = 0;
    riscv_private(vcpu)->vsstatus   = 0;
    riscv_private(vcpu)->vstvec     = 0;
    riscv_private(vcpu)->vsscratch  = 0;
    riscv_private(vcpu)->vsepc      = 0;
    riscv_private(vcpu)->vscause    = 0;
    riscv_private(vcpu)->vstval     = 0;
    riscv_private(vcpu)->vsatp      = 0;

    /* By default, make CY, TM, and IR counters accessible in VU mode */
    riscv_private(vcpu)->scounteren = 7;

    /* Reset nested state */
    cpu_vcpu_nested_reset(vcpu);

    /* Initialize FP state */
    cpu_vcpu_fp_init(vcpu);

    /* Initialize timer */
    cpu_vcpu_timer_init(vcpu, &riscv_timer_private(vcpu));

    return VMM_OK;

fail_free_isa:
    vmm_free(riscv_private(vcpu)->isa);
    riscv_private(vcpu)->isa = NULL;
fail_free_priv:
    vmm_free(vcpu->arch_private);
    vcpu->arch_private = NULL;
fail:
    return rc;
}

int arch_vcpu_deinit(vmm_vcpu_t *vcpu)
{
    int            rc;
    virtual_addr_t sp_exec;

    /* Free-up exception stack for Orphan VCPU */
    if (!vcpu->is_normal) {
        sp_exec = riscv_regs(vcpu)->sp_exec - CONFIG_IRQ_STACK_SIZE;
        vmm_page_pool_free(VMM_PAGE_POOL_NORMAL, sp_exec, VMM_SIZE_TO_PAGE(CONFIG_IRQ_STACK_SIZE));
    }

    /* For both Orphan & Normal VCPUs */

    /* Clear arch registers */
    memset(riscv_regs(vcpu), 0, sizeof(arch_regs_t));

    /* For Orphan VCPUs do nothing else */
    if (!vcpu->is_normal) {
        return VMM_OK;
    }

    /* Cleanup timer */
    rc = cpu_vcpu_timer_deinit(vcpu, &riscv_timer_private(vcpu));

    if (rc) {
        return rc;
    }

    /* Cleanup nested state */
    cpu_vcpu_nested_deinit(vcpu);

    /* Free ISA bitmap */
    vmm_free(riscv_private(vcpu)->isa);
    riscv_private(vcpu)->isa = NULL;

    /* Free private context */
    vmm_free(vcpu->arch_private);
    vcpu->arch_private = NULL;

    return VMM_OK;
}

void arch_vcpu_switch(vmm_vcpu_t *vcpu_on_thread, vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    struct riscv_priv *private;

    if (vcpu_on_thread) {
        memcpy(riscv_regs(vcpu_on_thread), regs, sizeof(*regs));

        if (vcpu_on_thread->is_normal) {
            private             = riscv_private(vcpu_on_thread);
            private->hie        = csr_read(CSR_HIE);
            private->hip        = csr_read(CSR_HIP);
            private->hvip       = csr_read(CSR_HVIP);
            private->vsstatus   = csr_read(CSR_VSSTATUS);
            private->vstvec     = csr_read(CSR_VSTVEC);
            private->vsscratch  = csr_read(CSR_VSSCRATCH);
            private->vsepc      = csr_read(CSR_VSEPC);
            private->vscause    = csr_read(CSR_VSCAUSE);
            private->vstval     = csr_read(CSR_VSTVAL);
            private->vsatp      = csr_read(CSR_VSATP);
            private->scounteren = csr_read(CSR_SCOUNTEREN);
            cpu_vcpu_fp_save(vcpu_on_thread, regs);
            cpu_vcpu_timer_save(vcpu_on_thread);
        }

        clrx();
    }

    memcpy(regs, riscv_regs(vcpu), sizeof(*regs));

    if (vcpu->is_normal) {
        private = riscv_private(vcpu);
        csr_write(CSR_HIE, private->hie);
        csr_write(CSR_HVIP, private->hvip);
        csr_write(CSR_VSSTATUS, private->vsstatus);
        csr_write(CSR_VSTVEC, private->vstvec);
        csr_write(CSR_VSSCRATCH, private->vsscratch);
        csr_write(CSR_VSEPC, private->vsepc);
        csr_write(CSR_VSCAUSE, private->vscause);
        csr_write(CSR_VSTVAL, private->vstval);
        csr_write(CSR_VSATP, private->vsatp);
        csr_write(CSR_SCOUNTEREN, private->scounteren);
        cpu_vcpu_envcfg_update(vcpu, riscv_nested_virt(vcpu));
        cpu_vcpu_timer_restore(vcpu);
        cpu_vcpu_fp_restore(vcpu, regs);
        cpu_vcpu_gstage_update(vcpu, riscv_nested_virt(vcpu));
        cpu_vcpu_irq_deleg_update(vcpu, riscv_nested_virt(vcpu));
    } else {
        cpu_vcpu_irq_deleg_update(vcpu, FALSE);
    }
}

void arch_vcpu_post_switch(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    /* For now nothing to do here. */
}

void cpu_vcpu_envcfg_update(vmm_vcpu_t *vcpu, bool nested_virt)
{
    uint64_t henvcfg = (nested_virt) ? 0 : riscv_private(vcpu)->henvcfg;

#ifdef CONFIG_32BIT
    csr_write(CSR_HENVCFG, (uint32_t)henvcfg);
    csr_write(CSR_HENVCFGH, (uint32_t)(henvcfg >> 32));
#else
    csr_write(CSR_HENVCFG, henvcfg);
#endif
}

void cpu_vcpu_irq_deleg_update(vmm_vcpu_t *vcpu, bool nested_virt)
{
    if (vcpu->is_normal && nested_virt) {
        /* Disable interrupt delegation */
        csr_write(CSR_HIDELEG, 0);

        /* Enable sip/siph and sie/sieh trapping */
        if (riscv_isa_extension_available(NULL, SxAIA)) {
            csr_set(CSR_HVICTL, HVICTL_VTI);
        }
    } else {
        /* Enable interrupt delegation */
        csr_write(CSR_HIDELEG, HIDELEG_DEFAULT);

        /* Disable sip/siph and sie/sieh trapping */
        if (riscv_isa_extension_available(NULL, SxAIA)) {
            csr_clear(CSR_HVICTL, HVICTL_VTI);
        }
    }
}

void cpu_vcpu_gstage_update(vmm_vcpu_t *vcpu, bool nested_virt)
{
    struct mmu_page_table *page_table = (nested_virt) ? riscv_nested_private(vcpu)->page_table : riscv_guest_private(vcpu->guest)->page_table;

    mmu_stage2_change_page_table(page_table);

    if (!mmu_page_table_has_hw_tag(page_table)) {
        /*
         * Invalidate entries related to all guests from both
         * G-stage TLB and VS-stage TLB.
         *
         * NOTE: Due to absence of VMID, there is not VMID tagging
         * in VS-stage TLB as well so to avoid one Guest seeing
         * VS-stage mappings of other Guest we have to invalidate
         * VS-stage TLB enteries as well.
         */
        __hfence_gvma_all();
        __hfence_vvma_all();
    }
}

void cpu_vcpu_dump_general_regs(vmm_char_device_t *cdev, arch_regs_t *regs)
{
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "       zero", regs->zero, "         ra", regs->ra);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         sp", regs->sp, "         gp", regs->gp);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         tp", regs->tp, "         s0", regs->s0);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         s1", regs->s1, "         a0", regs->a0);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         a1", regs->a1, "         a2", regs->a2);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         a3", regs->a3, "         a4", regs->a4);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         a5", regs->a5, "         a6", regs->a6);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         a7", regs->a7, "         s2", regs->s2);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         s3", regs->s3, "         s4", regs->s4);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         s5", regs->s5, "         s6", regs->s6);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         s7", regs->s7, "         int8_t", regs->int8_t);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         s9", regs->s9, "        s10", regs->s10);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "        s11", regs->s11, "         t0", regs->t0);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         t1", regs->t1, "         t2", regs->t2);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         t3", regs->t3, "         t4", regs->t4);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "         t5", regs->t5, "         t6", regs->t6);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "       sepc", regs->sepc, "    sstatus", regs->sstatus);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "    hstatus", regs->hstatus, "    sp_exec", regs->sp_exec);
}

void cpu_vcpu_dump_private_regs(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu)
{
    int  rc;
    char isa[128];
    struct riscv_priv *private        = riscv_private(vcpu);
    struct riscv_guest_priv *gprivate = riscv_guest_private(vcpu->guest);

    rc                                = riscv_isa_populate_string(private->xlen, private->isa, isa, sizeof(isa));

    if (rc) {
        vmm_cdev_printf(cdev, "Failed to populate ISA string\n");
        return;
    }

    vmm_cdev_printf(cdev, "\n");
    vmm_cdev_printf(cdev, "    %s=%s\n", "        isa", isa);
    vmm_cdev_printf(cdev, "\n");
#ifdef CONFIG_64BIT
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR "\n", " htimedelta", (ulong)gprivate->time_delta);
#else
    vmm_cdev_printf(
        cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", " htimedelta", (ulong)(gprivate->time_delta), "htimedeltah",
        (ulong)(gprivate->time_delta >> 32));
#endif
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "        hie", private->hie, "        hip", private->hip);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "       hvip", private->hvip, "   vsstatus", private->vsstatus);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "      vsatp", private->vsatp, "     vstvec", private->vstvec);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "  vsscratch", private->vsscratch, "      vsepc", private->vsepc);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "    vscause", private->vscause, "     vstval", private->vstval);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR "\n", " scounteren", private->scounteren);

    cpu_vcpu_nested_dump_regs(cdev, vcpu);

    cpu_vcpu_fp_dump_regs(cdev, vcpu);
}

void cpu_vcpu_dump_exception_regs(vmm_char_device_t *cdev, uint64_t scause, uint64_t stval, uint64_t htval, uint64_t htinst)
{
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "     scause", scause, "      stval", stval);
    vmm_cdev_printf(cdev, "    %s=0x%" PRIADDR " %s=0x%" PRIADDR "\n", "      htval", htval, "     htinst", htinst);
}

void arch_vcpu_regs_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu)
{
    cpu_vcpu_dump_general_regs(cdev, riscv_regs(vcpu));

    if (vcpu->is_normal) {
        cpu_vcpu_dump_private_regs(cdev, vcpu);
    }
}

static const char trap_names[][32] = {
    [CAUSE_MISALIGNED_FETCH]         = "Misaligned Fetch Fault",
    [CAUSE_FETCH_ACCESS]             = "Fetch Access Fault",
    [CAUSE_ILLEGAL_INSTRUCTION]      = "Illegal Instruction Fault",
    [CAUSE_BREAKPOINT]               = "Breakpoint Fault",
    [CAUSE_MISALIGNED_LOAD]          = "Misaligned Load Fault",
    [CAUSE_LOAD_ACCESS]              = "Load Access Fault",
    [CAUSE_MISALIGNED_STORE]         = "Misaligned Store Fault",
    [CAUSE_STORE_ACCESS]             = "Store Access Fault",
    [CAUSE_USER_ECALL]               = "User Ecall",
    [CAUSE_SUPERVISOR_ECALL]         = "Supervisor Ecall",
    [CAUSE_VIRTUAL_SUPERVISOR_ECALL] = "Virtual Supervisor Ecall",
    [CAUSE_MACHINE_ECALL]            = "Machine Ecall",
    [CAUSE_FETCH_PAGE_FAULT]         = "Fetch Page Fault",
    [CAUSE_LOAD_PAGE_FAULT]          = "Load Page Fault",
    [CAUSE_STORE_PAGE_FAULT]         = "Store Page Fault",
    [CAUSE_FETCH_GUEST_PAGE_FAULT]   = "Fetch Guest Page Fault",
    [CAUSE_LOAD_GUEST_PAGE_FAULT]    = "Load Guest Page Fault",
    [CAUSE_VIRTUAL_INST_FAULT]       = "Virtual Instruction Fault",
    [CAUSE_STORE_GUEST_PAGE_FAULT]   = "Store Guest Page Fault",
};

void arch_vcpu_stat_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu)
{
    int  i;
    bool have_traps = FALSE;

    for (i = 0; i < RISCV_PRIV_MAX_TRAP_CAUSE; i++) {
        if (!riscv_stats_private(vcpu)->trap[i]) {
            continue;
        }

        vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", trap_names[i], riscv_stats_private(vcpu)->trap[i]);
        have_traps = TRUE;
    }

    if (have_traps) {
        vmm_cdev_printf(cdev, "\n");
    }

    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested Enter", riscv_stats_private(vcpu)->nested_enter);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested Exit", riscv_stats_private(vcpu)->nested_exit);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested Virtual Interrupt", riscv_stats_private(vcpu)->nested_vsirq);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested S-mode CSR Access", riscv_stats_private(vcpu)->nested_smode_csr_rmw);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested HS-mode CSR Access", riscv_stats_private(vcpu)->nested_hext_csr_rmw);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested Load Guest Page Fault", riscv_stats_private(vcpu)->nested_load_guest_page_fault);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested Store Guest Page Fault", riscv_stats_private(vcpu)->nested_store_guest_page_fault);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested Fetch Guest Page Fault", riscv_stats_private(vcpu)->nested_fetch_guest_page_fault);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested HFENCE.VVMA Instruction", riscv_stats_private(vcpu)->nested_hfence_vvma);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested HFENCE.GVMA Instruction", riscv_stats_private(vcpu)->nested_hfence_gvma);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested HLV Instruction", riscv_stats_private(vcpu)->nested_hlv);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested HSV Instruction", riscv_stats_private(vcpu)->nested_hsv);
    vmm_cdev_printf(cdev, "%-32s: 0x%" PRIx64 "\n", "Nested SBI Ecall", riscv_stats_private(vcpu)->nested_sbi);
}
