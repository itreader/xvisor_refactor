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
 * @file vmm_vmsg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟消息子系统实现
 */

#include <libs/idr.h>
#include <libs/mempool.h>
#include <libs/stringlib.h>
#include <vio/vmm_vmsg.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_modules.h>
#include <vmm_per_cpu.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC      "Virtual Messaging Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VMSG_IPRIORITY)
#define MODULE_INIT      vmm_vmsg_init
#define MODULE_EXIT      vmm_vmsg_exit

/**
 * @brief 虚拟消息控制结构（内部），管理消息域的创建和节点注册
 */
struct vmm_vmsg_control {
    vmm_mutex_t   lock; /**< 自旋锁 */
    double_list_t domain_list; /**< 域链表 */
    double_list_t node_list; /**< node_list成员 */
    DECLARE_IDA(node_ida);
    vmm_blocking_notifier_chain_t notifier_chain; /**< 通知器链 */
    struct vmm_vmsg_domain       *default_domain; /**< default_domain成员 */
};

static struct vmm_vmsg_control vmctrl;

/**
 * @brief 注册虚拟消息客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&vmctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_register_client);

/**
 * @brief 注销虚拟消息客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&vmctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_unregister_client);

/**
 * @brief 增加虚拟消息引用计数
 * @param msg 消息字符串
 */
void vmm_vmsg_ref(struct vmm_vmsg *msg)
{
    if (msg) {
        xref_get(&msg->ref_count);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_unregister_client);

/**
 * @brief   释放虚拟消息对象
 * @param ref 引用计数结构体指针
 */
static void __vmsg_free(struct xref *ref)
{
    struct vmm_vmsg *msg = container_of(ref, struct vmm_vmsg, ref_count);

    if (msg->free_data) {
        msg->free_data(msg);
    }

    if (msg->free_hdr) {
        msg->free_hdr(msg);
    }
}

/**
 * @brief 减少虚拟消息引用计数
 * @param msg 消息字符串
 */
void vmm_vmsg_dref(struct vmm_vmsg *msg)
{
    if (msg) {
        xref_put(&msg->ref_count, __vmsg_free);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_dref);

/**
 * @brief 释放虚拟消息的数据部分
 * @param msg 消息字符串
 */
static void vmsg_free_data(struct vmm_vmsg *msg)
{
    vmm_free(msg->data);
}

/**
 * @brief 释放虚拟消息的头部结构
 * @param msg 消息字符串
 */
static void vmsg_free_hdr(struct vmm_vmsg *msg)
{
    vmm_free(msg);
}

struct vmm_vmsg *vmm_vmsg_alloc_ext(
    uint32_t dst, uint32_t src, uint32_t local, void *data, size_t len, void *private, void (*free_data)(struct vmm_vmsg *))
{
    struct vmm_vmsg *msg; /**< 消息 */

    msg = vmm_malloc(sizeof(*msg)); /**< 消息 */

    if (!msg) {
        return NULL; /**< NULL成员 */
    }

    INIT_VMSG(msg, dst, src, local, data, len, private, free_data, vmsg_free_hdr); /**< vmsg_free_hdr)成员 */

    return msg; /**< 消息 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_alloc_ext);

struct vmm_vmsg *vmm_vmsg_alloc(uint32_t dst, uint32_t src, uint32_t local, size_t len, void *private)
{
    void            *data; /**< 数据 */
    struct vmm_vmsg *msg; /**< 消息 */

    if (!len) {
        return NULL; /**< NULL成员 */
    }

    data = vmm_malloc(len); /**< vmm_malloc(len)成员 */

    if (!data) {
        return NULL; /**< NULL成员 */
    }

    msg = vmm_vmsg_alloc_ext(dst, src, local, data, len, private, vmsg_free_data); /**< vmsg_free_data)成员 */

    if (!msg) {
        vmm_free(data);
        return NULL; /**< NULL成员 */
    }

    return msg; /**< 消息 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_alloc);

struct vmsg_worker;

/**
 * @brief 消息工作任务结构，封装异步消息处理的工作队列条目
 */
struct vmsg_work {
    double_list_t           head; /**< 链表头 */
    struct vmsg_worker     *worker; /**< worker成员 */
    struct vmm_vmsg_domain *domain; /**< 域 */
    struct vmm_vmsg        *msg; /**< 消息 */
    char                    name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t                addr; /**< 地址 */
    int (*func)(struct vmsg_work *work); /**< 函数指针 */
    void (*free)(struct vmsg_work *work); /**< 可用量 */
};

/**
 * @brief 消息工作线程结构，维护处理消息的工作线程和队列
 */
struct vmsg_worker {
    struct mempool  *work_pool; /**< work_pool成员 */
    vmm_thread_t    *thread; /**< 线程 */
    vmm_completion_t bh_avail; /**< bh_avail成员 */
    vmm_spinlock_t   bh_lock; /**< bh_lock成员 */
    double_list_t    work_list; /**< 工作列表 */
    double_list_t    lazy_list; /**< lazy_list成员 */
};

static DEFINE_PER_CPU(struct vmsg_worker, vworker);

/**
 * @brief 释放虚拟消息池的工作项回调
 * @param work 工作项结构体指针
 */
static void vmsg_free_pool_work(struct vmsg_work *work)
{
    mempool_free(work->worker->work_pool, work);
}

/**
 * @brief 释放虚拟消息堆的工作项回调
 * @param work 工作项结构体指针
 */
static void vmsg_free_heap_work(struct vmsg_work *work)
{
    vmm_free(work);
}

/**
 * @brief 虚拟消息 入队 工作项
 * @param domain 域结构体指针
 * @param msg 消息字符串
 * @param name 目标对象的名称
 * @param addr 地址值
 * @param (*func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_enqueue_work(struct vmm_vmsg_domain *domain, struct vmm_vmsg *msg, const char *name, uint32_t addr, int (*func)(struct vmsg_work *))
{
    irq_flags_t         flags;
    struct vmsg_work   *work;
    struct vmsg_worker *worker = &this_cpu(vworker);

    if (!domain || !func) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    work = mempool_malloc(worker->work_pool);

    if (!work) {
        work = vmm_malloc(sizeof(*work));

        if (!work) {
            return VMM_ERR_NOMEM;
        }

        work->free = vmsg_free_heap_work;
    } else {
        work->free = vmsg_free_pool_work;
    }

    INIT_LIST_HEAD(&work->head);
    work->worker = worker;
    work->domain = domain;
    work->msg    = msg;
    strncpy(work->name, name, sizeof(work->name));
    work->addr = addr;
    work->func = func;

    if (work->msg) {
        vmm_vmsg_ref(work->msg);
    }

    vmm_spin_lock_irq_save(&worker->bh_lock, flags);
    list_add_tail(&work->head, &worker->work_list);
    vmm_spin_unlock_irq_restore(&worker->bh_lock, flags);

    vmm_completion_complete(&worker->bh_avail);

    return VMM_OK;
}

/**
 * @brief 延迟将虚拟消息入队
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_enqueue_lazy(struct vmm_vmsg_node_lazy *lazy)
{
    irq_flags_t         flags;
    struct vmsg_worker *worker = &this_cpu(vworker);

    if (!lazy) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_spin_lock_irq_save(&worker->bh_lock, flags);
    list_add_tail(&lazy->head, &worker->lazy_list);
    vmm_spin_unlock_irq_restore(&worker->bh_lock, flags);

    vmm_completion_complete(&worker->bh_avail);

    return VMM_OK;
}

/**
 * @brief 虚拟消息 出队
 * @param worker 工作线程指针
 * @param lazyp 延迟标志指针
 * @param workp 工作项指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_dequeue(struct vmsg_worker *worker, struct vmm_vmsg_node_lazy **lazyp, struct vmsg_work **workp)
{
    irq_flags_t flags;

    if (!worker || !lazyp || !workp) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save(&worker->bh_lock, flags);

    if (list_empty(&worker->lazy_list) && list_empty(&worker->work_list)) {
        vmm_spin_unlock_irq_restore(&worker->bh_lock, flags);
        vmm_completion_wait(&worker->bh_avail);
        vmm_spin_lock_irq_save(&worker->bh_lock, flags);
    }

    if (!list_empty(&worker->lazy_list)) {
        *lazyp = list_entry(list_pop(&worker->lazy_list), struct vmm_vmsg_node_lazy, head);
    }

    if (!list_empty(&worker->work_list)) {
        *workp = list_entry(list_pop(&worker->work_list), struct vmsg_work, head);
    }

    vmm_spin_unlock_irq_restore(&worker->bh_lock, flags);

    return VMM_OK;
}

/**
 * @brief 强制停止延迟虚拟消息处理
 * @param lazy_orig 原始延迟标志指针
 */
static void vmsg_force_stop_lazy(struct vmm_vmsg_node_lazy *lazy_orig)
{
    uint32_t                   cpu;
    bool                       done = FALSE;
    irq_flags_t                flags;
    struct vmsg_worker        *worker;
    struct vmm_vmsg_node_lazy *lazy = NULL;
    struct vmm_vmsg_node_lazy *lazy1 = NULL;

    for_each_online_cpu(cpu)
    {
        worker = &per_cpu(vworker, cpu); /**< cpu)成员 */

        vmm_spin_lock_irq_save(&worker->bh_lock, flags); /**< flags)成员 */

        list_for_each_entry_safe(lazy, lazy1, &worker->lazy_list, head)
        {
            if (lazy == lazy_orig) {
                list_del(&lazy->head);
                arch_atomic_write(&lazy->sched_count, 0); /**< 0) */
                done = TRUE; /**< TRUE成员 */
                break;
            }
        }

        vmm_spin_unlock_irq_restore(&worker->bh_lock, flags); /**< flags)成员 */

        if (done) {
            break;
        }
    }
}

/**
 * @brief 虚拟消息工作线程的主函数
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_worker_main(void *data)
{
    int                        rc;
    irq_flags_t                f;
    struct vmsg_work          *work;
    struct vmm_vmsg_node_lazy *lazy;
    struct vmsg_worker        *worker = data;

    while (1) {
        lazy = NULL; /**< NULL成员 */
        work = NULL; /**< NULL成员 */
        rc   = vmsg_dequeue(worker, &lazy, &work); /**< &work)成员 */

        if (rc) {
            continue;
        }

        if (work) {
            /* Call work function */
            rc = work->func(work); /**< work->func(work)成员 */

            if (rc == VMM_ERR_AGAIN) {
                vmm_spin_lock_irq_save(&worker->bh_lock, f); /**< f) */
                list_add_tail(&work->head, &worker->work_list); /**< &worker->work_list)成员 */
                vmm_spin_unlock_irq_restore(&worker->bh_lock, f); /**< f) */

                vmm_completion_complete(&worker->bh_avail);

                continue;
            }

            /* Free-up msg */
            if (work->msg) {
                vmm_vmsg_dref(work->msg);
                work->msg = NULL; /**< NULL成员 */
            }

            /* Free-up work */
            work->free(work);
        }

        if (lazy) {
            /* Call lazy xfer function */
            lazy->xfer(lazy->node, lazy->arg, lazy->budget); /**< lazy->budget)成员 */

            /* Add back to netswitch bh queue if required */
            if (arch_atomic_sub_return(&lazy->sched_count, 1) > 0) {
                vmm_spin_lock_irq_save(&worker->bh_lock, f); /**< f) */
                list_add_tail(&lazy->head, &worker->lazy_list); /**< &worker->lazy_list)成员 */
                vmm_spin_unlock_irq_restore(&worker->bh_lock, f); /**< f) */

                vmm_completion_complete(&worker->bh_avail);
            }
        }
    }

    return VMM_OK;
}

/**
 * @brief 虚拟消息节点对端下线处理函数
 * @param work 工作项结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_peer_down_func(struct vmsg_work *work)
{
    struct vmm_vmsg_node   *node;
    struct vmm_vmsg_domain *domain    = work->domain;
    const char             *peer_name = work->name;
    uint32_t                peer_addr = work->addr;

    vmm_mutex_lock(&domain->node_lock);

    list_for_each_entry(node, &domain->node_list, domain_head)
    {
        if ((node->addr == peer_addr) || !arch_atomic_read(&node->is_ready)) {
            continue;
        }

        if (node->ops->peer_down) {
            node->ops->peer_down(node, peer_name, peer_addr);
        }
    }

    vmm_mutex_unlock(&domain->node_lock);

    return VMM_OK;
}

/**
 * @brief 通知虚拟消息节点对端已下线
 * @param node 设备树节点指针
 * @return 通知结果
 */
static int vmsg_node_peer_down(struct vmm_vmsg_node *node)
{
    int                     err    = VMM_OK;
    struct vmm_vmsg_domain *domain = node->domain;

    DPRINTF("%s: node=%s\n", __func__, node->name);

    if (arch_atomic_cmpxchg(&node->is_ready, 1, 0)) {
        err = vmsg_enqueue_work(domain, NULL, node->name, node->addr, vmsg_node_peer_down_func); /**< vmsg_node_peer_down_func)成员 */

        if (err) {
            vmm_printf("%s: node=%s error=%d\n", __func__, node->name, err); /**< err)成员 */
            return err; /**< 错误码 */
        }
    }

    return err;
}

/**
 * @brief 虚拟消息节点对端上线处理函数
 * @param work 工作项结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_peer_up_func(struct vmsg_work *work)
{
    struct vmm_vmsg_node *node = NULL;
    struct vmm_vmsg_node *peer_node = NULL;
    struct vmm_vmsg_domain *domain    = work->domain;
    const char             *peer_name = work->name;
    uint32_t                peer_addr = work->addr;

    vmm_mutex_lock(&domain->node_lock);

    peer_node = NULL;
    list_for_each_entry(node, &domain->node_list, domain_head)
    {
        if (node->addr == peer_addr) {
            peer_node = node;
            break;
        }
    }

    list_for_each_entry(node, &domain->node_list, domain_head)
    {
        if ((node->addr == peer_addr) || !arch_atomic_read(&node->is_ready)) {
            continue;
        }

        if (node->ops->peer_up) {
            node->ops->peer_up(node, peer_name, peer_addr);
        }

        if (peer_node && peer_node->ops->peer_up) {
            peer_node->ops->peer_up(peer_node, node->name, node->addr);
        }
    }

    vmm_mutex_unlock(&domain->node_lock);

    return VMM_OK;
}

/**
 * @brief 通知虚拟消息节点对端已上线
 * @param node 设备树节点指针
 * @return 通知结果
 */
static int vmsg_node_peer_up(struct vmm_vmsg_node *node)
{
    int                     err;
    struct vmm_vmsg_domain *domain = node->domain;

    DPRINTF("%s: node=%s\n", __func__, node->name);

    if (!arch_atomic_cmpxchg(&node->is_ready, 0, 1)) {
        err = vmsg_enqueue_work(domain, NULL, node->name, node->addr, vmsg_node_peer_up_func); /**< vmsg_node_peer_up_func)成员 */

        if (err) {
            vmm_printf("%s: node=%s error=%d\n", __func__, node->name, err); /**< err)成员 */
            return err; /**< 错误码 */
        }
    }

    return VMM_OK;
}

/**
 * @brief 虚拟消息节点快速发送处理函数
 * @param msg 消息字符串
 * @param domain 域结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_send_fast_func(struct vmm_vmsg *msg, struct vmm_vmsg_domain *domain)
{
    int                   err;
    struct vmm_vmsg_node *node;

    vmm_mutex_lock(&domain->node_lock);

    list_for_each_entry(node, &domain->node_list, domain_head)
    {
        if ((node->addr == msg->src) || !arch_atomic_read(&node->is_ready)) {
            continue;
        }

        if (((node->addr == msg->dst) || (msg->dst == VMM_VMSG_NODE_ADDR_ANY)) && (msg->len <= node->max_data_len)) {
            if (node->ops->can_recv_msg && node->ops->recv_msg) {
                if (!node->ops->can_recv_msg(node) && (msg->dst != VMM_VMSG_NODE_ADDR_ANY)) {
                    vmm_mutex_unlock(&domain->node_lock);
                    return VMM_ERR_AGAIN;
                }

                err = node->ops->recv_msg(node, msg);

                if (err) {
                    vmm_printf("%s: node=%s error=%d\n", __func__, node->name, err);
                }
            }
        }
    }

    vmm_mutex_unlock(&domain->node_lock);

    return VMM_OK;
}

/**
 * @brief 虚拟消息节点发送处理函数
 * @param work 工作项结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_send_func(struct vmsg_work *work)
{
    return vmsg_node_send_fast_func(work->msg, work->domain);
}

/**
 * @brief 向虚拟消息节点发送数据
 * @param node 设备树节点指针
 * @param msg 消息字符串
 * @param fast 是否快速模式
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_send(struct vmm_vmsg_node *node, struct vmm_vmsg *msg, bool fast)
{
    if (!node || !node->domain || !msg || !msg->data || !msg->len || (msg->dst == node->addr) || (msg->dst < VMM_VMSG_NODE_ADDR_MIN)) {
        return VMM_ERR_INVALID;
    }

    msg->src = node->addr;

    DPRINTF("%s: node=%s src=0x%x dst=0x%x len=0x%zx\n", __func__, node->name, msg->src, msg->dst, msg->len);

    if (fast) {
        return vmsg_node_send_fast_func(msg, node->domain);
    }

    return vmsg_enqueue_work(node->domain, msg, node->name, node->addr, vmsg_node_send_func);
}

/**
 * @brief 延迟启动虚拟消息节点
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_start_lazy(struct vmm_vmsg_node_lazy *lazy)
{
    int                   rc = VMM_ERR_BUSY;
    struct vmm_vmsg_node *node;
    long                  sched_count;

    if (!lazy || !lazy->node) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    node = lazy->node;

    DPRINTF("%s: node=%s lazy=0x%p\n", __func__, node->name, lazy);

    sched_count = arch_atomic_add_return(&lazy->sched_count, 1);

    if (sched_count == 1) {
        rc = vmsg_enqueue_lazy(lazy);

        if (rc) {
            vmm_printf("%s: node=%s lazy bh enqueue failed.\n", __func__, node->name);
        } else {
            rc = VMM_OK;
        }
    }

    return rc;
}

/**
 * @brief 延迟停止虚拟消息节点
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int vmsg_node_stop_lazy(struct vmm_vmsg_node_lazy *lazy)
{
    if (!lazy || !lazy->node) {
        return VMM_ERR_INVALID;
    }

    DPRINTF("%s: node=%s lazy=0x%p\n", __func__, lazy->node->name, lazy);

    vmsg_force_stop_lazy(lazy);

    return VMM_OK;
}

struct vmm_vmsg_domain *vmm_vmsg_domain_create(const char *name, void *private)
{
    bool                    found; /**< found成员 */
    struct vmm_vmsg_event   event; /**< 事件 */
    struct vmm_vmsg_domain *vmd, *new_vmd; /**< new_vmd成员 */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    vmm_mutex_lock(&vmctrl.lock);

    found = FALSE; /**< FALSE成员 */
    list_for_each_entry(vmd, &vmctrl.domain_list, head)
    {
        if (strcmp(vmd->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&vmctrl.lock);
        return NULL; /**< NULL成员 */
    }

    new_vmd = vmm_zalloc(sizeof(*new_vmd)); /**< new_vmd成员 */

    if (!new_vmd) {
        vmm_mutex_unlock(&vmctrl.lock);
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&new_vmd->head);
    strncpy(new_vmd->name, name, sizeof(new_vmd->name)); /**< sizeof(new_vmd->name))成员 */
    new_vmd->private = private; /**< 私有数据 */
    INIT_MUTEX(&new_vmd->node_lock);
    INIT_LIST_HEAD(&new_vmd->node_list);

    list_add_tail(&new_vmd->head, &vmctrl.domain_list); /**< &vmctrl.domain_list)成员 */

    vmm_mutex_unlock(&vmctrl.lock);

    event.data = new_vmd; /**< new_vmd成员 */
    vmm_blocking_notifier_call(&vmctrl.notifier_chain, VMM_VMSG_EVENT_CREATE_DOMAIN, &event); /**< &event)成员 */

    return new_vmd; /**< new_vmd成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_domain_create);

/**
 * @brief 销毁消息域
 * @param domain 域结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_domain_destroy(struct vmm_vmsg_domain *domain)
{
    bool                    found;
    struct vmm_vmsg_event   event;
    struct vmm_vmsg_domain *vmd;

    if (!domain) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    event.data = domain;
    vmm_blocking_notifier_call(&vmctrl.notifier_chain, VMM_VMSG_EVENT_DESTROY_DOMAIN, &event);

    vmm_mutex_lock(&vmctrl.lock);

    vmm_mutex_lock(&domain->node_lock);

    if (!list_empty(&domain->node_list)) {
        vmm_mutex_unlock(&domain->node_lock);
        vmm_mutex_unlock(&vmctrl.lock);
        return VMM_ERR_BUSY;
    }

    vmm_mutex_unlock(&domain->node_lock);

    found = FALSE;
    list_for_each_entry(vmd, &vmctrl.domain_list, head)
    {
        if (strcmp(vmd->name, domain->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&vmctrl.lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&domain->head);
    vmm_free(domain);

    vmm_mutex_unlock(&vmctrl.lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_domain_destroy);

/**
 * @brief 虚拟消息 域 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_domain_iterate(struct vmm_vmsg_domain *start, void *data, int (*fn)(struct vmm_vmsg_domain *, void *))
{
    int                     rc          = VMM_OK;
    bool                    start_found = (start) ? FALSE : TRUE;
    struct vmm_vmsg_domain *vmd         = NULL;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&vmctrl.lock);

    list_for_each_entry(vmd, &vmctrl.domain_list, head)
    {
        if (!start_found) {
            if (start && start == vmd) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vmd, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&vmctrl.lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_domain_iterate);

/**
 * @brief 消息域查找控制结构，在域列表中搜索目标域的上下文
 */
struct vmsg_domain_find_ctrl {
    const char             *name; /**< 名称 */
    struct vmm_vmsg_domain *domain; /**< 域 */
};

/**
 * @brief 查找虚拟消息域
 * @param domain 域结构体指针
 * @param data 用户自定义数据指针
 * @return 查找结果，失败返回错误码
 */
static int vmsg_domain_find(struct vmm_vmsg_domain *domain, void *data)
{
    struct vmsg_domain_find_ctrl *c = data;

    if ((strcmp(domain->name, c->name) == 0)) {
        c->domain = domain; /**< 域 */
        return 1; /**< 1 */
    }

    return 0;
}

struct vmm_vmsg_domain *vmm_vmsg_domain_find(const char *name)
{
    struct vmsg_domain_find_ctrl c; /**< c */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    c.name   = name; /**< 名称 */
    c.domain = NULL; /**< NULL成员 */
    vmm_vmsg_domain_iterate(NULL, &c, vmsg_domain_find); /**< vmsg_domain_find)成员 */

    return c.domain; /**< c.domain成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_domain_find);

/**
 * @brief 虚拟消息 域 数量
 * @param domain 域结构体指针
 * @param data 用户自定义数据指针
 * @return 数量值
 */
static int vmsg_domain_count(struct vmm_vmsg_domain *domain, void *data)
{
    uint32_t *count_ptr = data;

    (*count_ptr)++;

    return 0;
}

/**
 * @brief 获取消息域的数量
 * @return 数量值
 */
uint32_t vmm_vmsg_domain_count(void)
{
    uint32_t retval = 0;

    vmm_vmsg_domain_iterate(NULL, &retval, vmsg_domain_count);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_domain_count);

/**
 * @brief 遍历虚拟消息域中的所有节点
 * @param domain 域结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_domain_node_iterate(struct vmm_vmsg_domain *domain, struct vmm_vmsg_node *start, void *data, int (*fn)(struct vmm_vmsg_node *, void *))
{
    int                   rc          = VMM_OK;
    bool                  start_found = (start) ? FALSE : TRUE;
    struct vmm_vmsg_node *vmn;

    if (!domain || !fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&domain->node_lock);

    list_for_each_entry(vmn, &domain->node_list, domain_head)
    {
        if (!start_found) {
            if (start && start == vmn) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vmn, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&domain->node_lock);

    return rc;
}

/**
 * @brief 获取消息域的名称
 * @param domain 域结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_vmsg_domain_get_name(struct vmm_vmsg_domain *domain)
{
    return (domain) ? domain->name : NULL;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_domain_get_name);

struct vmm_vmsg_node *vmm_vmsg_node_create(
    const char *name, uint32_t addr, uint32_t max_data_len, struct vmm_vmsg_node_ops *ops, struct vmm_vmsg_domain *domain, void *private)
{
    bool                  found; /**< found成员 */
    int                   id_min, id_max, a; /**< a */
    struct vmm_vmsg_event event; /**< 事件 */
    struct vmm_vmsg_node *vmn, *new_vmn; /**< new_vmn成员 */

    if (!name || !ops) {
        return NULL; /**< NULL成员 */
    }

    if (!domain) {
        domain = vmctrl.default_domain; /**< vmctrl.default_domain成员 */
    }

    vmm_mutex_lock(&vmctrl.lock);

    found = FALSE; /**< FALSE成员 */
    list_for_each_entry(vmn, &vmctrl.node_list, head)
    {
        if (strcmp(vmn->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&vmctrl.lock);
        return NULL; /**< NULL成员 */
    }

    new_vmn = vmm_zalloc(sizeof(*new_vmn)); /**< new_vmn成员 */

    if (!new_vmn) {
        vmm_mutex_unlock(&vmctrl.lock);
        return NULL; /**< NULL成员 */
    }

    if (addr == VMM_VMSG_NODE_ADDR_ANY) {
        id_min = VMM_VMSG_NODE_ADDR_MIN; /**< VMM_VMSG_NODE_ADDR_MIN成员 */
        id_max = 0; /**< 0 */
    } else if (addr > VMM_VMSG_NODE_ADDR_MIN) {
        id_min = addr; /**< 地址 */
        id_max = addr + 1; /**< 1 */
    } else {
        vmm_free(new_vmn);
        vmm_mutex_unlock(&vmctrl.lock);
        return NULL; /**< NULL成员 */
    }

    a = ida_simple_get(&vmctrl.node_ida, id_min, id_max, 0); /**< 0) */

    if (a < 0) {
        vmm_free(new_vmn);
        vmm_mutex_unlock(&vmctrl.lock);
        return NULL; /**< NULL成员 */
    }

    new_vmn->addr = a; /**< a */
    INIT_LIST_HEAD(&new_vmn->head);
    INIT_LIST_HEAD(&new_vmn->domain_head);
    strncpy(new_vmn->name, name, sizeof(new_vmn->name)); /**< sizeof(new_vmn->name))成员 */
    new_vmn->max_data_len = max_data_len; /**< max_data_len成员 */
    new_vmn->private      = private; /**< 私有数据 */
    arch_atomic_write(&new_vmn->is_ready, 0); /**< 0) */
    new_vmn->domain = domain; /**< 域 */
    new_vmn->ops    = ops; /**< 操作集 */

    list_add_tail(&new_vmn->head, &vmctrl.node_list); /**< &vmctrl.node_list)成员 */

    vmm_mutex_lock(&domain->node_lock);
    list_add_tail(&new_vmn->domain_head, &domain->node_list); /**< &domain->node_list)成员 */
    vmm_mutex_unlock(&domain->node_lock);

    vmm_mutex_unlock(&vmctrl.lock);

    event.data = new_vmn; /**< new_vmn成员 */
    vmm_blocking_notifier_call(&vmctrl.notifier_chain, VMM_VMSG_EVENT_CREATE_NODE, &event); /**< &event)成员 */

    return new_vmn; /**< new_vmn成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_create);

/**
 * @brief 销毁消息节点
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_destroy(struct vmm_vmsg_node *node)
{
    int                     err;
    struct vmm_vmsg_domain *domain;
    struct vmm_vmsg_event   event;

    if (!node) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    domain = node->domain;

    err    = vmsg_node_peer_down(node);

    if (err) {
        return err;
    }

    event.data = node;
    vmm_blocking_notifier_call(&vmctrl.notifier_chain, VMM_VMSG_EVENT_DESTROY_NODE, &event);

    vmm_mutex_lock(&vmctrl.lock);

    vmm_mutex_lock(&domain->node_lock);
    list_del(&node->domain_head);
    vmm_mutex_unlock(&domain->node_lock);

    list_del(&node->head);

    ida_simple_remove(&vmctrl.node_ida, node->addr);

    vmm_free(node);

    vmm_mutex_unlock(&vmctrl.lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_destroy);

/**
 * @brief 虚拟消息 节点 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_iterate(struct vmm_vmsg_node *start, void *data, int (*fn)(struct vmm_vmsg_node *, void *))
{
    int                   rc          = VMM_OK;
    bool                  start_found = (start) ? FALSE : TRUE;
    struct vmm_vmsg_node *vmn;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&vmctrl.lock);

    list_for_each_entry(vmn, &vmctrl.node_list, head)
    {
        if (!start_found) {
            if (start && start == vmn) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vmn, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&vmctrl.lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_iterate);

/**
 * @brief 消息节点查找控制结构，在节点列表中搜索目标节点
 */
struct vmsg_node_find_ctrl {
    const char           *name; /**< 名称 */
    struct vmm_vmsg_node *node; /**< 节点 */
};

/**
 * @brief 查找虚拟消息节点
 * @param node 设备树节点指针
 * @param data 用户自定义数据指针
 * @return 查找结果，失败返回错误码
 */
static int vmsg_node_find(struct vmm_vmsg_node *node, void *data)
{
    struct vmsg_node_find_ctrl *c = data;

    if ((strcmp(node->name, c->name) == 0)) {
        c->node = node; /**< 节点 */
        return 1; /**< 1 */
    }

    return 0;
}

struct vmm_vmsg_node *vmm_vmsg_node_find(const char *name)
{
    struct vmsg_node_find_ctrl c; /**< c */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    c.name = name; /**< 名称 */
    c.node = NULL; /**< NULL成员 */
    vmm_vmsg_node_iterate(NULL, &c, vmsg_node_find); /**< vmsg_node_find)成员 */

    return c.node; /**< c.node成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_find);

/**
 * @brief 虚拟消息 节点 数量
 * @param node 设备树节点指针
 * @param data 用户自定义数据指针
 * @return 数量值
 */
static int vmsg_node_count(struct vmm_vmsg_node *node, void *data)
{
    uint32_t *count_ptr = data;

    (*count_ptr)++;

    return 0;
}

/**
 * @brief 获取消息节点的数量
 * @return 数量值
 */
uint32_t vmm_vmsg_node_count(void)
{
    uint32_t retval = 0;

    vmm_vmsg_node_iterate(NULL, &retval, vmsg_node_count);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_count);

/**
 * @brief 向虚拟消息节点发送数据
 * @param node 设备树节点指针
 * @param msg 消息字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_send(struct vmm_vmsg_node *node, struct vmm_vmsg *msg)
{
    if (!node || !msg) {
        return VMM_ERR_INVALID;
    }

    return vmsg_node_send(node, msg, false);
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_send);

/**
 * @brief 通过虚拟消息节点快速发送消息
 * @param node 设备树节点指针
 * @param msg 消息字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_send_fast(struct vmm_vmsg_node *node, struct vmm_vmsg *msg)
{
    int rc;

    if (!node || !msg) {
        return VMM_ERR_INVALID;
    }

    rc = vmsg_node_send(node, msg, true);

    if (rc == VMM_ERR_AGAIN) {
        return vmsg_node_send(node, msg, false);
    }

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_send_fast);

/**
 * @brief 延迟启动虚拟消息节点
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_start_lazy(struct vmm_vmsg_node_lazy *lazy)
{
    if (!lazy || !lazy->node || !lazy->xfer) {
        return VMM_ERR_INVALID;
    }

    return vmsg_node_start_lazy(lazy);
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_start_lazy);

/**
 * @brief 延迟停止虚拟消息节点
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_stop_lazy(struct vmm_vmsg_node_lazy *lazy)
{
    if (!lazy || !lazy->node || !lazy->xfer) {
        return VMM_ERR_INVALID;
    }

    return vmsg_node_stop_lazy(lazy);
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_stop_lazy);

/**
 * @brief 虚拟消息 节点 就绪
 * @param node 设备树节点指针
 */
void vmm_vmsg_node_ready(struct vmm_vmsg_node *node)
{
    if (!node) {
        return;
    }

    BUG_ON(vmsg_node_peer_up(node));
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_ready);

/**
 * @brief 将虚拟消息节点标记为未就绪
 * @param node 设备树节点指针
 */
void vmm_vmsg_node_notready(struct vmm_vmsg_node *node)
{
    if (!node) {
        return;
    }

    BUG_ON(vmsg_node_peer_down(node));
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_notready);

/**
 * @brief 检查虚拟消息节点是否就绪
 * @param node 设备树节点指针
 * @return 就绪返回TRUE，否则返回FALSE
 */
bool vmm_vmsg_node_is_ready(struct vmm_vmsg_node *node)
{
    if (node) {
        return arch_atomic_read(&node->is_ready) ? TRUE : FALSE;
    }

    return FALSE;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_is_ready);

/**
 * @brief 获取消息节点的名称
 * @param node 设备树节点指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_vmsg_node_get_name(struct vmm_vmsg_node *node)
{
    return (node) ? node->name : NULL;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_get_name);

/**
 * @brief 获取消息节点的addr
 * @param node 设备树节点指针
 * @return 成功返回节点地址，失败返回VMM_VMSG_NODE_ADDR_ANY
 */
uint32_t vmm_vmsg_node_get_addr(struct vmm_vmsg_node *node)
{
    return (node) ? node->addr : VMM_VMSG_NODE_ADDR_ANY;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_get_addr);

/**
 * @brief 获取消息节点的最大数据长度
 * @param node 设备树节点指针
 * @return 成功返回最大数据长度，失败返回0
 */
uint32_t vmm_vmsg_node_get_max_data_len(struct vmm_vmsg_node *node)
{
    return (node) ? node->max_data_len : 0x0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_get_max_data_len);

struct vmm_vmsg_domain *vmm_vmsg_node_get_domain(struct vmm_vmsg_node *node)
{
    return (node) ? node->domain : NULL; /**< NULL成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmsg_node_get_domain);

/**
 * @brief 创建虚拟消息工作线程
 * @param arg0 第零个参数值
 * @param arg1 第一个参数值
 * @param arg3 第三个参数值
 */
static void vmsg_create_workers(void *arg0, void *arg1, void *arg3)
{
    int                 ret;
    char                name[VMM_FIELD_NAME_SIZE];
    uint32_t            cpu    = vmm_smp_processor_id();
    struct vmsg_worker *worker = &this_cpu(vworker);

    worker->thread             = NULL;
    worker->work_pool          = NULL;
    INIT_COMPLETION(&worker->bh_avail);
    INIT_SPIN_LOCK(&worker->bh_lock);
    INIT_LIST_HEAD(&worker->work_list);
    INIT_LIST_HEAD(&worker->lazy_list);

    worker->work_pool = mempool_ram_create(sizeof(struct vmsg_work), 8, VMM_PAGE_POOL_NORMAL);

    if (!worker->work_pool) {
        vmm_printf("%s: cpu=%d failed to create work pool\n", __func__, cpu);
        return;
    }

    vmm_snprintf(name, sizeof(name), "vmsg/%d", cpu);
    worker->thread = vmm_threads_create(name, vmsg_worker_main, worker, VMM_THREAD_DEF_PRIORITY, VMM_THREAD_DEF_TIME_SLICE);

    if (!worker->thread) {
        vmm_printf("%s: cpu=%d failed to create thread\n", __func__, cpu);
        mempool_destroy(worker->work_pool);
        return;
    }

    ret = vmm_threads_set_affinity(worker->thread, vmm_cpumask_of(cpu));

    if (ret) {
        vmm_printf("%s: cpu=%d failed to set thread affinity\n", __func__, cpu);
        vmm_threads_destroy(worker->thread);
        mempool_destroy(worker->work_pool);
        return;
    }

    ret = vmm_threads_start(worker->thread);

    if (ret) {
        vmm_printf("%s: cpu=%d failed to start thread\n", __func__, cpu);
        vmm_threads_destroy(worker->thread);
        mempool_destroy(worker->work_pool);
        return;
    }
}

/**
 * @brief 初始化虚拟消息
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init vmm_vmsg_init(void)
{
    memset(&vmctrl, 0, sizeof(vmctrl));

    INIT_MUTEX(&vmctrl.lock);
    INIT_LIST_HEAD(&vmctrl.domain_list);
    INIT_LIST_HEAD(&vmctrl.node_list);
    INIT_IDA(&vmctrl.node_ida);
    BLOCKING_INIT_NOTIFIER_CHAIN(&vmctrl.notifier_chain);

    vmctrl.default_domain = vmm_vmsg_domain_create("vmsg_default", NULL);

    if (!vmctrl.default_domain) {
        return VMM_ERR_NOMEM;
    }

    vmm_smp_ipi_async_call(cpu_online_mask, vmsg_create_workers, NULL, NULL, NULL);

    return VMM_OK;
}

/**
 * @brief 虚拟消息子系统退出
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_vmsg_exit(void)
{
    /* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
