/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file generic_default_terminal.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic arch default terminal (default_terminal) interface
 */
#ifndef __ARCH_GENERIC_DEFTERM_H__
#define __ARCH_GENERIC_DEFTERM_H__

#include <vmm_types.h>

struct vmm_device_tree_node;
typedef struct vmm_device_tree_node vmm_device_tree_node_t;

/**
 * Representation of default_terminal operations
 * NOTE: all callbacks are mandatory
 */
struct default_terminal_ops {
    int (*putc)(uint8_t ch);
    int (*getc)(uint8_t *ch);
    int (*init)(vmm_device_tree_node_t *node);
};

/**
 * Set the initial default_terminal operations.
 * The initial default_terminal operations will be used when arch_default_terminal_init()
 * is not able to find console device based on /chosen DT node.
 */
void default_terminal_set_initial_ops(struct default_terminal_ops *initial_ops);

#endif
