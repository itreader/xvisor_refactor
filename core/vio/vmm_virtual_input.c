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
 * @file vmm_virtual_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟输入子系统源文件
 */

#include <libs/stringlib.h>
#include <vio/vmm_virtual_input.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>

#define MODULE_DESC      "Virtual Input Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VINPUT_IPRIORITY)
#define MODULE_INIT      vmm_virtual_input_init
#define MODULE_EXIT      vmm_virtual_input_exit

/**
 * @brief 虚拟输入控制结构（内部），管理输入设备的运行时状态
 */
struct vmm_virtual_input_ctrl {
    vmm_mutex_t                   vkbd_list_lock; /**< vkbd_list_lock成员 */
    double_list_t                 vkbd_list; /**< vkbd_list成员 */
    vmm_mutex_t                   vmou_list_lock; /**< vmou_list_lock成员 */
    double_list_t                 vmou_list; /**< vmou_list成员 */
    vmm_blocking_notifier_chain_t notifier_chain; /**< 通知器链 */
};

static struct vmm_virtual_input_ctrl victrl;

/**
 * @brief 注册虚拟输入客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_input_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&victrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_input_register_client);

/**
 * @brief 注销虚拟输入客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_input_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&victrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_input_unregister_client);

struct vmm_vkeyboard *vmm_vkeyboard_create(const char *name, void (*kbd_event)(struct vmm_vkeyboard *, int, int), void *private)
{
    bool                           found; /**< found成员 */
    struct vmm_vkeyboard          *vkbd; /**< vkbd成员 */
    struct vmm_virtual_input_event event; /**< 事件 */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    vkbd  = NULL; /**< NULL成员 */
    found = FALSE; /**< FALSE成员 */

    vmm_mutex_lock(&victrl.vkbd_list_lock);

    list_for_each_entry(vkbd, &victrl.vkbd_list, head)
    {
        if (strcmp(name, vkbd->name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&victrl.vkbd_list_lock);
        return NULL; /**< NULL成员 */
    }

    vkbd = vmm_malloc(sizeof(struct vmm_vkeyboard)); /**< vmm_vkeyboard))成员 */

    if (!vkbd) {
        vmm_mutex_unlock(&victrl.vkbd_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&vkbd->head);

    if (strlcpy(vkbd->name, name, sizeof(vkbd->name)) >= sizeof(vkbd->name)) {
        vmm_free(vkbd);
        vmm_mutex_unlock(&victrl.vkbd_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_SPIN_LOCK(&vkbd->ledstate_lock);
    vkbd->ledstate = 0; /**< 0 */
    INIT_LIST_HEAD(&vkbd->led_handler_list);
    vkbd->kbd_event = kbd_event; /**< kbd_event成员 */
    vkbd->private   = private; /**< 私有数据 */

    list_add_tail(&vkbd->head, &victrl.vkbd_list); /**< &victrl.vkbd_list)成员 */

    vmm_mutex_unlock(&victrl.vkbd_list_lock);

    /* Broadcast create event */
    event.data = vkbd; /**< vkbd成员 */
    vmm_blocking_notifier_call(&victrl.notifier_chain, VMM_VINPUT_EVENT_CREATE_KEYBOARD, &event); /**< &event)成员 */

    return vkbd; /**< vkbd成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_create);

/**
 * @brief 销毁vkeyboard
 * @param vkbd 虚拟键盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_destroy(struct vmm_vkeyboard *vkbd)
{
    bool                              found;
    irq_flags_t                       flags;
    struct vmm_vkeyboard             *vk;
    struct vmm_vkeyboard_led_handler *vklh;
    struct vmm_virtual_input_event    event;

    if (!vkbd) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    /* Broadcast destroy event */
    event.data = vkbd;
    vmm_blocking_notifier_call(&victrl.notifier_chain, VMM_VINPUT_EVENT_DESTROY_KEYBOARD, &event);

    vmm_spin_lock_irq_save(&vkbd->ledstate_lock, flags);

    while (!list_empty(&vkbd->led_handler_list)) {
        vklh = list_first_entry(&vkbd->led_handler_list, struct vmm_vkeyboard_led_handler, head);
        list_del(&vklh->head);
        vmm_free(vklh);
    }

    vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);

    vmm_mutex_lock(&victrl.vkbd_list_lock);

    if (list_empty(&victrl.vkbd_list)) {
        vmm_mutex_unlock(&victrl.vkbd_list_lock);
        return VMM_ERR_FAIL;
    }

    vk    = NULL;
    found = FALSE;
    list_for_each_entry(vk, &victrl.vkbd_list, head)
    {
        if (strcmp(vk->name, vkbd->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&victrl.vkbd_list_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&vk->head);
    vmm_free(vk);

    vmm_mutex_unlock(&victrl.vkbd_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_destroy);

/**
 * @brief 虚拟键盘事件处理
 * @param vkbd 虚拟键盘设备指针
 * @param vkeycode 虚拟键码值
 * @param vkey 虚拟键值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_event(struct vmm_vkeyboard *vkbd, int vkeycode, int vkey)
{
    if (!vkbd || !vkbd->kbd_event) {
        return VMM_ERR_INVALID;
    }

    vkbd->kbd_event(vkbd, vkeycode, vkey);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_event);

/**
 * @brief 为虚拟键盘添加LED状态处理器
 * @param vkbd 虚拟键盘设备指针
 * @param (*led_change 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_add_led_handler(struct vmm_vkeyboard *vkbd, void (*led_change)(struct vmm_vkeyboard *, int, void *), void *private)
{
    bool                              found;
    irq_flags_t                       flags;
    struct vmm_vkeyboard_led_handler *vklh;

    if (!vkbd || !led_change) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_spin_lock_irq_save(&vkbd->ledstate_lock, flags);

    vklh  = NULL;
    found = FALSE;
    list_for_each_entry(vklh, &vkbd->led_handler_list, head)
    {
        if ((vklh->led_change == led_change) && (vklh->private == private)) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);
        return VMM_ERR_EXIST;
    }

    vklh = vmm_zalloc(sizeof(struct vmm_vkeyboard_led_handler));

    if (!vklh) {
        vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);
        return VMM_ERR_NOMEM;
    }

    INIT_LIST_HEAD(&vklh->head);
    vklh->led_change = led_change;
    vklh->private    = private;
    list_add_tail(&vklh->head, &vkbd->led_handler_list);

    vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_add_led_handler);

/**
 * @brief 从虚拟键盘删除LED状态处理器
 * @param vkbd 虚拟键盘设备指针
 * @param (*led_change 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_del_led_handler(struct vmm_vkeyboard *vkbd, void (*led_change)(struct vmm_vkeyboard *, int, void *), void *private)
{
    bool                              found;
    irq_flags_t                       flags;
    struct vmm_vkeyboard_led_handler *vklh;

    if (!vkbd || !led_change) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_spin_lock_irq_save(&vkbd->ledstate_lock, flags);

    vklh  = NULL;
    found = FALSE;
    list_for_each_entry(vklh, &vkbd->led_handler_list, head)
    {
        if ((vklh->led_change == led_change) && (vklh->private == private)) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&vklh->head);
    vmm_free(vklh);

    vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_del_led_handler);

/**
 * @brief 设置虚拟键盘的LED状态
 * @param vkbd 虚拟键盘设备指针
 * @param ledstate LED状态值
 */
void vmm_vkeyboard_set_ledstate(struct vmm_vkeyboard *vkbd, int ledstate)
{
    irq_flags_t                       flags;
    struct vmm_vkeyboard_led_handler *vklh;

    if (!vkbd) {
        return;
    }

    vmm_spin_lock_irq_save(&vkbd->ledstate_lock, flags);
    vkbd->ledstate = ledstate;
    list_for_each_entry(vklh, &vkbd->led_handler_list, head)
    {
        vklh->led_change(vkbd, ledstate, vklh->private);
    }
    vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_set_ledstate);

/**
 * @brief 获取虚拟键盘的LED状态
 * @param vkbd 虚拟键盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_get_ledstate(struct vmm_vkeyboard *vkbd)
{
    int         ret;
    irq_flags_t flags;

    if (!vkbd) {
        return 0;
    }

    vmm_spin_lock_irq_save(&vkbd->ledstate_lock, flags);
    ret = vkbd->ledstate;
    vmm_spin_unlock_irq_restore(&vkbd->ledstate_lock, flags);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_get_ledstate);

struct vmm_vkeyboard *vmm_vkeyboard_find(const char *name)
{
    bool                  found; /**< found成员 */
    struct vmm_vkeyboard *vk; /**< vk */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    found = FALSE; /**< FALSE成员 */
    vk    = NULL; /**< NULL成员 */

    vmm_mutex_lock(&victrl.vkbd_list_lock);

    list_for_each_entry(vk, &victrl.vkbd_list, head)
    {
        if (strcmp(vk->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    vmm_mutex_unlock(&victrl.vkbd_list_lock);

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    return vk; /**< vk */
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_find);

/**
 * @brief 遍历虚拟键盘实例
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyboard_iterate(struct vmm_vkeyboard *start, void *data, int (*fn)(struct vmm_vkeyboard *vk, void *data))
{
    int                   rc          = VMM_OK;
    bool                  start_found = (start) ? FALSE : TRUE;
    struct vmm_vkeyboard *vk          = NULL;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&victrl.vkbd_list_lock);

    list_for_each_entry(vk, &victrl.vkbd_list, head)
    {
        if (!start_found) {
            if (start && start == vk) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vk, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&victrl.vkbd_list_lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_iterate);

/**
 * @brief 获取虚拟键盘的数量
 * @return 数量值
 */
uint32_t vmm_vkeyboard_count(void)
{
    uint32_t              retval = 0;
    struct vmm_vkeyboard *vk;

    vmm_mutex_lock(&victrl.vkbd_list_lock);

    list_for_each_entry(vk, &victrl.vkbd_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&victrl.vkbd_list_lock);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_vkeyboard_count);

struct vmm_vmouse *vmm_vmouse_create(
    const char *name, bool absolute, void (*mouse_event)(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state), void *private)
{
    bool                           found; /**< found成员 */
    struct vmm_vmouse             *vmou; /**< vmou成员 */
    struct vmm_virtual_input_event event; /**< 事件 */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    vmou  = NULL; /**< NULL成员 */
    found = FALSE; /**< FALSE成员 */

    vmm_mutex_lock(&victrl.vmou_list_lock);

    list_for_each_entry(vmou, &victrl.vmou_list, head)
    {
        if (strcmp(name, vmou->name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&victrl.vmou_list_lock);
        return NULL; /**< NULL成员 */
    }

    vmou = vmm_malloc(sizeof(struct vmm_vmouse)); /**< vmm_vmouse))成员 */

    if (!vmou) {
        vmm_mutex_unlock(&victrl.vmou_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&vmou->head);

    if (strlcpy(vmou->name, name, sizeof(vmou->name)) >= sizeof(vmou->name)) {
        vmm_free(vmou);
        vmm_mutex_unlock(&victrl.vmou_list_lock);
        return NULL; /**< NULL成员 */
    }

    vmou->absolute          = absolute; /**< absolute成员 */
    vmou->graphics_width    = 0; /**< 0 */
    vmou->graphics_height   = 0; /**< 0 */
    vmou->graphics_rotation = 0; /**< 0 */
    vmou->abs_x             = 0; /**< 0 */
    vmou->abs_y             = 0; /**< 0 */
    vmou->abs_z             = 0; /**< 0 */
    vmou->mouse_event       = mouse_event; /**< mouse_event成员 */
    vmou->private           = private; /**< 私有数据 */

    list_add_tail(&vmou->head, &victrl.vmou_list); /**< &victrl.vmou_list)成员 */

    vmm_mutex_unlock(&victrl.vmou_list_lock);

    /* Broadcast create event */
    event.data = vmou; /**< vmou成员 */
    vmm_blocking_notifier_call(&victrl.notifier_chain, VMM_VINPUT_EVENT_CREATE_MOUSE, &event); /**< &event)成员 */

    return vmou; /**< vmou成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_create);

/**
 * @brief 销毁vmouse
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_destroy(struct vmm_vmouse *vmou)
{
    bool                           found;
    struct vmm_vmouse             *vm;
    struct vmm_virtual_input_event event;

    if (!vmou) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    /* Broadcast destroy event */
    event.data = vmou;
    vmm_blocking_notifier_call(&victrl.notifier_chain, VMM_VINPUT_EVENT_DESTROY_MOUSE, &event);

    vmm_mutex_lock(&victrl.vmou_list_lock);

    if (list_empty(&victrl.vmou_list)) {
        vmm_mutex_unlock(&victrl.vmou_list_lock);
        return VMM_ERR_FAIL;
    }

    vm    = NULL;
    found = FALSE;
    list_for_each_entry(vm, &victrl.vmou_list, head)
    {
        if (strcmp(vm->name, vmou->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&victrl.vmou_list_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&vm->head);
    vmm_free(vm);

    vmm_mutex_unlock(&victrl.vmou_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_destroy);

/**
 * @brief 虚拟鼠标事件处理
 * @param vmou 虚拟鼠标设备指针
 * @param dx X方向位移增量
 * @param dy Y方向位移增量
 * @param dz Z方向位移增量
 * @param buttons_state 按键状态值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_event(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state)
{
    int w;
    int h;

    if (!vmou) {
        return VMM_ERR_INVALID;
    }

    if (!vmou->mouse_event) {
        return VMM_OK;
    }

    if (vmou->absolute) {
        w           = 0x7fff;
        h           = 0x7fff;
        vmou->abs_x = dx;
        vmou->abs_y = dy;
        vmou->abs_z = dz;
    } else {
        w = (int)vmou->graphics_width - 1;
        h = (int)vmou->graphics_height - 1;
        vmou->abs_x += dx;
        vmou->abs_y += dy;
        vmou->abs_z += dz;
    }

    switch (vmou->graphics_rotation) {
        case 0:
            vmou->mouse_event(vmou, dx, dy, dz, buttons_state);
            break;

        case 90:
            vmou->mouse_event(vmou, w - dy, dx, dz, buttons_state);
            break;

        case 180:
            vmou->mouse_event(vmou, w - dx, h - dy, dz, buttons_state);
            break;

        case 270:
            vmou->mouse_event(vmou, dy, h - dx, dz, buttons_state);
            break;
    };

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_event);

/**
 * @brief 复位vmouse
 * @param vmou 虚拟鼠标设备指针
 */
void vmm_vmouse_reset(struct vmm_vmouse *vmou)
{
    if (!vmou) {
        return;
    }

    vmou->abs_x = 0;
    vmou->abs_y = 0;
    vmou->abs_z = 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_reset);

/**
 * @brief 获取虚拟鼠标绝对X坐标
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_absolute_x(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->abs_x : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_absolute_x);

/**
 * @brief 获取虚拟鼠标绝对Y坐标
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_absolute_y(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->abs_y : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_absolute_y);

/**
 * @brief 获取虚拟鼠标绝对Z坐标
 * @param vmou 虚拟鼠标设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_absolute_z(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->abs_z : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_absolute_z);

/**
 * @brief 检查虚拟鼠标是否为绝对坐标模式
 * @param vmou 虚拟鼠标设备指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_vmouse_is_absolute(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->absolute : TRUE;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_is_absolute);

/**
 * @brief 设置虚拟鼠标的图形宽度
 * @param vmou 虚拟鼠标设备指针
 * @param width 标识符
 */
void vmm_vmouse_set_graphics_width(struct vmm_vmouse *vmou, uint32_t width)
{
    if (vmou) {
        vmou->graphics_width = width;
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_set_graphics_width);

/**
 * @brief 获取虚拟鼠标的图形宽度
 * @param vmou 虚拟鼠标设备指针
 * @return 编号值，失败返回负数错误码
 */
uint32_t vmm_vmouse_get_graphics_width(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->graphics_width : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_get_graphics_width);

/**
 * @brief 设置虚拟鼠标的图形高度
 * @param vmou 虚拟鼠标设备指针
 * @param height 高度值
 */
void vmm_vmouse_set_graphics_height(struct vmm_vmouse *vmou, uint32_t height)
{
    if (vmou) {
        vmou->graphics_height = height;
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_set_graphics_height);

/**
 * @brief 获取虚拟鼠标的图形高度
 * @param vmou 虚拟鼠标设备指针
 * @return 图形显示高度（像素），失败返回0
 */
uint32_t vmm_vmouse_get_graphics_height(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->graphics_height : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_get_graphics_height);

/**
 * @brief 设置虚拟鼠标的图形旋转角度
 * @param vmou 虚拟鼠标设备指针
 * @param rotation 旋转角度值
 */
void vmm_vmouse_set_graphics_rotation(struct vmm_vmouse *vmou, uint32_t rotation)
{
    if (vmou && ((rotation == 0) || (rotation == 90) || (rotation == 180) || (rotation == 270))) {
        vmou->graphics_rotation = rotation;
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_set_graphics_rotation);

/**
 * @brief 获取虚拟鼠标的图形旋转角度
 * @param vmou 虚拟鼠标设备指针
 * @return 图形显示旋转角度，失败返回0
 */
uint32_t vmm_vmouse_get_graphics_rotation(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->graphics_rotation : 0;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_get_graphics_rotation);

struct vmm_vmouse *vmm_vmouse_find(const char *name)
{
    bool               found; /**< found成员 */
    struct vmm_vmouse *vm; /**< vm */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    found = FALSE; /**< FALSE成员 */
    vm    = NULL; /**< NULL成员 */

    vmm_mutex_lock(&victrl.vmou_list_lock);

    list_for_each_entry(vm, &victrl.vmou_list, head)
    {
        if (strcmp(vm->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    vmm_mutex_unlock(&victrl.vmou_list_lock);

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    return vm; /**< vm */
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_find);

/**
 * @brief 遍历虚拟鼠标实例
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmouse_iterate(struct vmm_vmouse *start, void *data, int (*fn)(struct vmm_vmouse *vmou, void *data))
{
    int                rc          = VMM_OK;
    bool               start_found = (start) ? FALSE : TRUE;
    struct vmm_vmouse *vm          = NULL;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&victrl.vmou_list_lock);

    list_for_each_entry(vm, &victrl.vmou_list, head)
    {
        if (!start_found) {
            if (start && start == vm) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vm, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&victrl.vmou_list_lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_iterate);

/**
 * @brief 获取虚拟鼠标的数量
 * @return 数量值
 */
uint32_t vmm_vmouse_count(void)
{
    uint32_t           retval = 0;
    struct vmm_vmouse *vm;

    vmm_mutex_lock(&victrl.vmou_list_lock);

    list_for_each_entry(vm, &victrl.vmou_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&victrl.vmou_list_lock);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_vmouse_count);

/**
 * @brief 初始化虚拟输入
 * @return 数量值
 */
static int __init vmm_virtual_input_init(void)
{
    memset(&victrl, 0, sizeof(victrl));

    INIT_MUTEX(&victrl.vkbd_list_lock);
    INIT_LIST_HEAD(&victrl.vkbd_list);
    INIT_MUTEX(&victrl.vmou_list_lock);
    INIT_LIST_HEAD(&victrl.vmou_list);
    BLOCKING_INIT_NOTIFIER_CHAIN(&victrl.notifier_chain);

    return VMM_OK;
}

/**
 * @brief 虚拟输入子系统退出清理
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_virtual_input_exit(void)
{
    /* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
