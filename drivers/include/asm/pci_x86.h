/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file pci_x86.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 *
 * Derived work from Linux.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 *
 *  Low-Level PCI Access for i386 machines.
 *
 *  (c) 1999 Martin Mares <mj@ucw.cz>
 */

#undef DEBUG

#ifdef DEBUG
#define DBG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)                   \
    do {                                \
        if (0)                          \
            printk(fmt, ##__VA_ARGS__); \
    } while (0)
#endif

#define PCI_PROBE_BIOS              0x0001
#define PCI_PROBE_CONF1             0x0002
#define PCI_PROBE_CONF2             0x0004
#define PCI_PROBE_MMCONF            0x0008
#define PCI_PROBE_MASK              0x000f
#define PCI_PROBE_NOEARLY           0x0010

#define PCI_NO_CHECKS               0x0400
#define PCI_USE_PIRQ_MASK           0x0800
#define PCI_ASSIGN_ROMS             0x1000
#define PCI_BIOS_IRQ_SCAN           0x2000
#define PCI_ASSIGN_ALL_BUSSES       0x4000
#define PCI_CAN_SKIP_ISA_ALIGN      0x8000
#define PCI_USE__CRS                0x10000
#define PCI_CHECK_ENABLE_AMD_MMCONF 0x20000
#define PCI_HAS_IO_ECS              0x40000
#define PCI_NOASSIGN_ROMS           0x80000
#define PCI_ROOT_NO_CRS             0x100000
#define PCI_NOASSIGN_BARS           0x200000

extern uint32_t pci_probe;
extern uint64_t pirq_table_addr;

enum pci_bf_sort_state {
    pci_bf_sort_default,
    pci_force_nobf,
    pci_force_bf,
    pci_dmi_bf,
};

/* pci-i386.c */

void pcibios_resource_survey(void);
void pcibios_set_cache_line_size(void);

/* pci-pc.c */

extern int            pcibios_last_bus;
extern struct pci_ops pci_root_ops;

void pcibios_scan_specific_bus(int busn);

/* pci-irq.c */

struct irq_info {
    uint8_t bus, devfn;  /* Bus, device and function */

    struct {
        uint8_t  link;   /* IRQ line ID, chipset dependent,
                    0 = not routed */
        uint16_t bitmap; /* Available IRQs */
    } __attribute__((packed)) irq[4];

    uint8_t slot;        /* Slot number, 0=onboard */
    uint8_t rfu;
} __attribute__((packed));

struct irq_routing_table {
    uint32_t        signature;              /* PIRQ_SIGNATURE should be here */
    uint16_t        version;                /* PIRQ_VERSION */
    uint16_t        size;                   /* Table size in bytes */
    uint8_t         rtr_bus, rtr_devfn;     /* Where the interrupt router lies */
    uint16_t        exclusive_irqs;         /* IRQs devoted exclusively to
                               PCI usage */
    uint16_t        rtr_vendor, rtr_device; /* Vendor and device ID of
                           interrupt router */
    uint32_t        miniport_data;          /* Crap */
    uint8_t         rfu[11];
    uint8_t         checksum;               /* Modulo 256 checksum must give 0 */
    struct irq_info slots[0];
} __attribute__((packed));

extern uint32_t pcibios_irq_mask;

extern raw_spinlock_t pci_config_lock;

extern int (*pcibios_enable_irq)(struct pci_dev *dev);
extern void (*pcibios_disable_irq)(struct pci_dev *dev);

extern bool mp_should_keep_irq(struct device *dev);

struct pci_raw_ops {
    int (*read)(uint32_t domain, uint32_t bus, uint32_t devfn, int reg, int len, uint32_t *val);
    int (*write)(uint32_t domain, uint32_t bus, uint32_t devfn, int reg, int len, uint32_t val);
};

extern const struct pci_raw_ops *raw_pci_ops;
extern const struct pci_raw_ops *raw_pci_ext_ops;

extern const struct pci_raw_ops pci_mmcfg;
extern const struct pci_raw_ops pci_direct_conf1;
extern bool                     port_cf9_safe;

/* arch_initcall level */
extern int         pci_direct_probe(void);
extern void        pci_direct_init(int type);
extern void        pci_pcbios_init(void);
extern void __init dmi_check_pciprobe(void);
extern void __init dmi_check_skip_isa_align(void);

/* some common used subsys_initcalls */
extern int __init  pci_acpi_init(void);
extern void __init pcibios_irq_init(void);
extern int __init  pcibios_init(void);
extern int         pci_legacy_init(void);
extern void        pcibios_fixup_irqs(void);

/* pci-mmconfig.c */

/* "PCI MMCONFIG %04x [bus %02x-%02x]" */
#define PCI_MMCFG_RESOURCE_NAME_LEN (22 + 4 + 2 + 2)

struct pci_mmcfg_region {
    list_head_t     list;
    struct resource res;
    uint64_t        address;
    char __iomem   *virt;
    uint16_t        segment;
    uint8_t         start_bus;
    uint8_t         end_bus;
    char            name[PCI_MMCFG_RESOURCE_NAME_LEN];
};

extern int __init               pci_mmcfg_arch_init(void);
extern void __init              pci_mmcfg_arch_free(void);
extern int                      pci_mmcfg_arch_map(struct pci_mmcfg_region *cfg);
extern void                     pci_mmcfg_arch_unmap(struct pci_mmcfg_region *cfg);
extern int                      pci_mmconfig_insert(struct device *dev, uint16_t seg, uint8_t start, uint8_t end, phys_addr_t addr);
extern int                      pci_mmconfig_delete(uint16_t seg, uint8_t start, uint8_t end);
extern struct pci_mmcfg_region *pci_mmconfig_lookup(int segment, int bus);

extern list_head_t pci_mmcfg_list;

#define PCI_MMCFG_BUS_OFFSET(bus) ((bus) << 20)

/*
 * AMD Fam10h CPUs are buggy, and cannot access MMIO config space
 * on their northbrige except through the * %eax register. As such, you MUST
 * NOT use normal IOMEM accesses, you need to only use the magic mmio-config
 * accessor functions.
 * In fact just use pci_config_*, nothing else please.
 */
static inline unsigned char mmio_config_readb(void __iomem *pos)
{
    uint8_t val;
    asm volatile("movb (%1),%%al" : "=a"(val) : "r"(pos));
    return val;
}

static inline unsigned short mmio_config_readw(void __iomem *pos)
{
    uint16_t val;
    asm volatile("movw (%1),%%ax" : "=a"(val) : "r"(pos));
    return val;
}

static inline uint32_t mmio_config_readl(void __iomem *pos)
{
    uint32_t val;
    asm volatile("movl (%1),%%eax" : "=a"(val) : "r"(pos));
    return val;
}

static inline void mmio_config_writeb(void __iomem *pos, uint8_t val)
{
    asm volatile("movb %%al,(%1)" : : "a"(val), "r"(pos) : "memory");
}

static inline void mmio_config_writew(void __iomem *pos, uint16_t val)
{
    asm volatile("movw %%ax,(%1)" : : "a"(val), "r"(pos) : "memory");
}

static inline void mmio_config_writel(void __iomem *pos, uint32_t val)
{
    asm volatile("movl %%eax,(%1)" : : "a"(val), "r"(pos) : "memory");
}

#ifdef CONFIG_PCI
#ifdef CONFIG_ACPI
#define x86_default_pci_init pci_acpi_init
#else
#define x86_default_pci_init pci_legacy_init
#endif
#define x86_default_pci_init_irq   pcibios_irq_init
#define x86_default_pci_fixup_irqs pcibios_fixup_irqs
#else
#define x86_default_pci_init       NULL
#define x86_default_pci_init_irq   NULL
#define x86_default_pci_fixup_irqs NULL
#endif
