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
 * @file vmm_mbuf.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief 网络缓冲区处理
 *
 * The code has been adapted from NetBSD 5.1.2 src/sys/kern/uipc_mbuf.c
 */

/**
 * Copyright (c) 1996, 1997, 1999, 2001, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and Matt Thomas of 3am Software Foundry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Copyright (c) 1982, 1986, 1988, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)mbuf.h  8.5 (Berkeley) 2/19/95
 */

#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/mempool.h>
#include <libs/stringlib.h>
#include <net/vmm_mbuf.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

/*
 * Mbuffer pool.
 */

#define EPOOL_SLAB_COUNT 4

/**
 * @brief mbuf内存池控制结构，管理网络缓冲区的分配
 */
struct vmm_mbufpool_ctrl {
    struct mempool *mpool; /**< mpool成员 */
    struct mempool *epool_slabs[EPOOL_SLAB_COUNT]; /**< epool_slabs成员 */
};

static struct vmm_mbufpool_ctrl mbpctrl;

/**
 * @brief 获取消息缓冲区池中slab缓冲区的大小
 * @param slab slab分配器指针
 * @return 大小值（字节）
 */
static uint32_t epool_slab_buf_size(uint32_t slab)
{
    switch (slab) {
        case 0:
            return 512;

        case 1:
            return 1024;

        case 2:
            return 1536;

        case 3:
            return 2048;

        default:
            break;
    };

    return 0;
}

/**
 * @brief 获取消息缓冲区池中slab缓冲区的的数量
 * @param pool_sz 池大小
 * @param slab slab分配器指针
 * @return 数量值
 */
static uint32_t epool_slab_buf_count(uint32_t pool_sz, uint32_t slab)
{
    uint32_t slab_size;
    uint32_t buf_size;
    uint32_t weight;
    uint32_t total_weight;

    switch (slab) {
        case 0:
            weight = 1;
            break;

        case 1:
            weight = 1;
            break;

        case 2:
            weight = 4;
            break;

        case 3:
            weight = 2;
            break;

        default:
            return 0;
    };

    total_weight = 8;

    buf_size     = epool_slab_buf_size(slab);

    if (!buf_size) {
        return 0;
    }

    slab_size = udiv32(pool_sz, total_weight) * weight;

    if (!slab_size) {
        return 0;
    }

    return udiv32(slab_size, buf_size);
}

/**
 * @brief 初始化mbufpool
 * @return 数量值
 */
int __init vmm_mbufpool_init(void)
{
    uint32_t slab;
    uint32_t b_size;
    uint32_t b_count;
    uint32_t epool_sz;

    memset(&mbpctrl, 0, sizeof(mbpctrl));

    /* Create mbuf pool */
    b_size        = sizeof(struct vmm_mbuf);
    b_count       = CONFIG_NET_MBUF_POOL_SIZE;
    mbpctrl.mpool = mempool_ram_create(b_size, VMM_SIZE_TO_PAGE(b_size * b_count), VMM_PAGE_POOL_NORMAL);

    if (!mbpctrl.mpool) {
        return VMM_ERR_NOMEM;
    }

    /* Create ext slab pools */
    epool_sz = (CONFIG_NET_MBUF_EXT_POOL_SIZE_KB * 1024);

    for (slab = 0; slab < EPOOL_SLAB_COUNT; slab++) {
        b_size  = epool_slab_buf_size(slab);
        b_count = epool_slab_buf_count(epool_sz, slab);

        if (b_count && b_size) {
            mbpctrl.epool_slabs[slab] = mempool_ram_create(b_size, VMM_SIZE_TO_PAGE(b_size * b_count), VMM_PAGE_POOL_NORMAL);
        } else {
            mbpctrl.epool_slabs[slab] = NULL;
        }
    }

    return VMM_OK;
}

/**
 * @brief 内存缓冲池退出清理
 * @return 成功返回VMM_OK，失败返回错误码
 */
void __exit vmm_mbufpool_exit(void)
{
    uint32_t slab;

    /* Destroy mbuf pool */
    if (mbpctrl.mpool) {
        mempool_destroy(mbpctrl.mpool);
    }

    /* Destroy ext slab pools */
    for (slab = 0; slab < EPOOL_SLAB_COUNT; slab++) {
        if (mbpctrl.epool_slabs[slab]) {
            mempool_destroy(mbpctrl.epool_slabs[slab]);
        }
    }
}

/*
 * Mbuffer utility routines.
 */

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
/**
 * @brief m copydata
 * @param m 掩码或数据指针
 * @param off 偏移量
 * @param len 数据长度
 * @param vp 虚拟端口指针
 */
void m_copydata(struct vmm_mbuf *m, int off, int len, void *vp)
{
    unsigned count;
    void    *cp = vp;

    if (m == NULL || vp == NULL) {
        vmm_panic("%s: either m or vp is NULL\n", __func__);
    }

    if (off < 0 || len < 0) {
        vmm_panic("%s: off %d, len %d", __func__, off, len);
    }

    while (off > 0) {
        if (m == NULL) {
            vmm_panic("%s: m == NULL, off %d", __func__, off);
        }

        if (off < m->m_len) {
            break;
        }

        off -= m->m_len;
        m = m->m_next;
    }

    while (len > 0) {
        count = min(m->m_len - off, len);
        memcpy(cp, mtod(m, char *) + off, count);
        len -= count;
        cp  = (char *)cp + count;
        off = 0;
        m   = m->m_next;
    }
}

VMM_ERR_XPORT_SYMBOL(m_copydata);

/**
 * @brief 释放消息缓冲区到池中
 * @param m 掩码或数据指针
 */
static void mbuf_pool_free(struct vmm_mbuf *m)
{
    mempool_free(mbpctrl.mpool, m);
}

/**
 * @brief 释放消息缓冲区到堆中
 * @param m 掩码或数据指针
 */
static void mbuf_heap_free(struct vmm_mbuf *m)
{
    vmm_free(m);
}

/*
 * Space allocation routines.
 * These are also available as macros
 * for critical paths.
 */
struct vmm_mbuf *m_get(int nowait, int flags)
{
    struct vmm_mbuf *m; /**< m */

    /* TODO: implement non-blocking variant */

    m = mempool_zalloc(mbpctrl.mpool); /**< mempool_zalloc(mbpctrl.mpool)成员 */

    if (m) {
        m->m_freefn = mbuf_pool_free; /**< mbuf_pool_free成员 */
    } else if (NULL != (m = vmm_zalloc(sizeof(struct vmm_mbuf)))) {
        m->m_freefn = mbuf_heap_free; /**< mbuf_heap_free成员 */
    } else {
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&m->m_list);
    m->m_list_private = NULL; /**< NULL成员 */
    m->m_next         = NULL; /**< NULL成员 */
    m->m_data         = NULL; /**< NULL成员 */
    m->m_len          = 0; /**< 0 */
    m->m_flags        = flags; /**< 标志位 */

    if (flags & M_PKTHDR) {
        m->m_pktlen = 0; /**< 0 */
    }

    m->m_ref = 1; /**< 1 */

    return m; /**< m */
}

VMM_ERR_XPORT_SYMBOL(m_get);

/**
 * @brief 将外部数据释放回池中
 * @param m 掩码或数据指针
 * @param ptr 通用指针
 * @param size 大小
 * @param arg 参数值
 */
static void ext_pool_free(struct vmm_mbuf *m, void *ptr, uint32_t size, void *arg)
{
    struct mempool *mp = arg;

    mempool_free(mp, ptr);
}

/**
 * @brief 将外部数据释放回堆中
 * @param m 掩码或数据指针
 * @param ptr 通用指针
 * @param size 大小
 * @param arg 参数值
 */
static void ext_heap_free(struct vmm_mbuf *m, void *ptr, uint32_t size, void *arg)
{
    vmm_free(ptr);
}

/**
 * @brief 释放DMA外部数据
 * @param m 掩码或数据指针
 * @param ptr 通用指针
 * @param size 大小
 * @param arg 参数值
 */
static void ext_dma_free(struct vmm_mbuf *m, void *ptr, uint32_t size, void *arg)
{
    vmm_dma_free(ptr);
}

/**
 * @brief 获取消息缓冲区的外部扩展数据
 * @param m 掩码或数据指针
 * @param size 大小
 * @param how 操作方式标识
 * @return 目标对象指针，不存在返回NULL
 */
void *m_ext_get(struct vmm_mbuf *m, uint32_t size, enum vmm_mbuf_alloc_types how)
{
    void           *buf;
    uint32_t        slab;
    struct mempool *mp = NULL;

    if (VMM_MBUF_ALLOC_DMA == how) {
        buf = vmm_dma_malloc(size); /**< vmm_dma_malloc(size)成员 */

        if (!buf) {
            return NULL; /**< NULL成员 */
        }

        m->m_flags |= M_EXT_DMA; /**< M_EXT_DMA成员 */
        MEXTADD(m, buf, size, ext_dma_free, NULL); /**< NULL)成员 */
    } else {
        for (slab = 0; slab < EPOOL_SLAB_COUNT; slab++) {
            if (size <= epool_slab_buf_size(slab)) {
                mp = mbpctrl.epool_slabs[slab];
                break;
            }
        }

        if (mp && (buf = mempool_malloc(mp))) {
            m->m_flags |= M_EXT_POOL;
            MEXTADD(m, buf, size, ext_pool_free, mp);
        } else if ((buf = vmm_malloc(size))) {
            m->m_flags |= M_EXT_HEAP;
            MEXTADD(m, buf, size, ext_heap_free, NULL);
        } else {
            return NULL;
        }
    }

    return m->m_extbuf;
}

VMM_ERR_XPORT_SYMBOL(m_ext_get);

/*
 * m_ext_dma_ensure: Ensure that the data buffer is DMA proof, reallocating
 * and copying data to do so.
 */
/**
 * @brief 确保消息缓冲区的外部数据适合DMA操作
 * @param m 掩码或数据指针
 */
void m_ext_dma_ensure(struct vmm_mbuf *m)
{
    char *buf = NULL;

    if (vmm_is__dma(m->m_extbuf)) {
        return;
    }

    buf = vmm_dma_malloc(m->m_len);
    memcpy(buf, m->m_extbuf, m->m_len);

    if (m->m_extfree) {
        m->m_extfree(m, m->m_extbuf, m->m_extlen, m->m_extarg);
    } else {
        vmm_free(m->m_extbuf);
    }

    MEXTADD(m, buf, m->m_len, ext_dma_free, 0);
}

/*
 * m_ext_free: release a reference to the mbuf external storage.
 * free the mbuf itself as well.
 */

/**
 * @brief 释放消息缓冲区的外部扩展数据
 * @param m 掩码或数据指针
 */
void m_ext_free(struct vmm_mbuf *m)
{
    if (!(--(m->m_extref)) && !(m->m_flags & M_EXT_DONTFREE)) {
        /* dropping the last reference */
        if (m->m_extfree) {
            (*m->m_extfree)(m, m->m_extbuf, m->m_extlen, m->m_extarg);
        } else {
            BUG_ON(1);
        }
    }

    if (!(--(m->m_ref))) {
        if (m->m_freefn) {
            m->m_freefn(m);
        } else {
            BUG_ON(1);
        }
    }
}

VMM_ERR_XPORT_SYMBOL(m_ext_free);

struct vmm_mbuf *m_free(struct vmm_mbuf *m)
{
    struct vmm_mbuf *n; /**< n */

    MFREE(m, n); /**< n) */
    return (n); /**< (n)成员 */
}

VMM_ERR_XPORT_SYMBOL(m_free);

/**
 * @brief m freem
 * @param m 掩码或数据指针
 */
void m_freem(struct vmm_mbuf *m)
{
    struct vmm_mbuf *n;

    if (m == NULL) {
        return;
    }

    do {
        MFREE(m, n);
        m = n;
    } while (m);
}

VMM_ERR_XPORT_SYMBOL(m_freem);

/**
 * @brief 输出消息缓冲区的数据内容用于调试
 * @param buf 数据缓冲区指针
 * @param buflen 大小
 */
static void mbuf_data_dump(char *buf, uint32_t buflen)
{
    int index;

    vmm_printf("%02x:%02x:%02x:%02x:%02x:%02x ", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    vmm_printf("%02x:%02x:%02x:%02x:%02x:%02x ", buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
    vmm_printf("%02x%02x\n", buf[12], buf[13]);

    for (index = 14; index < buflen; ++index) {
        vmm_printf("%02x", buf[index]);
    }

    vmm_printf("\n");
}

/**
 * @brief m dump
 * @param m 掩码或数据指针
 */
void m_dump(struct vmm_mbuf *m)
{
    vmm_printf("MBuf header\n");
    vmm_printf("  MBuf ref:      %d\n", m->m_ref);
    vmm_printf("  MBuf data:     %p\n", m->m_data);
    vmm_printf("  MBuf free fct: %p\n", m->m_freefn);
    vmm_printf("  MBuf len:      %d\n", m->m_len);
    vmm_printf("  MBuf flags:    0x%x\n", m->m_flags);
    vmm_printf("MBuf packet\n");
    vmm_printf("  MBuf len:      %d\n", m->m_pktlen);
    vmm_printf("MBuf ext\n");
    vmm_printf("  MBuf buf:      %p\n", m->m_extbuf);
    vmm_printf("  MBuf len:      %d\n", m->m_extlen);
    vmm_printf("  MBuf ref cnt:  %d\n", m->m_extref);
    vmm_printf("  MBuf free:     %p\n", m->m_extfree);
    vmm_printf("  MBuf free arg: %p\n", m->m_extarg);
    vmm_printf("\nMBuf data dump (%d):\n", m->m_len);
    mbuf_data_dump(m->m_data, m->m_len);
}

VMM_ERR_XPORT_SYMBOL(m_dump);
