/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_device_resource.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver resource management header
 */

#ifndef __VMM_DEVRES_H_
#define __VMM_DEVRES_H_

#include <vmm_types.h>

struct vmm_device;
typedef struct vmm_device vmm_device_t;

/** Device resource match and release functions */
typedef void (*vmm_device_resource_release_t)(vmm_device_t *dev, void *res);
typedef int (*vmm_device_resource_match_t)(vmm_device_t *dev, void *res, void *match_data);

/** Allocate device resource data of given size */
void *vmm_device_resource_alloc(vmm_device_resource_release_t release, size_t size);

/** Iterate over each device resource */
void vmm_device_resource_for_each_resource(
    vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data,
    void (*fn)(vmm_device_t *, void *, void *), void *data);

/** Free device resource data */
void vmm_device_resource_free(void *res);

/** Register device resource */
void vmm_device_resource_add(vmm_device_t *dev, void *res);

/** Find device resource */
void *vmm_device_resource_find(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/** Find devres, if non-existent, add one atomically */
void *vmm_device_resource_get(vmm_device_t *dev, void *new_res, vmm_device_resource_match_t match, void *match_data);

/** Find a device resource and remove it but don't free it. */
void *vmm_device_resource_remove(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/** Find a device resource and destroy it, without calling release */
int vmm_device_resource_destroy(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/** Find a device resource and destroy it, calling release */
int vmm_device_resource_release(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/** Release all managed resources */
int vmm_device_resource_release_all(vmm_device_t *dev);

/** Resource-managed malloc */
void *vmm_devm_malloc(vmm_device_t *dev, size_t size);

/** Resource-managed zalloc */
void *vmm_devm_zalloc(vmm_device_t *dev, size_t size);

/** Resource-managed malloc array */
void *vmm_devm_malloc_array(vmm_device_t *dev, size_t n, size_t size);

/** Resource-managed calloc */
void *vmm_devm_calloc(vmm_device_t *dev, size_t n, size_t size);

/** Allocate resource managed space and copy an existing string into that. */
char *vmm_devm_strdup(vmm_device_t *dev, const char *s);

/** Resource-managed free */
void vmm_devm_free(vmm_device_t *dev, void *p);

/** Add custom resource-managed action */
int vmm_devm_add_action(vmm_device_t *dev, void (*action)(void *), void *data);

/** Remove custom resource-managed action */
void vmm_devm_remove_action(vmm_device_t *dev, void (*action)(void *), void *data);

#endif /* __VMM_DEVRES_H_ */
