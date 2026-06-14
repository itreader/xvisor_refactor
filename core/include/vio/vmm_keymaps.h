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
 * @file vmm_keymaps.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 按键符号到键码转换（键盘映射）头文件
 *
 * The header has been largely adapted from QEMU sources:
 * ui/keymaps.h
 *
 * QEMU keysym to keycode conversion using rdesktop keymaps
 *
 * Copyright (c) 2004 Johannes Schindelin
 *
 * The original source is licensed under GPL.
 */

/*
 * Daemons such as VNC server, GUI render, etc will use vmm_keymaps APIs
 * for converting key press events (keysms) into intermediate scancodes
 * (keycode). These daemons will pass intermediate scancodes (keycode) to
 * Guest emulated keyboard device via vmm_virtual_input APIs. This in-turn causes
 * Guest OS to receive virtual key press events.
 *
 * The figure below sumarizes the above:
 *
 * -------------             ----------------            --------------
 * |  Daemon   |   Using     |              |   Using    |    Guest   |
 * | Key Press |============>| Intermediate |===========>|  Key Press |
 * |   Event   | vmm_keymaps |   Scancode   | vmm_virtual_input |    Event   |
 * -------------             ----------------            --------------
 *
 * The format of intermediate scancode is as follows:
 *
 *  --------------------------------------
 *  | Bits[11:7] | Bits[8:8] | Bits[7:0] |
 *  | Modifiers  |  Status   |  KeyCode  |
 *  --------------------------------------
 *
 *  KeyCode   = Key position/number
 *  Status    = Key state Up (=1) or Down (=0)
 *  Modifiers = Key state for SHIFT, CTRL, ALT, and ALTGR keys
 */

#ifndef __VMM_KEYMAPS_H_
#define __VMM_KEYMAPS_H_

#include <vmm_types.h>

/**
 * @brief 键名到键码映射条目，将字符串名称映射到键盘符号值
 */
struct vmm_name2keysym {
    const char *name; /**< 名称 */
    int         keysym; /**< 键符号 */
};

/**
 * @brief 键码范围结构，定义连续键码的起止值和映射偏移
 */
struct vmm_key_range {
    int                   start; /**< 起始 */
    int                   end; /**< 结束 */
    struct vmm_key_range *next; /**< 下一个 */
};

#define VMM_MAX_NORMAL_KEYCODE 512
#define VMM_MAX_EXTRA_COUNT    256

/**
 * @brief 键盘映射布局，维护一组键名映射和键码范围表
 */
struct vmm_keymap_layout {
    uint16_t keysym2keycode[VMM_MAX_NORMAL_KEYCODE]; /**< keysym2keycode成员 */

    struct {
        int      keysym; /**< 键符号 */
        uint16_t keycode; /**< 键码 */
    } keysym2keycode_extra[VMM_MAX_EXTRA_COUNT]; /**< keysym2keycode_extra成员 */

    int                   extra_count; /**< extra_count成员 */
    struct vmm_key_range *keypad_range; /**< keypad_range成员 */
    struct vmm_key_range *numlock_range; /**< numlock_range成员 */
};

/* scancode without modifiers */
#define SCANCODE_KEYMASK     0xff
/* scancode without grey or up bit */
#define SCANCODE_KEYCODEMASK 0x7f

/* "grey" keys will usually need a 0xe0 prefix */
#define SCANCODE_GREY        0x80
#define SCANCODE_EMUL0       0xE0
/* "up" flag */
#define SCANCODE_UP          0x80

/* Additional modifiers to use if not catched another way. */
#define SCANCODE_SHIFT       0x100
#define SCANCODE_CTRL        0x200
#define SCANCODE_ALT         0x400
#define SCANCODE_ALTGR       0x800

struct vmm_keymap_layout *vmm_keymap_alloc_layout(const struct vmm_name2keysym *table, const char *lang);
/**
 * @brief 释放键盘布局定义的内存
 * @param layout 布局定义指针
 */
void                      vmm_keymap_free_layout(struct vmm_keymap_layout *layout);
/**
 * @brief keysym2scancode
 * @param layout 布局定义指针
 * @param keysym 键盘符号值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int                       vmm_keysym2scancode(struct vmm_keymap_layout *layout, int keysym);
/**
 * @brief 检查键码是否为键盘区按键
 * @param layout 布局定义指针
 * @param keycode 键码值
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool                      vmm_keycode_is_keypad(struct vmm_keymap_layout *layout, int keycode);
/**
 * @brief 检查键符是否为NumLock键
 * @param layout 布局定义指针
 * @param keysym 键盘符号值
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool                      vmm_keysym_is_numlock(struct vmm_keymap_layout *layout, int keysym);

/* Virtual keys */
/**
 * @brief 虚拟按键结构，保存按键的键码和修饰键状态
 */
enum vmm_vkey {
    VMM_VKEY_SHIFT         = 0, /**< 0 */
    VMM_VKEY_SHIFT_R       = 1, /**< 1 */
    VMM_VKEY_ALT           = 2, /**< 2 */
    VMM_VKEY_ALT_R         = 3, /**< 3 */
    VMM_VKEY_ALTGR         = 4, /**< 4 */
    VMM_VKEY_ALTGR_R       = 5, /**< 5 */
    VMM_VKEY_CTRL          = 6, /**< 6 */
    VMM_VKEY_CTRL_R        = 7, /**< 7 */
    VMM_VKEY_MENU          = 8, /**< 8 */
    VMM_VKEY_ESC           = 9, /**< 9 */
    VMM_VKEY_1             = 10, /**< 10 */
    VMM_VKEY_2             = 11, /**< 11 */
    VMM_VKEY_3             = 12, /**< 12 */
    VMM_VKEY_4             = 13, /**< 13 */
    VMM_VKEY_5             = 14, /**< 14 */
    VMM_VKEY_6             = 15, /**< 15 */
    VMM_VKEY_7             = 16, /**< 16 */
    VMM_VKEY_8             = 17, /**< 17 */
    VMM_VKEY_9             = 18, /**< 18 */
    VMM_VKEY_0             = 19, /**< 19 */
    VMM_VKEY_MINUS         = 20, /**< 20 */
    VMM_VKEY_EQUAL         = 21, /**< 21 */
    VMM_VKEY_BACKSPACE     = 22, /**< 22 */
    VMM_VKEY_TAB           = 23, /**< 23 */
    VMM_VKEY_Q             = 24, /**< 24 */
    VMM_VKEY_W             = 25, /**< 25 */
    VMM_VKEY_E             = 26, /**< 26 */
    VMM_VKEY_R             = 27, /**< 27 */
    VMM_VKEY_T             = 28, /**< 28 */
    VMM_VKEY_Y             = 29, /**< 29 */
    VMM_VKEY_U             = 30, /**< 30 */
    VMM_VKEY_I             = 31, /**< 31 */
    VMM_VKEY_O             = 32, /**< 32 */
    VMM_VKEY_P             = 33, /**< 33 */
    VMM_VKEY_BRACKET_LEFT  = 34, /**< 34 */
    VMM_VKEY_BRACKET_RIGHT = 35, /**< 35 */
    VMM_VKEY_RET           = 36, /**< 36 */
    VMM_VKEY_A             = 37, /**< 37 */
    VMM_VKEY_S             = 38, /**< 38 */
    VMM_VKEY_D             = 39, /**< 39 */
    VMM_VKEY_F             = 40, /**< 40 */
    VMM_VKEY_G             = 41, /**< 41 */
    VMM_VKEY_H             = 42, /**< 42 */
    VMM_VKEY_J             = 43, /**< 43 */
    VMM_VKEY_K             = 44, /**< 44 */
    VMM_VKEY_L             = 45, /**< 45 */
    VMM_VKEY_SEMICOLON     = 46, /**< 46 */
    VMM_VKEY_APOSTROPHE    = 47, /**< 47 */
    VMM_VKEY_GRAVE_ACCENT  = 48, /**< 48 */
    VMM_VKEY_BACKSLASH     = 49, /**< 49 */
    VMM_VKEY_Z             = 50, /**< 50 */
    VMM_VKEY_X             = 51, /**< 51 */
    VMM_VKEY_C             = 52, /**< 52 */
    VMM_VKEY_V             = 53, /**< 53 */
    VMM_VKEY_B             = 54, /**< 54 */
    VMM_VKEY_N             = 55, /**< 55 */
    VMM_VKEY_M             = 56, /**< 56 */
    VMM_VKEY_COMMA         = 57, /**< 57 */
    VMM_VKEY_DOT           = 58, /**< 58 */
    VMM_VKEY_SLASH         = 59, /**< 59 */
    VMM_VKEY_ASTERISK      = 60, /**< 60 */
    VMM_VKEY_SPC           = 61, /**< 61 */
    VMM_VKEY_CAPS_LOCK     = 62, /**< 62 */
    VMM_VKEY_F1            = 63, /**< 63 */
    VMM_VKEY_F2            = 64, /**< 64 */
    VMM_VKEY_F3            = 65, /**< 65 */
    VMM_VKEY_F4            = 66, /**< 66 */
    VMM_VKEY_F5            = 67, /**< 67 */
    VMM_VKEY_F6            = 68, /**< 68 */
    VMM_VKEY_F7            = 69, /**< 69 */
    VMM_VKEY_F8            = 70, /**< 70 */
    VMM_VKEY_F9            = 71, /**< 71 */
    VMM_VKEY_F10           = 72, /**< 72 */
    VMM_VKEY_NUM_LOCK      = 73, /**< 73 */
    VMM_VKEY_SCROLL_LOCK   = 74, /**< 74 */
    VMM_VKEY_KP_DIVIDE     = 75, /**< 75 */
    VMM_VKEY_KP_MULTIPLY   = 76, /**< 76 */
    VMM_VKEY_KP_SUBTRACT   = 77, /**< 77 */
    VMM_VKEY_KP_ADD        = 78, /**< 78 */
    VMM_VKEY_KP_ENTER      = 79, /**< 79 */
    VMM_VKEY_KP_DECIMAL    = 80, /**< 80 */
    VMM_VKEY_SYSRQ         = 81, /**< 81 */
    VMM_VKEY_KP_0          = 82, /**< 82 */
    VMM_VKEY_KP_1          = 83, /**< 83 */
    VMM_VKEY_KP_2          = 84, /**< 84 */
    VMM_VKEY_KP_3          = 85, /**< 85 */
    VMM_VKEY_KP_4          = 86, /**< 86 */
    VMM_VKEY_KP_5          = 87, /**< 87 */
    VMM_VKEY_KP_6          = 88, /**< 88 */
    VMM_VKEY_KP_7          = 89, /**< 89 */
    VMM_VKEY_KP_8          = 90, /**< 90 */
    VMM_VKEY_KP_9          = 91, /**< 91 */
    VMM_VKEY_LESS          = 92, /**< 92 */
    VMM_VKEY_F11           = 93, /**< 93 */
    VMM_VKEY_F12           = 94, /**< 94 */
    VMM_VKEY_PRINT         = 95, /**< 95 */
    VMM_VKEY_HOME          = 96, /**< 96 */
    VMM_VKEY_PGUP          = 97, /**< 97 */
    VMM_VKEY_PGDN          = 98, /**< 98 */
    VMM_VKEY_END           = 99, /**< 99 */
    VMM_VKEY_LEFT          = 100, /**< 100成员 */
    VMM_VKEY_UP            = 101, /**< 101成员 */
    VMM_VKEY_DOWN          = 102, /**< 102成员 */
    VMM_VKEY_RIGHT         = 103, /**< 103成员 */
    VMM_VKEY_INSERT        = 104, /**< 104成员 */
    VMM_VKEY_DELETE        = 105, /**< 105成员 */
    VMM_VKEY_STOP          = 106, /**< 106成员 */
    VMM_VKEY_AGAIN         = 107, /**< 107成员 */
    VMM_VKEY_PROPS         = 108, /**< 108成员 */
    VMM_VKEY_UNDO          = 109, /**< 109成员 */
    VMM_VKEY_FRONT         = 110, /**< 110成员 */
    VMM_VKEY_COPY          = 111, /**< 111成员 */
    VMM_VKEY_OPEN          = 112, /**< 112成员 */
    VMM_VKEY_PASTE         = 113, /**< 113成员 */
    VMM_VKEY_FIND          = 114, /**< 114成员 */
    VMM_VKEY_CUT           = 115, /**< 115成员 */
    VMM_VKEY_LF            = 116, /**< 116成员 */
    VMM_VKEY_HELP          = 117, /**< 117成员 */
    VMM_VKEY_META_L        = 118, /**< 118成员 */
    VMM_VKEY_META_R        = 119, /**< 119成员 */
    VMM_VKEY_COMPOSE       = 120, /**< 120成员 */
    VMM_VKEY_MAX           = 121, /**< 121成员 */
};

/**
 * @brief vkeyname2vkey
 * @param key 键值或关键字
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeyname2vkey(const char *key);
/**
 * @brief vkeycode2vkey
 * @param vkeycode 虚拟键码值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkeycode2vkey(int vkeycode);
/**
 * @brief vkey2vkeycode
 * @param vkey 虚拟键值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vkey2vkeycode(int vkey);

#endif /* __VMM_KEYMAPS_H_ */
