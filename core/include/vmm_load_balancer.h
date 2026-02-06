/**
 * Copyright (c) 2013 Jean-Christophe Dubois
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
 * @file vmm_load_balancer.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for hypervisor load balancer
 */

#ifndef __VMM_LOAD_BALANCER_H__
#define __VMM_LOAD_BALANCER_H__

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_types.h>

/** Load balancing algo instance */
struct vmm_load_balancer_algo {
    double_list_t head;
    uint32_t      rating;
    char          name[VMM_FIELD_NAME_SIZE];
    int (*start)(struct vmm_load_balancer_algo *);
    void (*balance)(struct vmm_load_balancer_algo *);
    void (*stop)(struct vmm_load_balancer_algo *);
    void *private;
};

static inline void vmm_load_balancer_set_algo_private(struct vmm_load_balancer_algo *lbalgo, void *private)
{
    if (lbalgo) {
        lbalgo->private = private;
    }
}

static inline void *vmm_load_balancer_get_algo_private(struct vmm_load_balancer_algo *lbalgo)
{
    return (lbalgo) ? lbalgo->private : NULL;
}

/** Current (or best rated) load balancing algo instance
 *  Note: This function must be called from Orphan (or Thread) Context
 */
struct vmm_load_balancer_algo *vmm_load_balancer_current_algo(void);

/** Register load balancing algo instance
 *  Note: This function must be called from Orphan (or Thread) Context
 */
int vmm_load_balancer_register_algo(struct vmm_load_balancer_algo *lbalgo);

/** Unregister load balancing algo instance
 *  Note: This function must be called from Orphan (or Thread) Context
 */
int vmm_load_balancer_unregister_algo(struct vmm_load_balancer_algo *lbalgo);

/** Initialize load balancer on each host CPU */
int vmm_load_balancer_init(void);

#endif /* __VMM_LOADBAL_H__ */
