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
 * @file vmm_virtual_display.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟显示子系统头文件
 */

/* The virtual display subsystem has two important entites namely
 * vmm_virtual_display and vmm_surface.
 *
 * GUI rendering daemons (VNC daemon or FB daemon or ...) create
 * vmm_surface instance and add/bind it to a vmm_virtual_display instance.
 * More than one GUI rendering daemons can add their vmm_surface
 * instances to a single vmm_virtual_display instance. The GUI rendering
 * daemons will also use vmm_virtual_display_one_update() API to periodically
 * update/sync vmm_surface instance with vmm_virtual_display instance.
 *
 * Display (or framebuffer) emulators create vmm_virtual_display instance
 * to emulate a virtual display. The display emulator will also use
 * vmm_virtual_display_surface_xxx() APIs to give hints to vmm_surface
 * instances about changes in virtual display.
 */

#ifndef __VMM_VDISPLAY_H_
#define __VMM_VDISPLAY_H_

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_manager.h>
#include <vmm_notifier.h>
#include <vmm_types.h>

#define VMM_VDISPLAY_IPRIORITY     0

/* Notifier event when virtual display is created */
#define VMM_VDISPLAY_EVENT_CREATE  0x01
/* Notifier event when virtual display is destroyed */
#define VMM_VDISPLAY_EVENT_DESTROY 0x02

/** Representation of virtual input notifier event */
/**
 * @brief 虚拟显示事件，包含刷新和更新区域的坐标尺寸
 */
struct vmm_virtual_display_event {
    void *data; /**< 数据 */
};

/**
 * @brief 注册虚拟显示客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销虚拟显示客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_unregister_client(vmm_notifier_block_t *nb);

/** Representation of a pixel format */
/**
 * @brief 像素格式结构，定义颜色深度、分量掩码和字节序
 */
struct vmm_pixelformat {
    uint8_t  bits_per_pixel; /**< bits_per_pixel成员 */
    uint8_t  bytes_per_pixel; /**< bytes_per_pixel成员 */
    uint8_t  depth; /* color depth in bits */
    uint32_t rmask, gmask, bmask, amask; /**< amask成员 */
    uint8_t  rshift, gshift, bshift, ashift; /**< ashift成员 */
    uint8_t  rmax, gmax, bmax, amax; /**< 最大振幅 */
    uint8_t  rbits, gbits, bbits, abits; /**< abits成员 */
};

/**
 * @brief 初始化默认像素格式
 * @param pf 页帧结构体指针
 * @param bpp 每像素位数
 */
void vmm_pixelformat_init_default(struct vmm_pixelformat *pf, int bpp);

/**
 * @brief 初始化不同端像素格式
 * @param pf 页帧结构体指针
 * @param bpp 每像素位数
 */
void vmm_pixelformat_init_different_endian(struct vmm_pixelformat *pf, int bpp);

struct vmm_surface;

/** Representation of surface operations
 *  Note: All surface operations are optional.
 *  Note: All surface operations are usually called with the
 *  'surface_list_lock' of the associated virtual display held
 *  hence, we cannot sleep in these operations.
 *  Note: Typically, all surface operations (except copyto_data
 *  and copyfrom_data) should be used to schedule a background or
 *  bottom-half work.
 */
struct vmm_surface_ops {
    void (*write8)(struct vmm_surface *s, uint8_t *dst, uint8_t val); /**< write8成员 */
    uint8_t (*read8)(struct vmm_surface *s, uint8_t *src); /**< read8成员 */
    void (*write16)(struct vmm_surface *s, uint16_t *dst, uint16_t val); /**< write16成员 */
    uint16_t (*read16)(struct vmm_surface *s, uint16_t *src); /**< read16成员 */
    void (*write32)(struct vmm_surface *s, uint32_t *dst, uint16_t val); /**< write32成员 */
    uint16_t (*read32)(struct vmm_surface *s, uint32_t *src); /**< read32成员 */

    void (*refresh)(struct vmm_surface *s); /**< refresh成员 */

    void (*gfx_clear)(struct vmm_surface *s); /**< gfx_clear成员 */
    void (*gfx_update)(struct vmm_surface *s, int x, int y, int w, int h); /**< gfx_update成员 */
    void (*gfx_resize)(struct vmm_surface *s, int w, int h); /**< gfx_resize成员 */
    void (*gfx_copy)(struct vmm_surface *s, int src_x, int src_y, int dst_x, int dst_y, int w, int h); /**< gfx_copy成员 */

    void (*text_clear)(struct vmm_surface *s); /**< text_clear成员 */
    void (*text_cursor)(struct vmm_surface *s, int x, int y); /**< text_cursor成员 */
    void (*text_resize)(struct vmm_surface *s, int w, int h); /**< text_resize成员 */
    void (*text_update)(struct vmm_surface *s, int x, int y, int w, int h); /**< text_update成员 */
};

#define VMM_SURFACE_BIG_ENDIAN_FLAG 0x01
#define VMM_SURFACE_ALLOCED_FLAG    0x02

/** Representation of a surface */
/**
 * @brief 显示表面结构，管理帧缓冲区的像素数据、尺寸和格式
 */
struct vmm_surface {
    double_list_t                 head; /**< 链表头 */
    char                          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    void                         *data; /**< 数据 */
    uint32_t                      data_size; /**< data_size成员 */
    int                           height; /**< 高度 */
    int                           width; /**< 宽度 */
    uint32_t                      flags; /**< 标志位 */
    struct vmm_pixelformat        pf; /**< 页错误/预取 */
    const struct vmm_surface_ops *ops; /**< 操作集 */
    void *private; /**< 私有数据 */
};

/** Retrive private context of surface */
static inline void *vmm_surface_private(struct vmm_surface *s)
{
    return (s) ? s->private : NULL;
}

/**
 * @brief 向表面数据写入8位值
 */
static inline void vmm_surface_write8(struct vmm_surface *s, uint8_t *dst, uint8_t v)
{
    if (s && s->ops && s->ops->write8) {
        s->ops->write8(s, dst, v);
    } else {
        *dst = v;
    }
}

/**
 * @brief 从表面数据读取8位值
 */
static inline uint8_t vmm_surface_read8(struct vmm_surface *s, uint8_t *src)
{
    if (s && s->ops && s->ops->read8) {
        return s->ops->read8(s, src);
    } else {
        return *src;
    }
}

/**
 * @brief 向表面数据写入16位值
 */
static inline void vmm_surface_write16(struct vmm_surface *s, uint16_t *dst, uint16_t v)
{
    if (s && s->ops && s->ops->write16) {
        s->ops->write16(s, dst, v);
    } else {
        *dst = v;
    }
}

/**
 * @brief 从表面数据读取16位值
 */
static inline uint16_t vmm_surface_read16(struct vmm_surface *s, uint16_t *src)
{
    if (s && s->ops && s->ops->read16) {
        return s->ops->read16(s, src);
    } else {
        return *src;
    }
}

/**
 * @brief 向表面数据写入32位值
 */
static inline void vmm_surface_write32(struct vmm_surface *s, uint32_t *dst, uint16_t v)
{
    if (s && s->ops && s->ops->write32) {
        s->ops->write32(s, dst, v);
    } else {
        *dst = v;
    }
}

/**
 * @brief 从表面数据读取32位值
 */
static inline uint32_t vmm_surface_read32(struct vmm_surface *s, uint32_t *src)
{
    if (s && s->ops && s->ops->read32) {
        return s->ops->read32(s, src);
    } else {
        return *src;
    }
}

/**
 * @brief 从客户内存更新表面数据
 */
void vmm_surface_update(
    struct vmm_surface *s, struct vmm_guest *guest, physical_addr_t gphys, int cols, /* Width in pixels. */
    int rows,                                                                        /* Height in pixels. */
    int src_width,                                                                   /* Length of source line, in bytes. */
    int dest_row_pitch,                                                              /* Bytes between adjacent horizontal output pixels. */
    int dest_col_pitch,                                                              /* Bytes between adjacent vertical output pixels. */
    void (*fn)(struct vmm_surface *s, void *private, uint8_t *dst, const uint8_t *src, int width, int deststep), void *fn_private,
    int *first_row,                                                                  /* Input and output. */
    int *last_row);                                                                  /* Output only. */

/**
 * @brief 初始化显示表面
 */
int vmm_surface_init(
    struct vmm_surface *s, const char *name, void *data, uint32_t data_size, int height, int width, uint32_t flags, struct vmm_pixelformat *pf,
    const struct vmm_surface_ops *ops, void *private);

/** Alloc a new surface */
struct vmm_surface *vmm_surface_alloc(
    const char *name, void *data, uint32_t data_size, int height, int width, uint32_t flags, struct vmm_pixelformat *pf,
    const struct vmm_surface_ops *ops, void *private);

/**
 * @brief 释放surface
 * @param s 字符串或数据指针
 */
void vmm_surface_free(struct vmm_surface *s);

/**
 * @brief 获取给定表面的行步长
 */
static inline int vmm_surface_stride(struct vmm_surface *s)
{
    return (s) ? s->width * s->pf.bytes_per_pixel : 0;
}

/** Retrive data pointer of given surface */
static inline void *vmm_surface_data(struct vmm_surface *s)
{
    return (s) ? s->data : NULL;
}

/**
 * @brief 获取给定表面的宽度
 */
static inline int vmm_surface_width(struct vmm_surface *s)
{
    return (s) ? s->width : 0;
}

/**
 * @brief 获取给定表面的高度
 */
static inline int vmm_surface_height(struct vmm_surface *s)
{
    return (s) ? s->height : 0;
}

/**
 * @brief 获取给定表面的每像素位数
 */
static inline int vmm_surface_bits_per_pixel(struct vmm_surface *s)
{
    return (s) ? s->pf.bits_per_pixel : 0;
}

/**
 * @brief 获取给定表面的每像素字节数
 */
static inline int vmm_surface_bytes_per_pixel(struct vmm_surface *s)
{
    return (((s) ? s->pf.bits_per_pixel : 0) + 7) / 8;
}

struct vmm_virtual_display;

/** Representation of a virtual display operations */
/**
 * @brief 虚拟显示操作接口，定义屏幕更新和刷新回调
 */
struct vmm_virtual_display_ops {
    void (*invalidate)(struct vmm_virtual_display *vdis); /**< invalidate成员 */
    int (*gfx_pixeldata)(struct vmm_virtual_display *vdis, struct vmm_pixelformat *pf, uint32_t *rows, uint32_t *cols, physical_addr_t *pa); /**< gfx_pixeldata成员 */
    void (*gfx_update)(struct vmm_virtual_display *vdis, struct vmm_surface *s); /**< gfx_update成员 */
    void (*text_update)(struct vmm_virtual_display *vdis, uint64_t *text); /**< text_update成员 */
};

/** Representation of a virtual display */
/**
 * @brief 虚拟显示设备，绑定显示表面和操作回调
 */
struct vmm_virtual_display {
    double_list_t                         head; /**< 链表头 */
    char                                  name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    vmm_spinlock_t                        surface_list_lock; /**< surface_list_lock成员 */
    double_list_t                         surface_list; /**< surface_list成员 */
    const struct vmm_virtual_display_ops *ops; /**< 操作集 */
    void *private; /**< 私有数据 */
};

/**
 * @brief 获取给定虚拟显示的像素格式和主机物理地址
 */
int vmm_virtual_display_get_pixeldata(
    struct vmm_virtual_display *vdis, struct vmm_pixelformat *pf, uint32_t *rows, uint32_t *cols, physical_addr_t *pa);

/**
 * @brief 触发虚拟显示的单次更新
 * @param vdis 虚拟显示设备指针
 * @param s 字符串或数据指针
 */
void vmm_virtual_display_one_update(struct vmm_virtual_display *vdis, struct vmm_surface *s);

/**
 * @brief 更新虚拟显示内容
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_update(struct vmm_virtual_display *vdis);

/**
 * @brief 使虚拟显示的显示内容失效并触发刷新
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_invalidate(struct vmm_virtual_display *vdis);

/**
 * @brief 更新虚拟显示的文本内容
 * @param vdis 虚拟显示设备指针
 * @param chardata 字符数据值
 */
void vmm_virtual_display_text_update(struct vmm_virtual_display *vdis, uint64_t *chardata);

/**
 * @brief 刷新虚拟显示表面
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_surface_refresh(struct vmm_virtual_display *vdis);

/**
 * @brief 清除虚拟显示的图形表面
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_surface_gfx_clear(struct vmm_virtual_display *vdis);

/**
 * @brief 更新虚拟显示的图形表面
 * @param vdis 虚拟显示设备指针
 * @param x X坐标值
 * @param y Y坐标值
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_gfx_update(struct vmm_virtual_display *vdis, int x, int y, int w, int h);

/**
 * @brief 调整虚拟显示图形表面的尺寸
 * @param vdis 虚拟显示设备指针
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_gfx_resize(struct vmm_virtual_display *vdis, int w, int h);

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
void vmm_virtual_display_surface_gfx_copy(struct vmm_virtual_display *vdis, int src_x, int src_y, int dst_x, int dst_y, int w, int h);

/**
 * @brief 清除虚拟显示的文本文字表面
 * @param vdis 虚拟显示设备指针
 */
void vmm_virtual_display_surface_text_clear(struct vmm_virtual_display *vdis);

/**
 * @brief 设置虚拟显示文本表面的光标位置
 * @param vdis 虚拟显示设备指针
 * @param x X坐标值
 * @param y Y坐标值
 */
void vmm_virtual_display_surface_text_cursor(struct vmm_virtual_display *vdis, int x, int y);

/**
 * @brief 更新虚拟显示的文本文字表面
 * @param vdis 虚拟显示设备指针
 * @param x X坐标值
 * @param y Y坐标值
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_text_update(struct vmm_virtual_display *vdis, int x, int y, int w, int h);

/**
 * @brief 调整虚拟显示文本表面的尺寸
 * @param vdis 虚拟显示设备指针
 * @param w 宽度（像素）
 * @param h 高度（像素）
 */
void vmm_virtual_display_surface_text_resize(struct vmm_virtual_display *vdis, int w, int h);

/**
 * @brief 向虚拟显示添加表面
 * @param vdis 虚拟显示设备指针
 * @param s 字符串或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_add_surface(struct vmm_virtual_display *vdis, struct vmm_surface *s);

/**
 * @brief 从虚拟显示删除表面
 * @param vdis 虚拟显示设备指针
 * @param s 字符串或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_del_surface(struct vmm_virtual_display *vdis, struct vmm_surface *s);

/** Create a virtual display */
struct vmm_virtual_display *vmm_virtual_display_create(const char *name, const struct vmm_virtual_display_ops *ops, void *private);

/**
 * @brief 销毁虚拟显示
 * @param vdis 虚拟显示设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_destroy(struct vmm_virtual_display *vdis);

/** Retrive private context of virtual display */
static inline void *vmm_virtual_display_private(struct vmm_virtual_display *vdis)
{
    return (vdis) ? vdis->private : NULL;
}

/** Find a virtual display with given name */
struct vmm_virtual_display *vmm_virtual_display_find(const char *name);

/**
 * @brief 虚拟 显示 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_display_iterate(struct vmm_virtual_display *start, void *data, int (*fn)(struct vmm_virtual_display *vdis, void *data));

/**
 * @brief 获取虚拟显示的数量
 * @return 数量值
 */
uint32_t vmm_virtual_display_count(void);

#endif /* __VMM_VDISPLAY_H_ */
