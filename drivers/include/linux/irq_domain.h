#ifndef _LINUX_IRQDOMAIN_H
#define _LINUX_IRQDOMAIN_H

#include <linux/interrupt.h>
#include <linux/of.h>

static inline struct irq_domain *irq_domain_add_linear(struct device_node *of_node, uint32_t size, const struct irq_domain_ops *ops, void *host_data)
{
    return vmm_host_irq_domain_add(of_node, -1, size, ops, host_data);
}

static inline uint32_t irq_create_mapping(struct irq_domain *domain, irq_hw_number_t hw_irq_num)
{
    return vmm_host_irq_domain_create_mapping(domain, hw_irq_num);
}

static inline void irq_dispose_mapping(uint32_t hirq)
{
    vmm_host_irq_domain_dispose_mapping(hirq);
}

#endif
