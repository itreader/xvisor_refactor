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
 * @file vmm_share_memory.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for shared memory subsystem
 */

#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_mutex.h>
#include <vmm_share_memory.h>
#include <vmm_stdio.h>

struct vmm_share_memory_ctrl {
    vmm_mutex_t   lock;
    double_list_t share_memory_list;
};

static struct vmm_share_memory_ctrl shmctrl;

uint32_t vmm_share_memory_read(vmm_share_memory_t *share_memory, physical_addr_t off, void *dst, uint32_t len, bool cacheable)
{
    if (!share_memory || !dst) {
        return 0;
    }

    if (share_memory->size < (off + len)) {
        return 0;
    }

    return vmm_host_memory_read(share_memory->addr + off, dst, len, cacheable);
}

uint32_t vmm_share_memory_write(vmm_share_memory_t *share_memory, physical_addr_t off, void *src, uint32_t len, bool cacheable)
{
    if (!share_memory || !src) {
        return 0;
    }

    if (share_memory->size < (off + len)) {
        return 0;
    }

    return vmm_host_memory_write(share_memory->addr + off, src, len, cacheable);
}

uint32_t vmm_share_memory_set(vmm_share_memory_t *share_memory, physical_addr_t off, uint8_t byte, uint32_t len, bool cacheable)
{
    if (!share_memory) {
        return 0;
    }

    if (share_memory->size < (off + len)) {
        return 0;
    }

    return vmm_host_memory_set(share_memory->addr + off, byte, len, cacheable);
}

int vmm_share_memory_iterate(int (*iter)(vmm_share_memory_t *, void *), void *private)
{
    int                 rc = VMM_OK;
    vmm_share_memory_t *share_memory;

    if (!iter) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&shmctrl.lock);

    list_for_each_entry(share_memory, &shmctrl.share_memory_list, head)
    {
        rc = iter(share_memory, private);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&shmctrl.lock);

    return rc;
}

static int share_memory_count(vmm_share_memory_t *share_memory, void *private)
{
    uint32_t *cntp = private;

    if (cntp) {
        (*cntp)++;
    }

    return VMM_OK;
}

uint32_t vmm_share_memory_count(void)
{
    uint32_t count = 0;

    return (!vmm_share_memory_iterate(share_memory_count, &count)) ? count : 0;
}

struct share_memory_find_data {
    const char         *name;
    vmm_share_memory_t *share_memory;
};

static int share_memory_find_byname(vmm_share_memory_t *share_memory, void *private)
{
    struct share_memory_find_data *data = private;

    if (!data->share_memory) {
        if (!strncmp(share_memory->name, data->name, sizeof(share_memory->name))) {
            vmm_share_memory_ref(share_memory);
            data->share_memory = share_memory;
        }
    }

    return VMM_OK;
}

vmm_share_memory_t *vmm_share_memory_find_byname(const char *name)
{
    struct share_memory_find_data data;

    if (!name) {
        return NULL;
    }

    data.name         = name;
    data.share_memory = NULL;

    return (!vmm_share_memory_iterate(share_memory_find_byname, &data)) ? data.share_memory : NULL;
}

void vmm_share_memory_ref(vmm_share_memory_t *share_memory)
{
    if (!share_memory) {
        return;
    }

    xref_get(&share_memory->ref_count);
}

static void __share_memory_free(struct xref *ref)
{
    vmm_share_memory_t *share_memory = container_of(ref, vmm_share_memory_t, ref_count);

    vmm_mutex_lock(&shmctrl.lock);

    list_del(&share_memory->head);
    vmm_host_ram_free(share_memory->addr, share_memory->size);
    vmm_free(share_memory);

    vmm_mutex_unlock(&shmctrl.lock);
}

void vmm_share_memory_dref(vmm_share_memory_t *share_memory)
{
    if (share_memory) {
        xref_put(&share_memory->ref_count, __share_memory_free);
    }
}

vmm_share_memory_t *vmm_share_memory_create(const char *name, physical_size_t size, uint32_t align_order, void *private)
{
    bool                found = FALSE;
    vmm_share_memory_t *share_memory;

    if (!name || !size) {
        return VMM_ERR_PTR(VMM_EINVALID);
    }

    size = VMM_ROUNDUP2_PAGE_SIZE(size);

    vmm_mutex_lock(&shmctrl.lock);

    list_for_each_entry(share_memory, &shmctrl.share_memory_list, head)
    {
        if (!strncmp(share_memory->name, name, sizeof(share_memory->name))) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&shmctrl.lock);
        return VMM_ERR_PTR(VMM_EEXIST);
    };

    share_memory = vmm_zalloc(sizeof(*share_memory));

    if (!share_memory) {
        vmm_mutex_unlock(&shmctrl.lock);
        return VMM_ERR_PTR(VMM_ENOMEM);
    }

    INIT_LIST_HEAD(&share_memory->head);
    xref_init(&share_memory->ref_count);
    strncpy(share_memory->name, name, sizeof(share_memory->name));

    share_memory->size = vmm_host_ram_alloc(&share_memory->addr, size, align_order);

    if (!share_memory->size) {
        vmm_free(share_memory);
        vmm_mutex_unlock(&shmctrl.lock);
        return VMM_ERR_PTR(VMM_ENOMEM);
    }

    share_memory->align_order = align_order;
    share_memory->private     = private;

    list_add_tail(&share_memory->head, &shmctrl.share_memory_list);

    vmm_mutex_unlock(&shmctrl.lock);

    return share_memory;
}

int __init vmm_share_memory_init(void)
{
    memset(&shmctrl, 0, sizeof(shmctrl));

    INIT_MUTEX(&shmctrl.lock);
    INIT_LIST_HEAD(&shmctrl.share_memory_list);

    return VMM_OK;
}
