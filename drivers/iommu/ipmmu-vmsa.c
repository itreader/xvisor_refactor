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
 * @file ipmmu-vmsa.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief IOMMU driver for Renesas VMSA-compatible IPMMU.
 *
 * The source has been largely adapted from Linux
 * drivers/iommu/ipmmu-vmsa.c
 *
 * IPMMU VMSA
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 *
 * The original code is licensed under the GPL.
 */

#include <libs/list.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_iommu.h>
#include <vmm_macros.h>
#include <vmm_platform.h>
#include <vmm_spinlocks.h>

#include "io-page_table.h"

typedef struct ipmmu_vmsa_device {
    vmm_device_tree_node_t *node;
    void                   *base;
    list_head_t             list;

    uint32_t num_utlbs;

    /* io_xxx only updated at time of attaching device */
    vmm_device_t       *io_dev;
    vmm_iommu_domain_t *io_domain;

    vmm_iommu_controller_t controller;
} ipmmu_vmsa_device_t;

struct ipmmu_vmsa_domain {
    ipmmu_vmsa_device_t *mmu;
    vmm_iommu_domain_t   io_domain;

    struct io_page_table_cfg  cfg;
    struct io_page_table_ops *iop;

    uint32_t       context_id;
    vmm_spinlock_t lock; /* Protects mappings */
};

struct ipmmu_vmsa_archdata {
    ipmmu_vmsa_device_t *mmu;
    uint32_t            *utlbs;
    uint32_t             num_utlbs;
};

static DEFINE_SPINLOCK(ipmmu_devices_lock);
static LIST_HEAD(ipmmu_devices);

static struct ipmmu_vmsa_domain *to_vmsa_domain(vmm_iommu_domain_t *dom)
{
    return container_of(dom, struct ipmmu_vmsa_domain, io_domain);
}

#define TLB_LOOP_TIMEOUT            100 /* 100us */

/* -----------------------------------------------------------------------------
 * Registers Definition
 */

#define IM_NS_ALIAS_OFFSET          0x800

#define IM_CTX_SIZE                 0x40

#define IMCTR                       0x0000
#define IMCTR_TRE                   (1 << 17)
#define IMCTR_AFE                   (1 << 16)
#define IMCTR_RTSEL_MASK            (3 << 4)
#define IMCTR_RTSEL_SHIFT           4
#define IMCTR_TREN                  (1 << 3)
#define IMCTR_INTEN                 (1 << 2)
#define IMCTR_FLUSH                 (1 << 1)
#define IMCTR_MMUEN                 (1 << 0)

#define IMCAAR                      0x0004

#define IMTTBCR                     0x0008
#define IMTTBCR_EAE                 (1 << 31)
#define IMTTBCR_PMB                 (1 << 30)
#define IMTTBCR_SH1_NON_SHAREABLE   (0 << 28)
#define IMTTBCR_SH1_OUTER_SHAREABLE (2 << 28)
#define IMTTBCR_SH1_INNER_SHAREABLE (3 << 28)
#define IMTTBCR_SH1_MASK            (3 << 28)
#define IMTTBCR_ORGN1_NC            (0 << 26)
#define IMTTBCR_ORGN1_WB_WA         (1 << 26)
#define IMTTBCR_ORGN1_WT            (2 << 26)
#define IMTTBCR_ORGN1_WB            (3 << 26)
#define IMTTBCR_ORGN1_MASK          (3 << 26)
#define IMTTBCR_IRGN1_NC            (0 << 24)
#define IMTTBCR_IRGN1_WB_WA         (1 << 24)
#define IMTTBCR_IRGN1_WT            (2 << 24)
#define IMTTBCR_IRGN1_WB            (3 << 24)
#define IMTTBCR_IRGN1_MASK          (3 << 24)
#define IMTTBCR_TSZ1_MASK           (7 << 16)
#define IMTTBCR_TSZ1_SHIFT          16
#define IMTTBCR_SH0_NON_SHAREABLE   (0 << 12)
#define IMTTBCR_SH0_OUTER_SHAREABLE (2 << 12)
#define IMTTBCR_SH0_INNER_SHAREABLE (3 << 12)
#define IMTTBCR_SH0_MASK            (3 << 12)
#define IMTTBCR_ORGN0_NC            (0 << 10)
#define IMTTBCR_ORGN0_WB_WA         (1 << 10)
#define IMTTBCR_ORGN0_WT            (2 << 10)
#define IMTTBCR_ORGN0_WB            (3 << 10)
#define IMTTBCR_ORGN0_MASK          (3 << 10)
#define IMTTBCR_IRGN0_NC            (0 << 8)
#define IMTTBCR_IRGN0_WB_WA         (1 << 8)
#define IMTTBCR_IRGN0_WT            (2 << 8)
#define IMTTBCR_IRGN0_WB            (3 << 8)
#define IMTTBCR_IRGN0_MASK          (3 << 8)
#define IMTTBCR_SL0_LVL_2           (0 << 4)
#define IMTTBCR_SL0_LVL_1           (1 << 4)
#define IMTTBCR_TSZ0_MASK           (7 << 0)
#define IMTTBCR_TSZ0_SHIFT          O

#define IMBUSCR                     0x000c
#define IMBUSCR_DVM                 (1 << 2)
#define IMBUSCR_BUSSEL_SYS          (0 << 0)
#define IMBUSCR_BUSSEL_CCI          (1 << 0)
#define IMBUSCR_BUSSEL_IMCAAR       (2 << 0)
#define IMBUSCR_BUSSEL_CCI_IMCAAR   (3 << 0)
#define IMBUSCR_BUSSEL_MASK         (3 << 0)

#define IMTTLBR0                    0x0010
#define IMTTUBR0                    0x0014
#define IMTTLBR1                    0x0018
#define IMTTUBR1                    0x001c

#define IMSTR                       0x0020
#define IMSTR_ERRLVL_MASK           (3 << 12)
#define IMSTR_ERRLVL_SHIFT          12
#define IMSTR_ERRCODE_TLB_FORMAT    (1 << 8)
#define IMSTR_ERRCODE_ACCESS_PERM   (4 << 8)
#define IMSTR_ERRCODE_SECURE_ACCESS (5 << 8)
#define IMSTR_ERRCODE_MASK          (7 << 8)
#define IMSTR_MHIT                  (1 << 4)
#define IMSTR_ABORT                 (1 << 2)
#define IMSTR_PF                    (1 << 1)
#define IMSTR_TF                    (1 << 0)

#define IMMAIR0                     0x0028
#define IMMAIR1                     0x002c
#define IMMAIR_ATTR_MASK            0xff
#define IMMAIR_ATTR_DEVICE          0x04
#define IMMAIR_ATTR_NC              0x44
#define IMMAIR_ATTR_WBRWA           0xff
#define IMMAIR_ATTR_SHIFT(n)        ((n) << 3)
#define IMMAIR_ATTR_IDX_NC          0
#define IMMAIR_ATTR_IDX_WBRWA       1
#define IMMAIR_ATTR_IDX_DEV         2

#define IMEAR                       0x0030

#define IMPCTR                      0x0200
#define IMPSTR                      0x0208
#define IMPEAR                      0x020c
#define IMPMBA(n)                   (0x0280 + ((n) * 4))
#define IMPMBD(n)                   (0x02c0 + ((n) * 4))

#define IMUCTR(n)                   (0x0300 + ((n) * 16))
#define IMUCTR_FIXADDEN             (1 << 31)
#define IMUCTR_FIXADD_MASK          (0xff << 16)
#define IMUCTR_FIXADD_SHIFT         16
#define IMUCTR_TTSEL_MMU(n)         ((n) << 4)
#define IMUCTR_TTSEL_PMB            (8 << 4)
#define IMUCTR_TTSEL_MASK           (15 << 4)
#define IMUCTR_FLUSH                (1 << 1)
#define IMUCTR_MMUEN                (1 << 0)

#define IMUASID(n)                  (0x0308 + ((n) * 16))
#define IMUASID_ASID8_MASK          (0xff << 8)
#define IMUASID_ASID8_SHIFT         8
#define IMUASID_ASID0_MASK          (0xff << 0)
#define IMUASID_ASID0_SHIFT         0

/* -----------------------------------------------------------------------------
 * Read/Write Access
 */

static uint32_t ipmmu_read(ipmmu_vmsa_device_t *mmu, uint32_t offset)
{
    return vmm_readl(mmu->base + offset);
}

static void ipmmu_write(ipmmu_vmsa_device_t *mmu, uint32_t offset, uint32_t data)
{
    vmm_writel(data, mmu->base + offset);
}

static uint32_t ipmmu_ctx_read(struct ipmmu_vmsa_domain *domain, uint32_t reg)
{
    return ipmmu_read(domain->mmu, domain->context_id * IM_CTX_SIZE + reg);
}

static void ipmmu_ctx_write(struct ipmmu_vmsa_domain *domain, uint32_t reg, uint32_t data)
{
    ipmmu_write(domain->mmu, domain->context_id * IM_CTX_SIZE + reg, data);
}

/* -----------------------------------------------------------------------------
 * TLB and microTLB Management
 */

/* Wait for any pending TLB invalidations to complete */
static void ipmmu_tlb_sync(struct ipmmu_vmsa_domain *domain)
{
    uint32_t count = 0;

    while (ipmmu_ctx_read(domain, IMCTR) & IMCTR_FLUSH) {
        barrier();

        if (++count == TLB_LOOP_TIMEOUT) {
            vmm_lerror(domain->mmu->node->name, "TLB sync timed out -- MMU may be deadlocked\n");
            return;
        }

        vmm_udelay(1);
    }
}

static void ipmmu_tlb_invalidate(struct ipmmu_vmsa_domain *domain)
{
    uint32_t reg;

    reg = ipmmu_ctx_read(domain, IMCTR);
    reg |= IMCTR_FLUSH;
    ipmmu_ctx_write(domain, IMCTR, reg);

    ipmmu_tlb_sync(domain);
}

/*
 * Enable MMU translation for the microTLB.
 */
static void ipmmu_utlb_enable(struct ipmmu_vmsa_domain *domain, uint32_t utlb)
{
    ipmmu_vmsa_device_t *mmu = domain->mmu;

    /*
     * TODO: Reference-count the microTLB as several bus masters can be
     * connected to the same microTLB.
     */

    /* TODO: What should we set the ASID to ? */
    ipmmu_write(mmu, IMUASID(utlb), 0);
    /* TODO: Do we need to flush the microTLB ? */
    ipmmu_write(mmu, IMUCTR(utlb), IMUCTR_TTSEL_MMU(domain->context_id) | IMUCTR_FLUSH | IMUCTR_MMUEN);
}

/*
 * Disable MMU translation for the microTLB.
 */
static void ipmmu_utlb_disable(struct ipmmu_vmsa_domain *domain, uint32_t utlb)
{
    ipmmu_vmsa_device_t *mmu = domain->mmu;

    ipmmu_write(mmu, IMUCTR(utlb), 0);
}

static void ipmmu_tlb_flush_all(void *cookie)
{
    struct ipmmu_vmsa_domain *domain = cookie;

    ipmmu_tlb_invalidate(domain);
}

static void ipmmu_tlb_add_flush(physical_addr_t iova, size_t size, size_t granule, bool leaf, void *cookie)
{
    /* The hardware doesn't support selective TLB flush. */
}

static struct iommu_gather_ops ipmmu_gather_ops = {
    .tlb_flush_all = ipmmu_tlb_flush_all,
    .tlb_add_flush = ipmmu_tlb_add_flush,
    .tlb_sync      = ipmmu_tlb_flush_all,
};

/* -----------------------------------------------------------------------------
 * Domain/Context Management
 */

static int ipmmu_domain_init_context(struct ipmmu_vmsa_domain *domain)
{
    uint64_t ttbr;

    /*
     * Allocate the page table operations.
     *
     * VMSA states in section B3.6.3 "Control of Secure or Non-secure memory
     * access, Long-descriptor format" that the NStable bit being set in a
     * table descriptor will result in the NStable and NS bits of all child
     * entries being ignored and considered as being set. The IPMMU seems
     * not to comply with this, as it generates a secure access page fault
     * if any of the NStable and NS bits isn't set when running in
     * non-secure mode.
     */
    domain->cfg.quirks        = IO_PGTABLE_QUIRK_ARM_NS;
    domain->cfg.pgsize_bitmap = SZ_1G | SZ_2M | SZ_4K, domain->cfg.ias = 32;
    domain->cfg.oas = 40;
    domain->cfg.tlb = &ipmmu_gather_ops;

    domain->iop     = alloc_io_page_table_ops(ARM_32_LPAE_S1, &domain->cfg, domain);

    if (!domain->iop) {
        return VMM_ERR_INVALID;
    }

    /*
     * TODO: When adding support for multiple contexts, find an unused
     * context.
     */
    domain->context_id = 0;

    /* TTBR0 */
    ttbr               = domain->cfg.arm_lpae_s1_cfg.ttbr[0];
    ipmmu_ctx_write(domain, IMTTLBR0, ttbr);
    ipmmu_ctx_write(domain, IMTTUBR0, ttbr >> 32);

    /*
     * TTBCR
     * We use long descriptors with inner-shareable WBWA tables and allocate
     * the whole 32-bit VA space to TTBR0.
     */
    ipmmu_ctx_write(domain, IMTTBCR, IMTTBCR_EAE | IMTTBCR_SH0_INNER_SHAREABLE | IMTTBCR_ORGN0_WB_WA | IMTTBCR_IRGN0_WB_WA | IMTTBCR_SL0_LVL_1);

    /* MAIR0 */
    ipmmu_ctx_write(domain, IMMAIR0, domain->cfg.arm_lpae_s1_cfg.mair[0]);

    /* IMBUSCR */
    ipmmu_ctx_write(domain, IMBUSCR, ipmmu_ctx_read(domain, IMBUSCR) & ~(IMBUSCR_DVM | IMBUSCR_BUSSEL_MASK));

    /*
     * IMSTR
     * Clear all interrupt flags.
     */
    ipmmu_ctx_write(domain, IMSTR, ipmmu_ctx_read(domain, IMSTR));

    /*
     * IMCTR
     * Enable the MMU and interrupt generation. The long-descriptor
     * translation table format doesn't use TEX remapping. Don't enable AF
     * software management as we have no use for it. Flush the TLB as
     * required when modifying the context registers.
     */
    ipmmu_ctx_write(domain, IMCTR, IMCTR_INTEN | IMCTR_FLUSH | IMCTR_MMUEN);

    return 0;
}

static void ipmmu_domain_destroy_context(struct ipmmu_vmsa_domain *domain)
{
    /*
     * Disable the context. Flush the TLB as required when modifying the
     * context registers.
     *
     * TODO: Is TLB flush really needed ?
     */
    ipmmu_ctx_write(domain, IMCTR, IMCTR_FLUSH);
    ipmmu_tlb_sync(domain);
}

/* -----------------------------------------------------------------------------
 * Fault Handling
 */

static vmm_irq_return_t ipmmu_domain_irq(struct ipmmu_vmsa_domain *domain)
{
    const uint32_t       err_mask = IMSTR_MHIT | IMSTR_ABORT | IMSTR_PF | IMSTR_TF;
    ipmmu_vmsa_device_t *mmu      = domain->mmu;
    uint32_t             status;
    physical_addr_t      iova;

    status = ipmmu_ctx_read(domain, IMSTR);

    if (!(status & err_mask)) {
        return VMM_IRQ_NONE;
    }

    iova = ipmmu_ctx_read(domain, IMEAR);

    /*
     * Clear the error status flags. Unlike traditional interrupt flag
     * registers that must be cleared by writing 1, this status register
     * seems to require 0. The error address register must be read before,
     * otherwise its value will be 0.
     */
    ipmmu_ctx_write(domain, IMSTR, 0);

    /* Log fatal errors. */
    if (status & IMSTR_MHIT) {
        vmm_lerror(mmu->node->name, "Multiple TLB hits @0x%" PRIPADDR "\n", iova);
    }

    if (status & IMSTR_ABORT) {
        vmm_lerror(mmu->node->name, "Page Table Walk Abort @0x%" PRIPADDR "\n", iova);
    }

    if (!(status & (IMSTR_PF | IMSTR_TF))) {
        return VMM_IRQ_NONE;
    }

    /*
     * Try to handle page faults and translation faults.
     *
     * TODO: We need to look up the faulty device based on the I/O VA. Use
     * the first attached device for now.
     */
    if (!vmm_report_iommu_fault(&domain->io_domain, mmu->io_dev, iova, 0)) {
        return VMM_IRQ_HANDLED;
    }

    vmm_lerror(mmu->node->name, "Unhandled fault: status 0x%08x iova 0x%" PRIPADDR "\n", status, iova);

    return VMM_IRQ_HANDLED;
}

static vmm_irq_return_t ipmmu_irq(int irq, void *dev)
{
    ipmmu_vmsa_device_t      *mmu = dev;
    vmm_iommu_domain_t       *io_domain;
    struct ipmmu_vmsa_domain *domain;

    if (!mmu->io_domain) {
        return VMM_IRQ_NONE;
    }

    io_domain = mmu->io_domain;
    domain    = to_vmsa_domain(io_domain);

    return ipmmu_domain_irq(domain);
}

/* -----------------------------------------------------------------------------
 * IOMMU Operations
 */

static vmm_iommu_domain_t *ipmmu_domain_alloc(uint32_t type, vmm_iommu_controller_t *ctrl)
{
    struct ipmmu_vmsa_domain *domain;

    if (type != VMM_IOMMU_DOMAIN_UNMANAGED) {
        return NULL;
    }

    domain = vmm_zalloc(sizeof(*domain));

    if (!domain) {
        return NULL;
    }

    INIT_SPIN_LOCK(&domain->lock);

    return &domain->io_domain;
}

static void ipmmu_domain_free(vmm_iommu_domain_t *io_domain)
{
    struct ipmmu_vmsa_domain *domain = to_vmsa_domain(io_domain);

    /*
     * Free the domain resources. We assume that all devices have already
     * been detached.
     */
    ipmmu_domain_destroy_context(domain);
    free_io_page_table_ops(domain->iop);
    vmm_free(domain);
}

static int ipmmu_attach_device(vmm_iommu_domain_t *io_domain, vmm_device_t *dev)
{
    struct ipmmu_vmsa_archdata *archdata = dev->iommu_private;
    ipmmu_vmsa_device_t        *mmu      = archdata->mmu;
    struct ipmmu_vmsa_domain   *domain   = to_vmsa_domain(io_domain);
    irq_flags_t                 flags;
    uint32_t                    i;
    int                         ret = 0;

    if (!mmu) {
        vmm_lerror(dev->name, "Cannot attach to IPMMU\n");
        return VMM_ERR_NXIO;
    }

    vmm_spin_lock_irq_save(&domain->lock, flags);

    if (!domain->mmu) {
        /* The domain hasn't been used yet, initialize it. */
        domain->mmu    = mmu;
        mmu->io_dev    = dev;
        mmu->io_domain = io_domain;
        ret            = ipmmu_domain_init_context(domain);
    } else if (domain->mmu != mmu) {
        /*
         * Something is wrong, we can't attach two devices using
         * different IOMMUs to the same domain.
         */
        vmm_lerror(dev->name, "Can't attach IPMMU %s to domain on IPMMU %s\n", mmu->node->name, domain->mmu->node->name);
        ret = VMM_ERR_INVALID;
    }

    vmm_spin_unlock_irq_restore(&domain->lock, flags);

    if (ret < 0) {
        return ret;
    }

    for (i = 0; i < archdata->num_utlbs; ++i) {
        ipmmu_utlb_enable(domain, archdata->utlbs[i]);
    }

    return 0;
}

static void ipmmu_detach_device(vmm_iommu_domain_t *io_domain, vmm_device_t *dev)
{
    struct ipmmu_vmsa_archdata *archdata = dev->iommu_private;
    struct ipmmu_vmsa_domain   *domain   = to_vmsa_domain(io_domain);
    uint32_t                    i;

    for (i = 0; i < archdata->num_utlbs; ++i) {
        ipmmu_utlb_disable(domain, archdata->utlbs[i]);
    }

    /*
     * TODO: Optimize by disabling the context when no device is attached.
     */
}

static int ipmmu_map(vmm_iommu_domain_t *io_domain, physical_addr_t iova, physical_addr_t paddr, size_t size, int prot)
{
    struct ipmmu_vmsa_domain *domain = to_vmsa_domain(io_domain);

    if (!domain) {
        return VMM_ERR_NODEV;
    }

    return domain->iop->map(domain->iop, iova, paddr, size, prot);
}

static size_t ipmmu_unmap(vmm_iommu_domain_t *io_domain, physical_addr_t iova, size_t size)
{
    struct ipmmu_vmsa_domain *domain = to_vmsa_domain(io_domain);

    return domain->iop->unmap(domain->iop, iova, size);
}

static physical_addr_t ipmmu_iova_to_phys(vmm_iommu_domain_t *io_domain, physical_addr_t iova)
{
    struct ipmmu_vmsa_domain *domain = to_vmsa_domain(io_domain);

    /* TODO: Is locking needed ? */

    return domain->iop->iova_to_phys(domain->iop, iova);
}

static int ipmmu_find_utlbs(ipmmu_vmsa_device_t *mmu, vmm_device_t *dev, uint32_t *utlbs, uint32_t num_utlbs)
{
    uint32_t i;

    for (i = 0; i < num_utlbs; ++i) {
        struct vmm_device_tree_phandle_args args;
        int                                 ret;

        ret = vmm_device_tree_parse_phandle_with_args(dev->of_node, "iommus", "#iommu-cells", i, &args);

        if (ret < 0) {
            return ret;
        }

        vmm_device_tree_dref_node(args.np);

        if (args.np != mmu->node || args.args_count != 1) {
            return VMM_ERR_INVALID;
        }

        utlbs[i] = args.args[0];
    }

    return 0;
}

static int ipmmu_add_device(vmm_device_t *dev)
{
    struct ipmmu_vmsa_archdata *archdata;
    ipmmu_vmsa_device_t        *mmu;
    vmm_iommu_group_t          *group = NULL;
    uint32_t                   *utlbs;
    uint32_t                    i;
    int                         num_utlbs;
    int                         ret = VMM_ERR_NODEV;

    if (dev->iommu_private) {
        vmm_lwarning(dev->name, "IOMMU driver already assigned to device\n");
        return VMM_ERR_INVALID;
    }

    /* Find the master corresponding to the device. */

    num_utlbs = vmm_device_tree_count_phandle_with_args(dev->of_node, "iommus", "#iommu-cells");

    if (num_utlbs < 0) {
        return VMM_ERR_NODEV;
    }

    utlbs = vmm_zalloc(num_utlbs * sizeof(*utlbs));

    if (!utlbs) {
        return VMM_ERR_NOMEM;
    }

    vmm_spin_lock(&ipmmu_devices_lock);

    list_for_each_entry(mmu, &ipmmu_devices, list)
    {
        ret = ipmmu_find_utlbs(mmu, dev, utlbs, num_utlbs);

        if (!ret) {
            /*
             * TODO Take a reference to the MMU to protect
             * against device removal.
             */
            break;
        }
    }

    vmm_spin_unlock(&ipmmu_devices_lock);

    if (ret < 0) {
        goto error;
    }

    for (i = 0; i < num_utlbs; ++i) {
        if (utlbs[i] >= mmu->num_utlbs) {
            ret = VMM_ERR_INVALID;
            goto error;
        }
    }

    /* Create a device group and add the device to it. */
    group = vmm_iommu_group_alloc(dev->name, &mmu->controller);

    if (VMM_IS_ERR(group)) {
        vmm_lerror(dev->name, "Failed to allocate IOMMU group\n");
        ret = VMM_PTR_ERR(group);
        goto error;
    }

    ret = vmm_iommu_group_add_device(group, dev);

    if (ret < 0) {
        vmm_lerror(dev->name, "Failed to add device to IOMMU group\n");
        goto error_put_group;
    }

    /*
     * We put group in-advance so that group is free'ed
     * automatically when all device are removed from it.
     */
    vmm_iommu_group_put(group);

    archdata = vmm_zalloc(sizeof(*archdata));

    if (!archdata) {
        ret = VMM_ERR_NOMEM;
        goto error_remove_dev;
    }

    archdata->mmu       = mmu;
    archdata->utlbs     = utlbs;
    archdata->num_utlbs = num_utlbs;
    dev->iommu_private  = archdata;

    return 0;

error_remove_dev:
    vmm_iommu_group_remove_device(dev);
error_put_group:
    vmm_iommu_group_put(group);
error:
    vmm_free(utlbs);

    return ret;
}

static void ipmmu_remove_device(vmm_device_t *dev)
{
    struct ipmmu_vmsa_archdata *archdata = dev->iommu_private;

    vmm_iommu_group_remove_device(dev);

    vmm_free(dev->iommu_private);

    vmm_free(archdata->utlbs);

    dev->iommu_private = NULL;
}

static vmm_iommu_ops_t ipmmu_ops = {
    .domain_alloc  = ipmmu_domain_alloc,
    .domain_free   = ipmmu_domain_free,
    .attach_dev    = ipmmu_attach_device,
    .detach_dev    = ipmmu_detach_device,
    .map           = ipmmu_map,
    .unmap         = ipmmu_unmap,
    .iova_to_phys  = ipmmu_iova_to_phys,
    .add_device    = ipmmu_add_device,
    .remove_device = ipmmu_remove_device,
    .pgsize_bitmap = SZ_1G | SZ_2M | SZ_4K,
};

/* -----------------------------------------------------------------------------
 * Probe/remove and init
 */

static void ipmmu_device_reset(ipmmu_vmsa_device_t *mmu)
{
    uint32_t i;

    /* Disable all contexts. */
    for (i = 0; i < 4; ++i) {
        ipmmu_write(mmu, i * IM_CTX_SIZE + IMCTR, 0);
    }
}

static int ipmmu_init(vmm_device_tree_node_t *node)
{
    ipmmu_vmsa_device_t *mmu;
    virtual_addr_t       va;
    uint32_t             hirq;
    int                  ret;

    mmu = vmm_zalloc(sizeof(*mmu));

    if (!mmu) {
        vmm_lerror(node->name, "cannot allocate device data\n");
        return VMM_ERR_NOMEM;
    }

    ret = vmm_device_tree_request_regmap(node, &va, 0, "IPMMU");

    if (ret) {
        vmm_lerror(node->name, "cannot map device registers\n");
        vmm_free(mmu);
        return ret;
    }

    vmm_device_tree_ref_node(node);
    mmu->node = node;
    mmu->base = (void *)va;
    INIT_LIST_HEAD(&mmu->list);
    mmu->num_utlbs = 32;

    /*
     * The IPMMU has two register banks, for secure and non-secure modes.
     * The bank mapped at the beginning of the IPMMU address space
     * corresponds to the running mode of the CPU. When running in secure
     * mode the non-secure register bank is also available at an offset.
     *
     * Secure mode operation isn't clearly documented and is thus currently
     * not implemented in the driver. Furthermore, preliminary tests of
     * non-secure operation with the main register bank were not successful.
     * Offset the registers base unconditionally to point to the non-secure
     * alias space for now.
     */
    mmu->base += IM_NS_ALIAS_OFFSET;

    hirq = vmm_device_tree_irq_parse_map(node, 0);

    if (!hirq) {
        vmm_lerror(node->name, "cannot map device irq\n");
        vmm_device_tree_regunmap_release(node, va, 0);
        vmm_free(mmu);
        return VMM_ERR_NODEV;
    }

    if ((ret = vmm_host_irq_register(hirq, node->name, ipmmu_irq, mmu))) {
        vmm_lerror(node->name, "cannot request device irq\n");
        vmm_device_tree_regunmap_release(node, va, 0);
        vmm_free(mmu);
        return ret;
    }

    ipmmu_device_reset(mmu);

    /* Register IOMMU controller */
    if (strlcpy(mmu->controller.name, mmu->node->name, sizeof(mmu->controller.name)) >= sizeof(mmu->controller.name)) {
        vmm_lerror(node->name, "cannot copy controller name\n");
        vmm_host_irq_unregister(hirq, mmu);
        vmm_device_tree_regunmap_release(node, va, 0);
        vmm_free(mmu);
        return VMM_ERR_OVERFLOW;
    }

    ret = vmm_iommu_controller_register(&mmu->controller);

    if (ret) {
        vmm_lerror(node->name, "failed to register controller\n");
        vmm_host_irq_unregister(hirq, mmu);
        vmm_device_tree_regunmap_release(node, va, 0);
        vmm_free(mmu);
        return ret;
    }

    vmm_spin_lock(&ipmmu_devices_lock);
    list_add_tail(&mmu->list, &ipmmu_devices);
    vmm_spin_unlock(&ipmmu_devices_lock);

    if (!vmm_iommu_present(&platform_bus)) {
        vmm_bus_set_iommu(&platform_bus, &ipmmu_ops);
    }

    return VMM_OK;
}

VMM_IOMMU_INIT_DECLARE(ipmmu, "renesas,ipmmu-vmsa", ipmmu_init);
