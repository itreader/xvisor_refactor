/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_host_extend_irq.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Extended Host IRQ support.
 */

#ifndef _VMM_HOST_IRQEXT_H__
#define _VMM_HOST_IRQEXT_H__

#include <vmm_host_irq.h>
#include <vmm_types.h>

struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

struct vmm_host_irq *__vmm_host_extend_irq_get(uint32_t hirq);

int vmm_host_extend_irq_alloc_region(uint32_t size);

int vmm_host_extend_irq_free_region(uint32_t hirq, uint32_t size);

int vmm_host_extend_irq_create_mapping(uint32_t hirq, uint32_t hwirq);

int vmm_host_extend_irq_dispose_mapping(uint32_t hirq);

void vmm_host_extend_irq_debug_dump(vmm_char_device_t *cdev);

int vmm_host_extend_irq_init(void);

uint32_t vmm_host_extend_irq_count(void);

#endif /* _VMM_HOST_IRQEXT_H__ */
