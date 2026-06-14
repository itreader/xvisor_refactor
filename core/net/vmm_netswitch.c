/**
 * Copyright (c) 2012 Pranav Sawargaonkar.
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_netswitch.c
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @author Anup Patel <anup@brainfault.org>
 * @brief 通用网络交换机实现
 */

#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_protocol.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_per_cpu.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(fmt, ...)                                                                                                                            \
    do {                                                                                                                                             \
        vmm_printf(fmt, ##__VA_ARGS__);                                                                                                              \
    } while (0)

#define DUMP_NETSWITCH_PKT(mbuf)                                                                                                                     \
    do {                                                                                                                                             \
        char           tname[30];                                                                                                                    \
        const uint8_t *srcmac = ether_srcmac(mtod(mbuf, uint8_t *));                                                                                 \
        const uint8_t *dstmac = ether_dstmac(mtod(mbuf, uint8_t *));                                                                                 \
        const uint8_t *ip_frame, *icmp_frame, *tcp_frame;                                                                                            \
                                                                                                                                                     \
        DPRINTF("%s: got pkt with srcaddr[%s]", __func__, ethaddr_to_str(tname, srcmac));                                                            \
        DPRINTF(", dstaddr[%s]", ethaddr_to_str(tname, dstmac));                                                                                     \
        DPRINTF(", ethertype: 0x%04X\n", ether_type(mtod(mbuf, uint8_t *)));                                                                         \
        if (ether_type(mtod(mbuf, uint8_t *)) == 0x0806 /* ARP */) {                                                                                 \
            DPRINTF("\tARP-HType: 0x%04X\n", arp_htype(ether_payload(mtod(mbuf, uint8_t *))));                                                       \
            DPRINTF("\tARP-PType: 0x%04X\n", arp_ptype(ether_payload(mtod(mbuf, uint8_t *))));                                                       \
            DPRINTF("\tARP-Hlen: 0x%02X\n", arp_hlen(ether_payload(mtod(mbuf, uint8_t *))));                                                         \
            DPRINTF("\tARP-Plen: 0x%02X\n", arp_plen(ether_payload(mtod(mbuf, uint8_t *))));                                                         \
            DPRINTF("\tARP-Oper: 0x%04X\n", arp_oper(ether_payload(mtod(mbuf, uint8_t *))));                                                         \
            DPRINTF("\tARP-SHA: %s\n", ethaddr_to_str(tname, arp_sha(ether_payload((mtod(mbuf, uint8_t *))))));                                      \
            DPRINTF("\tARP-SPA: %s\n", ip4addr_to_str(tname, arp_spa(ether_payload((mtod(mbuf, uint8_t *))))));                                      \
            DPRINTF("\tARP-THA: %s\n", ethaddr_to_str(tname, arp_tha(ether_payload((mtod(mbuf, uint8_t *))))));                                      \
            DPRINTF("\tARP-TPA: %s\n", ip4addr_to_str(tname, arp_tpa(ether_payload((mtod(mbuf, uint8_t *))))));                                      \
        } else if (ether_type(mtod(mbuf, uint8_t *)) == 0x0800 /* IPv4 */) {                                                                         \
            ip_frame = ether_payload(mtod(mbuf, uint8_t *));                                                                                         \
            DPRINTF("\tIP-SRC: %s\n", ip4addr_to_str(tname, ip_srcaddr(ip_frame)));                                                                  \
            DPRINTF("\tIP-DST: %s\n", ip4addr_to_str(tname, ip_dstaddr(ip_frame)));                                                                  \
            DPRINTF("\tIP-LEN: %d\n", ip_len(ip_frame));                                                                                             \
            DPRINTF("\tIP-TTL: %d\n", ip_ttl(ip_frame));                                                                                             \
            DPRINTF("\tIP-CHKSUM: 0x%04X\n", ip_chksum(ip_frame));                                                                                   \
            DPRINTF("\tIP-PROTOCOL: %d\n", ip_protocol(ip_frame));                                                                                   \
            if (ip_protocol(ip_frame) == 0x01 /* ICMP */) {                                                                                          \
                icmp_frame = ip_payload(ip_frame);                                                                                                   \
                DPRINTF("\t\tICMP-TYPE: 0x%x\n", icmp_type(icmp_frame));                                                                             \
                DPRINTF("\t\tICMP-CODE: 0x%x\n", icmp_code(icmp_frame));                                                                             \
                DPRINTF("\t\tICMP-CHECKSUM: 0x%x\n", icmp_checksum(icmp_frame));                                                                     \
                DPRINTF("\t\tICMP-ID: 0x%x\n", icmp_id(icmp_frame));                                                                                 \
                DPRINTF("\t\tICMP-SEQUENCE: 0x%x\n", icmp_sequence(icmp_frame));                                                                     \
            } else if (ip_protocol(ip_frame) == 0x06 /* TCP */) {                                                                                    \
                tcp_frame = ip_payload(ip_frame);                                                                                                    \
                DPRINTF("\t\tTCP-SRCPORT: %d\n", tcp_srcport(tcp_frame));                                                                            \
                DPRINTF("\t\tTCP-DSTPORT: %d\n", tcp_dstport(tcp_frame));                                                                            \
                DPRINTF("\t\tTCP-SEQUENCE: 0x%x\n", tcp_sequence(tcp_frame));                                                                        \
                DPRINTF("\t\tTCP-ACKNUMBER: 0x%x\n", tcp_acknumber(tcp_frame));                                                                      \
                DPRINTF("\t\tTCP-FLAGS: 0x%x\n", tcp_flags(tcp_frame));                                                                              \
                DPRINTF("\t\tTCP-CHECKSUM: 0x%x\n", tcp_checksum(tcp_frame));                                                                        \
                DPRINTF("\t\tTCP-URGENT: 0x%x\n", tcp_urgent(tcp_frame));                                                                            \
            }                                                                                                                                        \
        }                                                                                                                                            \
    } while (0)
#else
#define DPRINTF(fmt, ...)                                                                                                                            \
    do {                                                                                                                                             \
    } while (0)
#define DUMP_NETSWITCH_PKT(mbuf)
#endif

/**
 * @brief 网络交换机下半部控制结构，处理延迟的数据包转发
 */
struct vmm_netswitch_bh_ctrl {
    vmm_thread_t    *thread; /**< 线程 */
    vmm_completion_t bh_cmpl; /**< bh_cmpl成员 */
    vmm_spinlock_t   bh_list_lock; /**< bh_list_lock成员 */
    double_list_t    mbuf_list; /**< mbuf_list成员 */
    double_list_t    lazy_list; /**< lazy_list成员 */
};

static DEFINE_PER_CPU(struct vmm_netswitch_bh_ctrl, nbctrl);

static DEFINE_MUTEX(policy_list_lock);
static LIST_HEAD(policy_list);

/**
 * @brief 初始化网络交换机的底半部处理
 * @param nbp 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __init netswitch_bh_init(struct vmm_netswitch_bh_ctrl *nbp)
{
    INIT_COMPLETION(&nbp->bh_cmpl);
    INIT_SPIN_LOCK(&nbp->bh_list_lock);
    INIT_LIST_HEAD(&nbp->mbuf_list);
    INIT_LIST_HEAD(&nbp->lazy_list);
}

/**
 * @brief 将网络交换机的底半部处理入队
 * @param nbp 通知器块指针
 * @param mbuf 网络消息缓冲区指针
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int netswitch_bh_enqueue(struct vmm_netswitch_bh_ctrl *nbp, struct vmm_mbuf *mbuf, struct vmm_netport_lazy *lazy)
{
    irq_flags_t flags;

    if (!nbp || (!mbuf && !lazy)) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save_lite(&nbp->bh_list_lock, flags);

    if (mbuf) {
        list_add_tail(&mbuf->m_list, &nbp->mbuf_list);
    }

    if (lazy) {
        list_add_tail(&lazy->head, &nbp->lazy_list);
    }

    vmm_spin_unlock_irq_restore_lite(&nbp->bh_list_lock, flags);

    vmm_completion_complete_once(&nbp->bh_cmpl);

    return VMM_OK;
}

/**
 * @brief 将网络交换机的底半部处理出队
 * @param nbp 通知器块指针
 * @param mbufp 消息缓冲区指针
 * @param lazyp 延迟标志指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int netswitch_bh_dequeue(struct vmm_netswitch_bh_ctrl *nbp, struct vmm_mbuf **mbufp, struct vmm_netport_lazy **lazyp)
{
    irq_flags_t flags;

    if (!nbp || !mbufp || !lazyp) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save_lite(&nbp->bh_list_lock, flags);

    while (list_empty(&nbp->mbuf_list) && list_empty(&nbp->lazy_list)) {
        vmm_spin_unlock_irq_restore_lite(&nbp->bh_list_lock, flags);
        vmm_completion_wait(&nbp->bh_cmpl);
        vmm_spin_lock_irq_save_lite(&nbp->bh_list_lock, flags);
    }

    if (!list_empty(&nbp->mbuf_list)) {
        *mbufp = list_entry(list_pop(&nbp->mbuf_list), struct vmm_mbuf, m_list);
    }

    if (!list_empty(&nbp->lazy_list)) {
        *lazyp = list_entry(list_pop(&nbp->lazy_list), struct vmm_netport_lazy, head);
    }

    vmm_spin_unlock_irq_restore_lite(&nbp->bh_list_lock, flags);

    return VMM_OK;
}

/**
 * @brief 刷新网络交换机端口的底半部处理
 * @param nbp 通知器块指针
 * @param port 端口编号或端口结构体指针
 */
static void netswitch_bh_port_flush(struct vmm_netswitch_bh_ctrl *nbp, struct vmm_netport *port)
{
    irq_flags_t              flags;
    struct vmm_mbuf *mbuf = NULL;
    struct vmm_mbuf *nmbuf = NULL;
    struct vmm_netport_lazy *lazy = NULL;
    struct vmm_netport_lazy *nlazy = NULL;

    vmm_spin_lock_irq_save_lite(&nbp->bh_list_lock, flags);

    list_for_each_entry_safe(mbuf, nmbuf, &nbp->mbuf_list, m_list)
    {
        if (mbuf->m_list_private == port) {
            list_del(&mbuf->m_list);
            mbuf->m_list_private = NULL;
            m_freem(mbuf);
        }
    }

    list_for_each_entry_safe(lazy, nlazy, &nbp->lazy_list, head)
    {
        if (lazy->port == port) {
            list_del(&lazy->head);
        }
    }

    vmm_spin_unlock_irq_restore_lite(&nbp->bh_list_lock, flags);
}

/**
 * @brief 网络交换机底半部处理的主函数
 * @param param 参数结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int netswitch_bh_main(void *param)
{
    int                           rc;
    struct vmm_netport           *port;
    struct vmm_netswitch         *nsw;
    struct vmm_mbuf              *mbuf;
    struct vmm_netport_lazy      *lazy;
    struct vmm_netswitch_bh_ctrl *nbp = param;

    while (1) {
        /* Try to get next request from list or block if empty */
        lazy = NULL; /**< NULL成员 */
        mbuf = NULL; /**< NULL成员 */
        rc   = netswitch_bh_dequeue(nbp, &mbuf, &lazy); /**< &lazy)成员 */

        if (rc) {
            continue;
        }

        /* Process mbuf request */
        if (mbuf) {
            /* Extract port from mbuf */
            port                 = mbuf->m_list_private; /**< mbuf->m_list_private成员 */
            nsw                  = port->nsw; /**< port->nsw成员 */
            mbuf->m_list_private = NULL; /**< NULL成员 */

            /* Port might have been removed from netswitch */
            if (!port || !nsw) {
                if (mbuf) {
                    m_freem(mbuf);
                }

                continue;
            }

            /* Print debug info */
            DPRINTF("%s: nsw=%s port=%s mbuf\n", __func__, nsw->name, port->name); /**< port->name)成员 */

            /* Dump packet */
            DUMP_NETSWITCH_PKT(mbuf);

            /* Call the rx function of net switch */
            nsw->port2switch_xfer(nsw, port, mbuf); /**< mbuf)成员 */

            /* Free mbuf */
            m_freem(mbuf);
        }

        /* Process lazy request */
        if (lazy) {
            /* Extract info from lazy request */
            port = lazy->port; /**< lazy->port成员 */
            nsw  = port->nsw; /**< port->nsw成员 */

            /* Print debug info */
            DPRINTF("%s: nsw=%s port=%s lazy\n", __func__, nsw->name, port->name); /**< port->name)成员 */

            /* Call lazy xfer function */
            lazy->xfer(port, lazy->arg, lazy->budget); /**< lazy->budget)成员 */

            /* Add back to netswitch bh queue if required */
            if (arch_atomic_sub_return(&lazy->sched_count, 1) > 0) {
                /* Enqueue lazy request */
                rc = netswitch_bh_enqueue(nbp, NULL, lazy); /**< lazy)成员 */

                if (rc) {
                    vmm_printf(
                        "%s: nsw=%s src=%s lazy bh "
                        "enqueue failed.\n", /**< failed.\n"成员 */
                        __func__, nsw->name, port->name); /**< port->name)成员 */
                }
            }
        }
    }

    return VMM_OK;
}

/**
 * @brief 端口到交换机转发消息缓冲区
 * @param src 源设备树节点
 * @param mbuf 网络消息缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_port2switch_xfer_mbuf(struct vmm_netport *src, struct vmm_mbuf *mbuf)
{
    int                           rc;
    struct vmm_netswitch         *nsw;
    struct vmm_netswitch_bh_ctrl *nbp;

    if (!mbuf) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    if (!src || !src->nsw) {
        vmm_printf("%s: invalid source port.\n", __func__);
        m_freem(mbuf);
        return VMM_ERR_FAIL;
    }

    nsw = src->nsw;
    nbp = &this_cpu(nbctrl);

    /* Print debug info */
    DPRINTF("%s: nsw=%s src=%s\n", __func__, nsw->name, src->name);

    /* Save port in mbuf */
    mbuf->m_list_private = src;

    /* Add mbuf bh queue */
    rc                   = netswitch_bh_enqueue(nbp, mbuf, NULL);

    if (rc) {
        vmm_printf("%s: nsw=%s src=%s mbuf bh enqueue failed.\n", __func__, nsw->name, src->name);
        mbuf->m_list_private = NULL;
    }

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_port2switch_xfer_mbuf);

/**
 * @brief 延迟将端口数据传输到网络交换机
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_port2switch_xfer_lazy(struct vmm_netport_lazy *lazy)
{
    int  rc = VMM_ERR_BUSY;
    long sched_count;

    if (!lazy || !lazy->xfer || !lazy->port || !lazy->port->nsw) {
        vmm_printf("%s: invalid lazy instance.\n", __func__);
        return VMM_ERR_INVALID;
    }

    /* Print debug info */
    DPRINTF("%s: nsw=%s port=%s xfer lazy\n", __func__, lazy->port->nsw->name, lazy->port->name);

    sched_count = arch_atomic_add_return(&lazy->sched_count, 1);

    if (sched_count == 1) {
        /* Print debug info */
        DPRINTF("%s: nsw=%s port=%s bh enqueue\n", __func__, lazy->port->nsw->name, lazy->port->name);

        /* Add xfer request to xfer ring */
        rc = netswitch_bh_enqueue(&this_cpu(nbctrl), NULL, lazy);

        if (rc) {
            vmm_printf(
                "%s: nsw=%s port=%s lazy bh "
                "enqueue failed.\n",
                __func__, lazy->port->nsw->name, lazy->port->name);
        } else {
            rc = VMM_OK;
        }
    }

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_port2switch_xfer_lazy);

/**
 * @brief 交换机到端口转发消息缓冲区
 * @param nsw 网络交换机结构体指针
 * @param dst 目标缓冲区指针
 * @param mbuf 网络消息缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_switch2port_xfer_mbuf(struct vmm_netswitch *nsw, struct vmm_netport *dst, struct vmm_mbuf *mbuf)
{
    int         rc;
    irq_flags_t f;

    if (!nsw || !dst || !mbuf) {
        return VMM_ERR_FAIL;
    }

    /* Print debug info */
    DPRINTF("%s: nsw=%s dst=%s\n", __func__, nsw->name, dst->name);

    if (dst->can_receive && !dst->can_receive(dst)) {
        return VMM_OK;
    }

    MADDREFERENCE(mbuf);
    MCLADDREFERENCE(mbuf);

    vmm_spin_lock_irq_save_lite(&dst->switch2port_xfer_lock, f);
    rc = dst->switch2port_xfer(dst, mbuf);
    vmm_spin_unlock_irq_restore_lite(&dst->switch2port_xfer_lock, f);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_switch2port_xfer_mbuf);

struct vmm_netswitch *vmm_netswitch_alloc(struct vmm_netswitch_policy *nsp, const char *name)
{
    struct vmm_netswitch *nsw = NULL; /**< NULL成员 */

    if (!nsp || !name) {
        goto vmm_netswitch_alloc_done; /**< vmm_netswitch_alloc_done成员 */
    }

    nsw = vmm_zalloc(sizeof(struct vmm_netswitch)); /**< vmm_netswitch))成员 */

    if (!nsw) {
        vmm_printf("%s Failed to allocate net switch\n", __func__); /**< __func__)成员 */
        goto vmm_netswitch_alloc_failed; /**< vmm_netswitch_alloc_failed成员 */
    }

    nsw->policy = nsp; /**< nsp成员 */
    strncpy(nsw->name, name, VMM_FIELD_NAME_SIZE); /**< VMM_FIELD_NAME_SIZE)成员 */

    INIT_RW_LOCK(&nsw->port_list_lock);
    INIT_LIST_HEAD(&nsw->port_list);

    goto vmm_netswitch_alloc_done; /**< vmm_netswitch_alloc_done成员 */

vmm_netswitch_alloc_failed:

    if (nsw) {
        vmm_free(nsw);
        nsw = NULL; /**< NULL成员 */
    }

vmm_netswitch_alloc_done:
    return nsw; /**< nsw成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_alloc);

/**
 * @brief 释放网络交换机
 * @param nsw 网络交换机结构体指针
 */
void vmm_netswitch_free(struct vmm_netswitch *nsw)
{
    if (nsw) {
        vmm_free(nsw);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_free);

/**
 * @brief 网络交换机 端口 添加
 * @param nsw 网络交换机结构体指针
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_port_add(struct vmm_netswitch *nsw, struct vmm_netport *port)
{
    int         rc = VMM_OK;
    irq_flags_t f;

    if (!nsw || !port) {
        return VMM_ERR_FAIL;
    }

    /* Call the netswitch's port_add callback */
    if (nsw->port_add) {
        rc = nsw->port_add(nsw, port);
    }

    if (rc == VMM_OK) {
        /* Add the port to the port_list */
        vmm_write_lock_irq_save_lite(&nsw->port_list_lock, f);
        list_add_tail(&port->head, &nsw->port_list);
        vmm_write_unlock_irq_restore_lite(&nsw->port_list_lock, f);

        /* Mark this port to belong to the netswitch */
        port->nsw = nsw;

        /* Notify the port about the link-status change */
        port->flags |= VMM_NETPORT_LINK_UP;
        port->link_changed(port);
    }

#ifdef CONFIG_VERBOSE_MODE
    {
        char tname[30];
        vmm_printf(
            "NET: Port(\"%s\") added to Switch(\"%s\"), "
            "MAC[%s]\n",
            port->name, nsw->name, ethaddr_to_str(tname, port->macaddr));
    }
#endif

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_port_add);

/**
 * @brief 网络交换机 端口 移除
 * @param nsw 网络交换机结构体指针
 * @param port 端口编号或端口结构体指针
 */
static void netswitch_port_remove(struct vmm_netswitch *nsw, struct vmm_netport *port)
{
    uint32_t                      c;
    irq_flags_t                   f;
    struct vmm_netswitch_bh_ctrl *nbp;

    /* Notify the port about the link-status change */
    port->flags &= ~VMM_NETPORT_LINK_UP;
    port->link_changed(port);

    /* Mark the port to belong to NULL netswitch */
    port->nsw = NULL;

    /* Flush all xfer request related to this port */
    for_each_online_cpu(c)
    {
        nbp = &per_cpu(nbctrl, c);
        netswitch_bh_port_flush(nbp, port);
    }

    /* Remove the port from port_list */
    vmm_write_lock_irq_save_lite(&nsw->port_list_lock, f);
    list_del(&port->head);
    vmm_write_unlock_irq_restore_lite(&nsw->port_list_lock, f);

    /* Call the netswitch's port_remove handler */
    if (nsw->port_remove) {
        nsw->port_remove(nsw, port);
    }
}

/**
 * @brief 网络交换机 端口 移除
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_port_remove(struct vmm_netport *port)
{
    if (!port) {
        return VMM_ERR_FAIL;
    }

    if (!port->nsw) {
        return VMM_OK;
    }

#ifdef CONFIG_VERBOSE_MODE
    vmm_printf("NET: Port(\"%s\") removed from Switch(\"%s\")\n", port->name, port->nsw->name);
#endif

    netswitch_port_remove(port->nsw, port);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_port_remove);

static vmm_class_t nsw_class = {
    .name = VMM_NETSWITCH_CLASS_NAME,
};

/**
 * @brief 注册网络交换机
 * @param nsw 网络交换机结构体指针
 * @param parent 父设备树节点
 * @param private 私有数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_register(struct vmm_netswitch *nsw, vmm_device_t *parent, void *private)
{
    int rc;

    if (!nsw || !nsw->policy) {
        return VMM_ERR_INVALID;
    }

    vmm_device_driver_initialize_device(&nsw->dev);

    if (strlcpy(nsw->dev.name, nsw->name, sizeof(nsw->dev.name)) >= sizeof(nsw->dev.name)) {
        return VMM_ERR_OVERFLOW;
    }

    nsw->dev.parent = parent;
    nsw->dev.class  = &nsw_class;
    vmm_device_driver_set_data(&nsw->dev, nsw);

    rc = vmm_device_driver_register_device(&nsw->dev);

    if (rc != VMM_OK) {
        vmm_printf(
            "%s: Failed to class register network switch %s "
            "with err 0x%x\n",
            __func__, nsw->name, rc);
        return rc;
    }

    nsw->private = private;

#ifdef CONFIG_VERBOSE_MODE
    vmm_printf("Successfully registered VMM net switch: %s\n", nsw->name);
#endif

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_register);

/**
 * @brief 注销网络交换机
 * @param nsw 网络交换机结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw)
{
    irq_flags_t         f;
    struct vmm_netport *port;

    if (!nsw) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    vmm_read_lock_irq_save_lite(&nsw->port_list_lock, f);

    /* Remove any ports still attached to this nsw */
    while (!list_empty(&nsw->port_list)) {
        port = list_port(list_first(&nsw->port_list));
        vmm_read_unlock_irq_restore_lite(&nsw->port_list_lock, f);
        netswitch_port_remove(nsw, port);
        vmm_read_lock_irq_save_lite(&nsw->port_list_lock, f);
    }

    vmm_read_unlock_irq_restore_lite(&nsw->port_list_lock, f);

    return vmm_device_driver_unregister_device(&nsw->dev);
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_unregister);

struct vmm_netswitch *vmm_netswitch_find(const char *name)
{
    vmm_device_t *dev; /**< 设备 */

    dev = vmm_device_driver_class_find_device_by_name(&nsw_class, name); /**< name)成员 */

    if (!dev) {
        return NULL; /**< NULL成员 */
    }

    return vmm_device_driver_get_data(dev); /**< vmm_device_driver_get_data(dev)成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_find);

/**
 * @brief 网络交换机遍历上下文结构，私有上下文
 */
struct netswitch_iterate_priv {
    void *data; /**< 数据 */
    int (*fn)(struct vmm_netswitch *nsw, void *data); /**< 函数指针 */
};

/**
 * @brief 网络交换机 遍历
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int netswitch_iterate(vmm_device_t *dev, void *data)
{
    struct netswitch_iterate_priv *p   = data;
    struct vmm_netswitch          *nsw = vmm_device_driver_get_data(dev);

    return p->fn(nsw, p->data);
}

/**
 * @brief 网络交换机 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_iterate(struct vmm_netswitch *start, void *data, int (*fn)(struct vmm_netswitch *nsw, void *data))
{
    vmm_device_t                 *st = (start) ? &start->dev : NULL;
    struct netswitch_iterate_priv p;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&nsw_class, st, &p, netswitch_iterate);
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_iterate);

/**
 * @brief 遍历默认网络交换机
 * @param nsw 网络交换机结构体指针
 * @param data 用户自定义数据指针
 * @return 遍历结果
 */
static int netswitch_default_iterate(struct vmm_netswitch *nsw, void *data)
{
    struct vmm_netswitch **out_nsw = data;

    if (!(*out_nsw)) {
        *out_nsw = nsw;
    }

    return VMM_OK;
}

struct vmm_netswitch *vmm_netswitch_default(void)
{
    struct vmm_netswitch *nsw = NULL; /**< NULL成员 */

    vmm_netswitch_iterate(NULL, &nsw, netswitch_default_iterate); /**< netswitch_default_iterate)成员 */

    return nsw; /**< nsw成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_default);

/**
 * @brief 获取网络交换机的数量
 * @return 数量值
 */
uint32_t vmm_netswitch_count(void)
{
    return vmm_device_driver_class_device_count(&nsw_class);
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_count);

/**
 * @brief 注册网络交换策略
 * @param nsp 节点特定数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_register(struct vmm_netswitch_policy *nsp)
{
    struct vmm_netswitch_policy *nsp1;

    if (!nsp || !nsp->create || !nsp->destroy) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&policy_list_lock);

    list_for_each_entry(nsp1, &policy_list, head)
    {
        if (strcmp(nsp1->name, nsp->name) == 0) {
            vmm_mutex_unlock(&policy_list_lock);
            return VMM_ERR_EXIST;
        }
    }

    INIT_LIST_HEAD(&nsp->head);
    list_add_tail(&nsp->head, &policy_list);

    vmm_mutex_unlock(&policy_list_lock);

    return VMM_OK;
}

/**
 * @brief 网络交换策略注销上下文结构，私有上下文
 */
struct netswitch_policy_unregister_priv {
    struct vmm_netswitch_policy *nsp; /**< 网络端口 */
    struct vmm_netswitch        *nsw; /**< 网络交换 */
};

/**
 * @brief 查找网络交换机策略注销回调
 * @param nsw 网络交换机结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int netswitch_policy_unregister_find(struct vmm_netswitch *nsw, void *data)
{
    struct netswitch_policy_unregister_priv *private = data;

    if (nsw->policy == private->nsp) {
        private->nsw = nsw; /**< nsw成员 */
    }

    return VMM_OK;
}

/**
 * @brief 注销网络交换策略
 * @param nsp 节点特定数据指针
 */
void vmm_netswitch_policy_unregister(struct vmm_netswitch_policy *nsp)
{
    int ret;
    struct netswitch_policy_unregister_priv private;

    if (!nsp) {
        return;
    }

    vmm_mutex_lock(&policy_list_lock);

    do {
        private.nsw = NULL;
        private.nsp = nsp;
        ret         = vmm_netswitch_iterate(NULL, &private, netswitch_policy_unregister_find);

        if (ret || !private.nsw) {
            break;
        }

        nsp->destroy(nsp, private.nsw);
    } while (1);

    list_del(&nsp->head);

    vmm_mutex_unlock(&policy_list_lock);
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_policy_unregister);

/**
 * @brief 按策略遍历网络交换机
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_iterate(struct vmm_netswitch_policy *start, void *data, int (*fn)(struct vmm_netswitch_policy *, void *))
{
    int                          ret         = VMM_OK;
    bool                         found_start = (start) ? FALSE : TRUE;
    struct vmm_netswitch_policy *nsp;

    vmm_mutex_lock(&policy_list_lock);

    list_for_each_entry(nsp, &policy_list, head)
    {
        if (start == nsp) {
            found_start = TRUE;
        }

        if (found_start) {
            ret = fn(nsp, data);

            if (ret) {
                vmm_mutex_unlock(&policy_list_lock);
                return ret;
            }
        }
    }

    vmm_mutex_unlock(&policy_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_policy_iterate);

/**
 * @brief 网络交换策略查找上下文结构，私有上下文
 */
struct netswitch_policy_find_priv {
    const char                  *name; /**< 名称 */
    struct vmm_netswitch_policy *nsp; /**< 网络端口 */
};

/**
 * @brief 查找网络交换机策略
 * @param nsp 节点特定数据指针
 * @param data 用户自定义数据指针
 * @return 遍历结果
 */
static int netswitch_policy_find(struct vmm_netswitch_policy *nsp, void *data)
{
    struct netswitch_policy_find_priv *private = data;

    if (strcmp(private->name, nsp->name) == 0) {
        private->nsp = nsp; /**< nsp成员 */
    }

    return VMM_OK;
}

struct vmm_netswitch_policy *vmm_netswitch_policy_find(const char *name)
{
    int ret; /**< 返回值 */
    struct netswitch_policy_find_priv private = {.name = name, .nsp = NULL}; /**< NULL}成员 */

    ret                                       = vmm_netswitch_policy_iterate(NULL, &private, netswitch_policy_find); /**< netswitch_policy_find)成员 */

    return (ret) ? NULL : private.nsp; /**< private.nsp成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_policy_find);

/**
 * @brief 获取网络交换机策略的数量
 * @param nsp 节点特定数据指针
 * @param data 用户自定义数据指针
 * @return 数量值
 */
static int netswitch_policy_count(struct vmm_netswitch_policy *nsp, void *data)
{
    uint32_t *ret = data;

    (*ret)++;

    return VMM_OK;
}

/**
 * @brief 获取网络交换策略的数量
 * @return 数量值
 */
uint32_t vmm_netswitch_policy_count(void)
{
    uint32_t ret = 0;

    vmm_netswitch_policy_iterate(NULL, &ret, netswitch_policy_count);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_policy_count);

/**
 * @brief 网络交换机策略创建交换机实例
 * @param policy_name 调度策略名称
 * @param switch_name 交换机名称
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_create_switch(const char *policy_name, const char *switch_name, int argc, char **argv)
{
    int                          ret = VMM_OK;
    struct vmm_netswitch        *nsw = NULL;
    struct vmm_netswitch_policy *nsp;

    if (!policy_name || !switch_name || ((argc > 0) && !argv)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&policy_list_lock);

    list_for_each_entry(nsp, &policy_list, head)
    {
        if (strcmp(nsp->name, policy_name) != 0) {
            continue;
        }

        nsw = nsp->create(nsp, switch_name, argc, argv);

        if (!nsw) {
            ret = VMM_ERR_FAIL;
            goto done_unlock;
        }

        break;
    }

    if (!nsw) {
        ret = VMM_ERR_INVALID;
    }

done_unlock:
    vmm_mutex_unlock(&policy_list_lock);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_policy_create_switch);

/**
 * @brief 网络交换机策略销毁交换机实例
 * @param nsw 网络交换机结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_destroy_switch(struct vmm_netswitch *nsw)
{
    if (!nsw || !nsw->policy) {
        return VMM_ERR_INVALID;
    }

    vmm_mutex_lock(&policy_list_lock);
    nsw->policy->destroy(nsw->policy, nsw);
    vmm_mutex_unlock(&policy_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_netswitch_policy_destroy_switch);

/**
 * @brief 初始化网络交换机每CPU数据
 * @param a1 参数寄存器a1值
 * @param a2 参数寄存器a2值
 * @param a3 参数寄存器a3值
 */
static void vmm_netswitch_per_cpu_init(void *a1, void *a2, void *a3)
{
    char                          name[VMM_FIELD_NAME_SIZE];
    uint32_t                      cpu = vmm_smp_processor_id();
    struct vmm_netswitch_bh_ctrl *nbp = &per_cpu(nbctrl, cpu);

    vmm_snprintf(name, sizeof(name), "%s/%d", VMM_NETSWITCH_CLASS_NAME, cpu);

    nbp->thread = vmm_threads_create(name, netswitch_bh_main, nbp, VMM_THREAD_DEF_PRIORITY, VMM_THREAD_DEF_TIME_SLICE);

    if (!nbp->thread) {
        vmm_printf("%s: CPU%d: Failed to create thread\n", __func__, cpu);
        return;
    }

    if (vmm_threads_set_affinity(nbp->thread, vmm_cpumask_of(cpu))) {
        vmm_printf("%s: CPU%d: Failed to set thread affinity\n", __func__, cpu);
        vmm_threads_destroy(nbp->thread);
        return;
    }

    netswitch_bh_init(nbp);

    vmm_threads_start(nbp->thread);
}

/**
 * @brief 初始化网络交换机
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_netswitch_init(void)
{
    int rc;

    vmm_init_printf("network switch framework\n");

    rc = vmm_device_driver_register_class(&nsw_class);

    if (rc) {
        vmm_printf("Failed to register %s class\n", VMM_NETSWITCH_CLASS_NAME);
        return rc;
    }

    vmm_smp_ipi_async_call(cpu_online_mask, vmm_netswitch_per_cpu_init, NULL, NULL, NULL);

    return VMM_OK;
}

/**
 * @brief 网络交换机每CPU资源退出清理
 * @param a1 参数寄存器a1值
 * @param a2 参数寄存器a2值
 * @param a3 参数寄存器a3值
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_netswitch_per_cpu_exit(void *a1, void *a2, void *a3)
{
    struct vmm_netswitch_bh_ctrl *nbp = &this_cpu(nbctrl);

    vmm_threads_stop(nbp->thread);

    vmm_threads_destroy(nbp->thread);
}

/**
 * @brief 网络交换机子系统退出
 * @return 成功返回VMM_OK，失败返回错误码
 */
void __exit vmm_netswitch_exit(void)
{
    int          rc;
    vmm_class_t *c;

    vmm_smp_ipi_sync_call(cpu_online_mask, 1000, vmm_netswitch_per_cpu_exit, NULL, NULL, NULL);

    c = vmm_device_driver_find_class(VMM_NETSWITCH_CLASS_NAME);

    if (!c) {
        return;
    }

    rc = vmm_device_driver_unregister_class(c);

    if (rc) {
        vmm_printf("Failed to unregister %s class", VMM_NETSWITCH_CLASS_NAME);
        return;
    }

    vmm_free(c);

    return;
}
