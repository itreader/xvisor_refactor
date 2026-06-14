/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_bridge.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief 软件网桥网络交换机实现
 */

#include <libs/stringlib.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_protocol.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>

#undef DEBUG_BRIDGE

#ifdef DEBUG_BRIDGE
#define DPRINTF(fmt, ...)                                                                                                                            \
    do {                                                                                                                                             \
        vmm_printf(fmt, ##__VA_ARGS__);                                                                                                              \
    } while (0)
#else
#define DPRINTF(fmt, ...)                                                                                                                            \
    do {                                                                                                                                             \
    } while (0)
#endif

#define BRIDGE_MAC_TABLE_SZ 32
#define BRIDGE_MAC_EXPIRY   30000000000LLU

/* We maintain a table of learned mac addresses
 * (please note that the mac of the immediate netports are not
 * kept in this table) */
/**
 * @brief 网桥MAC条目结构，条目结构
 */
struct bridge_mac_entry {
    struct vmm_netport *port; /**< 端口 */
    uint8_t             macaddr[6]; /**< MAC地址 */
    uint64_t            timestamp; /**< 时间戳 */
};

/**
 * @brief 网桥控制结构，控制结构
 */
struct bridge_ctrl {
    struct vmm_netswitch    *nsw; /**< 网络交换 */
    vmm_timer_event_t        ev; /**< 事件 */
    vmm_rwlock_t             mac_table_lock; /**< mac_table_lock成员 */
    uint32_t                 mac_table_sz; /**< mac_table_sz成员 */
    struct bridge_mac_entry *mac_table; /**< mac_table成员 */
};

/**
 * @brief 清理网桥MAC地址表中指定端口的条目
 * @param br 块请求结构体指针
 * @param port 端口编号或端口结构体指针
 */
static void bridge_mactable_cleanup_port(struct bridge_ctrl *br, struct vmm_netport *port)
{
    uint32_t    m;
    irq_flags_t f;

    vmm_write_lock_irq_save_lite(&br->mac_table_lock, f);

    for (m = 0; m < br->mac_table_sz; m++) {
        if (br->mac_table[m].port == port) {
            br->mac_table[m].port = NULL;
        }
    }

    vmm_write_unlock_irq_restore_lite(&br->mac_table_lock, f);
}

/**
 * @brief 查找桥接MAC地址表学习回调
 * @param br 块请求结构体指针
 * @param dstmac 目标MAC地址
 * @param srcmac 源MAC地址
 * @param src 源设备树节点
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_netport *bridge_mactable_learn_find(struct bridge_ctrl *br, const uint8_t *dstmac, const uint8_t *srcmac, struct vmm_netport *src)
{
    uint32_t                 i;
    uint64_t                 tstamp;
    irq_flags_t              f;
    bool learn;
    bool update;
    bool found;
    struct vmm_netport      *dst;
    struct bridge_mac_entry *m;

    /* Acquire read lock */
    vmm_read_lock_irq_save_lite(&br->mac_table_lock, f);

    /* Check for for dstmac and whether we need
     * to Learn (srcmac, src) mapping ??
     */
    learn = TRUE;
    dst   = NULL;

    for (i = 0; i < br->mac_table_sz; i++) {
        m = &br->mac_table[i];

        /* If mac table entry is unused then continue */
        if (!m->port) {
            continue;
        }

        /* Match (srcmac, srcport) */
        if (learn && !compare_ether_addr(m->macaddr, srcmac) && (m->port == src)) {
            learn = FALSE;
        }

        /* Match (dstmac) */
        if (!dst && !compare_ether_addr(m->macaddr, dstmac)) {
            dst = m->port;
        }

        /* If no need to learn and found
         * destination port then break
         */
        if (!learn && dst) {
            break;
        }
    }

    /* Release read lock */
    vmm_read_unlock_irq_restore_lite(&br->mac_table_lock, f);

    /* If leaning required then update mac table */
    if (learn) {
        /* Retrive current timestamp */
        tstamp = vmm_timer_timestamp();

        /* Acquire write lock */
        vmm_write_lock_irq_save_lite(&br->mac_table_lock, f);

        /* If mac entry already exist then
         * update only port and timestamp
         */
        update = TRUE;

        for (i = 0; i < br->mac_table_sz; i++) {
            m = &br->mac_table[i];

            if (!compare_ether_addr(m->macaddr, srcmac)) {
                m->port      = src;
                m->timestamp = tstamp;
                update       = FALSE;
                break;
            }
        }

        /* If mac entry does not exist then
         * save (mac, port, timestamp) in a
         * free mac table entry.
         */
        if (update) {
            found = FALSE;

            for (i = 0; i < br->mac_table_sz; i++) {
                m = &br->mac_table[i];

                if (m->port == NULL) {
                    found = TRUE;
                    break;
                }
            }

            if (found) {
                m->port = src;
                memcpy(m->macaddr, srcmac, 6);
                m->timestamp = tstamp;
            }
        }

        /* Release write lock */
        vmm_write_unlock_irq_restore_lite(&br->mac_table_lock, f);
    }

    return dst;
}

/**
 * @brief 桥接 定时器 事件
 * @param ev 定时器事件
 */
static void bridge_timer_event(vmm_timer_event_t *ev)
{
    uint32_t                 i;
    uint64_t                 tstamp;
    irq_flags_t              f;
    struct bridge_ctrl      *br = ev->private;
    struct bridge_mac_entry *m;

    DPRINTF("%s: bridge expiry event nsw=%s\n", __func__, br->nsw->name);

    /* Retrive current timestamp */
    tstamp = vmm_timer_timestamp();

    /* Acquire write lock */
    vmm_write_lock_irq_save_lite(&br->mac_table_lock, f);

    /* Purge old enteries */
    for (i = 0; i < br->mac_table_sz; i++) {
        m = &br->mac_table[i];

        if (m->port && ((m->timestamp - tstamp) > BRIDGE_MAC_EXPIRY)) {
            DPRINTF("%s: purge port=%s\n", __func__, m->port->name);
            m->port = NULL;
            memset(m->macaddr, 0, 6);
            m->timestamp = 0;
        }
    }

    /* Release write lock */
    vmm_write_unlock_irq_restore_lite(&br->mac_table_lock, f);

    /* Again start the bridge timer event */
    vmm_timer_event_start(&br->ev, BRIDGE_MAC_EXPIRY);
}

/**
 *  Thread body responsible for sending the RX buffer packets
 *  to the destination port(s)
 */
static int bridge_rx_handler(struct vmm_netswitch *nsw, struct vmm_netport *src, struct vmm_mbuf *mbuf)
{
    irq_flags_t         f;
    const uint8_t *srcmac = NULL;
    const uint8_t *dstmac = NULL;
    bool                broadcast = TRUE;
    double_list_t      *l, *l1;
    struct vmm_netport *dst = NULL;
    struct vmm_netport *port = NULL;
    struct bridge_ctrl *br = nsw->private;

    /* Get source and destination mac addresses */
    srcmac                 = ether_srcmac(mtod(mbuf, uint8_t *));
    dstmac                 = ether_dstmac(mtod(mbuf, uint8_t *));

    /* Learn source mac address and find port
     * matching destination mac address
     */
    dst                    = bridge_mactable_learn_find(br, dstmac, srcmac, src);

    /* If the frame below cases then it should be unicast.
     *
     * Case 1: destination MAC address is not broadcast address
     * Case 2: We found port matching destination mac address
     */
    if (!is_broadcast_ether_addr(dstmac) && dst) {
        /* Find port fordestination mac address */
        broadcast = FALSE;
    }

    /* Transfer mbuf to appropriate ports */
    if (broadcast) {
        DPRINTF("%s: broadcasting\n", __func__);
        vmm_read_lock_irq_save_lite(&nsw->port_list_lock, f);
        list_for_each_safe(l, l1, &nsw->port_list)
        {
            port = list_port(l);

            if (port == src) {
                continue;
            }

            vmm_read_unlock_irq_restore_lite(&nsw->port_list_lock, f);
            vmm_switch2port_xfer_mbuf(nsw, port, mbuf);
            vmm_read_lock_irq_save_lite(&nsw->port_list_lock, f);
        }
        vmm_read_unlock_irq_restore_lite(&nsw->port_list_lock, f);
    } else {
        DPRINTF("%s: unicasting to \"%s\"\n", __func__, dst->name);
        vmm_switch2port_xfer_mbuf(nsw, dst, mbuf);
    }

    return VMM_OK;
}

/**
 * @brief 桥接 端口 添加
 * @param nsw 网络交换机结构体指针
 * @param port 端口编号或端口结构体指针
 * @return 编号值
 */
static int bridge_port_add(struct vmm_netswitch *nsw, struct vmm_netport *port)
{
    /* For now nothing to do here. */
    return VMM_OK;
}

/**
 * @brief 桥接 端口 移除
 * @param nsw 网络交换机结构体指针
 * @param port 端口编号或端口结构体指针
 * @return 编号值
 */
static int bridge_port_remove(struct vmm_netswitch *nsw, struct vmm_netport *port)
{
    struct bridge_ctrl *br = nsw->private;

    /* Cleanup mactable enteries for this port */
    bridge_mactable_cleanup_port(br, port);

    return VMM_OK;
}

/**
 * @brief 创建网桥实例
 * @param policy 策略标识
 * @param name 目标对象的名称
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_netswitch *bridge_create(struct vmm_netswitch_policy *policy, const char *name, int argc, char **argv)
{
    int                   rc;
    struct bridge_ctrl   *br;
    struct vmm_netswitch *nsw = NULL;

    nsw                       = vmm_netswitch_alloc(policy, name);

    if (!nsw) {
        goto bridge_netswitch_alloc_failed; /**< bridge_netswitch_alloc_failed成员 */
    }

    nsw->port2switch_xfer = bridge_rx_handler;
    nsw->port_add         = bridge_port_add;
    nsw->port_remove      = bridge_port_remove;

    br                    = vmm_zalloc(sizeof(struct bridge_ctrl));

    if (!br) {
        goto bridge_alloc_failed;
    }

    br->nsw = nsw;
    INIT_TIMER_EVENT(&br->ev, bridge_timer_event, br);
    INIT_RW_LOCK(&br->mac_table_lock);
    br->mac_table_sz = BRIDGE_MAC_TABLE_SZ;
    br->mac_table    = vmm_zalloc(sizeof(struct bridge_mac_entry) * br->mac_table_sz);

    if (!br->mac_table) {
        goto bridge_alloc_mac_table_fail;
    }

    rc = vmm_netswitch_register(nsw, NULL, br);

    if (rc) {
        goto bridge_netswitch_register_fail;
    }

    vmm_timer_event_start(&br->ev, BRIDGE_MAC_EXPIRY);

    return nsw;

bridge_netswitch_register_fail:
    vmm_free(br->mac_table);
bridge_alloc_mac_table_fail:
    vmm_free(br);
bridge_alloc_failed:
    vmm_netswitch_free(nsw);
bridge_netswitch_alloc_failed:
    return NULL;
}

/**
 * @brief 销毁网桥实例
 * @param policy 策略标识
 * @param nsw 网络交换机结构体指针
 */
static void bridge_destroy(struct vmm_netswitch_policy *policy, struct vmm_netswitch *nsw)
{
    struct bridge_ctrl *br;

    if (!nsw || !nsw->private) {
        return;
    }

    br = nsw->private;

    vmm_timer_event_stop(&br->ev);

    vmm_netswitch_unregister(nsw);

    vmm_free(br->mac_table);
    vmm_free(br);

    vmm_netswitch_free(nsw);
}

static struct vmm_netswitch_policy bridge = {
    .name    = "bridge",
    .create  = bridge_create,
    .destroy = bridge_destroy,
};

/**
 * @brief 初始化桥接
 * @return 编号值
 */
int __init vmm_bridge_init(void)
{
    return vmm_netswitch_policy_register(&bridge);
}

/**
 * @brief 网桥子系统退出
 * @return 编号值
 */
void __exit vmm_bridge_exit(void)
{
    vmm_netswitch_policy_unregister(&bridge);
}
