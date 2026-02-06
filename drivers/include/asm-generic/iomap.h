#ifndef __GENERIC_IO_H
#define __GENERIC_IO_H

#include <asm/byteorder.h>
#include <linux/linkage.h>

/*
 * These are the "generic" interfaces for doing new-style
 * memory-mapped or PIO accesses. Architectures may do
 * their own arch-optimized versions, these just act as
 * wrappers around the old-style IO register access functions:
 * read[bwl]/write[bwl]/in[bwl]/out[bwl]
 *
 * Don't include this directly, include it from <asm/io.h>.
 */

/*
 * Read/write from/to an (offsettable) iomem cookie. It might be a PIO
 * access or a MMIO access, these functions don't care. The info is
 * encoded in the hardware mapping set up by the mapping functions
 * (or the cookie itself, depending on implementation and hw).
 *
 * The generic routines just encode the PIO/MMIO as part of the
 * cookie, and coldly assume that the MMIO IO mappings are not
 * in the low address range. Architectures for which this is not
 * true can't use this generic implementation.
 */
extern uint32_t ioread8(void __iomem *);
extern uint32_t ioread16(void __iomem *);
extern uint32_t ioread16be(void __iomem *);
extern uint32_t ioread32(void __iomem *);
extern uint32_t ioread32be(void __iomem *);

extern void iowrite8(uint8_t, void __iomem *);
extern void iowrite16(uint16_t, void __iomem *);
extern void iowrite16be(uint16_t, void __iomem *);
extern void iowrite32(uint32_t, void __iomem *);
extern void iowrite32be(uint32_t, void __iomem *);

/*
 * "string" versions of the above. Note that they
 * use native byte ordering for the accesses (on
 * the assumption that IO and memory agree on a
 * byte order, and CPU byteorder is irrelevant).
 *
 * They do _not_ update the port address. If you
 * want MMIO that copies stuff laid out in MMIO
 * memory across multiple ports, use "memcpy_toio()"
 * and friends.
 */
extern void ioread8_rep(void __iomem *port, void *buf, uint64_t count);
extern void ioread16_rep(void __iomem *port, void *buf, uint64_t count);
extern void ioread32_rep(void __iomem *port, void *buf, uint64_t count);

extern void iowrite8_rep(void __iomem *port, const void *buf, uint64_t count);
extern void iowrite16_rep(void __iomem *port, const void *buf, uint64_t count);
extern void iowrite32_rep(void __iomem *port, const void *buf, uint64_t count);

#ifdef CONFIG_HAS_IOPORT_MAP
/* Create a virtual mapping cookie for an IO port range */
extern void __iomem *ioport_map(uint64_t port, uint32_t nr);
extern void          ioport_unmap(void __iomem *);
#endif

#ifndef ARCH_HAS_IOREMAP_WC
#define ioremap_wc ioremap_nocache
#endif

#ifdef CONFIG_PCI
/* Destroy a virtual mapping cookie for a PCI BAR (memory or IO) */
struct pci_dev;
extern void pci_iounmap(struct pci_dev *dev, void __iomem *);
#elif defined(CONFIG_GENERIC_IOMAP)
struct pci_dev;

static inline void pci_iounmap(struct pci_dev *dev, void __iomem *addr) {}
#endif

#include <asm-generic/pci_iomap.h>

#endif
