/*
 * Copyright (C) 2004 Matthew Wilcox <matthew@wil.cx>
 * Copyright (C) 2004 Intel Corp.
 *
 * This code is released under the GNU General Public License version 2.
 */

/*
 * mmconfig.c - Low-level direct PCI config space access via MMCONFIG
 */

#include <asm/e820.h>
#include <asm/pci_x86.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/rcupdate.h>

/* Assume systems with more busses have correct MCFG */
#define mmcfg_virt_addr ((void __iomem *)fix_to_virt(FIX_PCIE_MCFG))

/* The base address of the last MMCONFIG device accessed */
static uint32_t mmcfg_last_accessed_device;
static int      mmcfg_last_accessed_cpu;

/*
 * Functions for accessing PCI configuration space with MMCONFIG accesses
 */
static uint32_t get_base_addr(uint32_t seg, int bus, unsigned devfn)
{
    struct pci_mmcfg_region *cfg = pci_mmconfig_lookup(seg, bus);

    if (cfg) {
        return cfg->address;
    }

    return 0;
}

/*
 * This is always called under pci_config_lock
 */
static void pci_exp_set_dev_base(uint32_t base, int bus, int devfn)
{
    uint32_t dev_base = base | PCI_MMCFG_BUS_OFFSET(bus) | (devfn << 12);
    int      cpu      = smp_processor_id();

    if (dev_base != mmcfg_last_accessed_device || cpu != mmcfg_last_accessed_cpu) {
        mmcfg_last_accessed_device = dev_base;
        mmcfg_last_accessed_cpu    = cpu;
        set_fixmap_nocache(FIX_PCIE_MCFG, dev_base);
    }
}

static int pci_mmcfg_read(uint32_t seg, uint32_t bus, uint32_t devfn, int reg, int len, uint32_t *value)
{
    uint64_t flags;
    uint32_t base;

    if ((bus > 255) || (devfn > 255) || (reg > 4095)) {
    err:
        *value = -1;
        return -EINVAL;
    }

    rcu_read_lock();
    base = get_base_addr(seg, bus, devfn);

    if (!base) {
        rcu_read_unlock();
        goto err;
    }

    raw_spin_lock_irq_save(&pci_config_lock, flags);

    pci_exp_set_dev_base(base, bus, devfn);

    switch (len) {
        case 1:
            *value = mmio_config_readb(mmcfg_virt_addr + reg);
            break;

        case 2:
            *value = mmio_config_readw(mmcfg_virt_addr + reg);
            break;

        case 4:
            *value = mmio_config_readl(mmcfg_virt_addr + reg);
            break;
    }

    raw_spin_unlock_irq_restore(&pci_config_lock, flags);
    rcu_read_unlock();

    return 0;
}

static int pci_mmcfg_write(uint32_t seg, uint32_t bus, uint32_t devfn, int reg, int len, uint32_t value)
{
    uint64_t flags;
    uint32_t base;

    if ((bus > 255) || (devfn > 255) || (reg > 4095)) {
        return -EINVAL;
    }

    rcu_read_lock();
    base = get_base_addr(seg, bus, devfn);

    if (!base) {
        rcu_read_unlock();
        return -EINVAL;
    }

    raw_spin_lock_irq_save(&pci_config_lock, flags);

    pci_exp_set_dev_base(base, bus, devfn);

    switch (len) {
        case 1:
            mmio_config_writeb(mmcfg_virt_addr + reg, value);
            break;

        case 2:
            mmio_config_writew(mmcfg_virt_addr + reg, value);
            break;

        case 4:
            mmio_config_writel(mmcfg_virt_addr + reg, value);
            break;
    }

    raw_spin_unlock_irq_restore(&pci_config_lock, flags);
    rcu_read_unlock();

    return 0;
}

const struct pci_raw_ops pci_mmcfg = {
    .read  = pci_mmcfg_read,
    .write = pci_mmcfg_write,
};

int __init pci_mmcfg_arch_init(void)
{
    printk(KERN_INFO "PCI: Using MMCONFIG for extended config space\n");
    raw_pci_ext_ops = &pci_mmcfg;
    return 1;
}

void __init pci_mmcfg_arch_free(void) {}

int pci_mmcfg_arch_map(struct pci_mmcfg_region *cfg)
{
    return 0;
}

void pci_mmcfg_arch_unmap(struct pci_mmcfg_region *cfg)
{
    uint64_t flags;

    /* Invalidate the cached mmcfg map entry. */
    raw_spin_lock_irq_save(&pci_config_lock, flags);
    mmcfg_last_accessed_device = 0;
    raw_spin_unlock_irq_restore(&pci_config_lock, flags);
}
