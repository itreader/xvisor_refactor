/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vmm_virtual_display.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟显示子系统源文件
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vio/vmm_virtual_display.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>

#define MODULE_DESC      "Virtual Display Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VDISPLAY_IPRIORITY)
#define MODULE_INIT      vmm_virtual_display_init
#define MODULE_EXIT      vmm_virtual_display_exit

/**
 * @brief 虚拟显示控制结构（内部），管理显示设备的运行时状态
 */
struct vmm_virtual_display_ctrl {
    vmm_mutex_t                   vdis_list_lock; /**< vdis_list_lock成员 */
    double_list_t                 vdis_list; /**< vdis_list成员 */
    vmm_blocking_notifier_chain_t notifier_chain; /**< 通知器链 */
};

static struct vmm_virtual_display_ctrl vdctrl;

/**
 * @brief 注册虚拟显示客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&vdctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_register_client);

/**
 * @brief 注销虚拟显示客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&vdctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_unregister_client);

/**
 * @brief 初始化默认像素格式
 * @param pf 页帧结构体指针
 * @param bpp 每像素位数
 */
void vmm_pixelformat_init_default(struct vmm_pixelformat *pf, int bpp)
{
    if (!pf) {
        return;
    }

    memset(pf, 0x00, sizeof(struct vmm_pixelformat));

    pf->bits_per_pixel  = bpp;
    pf->bytes_per_pixel = DIV_ROUND_UP(bpp, 8);
    pf->depth           = bpp == 32 ? 24 : bpp;

    switch (bpp) {
        case 15:
            pf->bits_per_pixel = 16;
            pf->rmask          = 0x00007c00;
            pf->gmask          = 0x000003E0;
            pf->bmask          = 0x0000001F;
            pf->rmax           = 31;
            pf->gmax           = 31;
            pf->bmax           = 31;
            pf->rshift         = 10;
            pf->gshift         = 5;
            pf->bshift         = 0;
            pf->rbits          = 5;
            pf->gbits          = 5;
            pf->bbits          = 5;
            break;

        case 16:
            pf->rmask  = 0x0000F800;
            pf->gmask  = 0x000007E0;
            pf->bmask  = 0x0000001F;
            pf->rmax   = 31;
            pf->gmax   = 63;
            pf->bmax   = 31;
            pf->rshift = 11;
            pf->gshift = 5;
            pf->bshift = 0;
            pf->rbits  = 5;
            pf->gbits  = 6;
            pf->bbits  = 5;
            break;

        case 24:
            pf->rmask  = 0x00FF0000;
            pf->gmask  = 0x0000FF00;
            pf->bmask  = 0x000000FF;
            pf->rmax   = 255;
            pf->gmax   = 255;
            pf->bmax   = 255;
            pf->rshift = 16;
            pf->gshift = 8;
            pf->bshift = 0;
            pf->rbits  = 8;
            pf->gbits  = 8;
            pf->bbits  = 8;
            break;

        case 32:
            pf->rmask  = 0x00FF0000;
            pf->gmask  = 0x0000FF00;
            pf->bmask  = 0x000000FF;
            pf->rmax   = 255;
            pf->gmax   = 255;
            pf->bmax   = 255;
            pf->rshift = 16;
            pf->gshift = 8;
            pf->bshift = 0;
            pf->rbits  = 8;
            pf->gbits  = 8;
            pf->bbits  = 8;
            break;

        default:
            break;
    };
}

VMM_ERR_XPORT_SYMBOL(vmm_pixelformat_init_default);

/**
 * @brief 初始化不同端像素格式
 * @param pf 页帧结构体指针
 * @param bpp 每像素位数
 */
void vmm_pixelformat_init_different_endian(struct vmm_pixelformat *pf, int bpp)
{
    if (!pf) {
        return;
    }

    memset(pf, 0x00, sizeof(struct vmm_pixelformat));

    pf->bits_per_pixel  = bpp;
    pf->bytes_per_pixel = DIV_ROUND_UP(bpp, 8);
    pf->depth           = bpp == 32 ? 24 : bpp;

    switch (bpp) {
        case 24:
            pf->rmask  = 0x000000FF;
            pf->gmask  = 0x0000FF00;
            pf->bmask  = 0x00FF0000;
            pf->rmax   = 255;
            pf->gmax   = 255;
            pf->bmax   = 255;
            pf->rshift = 0;
            pf->gshift = 8;
            pf->bshift = 16;
            pf->rbits  = 8;
            pf->gbits  = 8;
            pf->bbits  = 8;
            break;

        case 32:
            pf->rmask  = 0x0000FF00;
            pf->gmask  = 0x00FF0000;
            pf->bmask  = 0xFF000000;
            pf->amask  = 0x00000000;
            pf->amax   = 255;
            pf->rmax   = 255;
            pf->gmax   = 255;
            pf->bmax   = 255;
            pf->ashift = 0;
            pf->rshift = 8;
            pf->gshift = 16;
            pf->bshift = 24;
            pf->rbits  = 8;
            pf->gbits  = 8;
            pf->bbits  = 8;
            pf->abits  = 8;
            break;

        default:
            break;
    };
}

VMM_ERR_XPORT_SYMBOL(vmm_pixelformat_init_different_endian);

/**
 * @brief 更新显示表面
 */
void vmm_surface_update(
    struct vmm_surface *s, struct vmm_guest *guest, physical_addr_t src_gphys, int cols, int rows, int src_width, int dst_row_pitch,
    int dst_col_pitch, void (*fn)(struct vmm_surface *s, void *private, uint8_t *dst, const uint8_t *src, int width, int dststep), void *fn_private,
    int *first_row, int *last_row)
{
#define CHUNK_SIZE 256
    uint32_t len; /**< 长度 */
    int      i, j; /**< j */
    int      chunk_len, chunk_cols, chunk_dst_row_pitch; /**< chunk_dst_row_pitch成员 */
    uint8_t *dst, chunk[CHUNK_SIZE]; /**< chunk成员 */

    /* Sanity check */
    if (!s || !guest || !first_row || !last_row) {
        return;
    }

    if ((rows <= 0) || (cols <= 0)) {
        return;
    }

    if ((src_width <= 0) || (dst_row_pitch == 0)) {
        return;
    }

    /* Clip rows and cols to fit the surface */
    rows = min(rows, vmm_surface_height(s)); /**< vmm_surface_height(s))成员 */
    cols = min(cols, vmm_surface_width(s)); /**< vmm_surface_width(s))成员 */

    /* Ensure that first_row is within limit */
    if ((*first_row < 0) || (rows <= *first_row)) {
        return;
    }

    /* Determine dst pointer */
    dst = vmm_surface_data(s); /**< vmm_surface_data(s)成员 */

    if (dst_col_pitch < 0) {
        dst -= dst_col_pitch * (cols - 1); /**< 1) */
    }

    if (dst_row_pitch < 0) {
        dst -= dst_row_pitch * (rows - 1); /**< 1) */
    }

    dst += (*first_row) * dst_row_pitch; /**< first_row成员 */

    /* Determine src guest physical address */
    src_gphys += (*first_row) * src_width; /**< first_row成员 */

    /* Update surface data in chunks */
    for (i = *first_row; i < rows; i++) {
        j = 0; /**< 0 */

        while (j < src_width) {
            chunk_len           = min(src_width - j, CHUNK_SIZE); /**< CHUNK_SIZE)成员 */
            chunk_cols          = sdiv32((chunk_len * cols), src_width); /**< src_width)成员 */
            chunk_len           = sdiv32((chunk_cols * src_width), cols); /**< cols)成员 */
            chunk_dst_row_pitch = sdiv32((chunk_len * dst_row_pitch), src_width); /**< src_width)成员 */

            len                 = vmm_guest_memory_read(guest, src_gphys, chunk, chunk_len, FALSE); /**< FALSE)成员 */

            if (len != chunk_len) {
                goto next_chunk; /**< next_chunk成员 */
            }

            fn(s, fn_private, dst, chunk, chunk_cols, dst_col_pitch); /**< dst_col_pitch)成员 */

        next_chunk:
            j += chunk_len; /**< chunk_len成员 */
            src_gphys += chunk_len; /**< chunk_len成员 */
            dst += chunk_dst_row_pitch; /**< chunk_dst_row_pitch成员 */
        }
    }

    *last_row = i;
}

VMM_ERR_XPORT_SYMBOL(vmm_surface_update);

/**
 * @brief 初始化surface
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_surface_init(
    struct vmm_surface *s, const char *name, void *data, uint32_t data_size, int height, int width, uint32_t flags, struct vmm_pixelformat *pf,
    const struct vmm_surface_ops *ops, void *private)
{
    if (!s || !name || !data || !pf || !ops) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    if (height <= 0 || width <= 0) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (data_size < (width * height * pf->bytes_per_pixel)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    INIT_LIST_HEAD(&s->head);

    if (strlcpy(s->name, name, sizeof(s->name)) >= sizeof(s->name)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    s->data      = data; /**< 数据 */
    s->data_size = data_size; /**< data_size成员 */
    s->height    = height; /**< 高度 */
    s->width     = width; /**< 宽度 */
    s->flags     = flags; /**< 标志位 */
#ifdef CONFIG_CPU_BE
    s->flags |= VMM_SURFACE_BIG_ENDIAN_FLAG; /**< VMM_SURFACE_BIG_ENDIAN_FLAG成员 */
#endif
    memcpy(&s->pf, pf, sizeof(struct vmm_pixelformat)); /**< vmm_pixelformat))成员 */
    s->ops     = ops; /**< 操作集 */
    s->private = NULL; /**< NULL成员 */

    return VMM_OK; /**< VMM_OK成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_surface_init);

struct vmm_surface *vmm_surface_alloc(
    const char *name, void *data, uint32_t data_size, int height, int width, uint32_t flags, struct vmm_pixelformat *pf,
    const struct vmm_surface_ops *ops, void *private)
{
    struct vmm_surface *s; /**< s */

    s = vmm_zalloc(sizeof(struct vmm_surface)); /**< vmm_surface))成员 */

    if (!s) {
        return NULL; /**< NULL成员 */
    }

    if (vmm_surface_init(s, name, data, data_size, height, width, flags | VMM_SURFACE_ALLOCED_FLAG, pf, ops, private)) {
        vmm_free(s);
        return NULL; /**< NULL成员 */
    }

    return s; /**< s */
}

VMM_ERR_XPORT_SYMBOL(vmm_surface_alloc);

/**
 * @brief 释放surface
 * @param s 字符串或数据指针
 */
void vmm_surface_free(struct vmm_surface *s)
{
    if (!s) {
        return;
    }

    if (!(s->flags & VMM_SURFACE_ALLOCED_FLAG)) {
        return;
    }

    vmm_free(s);
}

VMM_ERR_XPORT_SYMBOL(vmm_surface_free);

/**
 * @brief 获取虚拟显示的像素数据
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_get_pixeldata(
    struct vmm_virtual_display *vdis, struct vmm_pixelformat *pf, uint32_t *rows, uint32_t *cols, physical_addr_t *pa)
{
    if (!vdis || !pf || !rows || !cols || !pa) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    if (vdis->ops && vdis->ops->gfx_pixeldata) {
        return vdis->ops->gfx_pixeldata(vdis, pf, rows, cols, pa); /**< pa)成员 */
    }

    return VMM_ERR_OPNOTSUPP; /**< VMM_ERR_OPNOTSUPP成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_get_pixeldata);

/**
 * @brief 触发虚拟显示的单次更新
 * @param vdis 虚拟显示设备指针
 * @param s 字符串或数据指针
 */
void vmm_virtual_display_one_update(struct vmm_virtual_display *vdis, struct vmm_surface *s)
{
    if (!vdis || !s) {
        return;
    }

    if (vdis->ops && vdis->ops->gfx_update) {
        vdis->ops->gfx_update(vdis, s);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_one_update);

/**
 * @brief 更新虚拟显示内容
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_update(struct vmm_virtual_display *vdis)
{
    irq_flags_t         flags;
    struct vmm_surface *s;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(s, &vdis->surface_list, head)
    {
        vmm_virtual_display_one_update(vdis, s);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_update);

/**
 * @brief 使虚拟显示的显示内容失效并触发刷新
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_invalidate(struct vmm_virtual_display *vdis)
{
    if (!vdis) {
        return;
    }

    if (vdis->ops && vdis->ops->invalidate) {
        vdis->ops->invalidate(vdis);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_invalidate);

/**
 * @brief 更新虚拟显示的文本内容
 * @param vdis 虚拟显示设备指针
 * @param chardata 字符数据值
 */
void vmm_virtual_display_text_update(struct vmm_virtual_display *vdis, uint64_t *chardata)
{
    if (!vdis || !chardata) {
        return;
    }

    if (vdis->ops && vdis->ops->text_update) {
        vdis->ops->text_update(vdis, chardata);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_text_update);

/**
 * @brief   刷新显示表面内容
 * @param sf 状态标志
 */
static void __surface_refresh(struct vmm_surface *sf)
{
    if (sf->ops && sf->ops->refresh) {
        sf->ops->refresh(sf);
    }
}

/**
 * @brief 刷新虚拟显示表面
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_surface_refresh(struct vmm_virtual_display *vdis)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_refresh(sf);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_refresh);

/**
 * @brief 清除图形表面的内容
 * @param sf 状态标志
 */
static void __surface_gfx_clear(struct vmm_surface *sf)
{
    if (sf->ops && sf->ops->gfx_clear) {
        sf->ops->gfx_clear(sf);
    }
}

/**
 * @brief 清除虚拟显示的图形表面
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_surface_gfx_clear(struct vmm_virtual_display *vdis)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_gfx_clear(sf);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_gfx_clear);

/**
 * @brief 更新图形表面的内容
 * @param sf 状态标志
 * @param x X坐标值
 * @param y Y坐标值
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
static void __surface_gfx_update(struct vmm_surface *sf, int x, int y, int w, int h)
{
    int width  = vmm_surface_width(sf);
    int height = vmm_surface_height(sf);

    x          = max(x, 0);
    y          = max(y, 0);
    x          = min(x, width);
    y          = min(y, height);
    w          = min(w, width - x);
    h          = min(h, height - y);

    if (sf->ops && sf->ops->gfx_update) {
        sf->ops->gfx_update(sf, x, y, w, h);
    }
}

/**
 * @brief 更新虚拟显示的图形表面
 * @param vdis 虚拟显示设备指针
 * @param x X坐标值
 * @param y Y坐标值
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_gfx_update(struct vmm_virtual_display *vdis, int x, int y, int w, int h)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_gfx_update(sf, x, y, w, h);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_gfx_update);

/**
 * @brief   调整图形显示表面大小
 * @param s 字符串或数据指针
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
static void __surface_gfx_resize(struct vmm_surface *s, int w, int h)
{
    w = max(w, 0);
    h = max(h, 0);

    if (s->ops && s->ops->gfx_resize) {
        s->ops->gfx_resize(s, w, h);
    }
}

/**
 * @brief 调整虚拟显示图形表面的尺寸
 * @param vdis 虚拟显示设备指针
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_gfx_resize(struct vmm_virtual_display *vdis, int w, int h)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_gfx_resize(sf, w, h);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_gfx_resize);

/**
 * @brief 复制图形表面数据
 * @param s 字符串或数据指针
 * @param src_x 源X坐标
 * @param src_y 源Y坐标
 * @param dst_x 目标X坐标
 * @param dst_y 目标Y坐标
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
static void __surface_gfx_copy(struct vmm_surface *s, int src_x, int src_y, int dst_x, int dst_y, int w, int h)
{
    int src_w;
    int src_h;
    int dst_w;
    int dst_h;
    int width  = vmm_surface_width(s);
    int height = vmm_surface_height(s);

    src_x      = max(src_x, 0);
    src_y      = max(src_y, 0);
    src_x      = min(src_x, width);
    src_y      = min(src_y, height);
    src_w      = min(w, width - src_x);
    src_h      = min(h, height - src_y);

    dst_x      = max(dst_x, 0);
    dst_y      = max(dst_y, 0);
    dst_x      = min(dst_x, width);
    dst_y      = min(dst_y, height);
    dst_w      = min(w, width - dst_x);
    dst_h      = min(h, height - dst_y);

    w          = min(src_w, dst_w);
    h          = min(src_h, dst_h);

    if (s->ops && s->ops->gfx_copy) {
        s->ops->gfx_copy(s, src_x, src_y, dst_x, dst_y, w, h);
    } else if (s->ops && s->ops->gfx_update) {
        /* FIXME: */
        s->ops->gfx_update(s, dst_x, dst_y, w, h);
    }
}

/**
 * @brief 复制虚拟显示的图形表面数据
 * @param vdis 虚拟显示设备指针
 * @param src_x 源X坐标
 * @param src_y 源Y坐标
 * @param dst_x 目标X坐标
 * @param dst_y 目标Y坐标
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_gfx_copy(struct vmm_virtual_display *vdis, int src_x, int src_y, int dst_x, int dst_y, int w, int h)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_gfx_copy(sf, src_x, src_y, dst_x, dst_y, w, h);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_gfx_copy);

/**
 * @brief 清除文本文字表面的内容
 * @param s 字符串或数据指针
 */
static void __surface_text_clear(struct vmm_surface *s)
{
    if (s->ops && s->ops->text_clear) {
        s->ops->text_clear(s);
    }
}

/**
 * @brief 清除虚拟显示的文本文字表面
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_surface_text_clear(struct vmm_virtual_display *vdis)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_text_clear(sf);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_text_clear);

/**
 * @brief   设置文本显示表面光标
 * @param s 字符串或数据指针
 * @param x X坐标值
 * @param y Y坐标值
 */
static void __surface_text_cursor(struct vmm_surface *s, int x, int y)
{
    if (s->ops && s->ops->text_cursor) {
        s->ops->text_cursor(s, x, y);
    }
}

/**
 * @brief 设置虚拟显示文本表面的光标位置
 * @param vdis 虚拟显示设备指针
 * @param x X坐标值
 * @param y Y坐标值
 */
void vmm_virtual_display_surface_text_cursor(struct vmm_virtual_display *vdis, int x, int y)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_text_cursor(sf, x, y);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_text_cursor);

/**
 * @brief 更新文本文字表面的内容
 * @param s 字符串或数据指针
 * @param x X坐标值
 * @param y Y坐标值
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
static void __surface_text_update(struct vmm_surface *s, int x, int y, int w, int h)
{
    if (s->ops && s->ops->text_update) {
        s->ops->text_update(s, x, y, w, h);
    }
}

/**
 * @brief 更新虚拟显示的文本文字表面
 * @param vdis 虚拟显示设备指针
 * @param x X坐标值
 * @param y Y坐标值
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_text_update(struct vmm_virtual_display *vdis, int x, int y, int w, int h)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_text_update(sf, x, y, w, h);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_text_update);

/**
 * @brief   调整文本显示表面大小
 * @param s 字符串或数据指针
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
static void __surface_text_resize(struct vmm_surface *s, int w, int h)
{
    if (s->ops && s->ops->text_resize) {
        s->ops->text_resize(s, w, h);
    }
}

/**
 * @brief 调整虚拟显示文本表面的尺寸
 * @param vdis 虚拟显示设备指针
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_text_resize(struct vmm_virtual_display *vdis, int w, int h)
{
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis) {
        return;
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        __surface_text_resize(sf, w, h);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_surface_text_resize);

/**
 * @brief 向虚拟显示添加表面
 * @param vdis 虚拟显示设备指针
 * @param s 字符串或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_add_surface(struct vmm_virtual_display *vdis, struct vmm_surface *s)
{
    bool                found;
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis || !s) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    sf    = NULL;
    found = FALSE;
    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        if (strncmp(s->name, sf->name, sizeof(s->name)) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
        return VMM_ERR_EXIST;
    }

    INIT_LIST_HEAD(&s->head);
    list_add_tail(&s->head, &vdis->surface_list);

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_add_surface);

/**
 * @brief 从虚拟显示删除表面
 * @param vdis 虚拟显示设备指针
 * @param s 字符串或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_del_surface(struct vmm_virtual_display *vdis, struct vmm_surface *s)
{
    bool                found;
    irq_flags_t         flags;
    struct vmm_surface *sf;

    if (!vdis || !s) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    sf    = NULL;
    found = FALSE;
    list_for_each_entry(sf, &vdis->surface_list, head)
    {
        if (strncmp(s->name, sf->name, sizeof(s->name)) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&sf->head);

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_del_surface);

struct vmm_virtual_display *vmm_virtual_display_create(const char *name, const struct vmm_virtual_display_ops *ops, void *private)
{
    bool                             found; /**< found成员 */
    struct vmm_virtual_display      *vdis; /**< vdis成员 */
    struct vmm_virtual_display_event event; /**< 事件 */

    if (!name || !ops) {
        return NULL; /**< NULL成员 */
    }

    vdis  = NULL; /**< NULL成员 */
    found = FALSE; /**< FALSE成员 */

    vmm_mutex_lock(&vdctrl.vdis_list_lock);

    list_for_each_entry(vdis, &vdctrl.vdis_list, head)
    {
        if (strcmp(name, vdis->name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&vdctrl.vdis_list_lock);
        return NULL; /**< NULL成员 */
    }

    vdis = vmm_malloc(sizeof(struct vmm_virtual_display)); /**< vmm_virtual_display))成员 */

    if (!vdis) {
        vmm_mutex_unlock(&vdctrl.vdis_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&vdis->head);

    if (strlcpy(vdis->name, name, sizeof(vdis->name)) >= sizeof(vdis->name)) {
        vmm_free(vdis);
        vmm_mutex_unlock(&vdctrl.vdis_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_SPIN_LOCK(&vdis->surface_list_lock);
    INIT_LIST_HEAD(&vdis->surface_list);
    vdis->ops     = ops; /**< 操作集 */
    vdis->private = private; /**< 私有数据 */

    list_add_tail(&vdis->head, &vdctrl.vdis_list); /**< &vdctrl.vdis_list)成员 */

    vmm_mutex_unlock(&vdctrl.vdis_list_lock);

    /* Broadcast create event */
    event.data = vdis; /**< vdis成员 */
    vmm_blocking_notifier_call(&vdctrl.notifier_chain, VMM_VDISPLAY_EVENT_CREATE, &event); /**< &event)成员 */

    return vdis; /**< vdis成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_create);

/**
 * @brief 销毁虚拟显示
 * @param vdis 虚拟显示设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_destroy(struct vmm_virtual_display *vdis)
{
    bool                             found;
    irq_flags_t                      flags;
    struct vmm_surface              *sf;
    struct vmm_virtual_display      *vd;
    struct vmm_virtual_display_event event;

    if (!vdis) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    /* Broadcast destroy event */
    event.data = vdis;
    vmm_blocking_notifier_call(&vdctrl.notifier_chain, VMM_VDISPLAY_EVENT_DESTROY, &event);

    vmm_spin_lock_irq_save(&vdis->surface_list_lock, flags);

    while (!list_empty(&vdis->surface_list)) {
        sf = list_first_entry(&vdis->surface_list, struct vmm_surface, head);
        list_del(&sf->head);
    }

    vmm_spin_unlock_irq_restore(&vdis->surface_list_lock, flags);

    vmm_mutex_lock(&vdctrl.vdis_list_lock);

    if (list_empty(&vdctrl.vdis_list)) {
        vmm_mutex_unlock(&vdctrl.vdis_list_lock);
        return VMM_ERR_FAIL;
    }

    vd    = NULL;
    found = FALSE;
    list_for_each_entry(vd, &vdctrl.vdis_list, head)
    {
        if (strcmp(vd->name, vdis->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&vdctrl.vdis_list_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&vd->head);
    vmm_free(vd);

    vmm_mutex_unlock(&vdctrl.vdis_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_destroy);

struct vmm_virtual_display *vmm_virtual_display_find(const char *name)
{
    bool                        found; /**< found成员 */
    struct vmm_virtual_display *vd; /**< vd */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    found = FALSE; /**< FALSE成员 */
    vd    = NULL; /**< NULL成员 */

    vmm_mutex_lock(&vdctrl.vdis_list_lock);

    list_for_each_entry(vd, &vdctrl.vdis_list, head)
    {
        if (strcmp(vd->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    vmm_mutex_unlock(&vdctrl.vdis_list_lock);

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    return vd; /**< vd */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_find);

/**
 * @brief 虚拟 显示 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_iterate(struct vmm_virtual_display *start, void *data, int (*fn)(struct vmm_virtual_display *vdis, void *data))
{
    int                         rc          = VMM_OK;
    bool                        start_found = (start) ? FALSE : TRUE;
    struct vmm_virtual_display *vd          = NULL;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&vdctrl.vdis_list_lock);

    list_for_each_entry(vd, &vdctrl.vdis_list, head)
    {
        if (!start_found) {
            if (start && start == vd) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vd, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&vdctrl.vdis_list_lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_iterate);

/**
 * @brief 获取虚拟显示的数量
 * @return 数量值
 */
uint32_t vmm_virtual_display_count(void)
{
    uint32_t                    retval = 0;
    struct vmm_virtual_display *vd;

    vmm_mutex_lock(&vdctrl.vdis_list_lock);

    list_for_each_entry(vd, &vdctrl.vdis_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&vdctrl.vdis_list_lock);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_display_count);

/**
 * @brief 初始化虚拟显示
 * @return 数量值
 */
static int __init vmm_virtual_display_init(void)
{
    memset(&vdctrl, 0, sizeof(vdctrl));

    INIT_MUTEX(&vdctrl.vdis_list_lock);
    INIT_LIST_HEAD(&vdctrl.vdis_list);
    BLOCKING_INIT_NOTIFIER_CHAIN(&vdctrl.notifier_chain);

    return VMM_OK;
}

/**
 * @brief 虚拟显示子系统退出清理
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_virtual_display_exit(void)
{
    /* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
