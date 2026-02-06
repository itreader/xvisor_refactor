/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_vspi.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual spi framework
 */
#ifndef _VMM_VSPI_H__
#define _VMM_VSPI_H__

#include <libs/list.h>
#include <vmm_completion.h>
#include <vmm_mutex.h>
#include <vmm_types.h>

#define VMM_VSPI_IPRIORITY 0

struct vmm_thread;
struct vmm_emulate_device;
typedef struct vmm_emulate_device vmm_emulate_device_t;
struct vmm_virtual_spi_host;
struct vmm_virtual_spi_slave;

typedef struct vmm_thread            vmm_thread_t;
typedef struct vmm_virtual_spi_host  vmm_virtual_spi_host_t;
typedef struct vmm_virtual_spi_slave vmm_virtual_spi_slave_t;

/** Representation of a virtual spi slave */
struct vmm_virtual_spi_slave {
    vmm_emulate_device_t   *edev;
    vmm_virtual_spi_host_t *vsh;
    char                    name[VMM_FIELD_NAME_SIZE];
    uint32_t                chip_select;
    uint32_t (*xfer)(struct vmm_virtual_spi_slave *vss, uint32_t data, void *private);
    void *private;
};

/** Representation of a virtual spi host */
struct vmm_virtual_spi_host {
    double_list_t         head;
    vmm_emulate_device_t *edev;
    char                  name[VMM_FIELD_NAME_SIZE];

    void (*xfer)(struct vmm_virtual_spi_host *vsh, void *private);
    vmm_completion_t xfer_avail;
    vmm_thread_t    *xfer_worker;

    uint32_t chip_select_count;

    vmm_mutex_t               slaves_lock;
    vmm_virtual_spi_slave_t **slaves;

    void *private;
};

/** Get virtual spi host for virtual spi slave */
vmm_virtual_spi_host_t *vmm_vspislave_get_host(vmm_virtual_spi_slave_t *vss);

/** Get name of virtual spi slave */
const char *vmm_vspislave_get_name(vmm_virtual_spi_slave_t *vss);

/** Get chip select of virtual spi slave */
uint32_t vmm_vspislave_get_chip_select(vmm_virtual_spi_slave_t *vss);

/** Create a virtual spi slave */
vmm_virtual_spi_slave_t *vmm_vspislave_create(
    vmm_emulate_device_t *edev, uint32_t chip_select, uint32_t (*xfer)(vmm_virtual_spi_slave_t *, uint32_t, void *), void *private);

/** Destroy a virtual spi slave */
int vmm_vspislave_destroy(vmm_virtual_spi_slave_t *vss);

/** Transfer data to a virtual spi slave of given virtual spi host */
uint32_t vmm_vspihost_xfer_data(vmm_virtual_spi_host_t *vsh, uint32_t chip_select, uint32_t data);

/** Schedule transfer for given virtual spi host */
void vmm_vspihost_schedule_xfer(vmm_virtual_spi_host_t *vsh);

/** Get name of virtual spi host */
const char *vmm_vspihost_get_name(vmm_virtual_spi_host_t *vsh);

/** Get number of chip selects for virtual spi host */
uint32_t vmm_vspihost_get_chip_select_count(vmm_virtual_spi_host_t *vsh);

/** Iterate over each virtual spi slave of given virtual spi host */
int vmm_vspihost_iterate_slaves(vmm_virtual_spi_host_t *vsh, void *data, int (*fn)(vmm_virtual_spi_host_t *, vmm_virtual_spi_slave_t *, void *));

/** Create a virtual spi host */
vmm_virtual_spi_host_t *vmm_vspihost_create(
    const char *name_prefix, vmm_emulate_device_t *edev, void (*xfer)(vmm_virtual_spi_host_t *, void *), uint32_t chip_select_count, void *private);

/** Destroy a virtual spi host */
int vmm_vspihost_destroy(vmm_virtual_spi_host_t *vsh);

/** Find virtual spi host for given emulated device */
vmm_virtual_spi_host_t *vmm_vspihost_find(vmm_emulate_device_t *edev);

/** Iterate over each virtual spi host */
int vmm_vspihost_iterate(vmm_virtual_spi_host_t *start, void *data, int (*fn)(vmm_virtual_spi_host_t *, void *));

/** Count of available virtual spi hosts */
uint32_t vmm_vspihost_count(void);

#endif
