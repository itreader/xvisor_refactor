/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_vserial.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟串口源代码
 */

#include <libs/stringlib.h>
#include <vio/vmm_vserial.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>

#define MODULE_DESC      "Virtual Serial Port Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VSERIAL_IPRIORITY)
#define MODULE_INIT      vmm_vserial_init
#define MODULE_EXIT      vmm_vserial_exit

/**
 * @brief 虚拟串口控制结构（内部），维护串口设备的运行时状态
 */
struct vmm_vserial_ctrl {
    vmm_mutex_t                   vser_list_lock; /**< vser_list_lock成员 */
    double_list_t                 vser_list; /**< vser_list成员 */
    vmm_blocking_notifier_chain_t notifier_chain; /**< 通知器链 */
};

static struct vmm_vserial_ctrl vsctrl;

/**
 * @brief 注册虚拟串口客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&vsctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_register_client);

/**
 * @brief 注销虚拟串口客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&vsctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_unregister_client);

/**
 * @brief 虚拟串口 发送
 * @param vser 虚拟串口设备指针
 * @param src 源设备树节点
 * @param len 大小
 * @return 成功返回发送的字节数，失败返回0
 */
uint32_t vmm_vserial_send(struct vmm_vserial *vser, uint8_t *src, uint32_t len)
{
    uint32_t i;

    if (!vser || !src) {
        return 0;
    }

    if (!vser->can_send || !vser->send) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (!vser->can_send(vser)) {
            break;
        }

        vser->send(vser, src[i]);
    }

    return i;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_send);

/**
 * @brief 虚拟串口 接收
 * @param vser 虚拟串口设备指针
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @return 成功返回接收的字节数，失败返回0
 */
uint32_t vmm_vserial_receive(struct vmm_vserial *vser, uint8_t *dst, uint32_t len)
{
    uint32_t                     i;
    irq_flags_t                  flags;
    struct vmm_vserial_receiver *receiver;

    if (!vser || !dst) {
        return 0; /**< 0 */
    }

    vmm_spin_lock_irq_save(&vser->receiver_list_lock, flags);

    if (list_empty(&vser->receiver_list)) {
        vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);

        for (i = 0; i < len; i++) {
            fifo_enqueue(vser->receive_fifo, &dst[i], TRUE);
        }

        return i;
    }

    for (i = 0; i < len; i++) {
        list_for_each_entry(receiver, &vser->receiver_list, head)
        {
            receiver->recv(vser, receiver->private, dst[i]);
        }
    }

    vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);

    return i;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_receive);

/**
 * @brief 注册虚拟串口接收器
 * @param vser 虚拟串口设备指针
 * @param (*recv 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_register_receiver(struct vmm_vserial *vser, void (*recv)(struct vmm_vserial *, void *, uint8_t), void *private)
{
    uint8_t                      chval;
    bool                         found;
    irq_flags_t                  flags;
    struct vmm_vserial_receiver *receiver;

    if (!vser || !recv) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    receiver = NULL;
    found    = FALSE;

    vmm_spin_lock_irq_save(&vser->receiver_list_lock, flags);

    list_for_each_entry(receiver, &vser->receiver_list, head)
    {
        if (receiver->recv == recv) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);
        return VMM_ERR_INVALID;
    }

    receiver = vmm_malloc(sizeof(struct vmm_vserial_receiver));

    if (!receiver) {
        vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);
        return VMM_ERR_FAIL;
    }

    INIT_LIST_HEAD(&receiver->head);
    receiver->recv    = recv;
    receiver->private = private;

    list_add_tail(&receiver->head, &vser->receiver_list);

    vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);

    while (!fifo_isempty(vser->receive_fifo)) {
        if (!fifo_dequeue(vser->receive_fifo, &chval)) {
            break;
        }

        list_for_each_entry(receiver, &vser->receiver_list, head)
        {
            receiver->recv(vser, receiver->private, chval);
        }
    }

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_register_receiver);

/**
 * @brief 注销虚拟串口接收器
 * @param vser 虚拟串口设备指针
 * @param (*recv 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_unregister_receiver(struct vmm_vserial *vser, void (*recv)(struct vmm_vserial *, void *, uint8_t), void *private)
{
    bool                         found;
    irq_flags_t                  flags;
    struct vmm_vserial_receiver *receiver;

    if (!vser || !recv) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    receiver = NULL;
    found    = FALSE;

    vmm_spin_lock_irq_save(&vser->receiver_list_lock, flags);

    list_for_each_entry(receiver, &vser->receiver_list, head)
    {
        if ((receiver->recv == recv) && (receiver->private == private)) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);
        return VMM_ERR_INVALID;
    }

    list_del(&receiver->head);

    vmm_spin_unlock_irq_restore(&vser->receiver_list_lock, flags);

    vmm_free(receiver);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_unregister_receiver);

struct vmm_vserial *vmm_vserial_create(
    const char *name, bool (*can_send)(struct vmm_vserial *), int (*send)(struct vmm_vserial *, uint8_t), uint32_t receive_fifo_size, void *private)
{
    bool                     found; /**< found成员 */
    struct vmm_vserial      *vser; /**< vser成员 */
    struct vmm_vserial_event event; /**< 事件 */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    vser  = NULL; /**< NULL成员 */
    found = FALSE; /**< FALSE成员 */

    vmm_mutex_lock(&vsctrl.vser_list_lock);

    list_for_each_entry(vser, &vsctrl.vser_list, head)
    {
        if (strcmp(name, vser->name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&vsctrl.vser_list_lock);
        return NULL; /**< NULL成员 */
    }

    vser = vmm_malloc(sizeof(struct vmm_vserial)); /**< vmm_vserial))成员 */

    if (!vser) {
        vmm_mutex_unlock(&vsctrl.vser_list_lock);
        return NULL; /**< NULL成员 */
    }

    vser->receive_fifo = fifo_alloc(1, receive_fifo_size); /**< receive_fifo_size)成员 */

    if (!(vser->receive_fifo)) {
        vmm_free(vser);
        vmm_mutex_unlock(&vsctrl.vser_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&vser->head);

    if (strlcpy(vser->name, name, sizeof(vser->name)) >= sizeof(vser->name)) {
        fifo_free(vser->receive_fifo);
        vmm_free(vser);
        vmm_mutex_unlock(&vsctrl.vser_list_lock);
        return NULL; /**< NULL成员 */
    }

    vser->can_send = can_send; /**< can_send成员 */
    vser->send     = send; /**< send成员 */
    INIT_SPIN_LOCK(&vser->receiver_list_lock);
    INIT_LIST_HEAD(&vser->receiver_list);
    vser->private = private; /**< 私有数据 */

    list_add_tail(&vser->head, &vsctrl.vser_list); /**< &vsctrl.vser_list)成员 */

    vmm_mutex_unlock(&vsctrl.vser_list_lock);

    /* Broadcast create event */
    event.vser = vser; /**< vser成员 */
    event.data = NULL; /**< NULL成员 */
    vmm_blocking_notifier_call(&vsctrl.notifier_chain, VMM_VSERIAL_EVENT_CREATE, &event); /**< &event)成员 */

    return vser; /**< vser成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_create);

/**
 * @brief 销毁虚拟串口
 * @param vser 虚拟串口设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_destroy(struct vmm_vserial *vser)
{
    bool                     found;
    struct vmm_vserial      *vs;
    struct vmm_vserial_event event;

    if (!vser) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    /* Broadcast destroy event */
    event.vser = vser;
    event.data = NULL;
    vmm_blocking_notifier_call(&vsctrl.notifier_chain, VMM_VSERIAL_EVENT_DESTROY, &event);

    vmm_mutex_lock(&vsctrl.vser_list_lock);

    if (list_empty(&vsctrl.vser_list)) {
        vmm_mutex_unlock(&vsctrl.vser_list_lock);
        return VMM_ERR_FAIL;
    }

    vs    = NULL;
    found = FALSE;

    list_for_each_entry(vs, &vsctrl.vser_list, head)
    {
        if (strcmp(vs->name, vser->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&vsctrl.vser_list_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&vs->head);

    fifo_free(vs->receive_fifo);
    vmm_free(vs);

    vmm_mutex_unlock(&vsctrl.vser_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_destroy);

struct vmm_vserial *vmm_vserial_find(const char *name)
{
    bool                found; /**< found成员 */
    struct vmm_vserial *vs; /**< vs */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    found = FALSE; /**< FALSE成员 */
    vs    = NULL; /**< NULL成员 */

    vmm_mutex_lock(&vsctrl.vser_list_lock);

    list_for_each_entry(vs, &vsctrl.vser_list, head)
    {
        if (strcmp(vs->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    vmm_mutex_unlock(&vsctrl.vser_list_lock);

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    return vs; /**< vs */
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_find);

/**
 * @brief 虚拟串口 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_iterate(struct vmm_vserial *start, void *data, int (*fn)(struct vmm_vserial *vs, void *data))
{
    int                 rc          = VMM_OK;
    bool                start_found = (start) ? FALSE : TRUE;
    struct vmm_vserial *vs          = NULL;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&vsctrl.vser_list_lock);

    list_for_each_entry(vs, &vsctrl.vser_list, head)
    {
        if (!start_found) {
            if (start && start == vs) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vs, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&vsctrl.vser_list_lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_iterate);

/**
 * @brief 获取虚拟串口的数量
 * @return 数量值
 */
uint32_t vmm_vserial_count(void)
{
    uint32_t            retval = 0;
    struct vmm_vserial *vs;

    vmm_mutex_lock(&vsctrl.vser_list_lock);

    list_for_each_entry(vs, &vsctrl.vser_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&vsctrl.vser_list_lock);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_vserial_count);

/**
 * @brief 初始化虚拟串口
 * @return 数量值
 */
static int __init vmm_vserial_init(void)
{
    memset(&vsctrl, 0, sizeof(vsctrl));

    INIT_MUTEX(&vsctrl.vser_list_lock);
    INIT_LIST_HEAD(&vsctrl.vser_list);
    BLOCKING_INIT_NOTIFIER_CHAIN(&vsctrl.notifier_chain);

    return VMM_OK;
}

/**
 * @brief 虚拟串口子系统退出
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_vserial_exit(void)
{
    /* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
