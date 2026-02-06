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
 * @file arch_io.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Jim Huang (jserv@0xlab.org)
 * @brief header file for CPU I/O or Memory read/write functions
 */
#ifndef _ARCH_IO_H__
#define _ARCH_IO_H__

#include <arch_barrier.h>
#include <cpu_inline_asm.h>
#include <vmm_types.h>

#define __raw_write8(a, v)  (*(volatile uint8_t *)(a) = (v))
#define __raw_write16(a, v) (*(volatile uint16_t *)(a) = (v))
#define __raw_write32(a, v) (*(volatile uint32_t *)(a) = (v))
#define __raw_write64(a, v) (*(volatile uint64_t *)(a) = (v))

#define __raw_read8(a)      (*(volatile uint8_t *)(a))
#define __raw_read16(a)     (*(volatile uint16_t *)(a))
#define __raw_read32(a)     (*(volatile uint32_t *)(a))
#define __raw_read64(a)     (*(volatile uint64_t *)(a))

#define __iormb()           arch_rmb()
#define __iowmb()           arch_wmb()

/*
 * Endianness primitives
 * ------------------------
 */
#define arch_cpu_to_le16(v) (v)

#define arch_le16_to_cpu(v) (v)

#define arch_cpu_to_be16(v) rev16(v)

#define arch_be16_to_cpu(v) rev16(v)

#define arch_cpu_to_le32(v) (v)

#define arch_le32_to_cpu(v) (v)

#define arch_cpu_to_be32(v) rev32(v)

#define arch_be32_to_cpu(v) rev32(v)

#define arch_cpu_to_le64(v) (v)

#define arch_le64_to_cpu(v) (v)

#define arch_cpu_to_be64(v) rev64(v)

#define arch_be64_to_cpu(v) rev64(v)

#define __io(p)             ((void *)p)

/*
 * IO port access primitives
 * -------------------------
 *
 * The ARM doesn't have special IO access instructions; all IO is memory
 * mapped.  Note that these are defined to perform little endian accesses
 * only. For ARM, IO port read/write operations translate to a read/write
 * operation to memory address. All IO port read/write operations are
 * assumed to be little-endian.
 */
#define arch_outb(v, p)           \
    {                             \
        __iowmb();                \
        __raw_write8(__io(p), v); \
    }
#define arch_outw(v, p)            \
    {                              \
        __iowmb();                 \
        __raw_write16(__io(p), v); \
    }
#define arch_outl(v, p)            \
    {                              \
        __iowmb();                 \
        __raw_write32(__io(p), v); \
    }
#define arch_inb(p)                       \
    ({                                    \
        uint8_t v = __raw_read8(__io(p)); \
        __iormb();                        \
        v;                                \
    })
#define arch_inw(p)                         \
    ({                                      \
        uint16_t v = __raw_read16(__io(p)); \
        __iormb();                          \
        v;                                  \
    })
#define arch_inl(p)                         \
    ({                                      \
        uint32_t v = __raw_read32(__io(p)); \
        __iormb();                          \
        v;                                  \
    })

#define arch_outb_p(v, p) arch_outb((v), (p))
#define arch_outw_p(v, p) arch_outw((v), (p))
#define arch_outl_p(v, p) arch_outl((v), (p))
#define arch_inb_p(p)     arch_inb((p))
#define arch_inw_p(p)     arch_inw((p))
#define arch_inl_p(p)     arch_inl((p))

static inline void arch_insb(uint64_t p, void *b, int c)
{
    if (c) {
        uint8_t *buf = b;

        do {
            uint8_t x = arch_inb(p);
            *buf++    = x;
        } while (--c);
    }
}

static inline void arch_insw(uint64_t p, void *b, int c)
{
    if (c) {
        uint16_t *buf = b;

        do {
            uint16_t x = arch_inw(p);
            *buf++     = x;
        } while (--c);
    }
}

static inline void arch_insl(uint64_t p, void *b, int c)
{
    if (c) {
        uint32_t *buf = b;

        do {
            uint32_t x = arch_inl(p);
            *buf++     = x;
        } while (--c);
    }
}

static inline void arch_outsb(uint64_t p, const void *b, int c)
{
    if (c) {
        const uint8_t *buf = b;

        do {
            arch_outb(*buf++, p);
        } while (--c);
    }
}

static inline void arch_outsw(uint64_t p, const void *b, int c)
{
    if (c) {
        const uint16_t *buf = b;

        do {
            arch_outw(*buf++, p);
        } while (--c);
    }
}

static inline void arch_outsl(uint64_t p, const void *b, int c)
{
    if (c) {
        const uint32_t *buf = b;

        do {
            arch_outl(*buf++, p);
        } while (--c);
    }
}

/*
 * Memory access primitives
 * ------------------------
 */
#define arch_in_8(a)                \
    ({                              \
        uint8_t v = __raw_read8(a); \
        __iormb();                  \
        v;                          \
    })
#define arch_out_8(a, v)    \
    {                       \
        __iowmb();          \
        __raw_write8(a, v); \
    }
#define arch_in_le16(a)               \
    ({                                \
        uint16_t v = __raw_read16(a); \
        __iormb();                    \
        v;                            \
    })
#define arch_out_le16(a, v)  \
    ({                       \
        __raw_write16(a, v); \
        __iowmb();           \
    })
#define arch_in_be16(a)               \
    ({                                \
        uint16_t v = __raw_read16(a); \
        __iormb();                    \
        rev16(v);                     \
    })
#define arch_out_be16(a, v)           \
    {                                 \
        __iowmb();                    \
        __raw_write16(a, (rev16(v))); \
    }
#define arch_in_le32(a)               \
    ({                                \
        uint32_t v = __raw_read32(a); \
        __iormb();                    \
        v;                            \
    })
#define arch_out_le32(a, v)  \
    {                        \
        __iowmb();           \
        __raw_write32(a, v); \
    }
#define arch_in_be32(a)               \
    ({                                \
        uint32_t v = __raw_read32(a); \
        __iormb();                    \
        rev32(v);                     \
    })
#define arch_out_be32(a, v)         \
    {                               \
        __iowmb();                  \
        __raw_write32(a, rev32(v)); \
    }
#define arch_in_le64(a)               \
    ({                                \
        uint64_t v = __raw_read64(a); \
        __iormb();                    \
        v;                            \
    })
#define arch_out_le64(a, v)  \
    {                        \
        __iowmb();           \
        __raw_write64(a, v); \
    }
#define arch_in_be64(a)               \
    ({                                \
        uint64_t v = __raw_read64(a); \
        __iormb();                    \
        rev64(v);                     \
    })
#define arch_out_be64(a, v)         \
    {                               \
        __iowmb();                  \
        __raw_write64(a, rev64(v)); \
    }

#define arch_in_8_relax(a)          \
    ({                              \
        uint8_t v = __raw_read8(a); \
        v;                          \
    })
#define arch_out_8_relax(a, v) \
    {                          \
        __raw_write8(a, v);    \
    }
#define arch_in_le16_relax(a)         \
    ({                                \
        uint16_t v = __raw_read16(a); \
        v;                            \
    })
#define arch_out_le16_relax(a, v) ({ __raw_write16(a, v); })
#define arch_in_be16_relax(a)         \
    ({                                \
        uint16_t v = __raw_read16(a); \
        rev16(v);                     \
    })
#define arch_out_be16_relax(a, v)     \
    {                                 \
        __raw_write16(a, (rev16(v))); \
    }
#define arch_in_le32_relax(a)         \
    ({                                \
        uint32_t v = __raw_read32(a); \
        v;                            \
    })
#define arch_out_le32_relax(a, v) \
    {                             \
        __iowmb();                \
        __raw_write32(a, v);      \
    }
#define arch_in_be32_relax(a)         \
    ({                                \
        uint32_t v = __raw_read32(a); \
        rev32(v);                     \
    })
#define arch_out_be32_relax(a, v)   \
    {                               \
        __raw_write32(a, rev32(v)); \
    }
#define arch_in_le64_relax(a)         \
    ({                                \
        uint64_t v = __raw_read64(a); \
        v;                            \
    })
#define arch_out_le64_relax(a, v) \
    {                             \
        __raw_write64(a, v);      \
    }
#define arch_in_be64_relax(a)         \
    ({                                \
        uint64_t v = __raw_read64(a); \
        rev64(v);                     \
    })
#define arch_out_be64_relax(a, v)   \
    {                               \
        __raw_write64(a, rev64(v)); \
    }

#endif
