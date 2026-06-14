/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_command_manager.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 命令管理器实现
 */

#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>

#define VMM_CMD_DELIM_CHAR      ';'
#define VMM_CMD_ARG_MAXCOUNT    32
#define VMM_CMD_ARG_DELIM_CHAR  ' '
#define VMM_CMD_ARG_DELIM_CHAR1 '\t'

/**
 * @brief 命令管理器控制结构，管理命令的注册和查找
 */
struct vmm_command_manager_ctrl {
    vmm_mutex_t   cmd_list_lock; /**< 命令链表锁 */
    double_list_t cmd_list; /**< 命令链表 */
};

static struct vmm_command_manager_ctrl cmctrl;

/**
 * @brief 注册命令到命令管理器
 * @param cmd 命令标识或命令结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_register_cmd(vmm_command_t *cmd)
{
    bool           found;
    vmm_command_t *c;

    if (!cmd) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&cmctrl.cmd_list_lock);

    c     = NULL;
    found = FALSE;
    list_for_each_entry(c, &cmctrl.cmd_list, head)
    {
        if (strcmp(c->name, cmd->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&cmctrl.cmd_list_lock);
        return VMM_ERR_INVALID;
    }

    INIT_LIST_HEAD(&cmd->head);

    list_add_tail(&cmd->head, &cmctrl.cmd_list);

    vmm_mutex_unlock(&cmctrl.cmd_list_lock);

    return VMM_OK;
}

/**
 * @brief 从命令管理器注销命令
 * @param cmd 命令标识或命令结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_unregister_cmd(vmm_command_t *cmd)
{
    bool           found;
    vmm_command_t *c;

    if (!cmd) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&cmctrl.cmd_list_lock);

    if (list_empty(&cmctrl.cmd_list)) {
        vmm_mutex_unlock(&cmctrl.cmd_list_lock);
        return VMM_ERR_FAIL;
    }

    c     = NULL;
    found = FALSE;
    list_for_each_entry(c, &cmctrl.cmd_list, head)
    {
        if (strcmp(c->name, cmd->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&cmctrl.cmd_list_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&c->head);

    vmm_mutex_unlock(&cmctrl.cmd_list_lock);

    return VMM_OK;
}

/**
 * @brief 在命令管理器中查找指定名称的命令
 * @param cmd_name 命令名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_command_t *vmm_command_manager_find_cmd(const char *cmd_name)
{
    bool           found;
    vmm_command_t *c;

    if (!cmd_name) {
        return NULL;
    }

    found = FALSE;
    c     = NULL;

    vmm_mutex_lock(&cmctrl.cmd_list_lock);

    list_for_each_entry(c, &cmctrl.cmd_list, head)
    {
        if (strcmp(c->name, cmd_name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&cmctrl.cmd_list_lock);

    if (!found) {
        return NULL;
    }

    return c;
}

/**
 * @brief 获取命令管理器的命令
 * @param index 数组中的索引位置
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_command_t *vmm_command_manager_get_cmd(int index)
{
    bool           found;
    vmm_command_t *c;

    if (index < 0) {
        return NULL;
    }

    c     = NULL;
    found = FALSE;

    vmm_mutex_lock(&cmctrl.cmd_list_lock);

    list_for_each_entry(c, &cmctrl.cmd_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_mutex_unlock(&cmctrl.cmd_list_lock);

    if (!found) {
        return NULL;
    }

    return c;
}

/**
 * @brief 获取命令管理器命令的数量
 * @return 数量值
 */
uint32_t vmm_command_manager_cmd_count(void)
{
    uint32_t       retval;
    vmm_command_t *c;

    retval = 0;

    vmm_mutex_lock(&cmctrl.cmd_list_lock);

    list_for_each_entry(c, &cmctrl.cmd_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&cmctrl.cmd_list_lock);

    return retval;
}

/**
 * @brief 在命令管理器中查找并执行指定命令
 * @param cdev 字符设备指针
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_execute_cmd(vmm_char_device_t *cdev, int argc, char **argv)
{
    int            ret = VMM_OK;
    vmm_command_t *cmd = NULL;

    /* Find & execute the commad */
    if ((cmd = vmm_command_manager_find_cmd(argv[0]))) {
        /* Found a matching command so execute it. */
        if ((ret = cmd->exec(cdev, argc, argv))) {
            vmm_cdev_printf(
                cdev,
                "Error: command %s failed "
                "(code %d)\n",
                argv[0], ret);
        }
    } else {
        /* Did not find command. */
        vmm_cdev_printf(cdev, "Error: unknown command %s\n", argv[0]);
        ret = VMM_ERR_NOTAVAIL;
    }

    return ret;
}

/**
 * @brief 解析命令字符串并在命令管理器中执行
 * @param cdev 字符设备指针
 * @param cmds 命令数组指针
 * @param (*filter 布尔值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_execute_cmdstr(vmm_char_device_t *cdev, char *cmds, bool (*filter)(vmm_char_device_t *cdev, int argc, char **argv))
{
    int argc;
    int ret;
    char *argv[VMM_CMD_ARG_MAXCOUNT];
    char *c   = cmds;
    bool  eos = 0;
    argc      = 0;

    while (*c) {
        while (*c == VMM_CMD_ARG_DELIM_CHAR || *c == VMM_CMD_ARG_DELIM_CHAR1) {
            c++;
        }

        if (*c == '\0') {
            break;
        }

        if (argc < VMM_CMD_ARG_MAXCOUNT && *c != VMM_CMD_DELIM_CHAR) {
            argv[argc] = c;
            argc++;
        }

        while (*c != VMM_CMD_ARG_DELIM_CHAR && *c != VMM_CMD_ARG_DELIM_CHAR1 && *c != VMM_CMD_DELIM_CHAR && *c != '\0') {
            c++;
        }

        if (*c == '\0') {
            eos = 1;
        }

        if ((*c == VMM_CMD_DELIM_CHAR || *c == '\0') && argc > 0) {
            *c = '\0';
            c++;

            if (filter && filter(cdev, argc, argv)) {
                vmm_cdev_printf(
                    cdev,
                    "Error: command %s "
                    "filtered\n",
                    argv[0]);
            } else {
                ret = vmm_command_manager_execute_cmd(cdev, argc, argv);

                if (ret) {
                    return ret;
                }
            }

            argc = 0;

            if (eos) {
                break;
            }
        } else {
            *c = '\0';
            c++;
        }
    }

    if (argc > 0) {
        ret = vmm_command_manager_execute_cmd(cdev, argc, argv);

        if (ret) {
            return ret;
        }
    }

    return VMM_OK;
}

/**
 * @brief 输出命令管理器中所有已注册命令的使用信息
 * @param cdev 字符设备指针
 */
static void cmd_help_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage: ");
    vmm_cdev_printf(cdev, "   help\n");
    vmm_cdev_printf(cdev, "   help <cmd_name1> [<cmd_name2>] ...\n");
}

/**
 * @brief 执行help命令，显示所有可用命令列表
 * @param cdev 字符设备指针
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int cmd_help_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    uint32_t i;
    uint32_t cmd_count;
    vmm_command_t *cmd;

    if (argc == 1) {
        cmd_count = vmm_command_manager_cmd_count();

        for (i = 0; i < cmd_count; i++) {
            if ((cmd = vmm_command_manager_get_cmd(i))) {
                vmm_cdev_printf(cdev, "%-12s - %s\n", cmd->name, cmd->desc);
            }
        }
    } else if (argc > 1) {
        for (i = 1; i < argc; i++) {
            if ((cmd = vmm_command_manager_find_cmd(argv[i]))) {
                vmm_cdev_printf(cdev, "%-12s - %s\n", cmd->name, cmd->desc);
                cmd->usage(cdev);
            } else {
                vmm_cdev_printf(cdev, "Cannot find command %s\n", argv[i]);
                return VMM_ERR_NOTAVAIL;
            }

            vmm_printf("\n");
        }
    }

    return VMM_OK;
}

static vmm_command_t help_cmd = {
    .name  = "help",
    .desc  = "displays list of all commands",
    .usage = cmd_help_usage,
    .exec  = cmd_help_exec,
};

/**
 * @brief 初始化命令管理器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_command_manager_init(void)
{
    memset(&cmctrl, 0, sizeof(cmctrl));

    INIT_MUTEX(&cmctrl.cmd_list_lock);
    INIT_LIST_HEAD(&cmctrl.cmd_list);

    return vmm_command_manager_register_cmd(&help_cmd);
}
