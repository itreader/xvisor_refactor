/**
 * Copyright (c) 2010 Himanshu Chauhan.
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_heap.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @author Ankit Jindal (thatsjindal@gmail.com)
 * @brief 基于伙伴分配器的堆管理
 */

#include <libs/buddy.h>
#include <libs/stringlib.h>
#include <vmm_cache.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_stdio.h>

/**
 * @brief 堆内存控制结构，管理内存池的分配状态
 */
struct vmm_heap_control {
    struct buddy_allocator ba; /**< ba */
    void                  *hk_start; /**< hk_start成员 */
    uint64_t               hk_size; /**< hk_size成员 */
    void                  *mem_start; /**< mem_start成员 */
    uint64_t               mem_size; /**< mem_size成员 */
    void                  *heap_start; /**< heap_start成员 */
    physical_addr_t        heap_start_pa; /**< heap_start_pa成员 */
    uint64_t               heap_size; /**< heap_size成员 */
};

static struct vmm_heap_control normal_heap;
static struct vmm_heap_control dma_heap;

#define HEAP_MIN_BIN (VMM_CACHE_LINE_SHIFT)
#define HEAP_MAX_BIN (VMM_PAGE_SHIFT)

/**
 * @brief 堆内存分配
 * @param heap 堆结构体指针
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
static void *heap_malloc(struct vmm_heap_control *heap, virtual_size_t size)
{
    int      rc;
    uint64_t addr;

    if (!size) {
        return NULL;
    }

    rc = buddy_mem_alloc(&heap->ba, size, &addr);

    if (rc) {
        vmm_printf("%s: Failed to alloc size=%" PRISIZE " (error %d)\n", __func__, size, rc);
        return NULL;
    }

    return (void *)addr;
}

/**
 * @brief 从堆中分配指定大小的内存块
 * @param heap 堆结构体指针
 * @param ptr 通用指针
 * @return 大小值（字节）
 */
static virtual_size_t heap_alloc_size(struct vmm_heap_control *heap, const void *ptr)
{
    int      rc;
    uint64_t aaddr;
    uint64_t asize;

    BUG_ON(!ptr);
    BUG_ON(ptr < heap->mem_start);
    BUG_ON((heap->mem_start + heap->mem_size) <= ptr);

    rc = buddy_mem_find(&heap->ba, (uint64_t)ptr, &aaddr, NULL, &asize);

    if (rc) {
        return 0;
    }

    return asize - ((uint64_t)ptr - aaddr);
}

/**
 * @brief 堆内存释放
 * @param heap 堆结构体指针
 * @param ptr 通用指针
 */
static void heap_free(struct vmm_heap_control *heap, void *ptr)
{
    int rc;

    BUG_ON(!ptr);
    BUG_ON(ptr < heap->mem_start);
    BUG_ON((heap->mem_start + heap->mem_size) <= ptr);

    rc = buddy_mem_free(&heap->ba, (uint64_t)ptr);

    if (rc) {
        vmm_printf("%s: Failed to free ptr=%p (error %d)\n", __func__, ptr, rc);
    }
}

/**
 * @brief 堆物理地址转虚拟地址
 * @param heap 堆结构体指针
 * @param pa 待操作的物理地址
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int heap_physicalAddr_to_virtualAddr(struct vmm_heap_control *heap, physical_addr_t pa, virtual_addr_t *va)
{
    int rc = VMM_OK;

    if ((heap->heap_start_pa <= pa) && (pa < (heap->heap_start_pa + heap->heap_size))) {
        *va = (virtual_addr_t)heap->heap_start + (pa - heap->heap_start_pa);
    } else {
        rc = vmm_host_physicalAddr_to_virtualAddr(pa, va);
    }

    return rc;
}

/**
 * @brief 堆虚拟地址转物理地址
 * @param heap 堆结构体指针
 * @param va 待操作的虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int heap_virtualAddr_to_physicalAddr(struct vmm_heap_control *heap, virtual_addr_t va, physical_addr_t *pa)
{
    int rc = VMM_OK;

    if (((virtual_addr_t)heap->heap_start <= va) && (va < ((virtual_addr_t)heap->heap_start + heap->heap_size))) {
        *pa = (physical_addr_t)(va - (virtual_addr_t)heap->heap_start) + heap->heap_start_pa;
    } else {
        rc = vmm_host_virtualAddr_to_physicalAddr(va, pa);
    }

    return rc;
}

/**
 * @brief 堆 打印 状态
 * @param heap 堆结构体指针
 * @param cdev 字符设备指针
 * @param name 目标对象的名称
 * @return 状态值
 */
static int heap_print_state(struct vmm_heap_control *heap, vmm_char_device_t *cdev, const char *name)
{
    uint64_t idx;

    vmm_cdev_printf(cdev, "%s Heap State\n", name);

    for (idx = HEAP_MIN_BIN; idx <= HEAP_MAX_BIN; idx++) {
        if (idx < 10) {
            vmm_cdev_printf(cdev, "  [BLOCK %4dB]: ", 1 << idx);
        } else if (idx < 20) {
            vmm_cdev_printf(cdev, "  [BLOCK %4dK]: ", 1 << (idx - 10));
        } else {
            vmm_cdev_printf(cdev, "  [BLOCK %4dM]: ", 1 << (idx - 20));
        }

        vmm_cdev_printf(cdev, "%5lu area(s), %5lu free block(s)\n", buddy_bins_area_count(&heap->ba, idx), buddy_bins_block_count(&heap->ba, idx));
    }

    vmm_cdev_printf(cdev, "%s Heap House-Keeping State\n", name);
    vmm_cdev_printf(cdev, "  Buddy Areas: %lu free out of %lu\n", buddy_hk_area_free(&heap->ba), buddy_hk_area_total(&heap->ba));

    return VMM_OK;
}

/**
 * @brief 堆子系统初始化
 * @param heap 堆结构体指针
 * @param use_huge_page 是否使用大页标志
 * @param is_normal 是否为普通节点
 * @param size 数据大小（字节数）
 * @param memory_flags 标志位
 * @return 状态值
 */
static int heap_init(struct vmm_heap_control *heap, bool use_huge_page, bool is_normal, virtual_size_t size, uint32_t memory_flags)
{
    int      rc       = VMM_OK;
    uint32_t hp_shift = vmm_host_huge_page_shift();

    if (!size) {
        return VMM_ERR_INVALID;
    }

    if (use_huge_page) {
        size = roundup2_order_size(size, hp_shift);
    } else {
        size = roundup2_order_size(size, VMM_PAGE_SHIFT);
    }

    memset(heap, 0, sizeof(*heap));

    heap->heap_size = size;

    if (use_huge_page) {
        heap->heap_start = (void *)vmm_host_alloc_huge_pages((heap->heap_size >> hp_shift), memory_flags);
    } else {
        heap->heap_start = (void *)vmm_host_alloc_pages(VMM_SIZE_TO_PAGE(heap->heap_size), memory_flags);
    }

    if (!heap->heap_start) {
        return VMM_ERR_NOMEM;
    }

    rc = vmm_host_virtualAddr_to_physicalAddr((virtual_addr_t)heap->heap_start, &heap->heap_start_pa);

    if (rc) {
        goto fail_free_pages;
    }

    /* 12.5 percent for house-keeping */
    heap->hk_size = (heap->heap_size) / 8;

    /* Always have book keeping area for
     * non-normal heaps in normal heap
     */
    if (is_normal) {
        heap->hk_start  = heap->heap_start;
        heap->mem_start = heap->heap_start + heap->hk_size;
        heap->mem_size  = heap->heap_size - heap->hk_size;
    } else {
        heap->hk_start = vmm_malloc(heap->hk_size);

        if (!heap->hk_start) {
            rc = VMM_ERR_NOMEM;
            goto fail_free_pages;
        }

        heap->mem_start = heap->heap_start;
        heap->mem_size  = heap->heap_size;
    }

    rc = buddy_allocator_init(&heap->ba, heap->hk_start, heap->hk_size, (uint64_t)heap->mem_start, heap->mem_size, HEAP_MIN_BIN, HEAP_MAX_BIN);

    if (rc) {
        goto fail_free_pages;
    }

    return VMM_OK;

fail_free_pages:
    vmm_host_free_pages((virtual_addr_t)heap->heap_start, VMM_SIZE_TO_PAGE(heap->heap_size));
    return rc;
}

/**
 * @brief malloc
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_malloc(virtual_size_t size)
{
    return heap_malloc(&normal_heap, size);
}

/**
 * @brief zalloc
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_zalloc(virtual_size_t size)
{
    void *ret = vmm_malloc(size);

    if (ret) {
        memset(ret, 0, size);
    }

    return ret;
}

/**
 * @brief calloc
 * @param element_count 元素数量
 * @param element_size 单个元素大小（字节）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_calloc(virtual_size_t element_count, virtual_size_t element_size)
{
    if (!element_count) {
        element_count = 1;
    }

    return vmm_zalloc(element_count * element_size);
}

/**
 * @brief strdup
 * @param str 待处理的字符串
 * @return 成功返回分配的内存指针，失败返回NULL
 */
char *vmm_strdup(const char *str)
{
    char  *tstr;
    size_t tlen;

    if (!str) {
        return NULL;
    }

    tlen = strlen(str);
    tstr = vmm_zalloc(tlen + 1);

    if (!tstr) {
        return NULL;
    }

    strcpy(tstr, str);
    tstr[tlen] = '\0';

    return tstr;
}

/**
 * @brief 分配指定大小的内存
 * @param ptr 通用指针
 * @return 大小值（字节）
 */
virtual_size_t vmm_alloc_size(const void *ptr)
{
    return heap_alloc_size(&normal_heap, ptr);
}

/**
 * @brief free
 * @param ptr 通用指针
 */
void vmm_free(void *ptr)
{
    heap_free(&normal_heap, ptr);
}

/**
 * @brief 获取普通堆的起始虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_addr_t vmm_normal_heap_start_va(void)
{
    return (virtual_addr_t)normal_heap.heap_start;
}

/**
 * @brief 普通 堆 大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_normal_heap_size(void)
{
    return (virtual_size_t)normal_heap.heap_size;
}

/**
 * @brief 获取普通堆管理元数据的大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_normal_heap_hksize(void)
{
    return normal_heap.hk_size;
}

/**
 * @brief 获取普通堆的空闲内存大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_normal_heap_free_size(void)
{
    return buddy_bins_free_space(&normal_heap.ba);
}

/**
 * @brief 将普通堆的分配状态输出到字符设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_normal_heap_print_state(vmm_char_device_t *cdev)
{
    return heap_print_state(&normal_heap, cdev, "Normal");
}

/**
 * @brief 初始化堆
 * @return 状态值
 */
int __init vmm_heap_init(void)
{
    int rc;

    /*
     * Always create normal heap first as book keeping area for other heaps
     * is allocated from normal heap
     */

    /* Create Normal heap */
    rc = heap_init(&normal_heap, TRUE, TRUE, vmm_host_virtual_address_pool_size() / CONFIG_HEAP_SIZE_FACTOR, VMM_MEMORY_FLAGS_NORMAL);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}

/**
 * @brief DMA malloc
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_dma_malloc(virtual_size_t size)
{
    return heap_malloc(&dma_heap, size);
}

/**
 * @brief DMA zalloc
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_dma_zalloc(virtual_size_t size)
{
    void *ret = vmm_dma_malloc(size);

    if (ret) {
        memset(ret, 0, size);
    }

    return ret;
}

/**
 * @brief 分配DMA物理内存并清零
 * @param size 数据大小（字节数）
 * @param paddr 物理地址值
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_dma_zalloc_phy(virtual_size_t size, physical_addr_t *paddr)
{
    int        ret;
    void      *cpu_addr;
    dma_addr_t dma_addr = 0;

#if defined(CONFIG_IOMMU)
    /* TODO: Manage cases with IOMMU */
    BUG();
#endif /* defined(CONFIG_IOMMU) */

    cpu_addr = vmm_dma_zalloc(size);

    if (!cpu_addr) {
        return cpu_addr;
    }

    ret = vmm_host_virtualAddr_to_physicalAddr((virtual_addr_t)cpu_addr, &dma_addr);

    if (VMM_OK == ret) {
        *paddr = dma_addr;
    }

    return cpu_addr;
}

/**
 * @brief DMA物理地址转虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回分配结果，失败返回错误码
 */
virtual_addr_t vmm_dma_physicalAddr_to_virtualAddr(physical_addr_t pa)
{
    int            rc;
    virtual_addr_t va = 0x0;

    rc                = heap_physicalAddr_to_virtualAddr(&dma_heap, pa, &va);

    if (rc != VMM_OK) {
        BUG_ON(1);
    }

    return va;
}

/**
 * @brief DMA虚拟地址转物理地址
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
physical_addr_t vmm_dma_virtualAddr_to_physicalAddr(virtual_addr_t va)
{
    int             rc;
    physical_addr_t pa = 0x0;

    rc                 = heap_virtualAddr_to_physicalAddr(&dma_heap, va, &pa);

    if (rc != VMM_OK) {
        BUG_ON(1);
    }

    return pa;
}

/**
 * @brief is  DMA
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_is__dma(void *va)
{
    return ((va > dma_heap.heap_start) && (va < (dma_heap.heap_start + dma_heap.heap_size)));
}

/**
 * @brief 将DMA缓冲区同步到设备（CPU写完成后交给设备读取）
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param dir 方向标识
 */
void vmm_dma_sync_for_device(virtual_addr_t start, virtual_addr_t end, enum vmm_dma_direction dir)
{
    if (dir == DMA_FROM_DEVICE) {
        vmm_inv_dcache_range(start, end);
        vmm_inv_outer_cache_range(start, end);
    } else {
        vmm_clean_dcache_range(start, end);
        vmm_clean_outer_cache_range(start, end);
    }
}

/**
 * @brief 将DMA缓冲区同步到CPU（设备写完成后交给CPU读取）
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param dir 方向标识
 */
void vmm_dma_sync_for_cpu(virtual_addr_t start, virtual_addr_t end, enum vmm_dma_direction dir)
{
    if (dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL) {
        /* Cache prefetching */
        vmm_inv_dcache_range(start, end);
        vmm_inv_outer_cache_range(start, end);
    }
}

/**
 * @brief DMA 映射
 * @param vaddr 虚拟地址值
 * @param size 数据大小（字节数）
 * @param dir 方向标识
 * @return 成功返回VMM_OK，失败返回错误码
 */
physical_addr_t vmm_dma_map(virtual_addr_t vaddr, virtual_size_t size, enum vmm_dma_direction dir)
{
    vmm_dma_sync_for_device(vaddr, vaddr + size, dir);

    return vmm_dma_virtualAddr_to_physicalAddr(vaddr);
}

/**
 * @brief DMA unmap
 * @param dma_addr DMA物理地址
 * @param size 数据大小（字节数）
 * @param dir 方向标识
 */
void vmm_dma_unmap(physical_addr_t dma_addr, physical_size_t size, enum vmm_dma_direction dir)
{
    virtual_addr_t vaddr = vmm_dma_physicalAddr_to_virtualAddr((physical_addr_t)dma_addr);

    vmm_dma_sync_for_cpu(vaddr, vaddr + size, dir);
}

/**
 * @brief 从DMA堆中分配指定大小的内存
 * @param ptr 通用指针
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_alloc_size(const void *ptr)
{
    return heap_alloc_size(&dma_heap, ptr);
}

/**
 * @brief 释放DMA
 * @param ptr 通用指针
 */
void vmm_dma_free(void *ptr)
{
    heap_free(&dma_heap, ptr);
}

/**
 * @brief 获取DMA堆的起始虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_addr_t vmm_dma_heap_start_va(void)
{
    return (virtual_addr_t)dma_heap.heap_start;
}

/**
 * @brief 获取DMA堆的总大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_heap_size(void)
{
    return (virtual_size_t)dma_heap.heap_size;
}

/**
 * @brief 获取DMA堆管理元数据的大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_heap_hksize(void)
{
    return dma_heap.hk_size;
}

/**
 * @brief 获取DMA堆的空闲内存大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_heap_free_size(void)
{
    return buddy_bins_free_space(&dma_heap.ba);
}

/**
 * @brief 将DMA堆的分配状态输出到字符设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_dma_heap_print_state(vmm_char_device_t *cdev)
{
    return heap_print_state(&dma_heap, cdev, "DMA");
}

/**
 * @brief 初始化DMA堆
 * @return 状态值
 */
int __init vmm_dma_heap_init(void)
{
    int rc;

    /* Create DMA heap */
    rc = heap_init(&dma_heap, FALSE, FALSE, vmm_host_virtual_address_pool_size() / CONFIG_DMA_HEAP_SIZE_FACTOR, VMM_MEMORY_FLAGS_DMA_NONCOHERENT);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}
