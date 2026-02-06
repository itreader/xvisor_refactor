/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * @file vmm_host_extend_irq.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Extended IRQ support, kind of Xvior compatible Linux IRQ domain.
 */

#include <libs/list.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_extend_irq.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#define HOST_IRQEXT_CHUNK BITS_PER_LONG

struct vmm_host_extend_irq_ctrl {
    vmm_rwlock_t          lock;
    uint32_t              count;
    uint64_t             *bitmap;
    struct vmm_host_irq **irqs;
};

static struct vmm_host_extend_irq_ctrl iectrl;

uint32_t vmm_host_extend_irq_count(void)
{
    irq_flags_t flags;
    uint32_t    count;

    vmm_read_lock_irq_save_lite(&iectrl.lock, flags);
    count = iectrl.count;
    vmm_read_unlock_irq_restore_lite(&iectrl.lock, flags);

    return count;
}

struct vmm_host_irq *__vmm_host_extend_irq_get(uint32_t hirq)
{
    irq_flags_t          flags;
    struct vmm_host_irq *irq = NULL;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return NULL;
    }

    hirq -= CONFIG_HOST_IRQ_COUNT;

    vmm_read_lock_irq_save_lite(&iectrl.lock, flags);

    if (hirq < iectrl.count) {
        irq = iectrl.irqs[hirq];
    }

    vmm_read_unlock_irq_restore_lite(&iectrl.lock, flags);

    return irq;
}

void vmm_host_extend_irq_debug_dump(vmm_char_device_t *cdev)
{
    int         idx = 0;
    irq_flags_t flags;

    vmm_read_lock_irq_save_lite(&iectrl.lock, flags);

    vmm_cdev_printf(cdev, "%d extended IRQs\n", iectrl.count);
    vmm_cdev_printf(cdev, "  BITMAP:\n");

    for (idx = 0; idx < BITS_TO_LONGS(iectrl.count); ++idx) {
        if (0 == (idx % 4)) {
            vmm_cdev_printf(cdev, "\n    %d:", idx);
        }

        vmm_cdev_printf(cdev, " 0x%lx", iectrl.bitmap[idx]);
    }

    vmm_cdev_printf(cdev, "\n");

    vmm_read_unlock_irq_restore_lite(&iectrl.lock, flags);
}

static void *realloc(void *ptr, uint32_t old_size, uint32_t new_size)
{
    void *new_ptr = NULL;

    if (new_size < old_size) {
        return ptr;
    }

    if (NULL == (new_ptr = vmm_zalloc(new_size))) {
        return NULL;
    }

    if (!ptr) {
        return new_ptr;
    }

    memcpy(new_ptr, ptr, old_size);
    vmm_free(ptr);

    return new_ptr;
}

static int _extend_irq_expand(void)
{
    uint32_t              old_size = iectrl.count;
    uint32_t              new_size = iectrl.count + HOST_IRQEXT_CHUNK;
    struct vmm_host_irq **irqs     = NULL;
    uint64_t             *bitmap   = NULL;

    irqs                           = realloc(iectrl.irqs, old_size * sizeof(struct vmm_host_irq *), new_size * sizeof(struct vmm_host_irq *));

    if (!irqs) {
        vmm_printf(
            "%s: Failed to reallocate extended IRQ array from "
            "%d to %d bytes\n",
            __func__, old_size, new_size);
        return VMM_ENOMEM;
    }

    old_size = BITS_TO_LONGS(old_size) * sizeof(uint64_t);
    new_size = BITS_TO_LONGS(new_size) * sizeof(uint64_t);

    bitmap   = realloc(iectrl.bitmap, old_size, new_size);

    if (!bitmap) {
        vmm_printf(
            "%s: Failed to reallocate extended IRQ bitmap from "
            "%d to %d bytes\n",
            __func__, old_size, new_size);
        vmm_free(irqs);
        return VMM_ENOMEM;
    }

    iectrl.irqs   = irqs;
    iectrl.bitmap = bitmap;
    iectrl.count += HOST_IRQEXT_CHUNK;

    return VMM_OK;
}

int vmm_host_extend_irq_alloc_region(uint32_t size)
{
    irq_flags_t flags;
    int         tries, size_log = 0, pos = -1;

    while ((1 << size_log) < size) {
        ++size_log;
    }

    if (!size_log || size_log > BITS_PER_LONG) {
        return VMM_ENOTAVAIL;
    }

    tries = ((1U << size_log) / HOST_IRQEXT_CHUNK) + 1;
    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

try_again:
    pos = bitmap_find_free_region(iectrl.bitmap, iectrl.count, size_log);

    if (pos < 0) {
        /*
         * Give a second try, reallocate some memory for extended
         * IRQs
         */
        if (VMM_OK == _extend_irq_expand()) {
            if (tries) {
                tries--;
                goto try_again;
            }
        }
    }

    vmm_write_unlock_irq_restore_lite(&iectrl.lock, flags);

    if (pos < 0) {
        vmm_printf("%s: Failed to find an extended IRQ region\n", __func__);
        return pos;
    }

    return pos + CONFIG_HOST_IRQ_COUNT;
}

int vmm_host_extend_irq_free_region(uint32_t hirq, uint32_t size)
{
    irq_flags_t flags;
    int         rc = VMM_OK, size_log = 0, pos = 0;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return VMM_EINVALID;
    }

    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

    if ((CONFIG_HOST_IRQ_COUNT + iectrl.count) <= hirq) {
        rc = VMM_EINVALID;
        goto done;
    }

    pos = hirq - CONFIG_HOST_IRQ_COUNT;

    while ((1 << size_log) < size) {
        ++size_log;
    }

    bitmap_release_region(iectrl.bitmap, pos, size_log);

done:
    vmm_write_unlock_irq_restore_lite(&iectrl.lock, flags);

    return rc;
}

int vmm_host_extend_irq_create_mapping(uint32_t hirq, uint32_t hwirq)
{
    int                  rc = VMM_OK;
    irq_flags_t          flags;
    struct vmm_host_irq *irq = NULL;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return VMM_EINVALID;
    }

    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

    if (iectrl.count <= (hirq - CONFIG_HOST_IRQ_COUNT)) {
        rc = VMM_EINVALID;
        goto done;
    }

    irq = iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT];

    if (irq) {
        rc = VMM_OK;
        goto done;
    }

    if (NULL == (irq = vmm_malloc(sizeof(struct vmm_host_irq)))) {
        vmm_printf("%s: Failed to allocate host IRQ\n", __func__);
        rc = VMM_ENOMEM;
        goto done;
    }

    __vmm_host_irq_init_desc(irq, hirq, hwirq, VMM_IRQ_STATE_EXTENDED);

    iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT] = irq;

done:
    vmm_write_unlock_irq_restore_lite(&iectrl.lock, flags);

    return rc;
}

int vmm_host_extend_irq_dispose_mapping(uint32_t hirq)
{
    int                  rc = VMM_OK;
    irq_flags_t          flags;
    struct vmm_host_irq *irq = NULL;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return VMM_EINVALID;
    }

    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

    if (iectrl.count <= (hirq - CONFIG_HOST_IRQ_COUNT)) {
        rc = VMM_EINVALID;
        goto done;
    }

    irq                                       = iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT];
    iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT] = NULL;

    if (irq) {
        if (irq->name) {
            vmm_free((void *)irq->name);
        }

        vmm_free(irq);
    }

done:
    vmm_write_unlock_irq_restore_lite(&iectrl.lock, flags);

    return rc;
}

int __init vmm_host_extend_irq_init(void)
{
    memset(&iectrl, 0, sizeof(struct vmm_host_extend_irq_ctrl));
    INIT_RW_LOCK(&iectrl.lock);

    return VMM_OK;
}
