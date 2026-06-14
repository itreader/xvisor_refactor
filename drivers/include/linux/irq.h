#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

#include <linux/bug.h>
#include <linux/interrupt.h>

static inline uint32_t irq_find_mapping(struct irq_domain *host, irq_hw_number_t hw_irq_num)
{
    return vmm_host_irq_domain_find_mapping(host, hw_irq_num);
}

/* FIXME: Need to fix this */
static inline int can_request_irq(uint32_t irq, uint64_t irqflags)
{
    WARN_ONCE(1, "FIXME!! FIXME!!");
    return 1;
}
#endif
