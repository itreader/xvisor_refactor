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
 * @file vmm_virtual_input.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟输入子系统头文件
 */

#ifndef __VMM_VINPUT_H_
#define __VMM_VINPUT_H_

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_VINPUT_IPRIORITY              0

/* Notifier event when virtual keyboard is created */
#define VMM_VINPUT_EVENT_CREATE_KEYBOARD  0x01
/* Notifier event when virtual keyboard is destroyed */
#define VMM_VINPUT_EVENT_DESTROY_KEYBOARD 0x02
/* Notifier event when virtual mouse is created */
#define VMM_VINPUT_EVENT_CREATE_MOUSE     0x03
/* Notifier event when virtual mouse is destroyed */
#define VMM_VINPUT_EVENT_DESTROY_MOUSE    0x04

/** Representation of virtual input notifier event */
/**
 * @brief 虚拟输入事件结构，封装键码或坐标等输入事件数据
 */
struct vmm_virtual_input_event {
    void *data; /**< 数据 */
};

/**
 * @brief 注册虚拟输入客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_input_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销虚拟输入客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_input_unregister_client(vmm_notifier_block_t *nb);

/* Keyboard LED bits */
#define VMM_SCROLL_LOCK_LED (1 << 0)
#define VMM_NUM_LOCK_LED    (1 << 1)
#define VMM_CAPS_LOCK_LED   (1 << 2)

struct vmm_vkeyboard;

/** Representation of a virtual keyboard */
/**
 * @brief 虚拟键盘LED回调，处理键盘指示灯状态变更
 */
struct vmm_vkeyboard_led_handler {
    double_list_t head; /**< 链表头 */
    void (*led_change)(struct vmm_vkeyboard *vkbd, int ledstate, void *private); /**< led_change成员 */
    void *private; /**< 私有数据 */
};

/** Representation of a virtual keyboard */
/**
 * @brief 虚拟键盘设备，维护按键事件发送和LED状态回调
 */
struct vmm_vkeyboard {
    double_list_t  head; /**< 链表头 */
    char           name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    vmm_spinlock_t ledstate_lock; /**< ledstate_lock成员 */
    int            ledstate; /**< ledstate成员 */
    double_list_t  led_handler_list; /**< led_handler_list成员 */
    void (*kbd_event)(struct vmm_vkeyboard *vkbd, int vkeycode, int vkey); /**< kbd_event成员 */
    void *private; /**< 私有数据 */
};

/** Create a virtual keyboard */
struct vmm_vkeyboard *vmm_vkeyboard_create(const char *name, void (*kbd_event)(struct vmm_vkeyboard *, int, int), void *private);

/**
 * @brief 销毁vkeyboard
 * @param vkbd 虚拟键盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_destroy(struct vmm_vkeyboard *vkbd);

/** Retrive private context of virtual keyboard */
static inline void *vmm_vkeyboard_private(struct vmm_vkeyboard *vkbd)
{
    return (vkbd) ? vkbd->private : NULL;
}

/**
 * @brief 虚拟键盘事件处理
 * @param vkbd 虚拟键盘设备指针
 * @param vkeycode 虚拟键码值
 * @param vkey 虚拟键值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_event(struct vmm_vkeyboard *vkbd, int vkeycode, int vkey);

/**
 * @brief 为虚拟键盘添加LED状态处理器
 * @param vkbd 虚拟键盘设备指针
 * @param (*led_change 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_add_led_handler(struct vmm_vkeyboard *vkbd, void (*led_change)(struct vmm_vkeyboard *, int, void *), void *private);

/**
 * @brief 从虚拟键盘删除LED状态处理器
 * @param vkbd 虚拟键盘设备指针
 * @param (*led_change 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_del_led_handler(struct vmm_vkeyboard *vkbd, void (*led_change)(struct vmm_vkeyboard *, int, void *), void *private);

/**
 * @brief 设置虚拟键盘的LED状态
 * @param vkbd 虚拟键盘设备指针
 * @param ledstate LED状态值
 */
void vmm_vkeyboard_set_ledstate(struct vmm_vkeyboard *vkbd, int ledstate);

/**
 * @brief 获取虚拟键盘的LED状态
 * @param vkbd 虚拟键盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_get_ledstate(struct vmm_vkeyboard *vkbd);

/** Find a virtual keyboard with given name */
struct vmm_vkeyboard *vmm_vkeyboard_find(const char *name);

/**
 * @brief 遍历虚拟键盘实例
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_iterate(struct vmm_vkeyboard *start, void *data, int (*fn)(struct vmm_vkeyboard *vkbd, void *data));

/**
 * @brief 获取虚拟键盘的数量
 * @return 数量值
 */
uint32_t vmm_vkeyboard_count(void);

/* Mouse buttons */
#define VMM_MOUSE_LBUTTON 0x01
#define VMM_MOUSE_RBUTTON 0x02
#define VMM_MOUSE_MBUTTON 0x04

/** Representation of a virtual mouse */
/**
 * @brief 虚拟鼠标设备，维护鼠标坐标和按键事件的发送
 */
struct vmm_vmouse {
    double_list_t head; /**< 链表头 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    bool          absolute; /**< absolute成员 */
    uint32_t      graphics_width; /**< graphics_width成员 */
    uint32_t      graphics_height; /**< graphics_height成员 */
    uint32_t      graphics_rotation; /**< graphics_rotation成员 */
    int           abs_x; /**< abs_x成员 */
    int           abs_y; /**< abs_y成员 */
    int           abs_z; /**< abs_z成员 */
    void (*mouse_event)(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state); /**< mouse_event成员 */
    void *private; /**< 私有数据 */
};

/** Create a virtual mouse */
struct vmm_vmouse *vmm_vmouse_create(
    const char *name, bool absolute, void (*mouse_event)(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state), void *private);

/**
 * @brief 销毁vmouse
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_destroy(struct vmm_vmouse *vmou);

/** Retrive private context of virtual mouse */
static inline void *vmm_vmouse_private(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->private : NULL;
}

/**
 * @brief 虚拟鼠标事件处理
 * @param vmou 虚拟鼠标设备指针
 * @param dx X方向位移增量
 * @param dy Y方向位移增量
 * @param dz Z方向位移增量
 * @param buttons_state 按键状态值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_event(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state);

/**
 * @brief 复位vmouse
 * @param vmou 虚拟鼠标设备指针
 */
void vmm_vmouse_reset(struct vmm_vmouse *vmou);

/**
 * @brief 获取虚拟鼠标绝对X坐标
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_absolute_x(struct vmm_vmouse *vmou);

/**
 * @brief 获取虚拟鼠标绝对Y坐标
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_absolute_y(struct vmm_vmouse *vmou);

/**
 * @brief 获取虚拟鼠标绝对Z坐标
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_absolute_z(struct vmm_vmouse *vmou);

/**
 * @brief 检查虚拟鼠标是否为绝对坐标模式
 * @param vmou 虚拟鼠标设备指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_vmouse_is_absolute(struct vmm_vmouse *vmou);

/**
 * @brief 设置虚拟鼠标的图形宽度
 * @param vmou 虚拟鼠标设备指针
 * @param width 标识符
 */
void vmm_vmouse_set_graphics_width(struct vmm_vmouse *vmou, uint32_t width);

/**
 * @brief 获取虚拟鼠标的图形宽度
 * @param vmou 虚拟鼠标设备指针
 * @return 编号值，失败返回负数错误码
 */
uint32_t vmm_vmouse_get_graphics_width(struct vmm_vmouse *vmou);

/**
 * @brief 设置虚拟鼠标的图形高度
 * @param vmou 虚拟鼠标设备指针
 * @param height 高度值
 */
void vmm_vmouse_set_graphics_height(struct vmm_vmouse *vmou, uint32_t height);

/**
 * @brief 获取虚拟鼠标的图形高度
 * @param vmou 虚拟鼠标设备指针
 * @return 图形显示高度（像素），失败返回0
 */
uint32_t vmm_vmouse_get_graphics_height(struct vmm_vmouse *vmou);

/**
 * @brief 设置虚拟鼠标的图形旋转角度
 * @param vmou 虚拟鼠标设备指针
 * @param rotation 旋转角度值
 */
void vmm_vmouse_set_graphics_rotation(struct vmm_vmouse *vmou, uint32_t rotation);

/**
 * @brief 获取虚拟鼠标的图形旋转角度
 * @param vmou 虚拟鼠标设备指针
 * @return 图形显示旋转角度，失败返回0
 */
uint32_t vmm_vmouse_get_graphics_rotation(struct vmm_vmouse *vmou);

/** Find a virtual mouse with given name */
struct vmm_vmouse *vmm_vmouse_find(const char *name);

/**
 * @brief 遍历虚拟鼠标实例
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_iterate(struct vmm_vmouse *start, void *data, int (*fn)(struct vmm_vmouse *vmou, void *data));

/**
 * @brief 获取虚拟鼠标的数量
 * @return 数量值
 */
uint32_t vmm_vmouse_count(void);

#endif /* __VMM_VINPUT_H_ */
