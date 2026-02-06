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
 * @file vmm_command_manager.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file command manager
 */
#ifndef _VMM_command_manager_H__
#define _VMM_command_manager_H__

#include <libs/list.h>
#include <vmm_char_device.h>
#include <vmm_limits.h>
#include <vmm_types.h>

typedef struct vmm_command {
    double_list_t head;
    char          name[VMM_FIELD_NAME_SIZE];
    char          desc[VMM_FIELD_DESC_SIZE];
    void (*usage)(vmm_char_device_t *);
    int (*exec)(vmm_char_device_t *, int, char **);
} vmm_command_t;

/** Register command */
int vmm_command_manager_register_cmd(vmm_command_t *cmd);

/** Unregister command */
int vmm_command_manager_unregister_cmd(vmm_command_t *cmd);

/** Find a registered command */
vmm_command_t *vmm_command_manager_find_cmd(const char *cmd_name);

/** Get a registered command */
vmm_command_t *vmm_command_manager_get_cmd(int index);

/** Count of registered commands */
uint32_t vmm_command_manager_cmd_count(void);

/** Execute command */
int vmm_command_manager_execute_cmd(vmm_char_device_t *cdev, int argc, char **argv);

/** Execute command string */
int vmm_command_manager_execute_cmdstr(vmm_char_device_t *cdev, char *cmds, bool (*filter)(vmm_char_device_t *cdev, int argc, char **argv));

/** Initialize command manager */
int vmm_command_manager_init(void);

#endif
