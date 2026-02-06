#ifndef __CONTROL_REG_ACCESS_H
#define __CONTROL_REG_ACCESS_H

/*
 * Volatile isn't enough to prevent the compiler from reordering the
 * read/write functions for the control registers and messing everything up.
 * A memory clobber would solve the problem, but would prevent reordering of
 * all loads stores around it, which can hurt performance. Solution is to
 * use a variable and mimic reads and writes to it to enforce serialization
 */
extern uint64_t __force_order;

static inline uint64_t read_cr0(void)
{
    uint64_t val;
    asm volatile("mov %%cr0,%0\n\t" : "=r"(val), "=m"(__force_order));
    return val;
}

static inline void write_cr0(uint64_t val)
{
    asm volatile("mov %0,%%cr0" : : "r"(val), "m"(__force_order));
}

static inline uint64_t read_cr2(void)
{
    uint64_t val;
    asm volatile("mov %%cr2,%0\n\t" : "=r"(val), "=m"(__force_order));
    return val;
}

static inline void write_cr2(uint64_t val)
{
    asm volatile("mov %0,%%cr2" : : "r"(val), "m"(__force_order));
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    asm volatile("mov %%cr3,%0\n\t" : "=r"(val), "=m"(__force_order));
    return val;
}

static inline void write_cr3(uint64_t val)
{
    asm volatile("mov %0,%%cr3" : : "r"(val), "m"(__force_order));
}

static inline uint64_t read_cr4(void)
{
    uint64_t val;
    asm volatile("mov %%cr4,%0\n\t" : "=r"(val), "=m"(__force_order));
    return val;
}

static inline uint64_t read_cr4_safe(void)
{
    uint64_t val;
    /* This could fault if %cr4 does not exist. In x86_64, a cr4 always
     * exists, so it will never fail. */
#ifdef CONFIG_X86_32
    asm volatile("1: mov %%cr4, %0\n"
                 "2:\n" _ASM_EXCEPTION_TABLE(1b, 2b)
                 : "=r"(val), "=m"(__force_order)
                 : ""(0));
#else
    val = read_cr4();
#endif
    return val;
}

static inline void write_cr4(uint64_t val)
{
    asm volatile("mov %0,%%cr4" : : "r"(val), "m"(__force_order));
}

#ifdef CONFIG_X86_64
static inline uint64_t read_cr8(void)
{
    uint64_t cr8;
    asm volatile("movq %%cr8,%0" : "=r"(cr8));
    return cr8;
}

static inline void write_cr8(uint64_t val)
{
    asm volatile("movq %0,%%cr8" ::"r"(val) : "memory");
}
#endif

static inline void wbinvd(void)
{
    asm volatile("wbinvd" : : : "memory");
}

static inline __always_inline void set_in_cr4(uint64_t mask)
{
    // mmu_cr4_features |= mask;
    write_cr4(read_cr4() | mask);
}

static inline __always_inline void clear_in_cr4(uint64_t mask)
{
    // mmu_cr4_features &= ~mask;
    write_cr4(read_cr4() & ~mask);
}

static inline uint64_t read_msr(uint32_t msr)
{
    uint32_t low, high;

    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return low | ((uint64_t)high << 32);
}

static inline void write_msr(uint32_t msr, uint64_t val)
{
    asm volatile("wrmsr"
                 : /* no output */
                 : "c"(msr), "a"(val), "d"(val >> 32)
                 : "memory");
}

static inline uint32_t read_rflags(void)
{
    uint32_t rflags;

    asm volatile("pushf\n\t"
                 "popq %%rax\n\t"
                 : "=a"(rflags)::"memory");

    return rflags;
}

#endif /* __CONTROL_REG_ACCESS_H */
