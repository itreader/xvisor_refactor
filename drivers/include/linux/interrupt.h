#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include <vmm_host_extend_irq.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>

typedef vmm_irq_return_t irqreturn_t;
typedef uint32_t         irq_hw_number_t;

#define irq_chip                      vmm_host_irq_chip
#define irq_data                      vmm_host_irq
#define irq_domain                    vmm_host_irq_domain
#define irq_domain_ops                vmm_host_irq_domain_ops

#define IRQ_TYPE_EDGE_RISING          VMM_IRQ_TYPE_EDGE_RISING
#define IRQ_TYPE_EDGE_FALLING         VMM_IRQ_TYPE_EDGE_FALLING
#define IRQ_TYPE_EDGE_BOTH            VMM_IRQ_TYPE_EDGE_BOTH
#define IRQ_TYPE_LEVEL_HIGH           VMM_IRQ_TYPE_LEVEL_HIGH
#define IRQ_TYPE_LEVEL_LOW            VMM_IRQ_TYPE_LEVEL_LOW
#define IRQ_TYPE_NONE                 VMM_IRQ_TYPE_NONE

#define IRQ_NONE                      VMM_IRQ_NONE
#define IRQ_HANDLED                   VMM_IRQ_HANDLED

#define IRQF_SHARED                   0x0
#define IRQF_TRIGGER_RISING           VMM_IRQ_TYPE_EDGE_RISING

#define IRQ_RETVAL(x)                 ((x) != VMM_IRQ_NONE)

#define irq_data_get_irq_chip_data(d) vmm_host_irq_get_chip_data(d)

static inline int request_irq(uint32_t irq, vmm_host_irq_function_t func, uint64_t flags, const char *name, void *dev)
{
    return vmm_host_irq_register(irq, name, func, dev);
}

static inline void free_irq(uint32_t irq, void *dev)
{
    vmm_host_irq_unregister(irq, dev);
}

static inline void synchronize_irq(uint32_t irq)
{
    /* For now do nothing. */
}

static inline void enable_irq(uint32_t irq)
{
    vmm_host_irq_enable(irq);
}

static inline void disable_irq(uint32_t irq)
{
    vmm_host_irq_disable(irq);
}

#define disable_irq_nosync       disable_irq

#define local_irq_save(flags)    arch_cpu_irq_save(flags)
#define local_irq_restore(flags) arch_cpu_irq_restore(flags)

/* Placeholders */
#define tasklet_schedule(x)
#define tasklet_hi_schedule(x)
#define tasklet_kill(x)

#endif /* _INTERRUPT_H */
