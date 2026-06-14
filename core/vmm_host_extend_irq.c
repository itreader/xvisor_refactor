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
 * @brief 扩展IRQ支持，类似Linux IRQ域的Xvisor兼容实现
 */

#include <libs/list.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_extend_irq.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#define HOST_IRQEXT_CHUNK BITS_PER_LONG

/**
 * @brief 主机扩展中断控制结构，管理扩展中断的注册状态
 */
struct vmm_host_extend_irq_ctrl {
    vmm_rwlock_t          lock; /**< 自旋锁 */
    uint32_t              count; /**< 计数 */
    uint64_t             *bitmap; /**< bitmap成员 */
    vmm_host_irq_t **irqs; /**< 中断上下文 */
};

static struct vmm_host_extend_irq_ctrl iectrl;

/**
 * @brief 获取主机扩展中断的数量
 * @return 数量值
 */
uint32_t vmm_host_extend_irq_count(void)
{
    irq_flags_t flags;
    uint32_t    count;

    vmm_read_lock_irq_save_lite(&iectrl.lock, flags);
    count = iectrl.count;
    vmm_read_unlock_irq_restore_lite(&iectrl.lock, flags);

    return count;
}

/**
 * @brief 获取主机扩展中断控制器
 * @param hirq 中断号
 * @return 目标对象指针，不存在返回NULL
 */
vmm_host_irq_t *__vmm_host_extend_irq_get(uint32_t hirq)
{
    irq_flags_t          flags;
    vmm_host_irq_t *irq = NULL;

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

/**
 * @brief 输出主机扩展中断的调试信息
 * @param cdev 字符设备指针
 */
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

/**
 * @brief realloc
 * @param ptr 通用指针
 * @param old_size 大小
 * @param new_size 大小
 * @return 成功返回目标指针，失败返回NULL
 */
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

/**
 * @brief 扩展中断号范围
 * @return 中断处理结果
 */
static int _extend_irq_expand(void)
{
    uint32_t              old_size = iectrl.count;
    uint32_t              new_size = iectrl.count + HOST_IRQEXT_CHUNK;
    vmm_host_irq_t **irqs     = NULL;
    uint64_t             *bitmap   = NULL;

    irqs                           = realloc(iectrl.irqs, old_size * sizeof(vmm_host_irq_t *), new_size * sizeof(vmm_host_irq_t *));

    if (!irqs) {
        vmm_printf(
            "%s: Failed to reallocate extended IRQ array from "
            "%d to %d bytes\n",
            __func__, old_size, new_size);
        return VMM_ERR_NOMEM;
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
        return VMM_ERR_NOMEM;
    }

    iectrl.irqs   = irqs;
    iectrl.bitmap = bitmap;
    iectrl.count += HOST_IRQEXT_CHUNK;

    return VMM_OK;
}

/**
 * @brief 分配主机扩展中断区域
 * @param size 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_alloc_region(uint32_t size)
{
    irq_flags_t flags;
    int tries;
    int size_log = 0;
    int pos = -1;

    while ((1 << size_log) < size) {
        ++size_log;
    }

    if (!size_log || size_log > BITS_PER_LONG) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * @brief 释放主机扩展中断区域
 * @param hirq 中断号
 * @param size 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_free_region(uint32_t hirq, uint32_t size)
{
    irq_flags_t flags;
    int rc = VMM_OK;
    int size_log = 0;
    int pos = 0;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return VMM_ERR_INVALID;
    }

    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

    if ((CONFIG_HOST_IRQ_COUNT + iectrl.count) <= hirq) {
        rc = VMM_ERR_INVALID;
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

/**
 * @brief 创建主机扩展中断映射
 * @param hirq 中断号
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_create_mapping(uint32_t hirq, uint32_t hw_irq_num)
{
    int                  rc = VMM_OK;
    irq_flags_t          flags;
    vmm_host_irq_t *irq = NULL;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return VMM_ERR_INVALID;
    }

    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

    if (iectrl.count <= (hirq - CONFIG_HOST_IRQ_COUNT)) {
        rc = VMM_ERR_INVALID;
        goto done;
    }

    irq = iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT];

    if (irq) {
        rc = VMM_OK;
        goto done;
    }

    if (NULL == (irq = vmm_malloc(sizeof(vmm_host_irq_t)))) {
        vmm_printf("%s: Failed to allocate host IRQ\n", __func__);
        rc = VMM_ERR_NOMEM;
        goto done;
    }

    __vmm_host_irq_init_desc(irq, hirq, hw_irq_num, VMM_IRQ_STATE_EXTENDED);

    iectrl.irqs[hirq - CONFIG_HOST_IRQ_COUNT] = irq;

done:
    vmm_write_unlock_irq_restore_lite(&iectrl.lock, flags);

    return rc;
}

/**
 * @brief 释放主机扩展中断映射
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_dispose_mapping(uint32_t hirq)
{
    int                  rc = VMM_OK;
    irq_flags_t          flags;
    vmm_host_irq_t *irq = NULL;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return VMM_ERR_INVALID;
    }

    vmm_write_lock_irq_save_lite(&iectrl.lock, flags);

    if (iectrl.count <= (hirq - CONFIG_HOST_IRQ_COUNT)) {
        rc = VMM_ERR_INVALID;
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

/**
 * @brief 初始化主机扩展中断
 * @return 中断处理结果
 */
int __init vmm_host_extend_irq_init(void)
{
    memset(&iectrl, 0, sizeof(struct vmm_host_extend_irq_ctrl));
    INIT_RW_LOCK(&iectrl.lock);

    return VMM_OK;
}
