/**
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file serial.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Serial console
 */

#include <brd_default_terminal.h>
#include <drv/input.h>
#include <drv/serial/8250-uart.h>
#include <libs/fifo.h>
#include <libs/video_terminal_emulate.h>
#include <vmm_compiler.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_params.h>
#include <vmm_types.h>

#if defined(CONFIG_SERIAL_8250_UART)

static struct uart_8250_port uart8250_port;

static int parse_early_serial_options(char *options, physical_addr_t *addr, uint32_t *baud, uint32_t *clock)
{
    char       *opt_token, *opt_save;
    const char *opt_delim = ",";
    uint32_t    opt_tok_len;
    uint32_t    opt_number = 0;
    char       *token;

    *addr  = 0x3f8;
    *baud  = 115200;
    *clock = 24000000;

    for (opt_token = strtok_r(options, opt_delim, &opt_save); opt_token; opt_token = strtok_r(NULL, opt_delim, &opt_save)) {

        /* @,11520,24000000 -> @, is empty */
        opt_tok_len = strlen(opt_token);

        /* Empty */
        if (!opt_tok_len) {
            opt_number++;
            continue;
        }

        token = skip_spaces(opt_token);

        switch (opt_number) {
            case 0:
                *addr = (physical_addr_t)strtoull(token, NULL, 16);

                /* Port mnenomics */
                if (*addr == 0) {
                    *addr = 0x3f8;
                } else if (*addr == 1) {
                    *addr = 0x2f8;
                }

                opt_number++;
                break;

            case 1:
                *baud = strtoul(token, NULL, 10);
                opt_number++;
                break;

            case 2:
                *clock = strtoul(token, NULL, 10);
                opt_number++;
                break;
        }
    }

    return VMM_OK;
}

static int uart8250_default_terminal_putc(uint8_t ch)
{
    if (!uart_8250_lowlevel_can_putc(&uart8250_port)) {
        return VMM_ERR_FAIL;
    }

    uart_8250_lowlevel_putc(&uart8250_port, ch);
    return VMM_OK;
}

static int setup_early_serial_console(physical_addr_t addr, uint32_t baud, uint32_t clock)
{
    uart8250_port.base        = addr;
    uart8250_port.input_clock = clock;
    uart8250_port.baudrate    = baud;
    uart8250_port.reg_shift   = 2;
    uart8250_port.reg_width   = 1;

    uart_8250_lowlevel_init(&uart8250_port);

    early_putc = &uart8250_default_terminal_putc;

    return VMM_OK;
}

/* earlyprint=serial@<addr>,<baudrate> */
int init_early_serial_console(char *setup_string)
{
    char           *port_token, *port_save;
    const char     *port_delim = "@";
    uint32_t        centry = 0, found = 0, port_tok_len, check_len;
    physical_addr_t addr;
    uint32_t        baud, clock;

    for (port_token = strtok_r(setup_string, port_delim, &port_save); port_token; port_token = strtok_r(NULL, port_delim, &port_save)) {
        port_tok_len = strlen(port_token);

        if (!centry) {

            check_len = (strlen(SERIAL_CONSOLE_NAME) > port_tok_len ? port_tok_len : strlen(SERIAL_CONSOLE_NAME));

            if (!strncmp(setup_string, SERIAL_CONSOLE_NAME, check_len)) {
                found = 1;
            }
        } else {
            if (found) {
                if (parse_early_serial_options(port_token, &addr, &baud, &clock) != VMM_OK) {
                    return VMM_ERR_FAIL;
                }

                setup_early_serial_console(addr, baud, clock);
                found = 0;
                return VMM_OK;
            }
        }

        centry++;
    }

    return VMM_ERR_FAIL;
}

static int uart8250_default_terminal_getc(uint8_t *ch)
{
    if (!uart_8250_lowlevel_can_getc(&uart8250_port)) {
        return VMM_ERR_FAIL;
    }

    *ch = uart_8250_lowlevel_getc(&uart8250_port);
    return VMM_OK;
}

static int __init uart8250_default_terminal_init(void)
{
    uart_8250_lowlevel_init(&uart8250_port);

    return VMM_OK;
}

static struct default_terminal_ops uart8250_ops = {
    .putc = uart8250_default_terminal_putc, .getc = uart8250_default_terminal_getc, .init = uart8250_default_terminal_init};

/*-------------- UART DEFTERM --------------- */
static struct vmm_device_tree_nodeid default_terminal_devid_table[] = {
    {.compatible = "ns8250", .data = &uart8250_ops},
    {.compatible = "ns16450", .data = &uart8250_ops},
    {.compatible = "ns16550a", .data = &uart8250_ops},
    {.compatible = "ns16550", .data = &uart8250_ops},
    {.compatible = "ns16750", .data = &uart8250_ops},
    {.compatible = "ns16850", .data = &uart8250_ops},
    {/* end of list */},
};

struct default_terminal_ops *get_serial_default_terminal_ops(void *data)
{
    int                                  rc;
    const char                          *attr = NULL;
    vmm_device_tree_node_t              *node;
    const struct vmm_device_tree_nodeid *nodeid;
    struct default_terminal_ops         *ops = NULL;
    physical_addr_t                      addr;
    char                                *cmdline_console_string = (char *)data;

    /* Find choosen console node */
    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_CHOSEN_NODE_NAME);

    if (!node) {
        return NULL;
    }

    if (!strcmp(cmdline_console_string, "serial@0")) {
        attr = SERIAL0_CONFIG_DTS_PATH;
    } else if (!strcmp(cmdline_console_string, "serial@1")) {
        attr = SERIAL1_CONFIG_DTS_PATH;
    } else { /* if incorrectly specified, default to serial port 0 */
        attr = SERIAL0_CONFIG_DTS_PATH;
    }

    rc = vmm_device_tree_setattr(node, VMM_DEVICE_TREE_CONSOLE_ATTR_NAME, (void *)attr, VMM_DEVICE_TREE_ATTRTYPE_STRING, strlen(attr) + 1, FALSE);

    rc = vmm_device_tree_read_string(node, VMM_DEVICE_TREE_CONSOLE_ATTR_NAME, &attr);
    vmm_device_tree_dref_node(node);

    if (rc) {
        return NULL;
    }

    node = vmm_device_tree_getnode(attr);

    if (!node) {
        return NULL;
    }

    /* Find appropriate default_terminal ops */
    nodeid = vmm_device_tree_match_node(default_terminal_devid_table, node);

    if (!nodeid) {
        return NULL;
    }

    ops = (struct default_terminal_ops *)nodeid->data;

    if (vmm_device_tree_read_physaddr(node, VMM_DEVICE_TREE_REG_ATTR_NAME, &addr) == VMM_OK) {
        uart8250_port.base = (virtual_addr_t)addr;
    }

    rc = vmm_device_tree_clock_frequency(node, &uart8250_port.input_clock);

    if (rc) {
        return NULL;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &uart8250_port.baudrate)) {
        uart8250_port.baudrate = 115200;
    }

    if (vmm_device_tree_read_u32(node, "reg-shift", &uart8250_port.reg_shift)) {
        uart8250_port.reg_shift = 2;
    }

    if (vmm_device_tree_read_u32(node, "reg-io-width", &uart8250_port.reg_width)) {
        uart8250_port.reg_width = 1;
    }

    return ops;
}
#else
struct default_terminal_ops *get_uart8250_default_terminal_ops(void)
{
    return NULL;
}
#endif
