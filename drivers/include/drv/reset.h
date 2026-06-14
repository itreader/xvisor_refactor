/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Code from the Linux kernel 3.16.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU Lesser General
 * Public License.  You may obtain a copy of the GNU Lesser General
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 *
 * @file reset.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief This file contains the reset driver support API.
 */

#ifndef _RESET_H_
#define _RESET_H_

#include <vmm_error.h>
#include <vmm_stdio.h>

struct vmm_device;
typedef struct vmm_device vmm_device_t;
struct vmm_device_tree_node;
typedef struct vmm_device_tree_node vmm_device_tree_node_t;
struct reset_control;

#if IS_ENABLED(CONFIG_RESET_CONTROLLER)

int reset_control_reset(struct reset_control *rstc);
int reset_control_assert(struct reset_control *rstc);
int reset_control_deassert(struct reset_control *rstc);

struct reset_control *reset_control_get(vmm_device_t *dev, const char *id);
void                  reset_control_put(struct reset_control *rstc);
struct reset_control *devm_reset_control_get(vmm_device_t *dev, const char *id);

int device_reset(vmm_device_t *dev);

static inline int device_reset_optional(vmm_device_t *dev)
{
    return device_reset(dev);
}

static inline struct reset_control *reset_control_get_optional(vmm_device_t *dev, const char *id)
{
    return reset_control_get(dev, id);
}

static inline struct reset_control *devm_reset_control_get_optional(vmm_device_t *dev, const char *id)
{
    return devm_reset_control_get(dev, id);
}

struct reset_control *of_reset_control_get(vmm_device_tree_node_t *node, const char *id);

#else

static inline int reset_control_reset(struct reset_control *rstc)
{
    WARN_ON(1);
    return 0;
}

static inline int reset_control_assert(struct reset_control *rstc)
{
    WARN_ON(1);
    return 0;
}

static inline int reset_control_deassert(struct reset_control *rstc)
{
    WARN_ON(1);
    return 0;
}

static inline void reset_control_put(struct reset_control *rstc)
{
    WARN_ON(1);
}

static inline int device_reset_optional(vmm_device_t *dev)
{
    return VMM_ERR_NOSYS;
}

static inline struct reset_control *reset_control_get_optional(vmm_device_t *dev, const char *id)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOSYS);
}

static inline struct reset_control *devm_reset_control_get_optional(vmm_device_t *dev, const char *id)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOSYS);
}

static inline struct reset_control *of_reset_control_get(vmm_device_tree_node_t *node, const char *id)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOSYS);
}

#endif /* CONFIG_RESET_CONTROLLER */

#endif /* _RESET_H_ */
