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
 * @file cmd_vserial.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vserial command
 */

#include <libs/stringlib.h>
#include <vio/vmm_vserial.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command vserial"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VSERIAL_IPRIORITY + 1)
#define MODULE_INIT      cmd_vserial_init
#define MODULE_EXIT      cmd_vserial_exit

static void cmd_vserial_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   vserial bind <name>\n");
    vmm_cdev_printf(cdev, "   vserial dump <name> [<byte_count>]\n");
    vmm_cdev_printf(cdev, "   vserial help\n");
    vmm_cdev_printf(cdev, "   vserial list\n");
}

#define VSERIAL_ESCMD_SIZE (17 * 3)
#define VSERIAL_ESC_NPAR   (16)

struct cmd_vserial_recvcntx {
    const char        *name;
    int                chcount;
    uint8_t            esc_cmd[VSERIAL_ESCMD_SIZE];
    uint16_t           esc_attrib[VSERIAL_ESC_NPAR];
    uint8_t            esc_cmd_count;
    uint8_t            esc_attrib_count;
    bool               esc_cmd_active;
    vmm_char_device_t *cdev;
};

static int cmd_vserial_recv_putchar(struct cmd_vserial_recvcntx *v, uint8_t ch)
{
    switch (ch) {
        case '\r':
            vmm_cdev_printf(v->cdev, "\r[%s] ", v->name);
            break;

        case '\n':
            vmm_cdev_printf(v->cdev, "\n[%s] ", v->name);
            break;

        default:
            vmm_cdev_putc(v->cdev, ch);
            break;
    };

    return VMM_OK;
}

static int cmd_vserial_recv_startesc(struct cmd_vserial_recvcntx *v)
{
    v->esc_cmd_active   = TRUE;
    v->esc_cmd_count    = 0;
    v->esc_attrib_count = 0;
    v->esc_attrib[0]    = 0;
    return VMM_OK;
}

static int cmd_vserial_recv_flushesc(struct cmd_vserial_recvcntx *v)
{
    uint32_t index    = v->esc_cmd_count < sizeof(v->esc_cmd) ? v->esc_cmd_count : sizeof(v->esc_cmd) - 1;
    v->esc_cmd[index] = '\0';
    vmm_cdev_putc(v->cdev, '\e');
    vmm_cdev_puts(v->cdev, (char *)v->esc_cmd);
    v->esc_cmd_active = FALSE;
    return VMM_OK;
}

static int cmd_vserial_recv_putesc(struct cmd_vserial_recvcntx *v, uint8_t ch)
{
    char str[32];

    if (v->esc_cmd_count < VSERIAL_ESCMD_SIZE) {
        v->esc_cmd[v->esc_cmd_count] = ch;
        v->esc_cmd_count++;
    } else {
        cmd_vserial_recv_flushesc(v);
        return VMM_OK;
    }

    switch (v->esc_cmd[0]) {
        case 'c': /* Reset */
        case 'r': /* Enable Scrolling */
        case 'D': /* Scroll Down one line or linefeed */
        case 'M': /* Scroll Up one line or reverse-linefeed */
        case 'E': /* Newline */
        case '7': /* Save Cursor Position and Attrs */
        case '8': /* Restore Cursor Position and Attrs */
            cmd_vserial_recv_flushesc(v);
            break;

        case '[': /* CSI codes */
            if (v->esc_cmd_count == 1) {
                break;
            }

            switch (v->esc_cmd[v->esc_cmd_count - 1]) {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    v->esc_attrib[v->esc_attrib_count] *= 10;
                    v->esc_attrib[v->esc_attrib_count] += (v->esc_cmd[v->esc_cmd_count - 1] - '0');
                    break;

                case ';':
                    v->esc_attrib_count++;
                    v->esc_attrib[v->esc_attrib_count] = 0;
                    break;

                case 'D': /* Move Left */
                case 'C': /* Move Right */
                case 'B': /* Move Down */
                case 'A': /* Move Up */
                case 'm': /* Set Display Attributes */
                case 'c': /* Request Terminal Type */
                    cmd_vserial_recv_flushesc(v);
                    break;

                case 'n':
                    switch (v->esc_attrib[0]) {
                        case 5: /* Request Terminal Status */
                        case 6: /* Request Cursor Position */
                            cmd_vserial_recv_flushesc(v);
                            break;
                    };

                    v->esc_cmd_active = FALSE;

                    break;

                case 's': /* Save Cursor Position */
                case 'u': /* Restore Cursor Position */
                    cmd_vserial_recv_flushesc(v);
                    break;

                case 'H': /* Cursor Home */
                case 'f': /* Force Cursor Position */
                    vmm_sprintf(str, "[%s] ", v->name);

                    if (v->esc_attrib_count == 0) {
                        cmd_vserial_recv_flushesc(v);
                        vmm_cdev_puts(v->cdev, str);
                    } else {
                        vmm_cdev_printf(v->cdev, "\e[%d;%df", v->esc_attrib[0], 0);
                        vmm_cdev_puts(v->cdev, str);
                        vmm_cdev_printf(v->cdev, "\e[%d;%zuf", v->esc_attrib[0], v->esc_attrib[1] + strlen(str));
                    }

                    v->esc_cmd_active = FALSE;
                    break;

                case 'J': /* Clear screen */
                    cmd_vserial_recv_flushesc(v);
                    break;

                default:
                    goto unhandled;
            }

            break;

        default:
            goto unhandled;
    };

    return VMM_OK;

unhandled:
    cmd_vserial_recv_flushesc(v);

    return VMM_OK;
}

static void cmd_vserial_recv(struct vmm_vserial *vser, void *private, uint8_t ch)
{
    struct cmd_vserial_recvcntx *v;
    v = (struct cmd_vserial_recvcntx *)private;

    if (!v) {
        return;
    }

    if (!v->chcount) {
        return;
    }

    if (v->esc_cmd_active) {
        cmd_vserial_recv_putesc(v, ch);
    } else if (ch == '\e') {
        cmd_vserial_recv_startesc(v);
    } else {
        cmd_vserial_recv_putchar(v, ch);

        if (-1 < v->chcount) {
            if (v->chcount) {
                v->chcount--;
            }
        }
    }
}

static int cmd_vserial_bind(vmm_char_device_t *cdev, const char *name)
{
    int                         rc = VMM_OK;
    uint32_t                    tmp, ecount = 0, eattrib[2], eacount;
    bool                        eactive = FALSE;
    char                        ecmd[VSERIAL_ESCMD_SIZE], ch;
    struct vmm_vserial         *vser = vmm_vserial_find(name);
    struct cmd_vserial_recvcntx recvcntx;

    if (!vser) {
        vmm_cdev_printf(cdev, "Failed to find virtual serial port\n");
        return VMM_EFAIL;
    }

    vmm_cdev_printf(cdev, "[%s] ", name);

    recvcntx.name             = name;
    recvcntx.chcount          = -1;
    recvcntx.esc_cmd_active   = FALSE;
    recvcntx.esc_cmd_count    = 0;
    recvcntx.esc_attrib_count = 0;
    recvcntx.esc_attrib[0]    = 0;
    recvcntx.cdev             = cdev;

    rc                        = vmm_vserial_register_receiver(vser, &cmd_vserial_recv, &recvcntx);

    if (rc) {
        return rc;
    }

    eactive    = FALSE;
    eattrib[0] = 0;
    eacount    = 0;

    while (1) {
        if (!vmm_scanchars(cdev, &ch, 1, TRUE)) {
            if (eactive) {
                if (ecount < VSERIAL_ESCMD_SIZE) {
                    ecmd[ecount] = ch;
                    ecount++;
                } else {
                    goto send_eflush_continue;
                }

                switch (ecmd[0]) {
                    case 'x':
                        if (ecount == 1) {
                            break;
                        }

                        switch (ecmd[1]) {
                            case 'q':
                                goto send_break;

                            default:
                                goto send_eflush_continue;
                        }

                        break;

                    case '[':
                        if (ecount == 1) {
                            break;
                        }

                        switch (ecmd[ecount - 1]) {
                            case '0':
                            case '1':
                            case '2':
                            case '3':
                            case '4':
                            case '5':
                            case '6':
                            case '7':
                            case '8':
                            case '9':
                                eattrib[eacount] *= 10;
                                eattrib[eacount] += (ecmd[ecount - 1] - '0');
                                break;

                            case ';':
                                if (eacount == 2) {
                                    goto send_eflush_continue;
                                }

                                eacount++;
                                eattrib[eacount] = 0;
                                break;

                            case 'R': /* Response */
                                tmp = strlen(name) + 3;

                                if (eattrib[1] < tmp) {
                                    tmp = 0;
                                } else {
                                    tmp = eattrib[1] - tmp;
                                }

                                vmm_sprintf(ecmd, "[%d;%dR", eattrib[0], tmp);
                                goto send_eflush_continue;

                            default:
                                goto send_eflush_continue;
                        }

                        break;

                    default:
                        goto send_eflush_continue;
                };
            } else {
                if (ch == '\e') {
                    eactive = TRUE;
                    ecount  = 0;
                } else {
                    goto send_ch_continue;
                }
            }
        }

        continue;
    send_ch_continue:

        while (!vmm_vserial_send(vser, (uint8_t *)&ch, 1))
            ;

        continue;
    send_eflush_continue:
        ch = '\e';

        while (!vmm_vserial_send(vser, (uint8_t *)&ch, 1))
            ;

        while (!vmm_vserial_send(vser, (uint8_t *)&ecmd, ecount))
            ;

        eactive    = FALSE;
        eattrib[0] = 0;
        eacount    = 0;
        continue;
    send_break:
        break;
    }

    vmm_cdev_printf(cdev, "\n");

    rc = vmm_vserial_unregister_receiver(vser, &cmd_vserial_recv, &recvcntx);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}

static int cmd_vserial_dump(vmm_char_device_t *cdev, const char *name, int bcount)
{
    int                         rc   = VMM_OK;
    struct vmm_vserial         *vser = vmm_vserial_find(name);
    struct cmd_vserial_recvcntx recvcntx;

    if (!vser) {
        vmm_cdev_printf(cdev, "Failed to find virtual serial port\n");
        return VMM_EFAIL;
    }

    vmm_cdev_printf(cdev, "[%s] ", name);

    recvcntx.name             = name;
    recvcntx.chcount          = (0 < bcount) ? bcount : -1;
    recvcntx.esc_cmd_active   = FALSE;
    recvcntx.esc_cmd_count    = 0;
    recvcntx.esc_attrib_count = 0;
    recvcntx.esc_attrib[0]    = 0;
    recvcntx.cdev             = cdev;

    rc                        = vmm_vserial_register_receiver(vser, &cmd_vserial_recv, &recvcntx);

    if (rc) {
        return rc;
    }

    vmm_cdev_printf(cdev, "\n");

    rc = vmm_vserial_unregister_receiver(vser, &cmd_vserial_recv, &recvcntx);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}

static int cmd_vserial_list_iter(struct vmm_vserial *vser, void *data)
{
    vmm_char_device_t *cdev = data;

    vmm_cdev_printf(cdev, " %-39s\n", vser->name);

    return VMM_OK;
}

static void cmd_vserial_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-39s\n", "Name");
    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_vserial_iterate(NULL, cdev, cmd_vserial_list_iter);
    vmm_cdev_printf(cdev, "----------------------------------------\n");
}

static int cmd_vserial_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    int bcount = -1;

    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_vserial_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_vserial_list(cdev);
            return VMM_OK;
        }
    }

    if (argc < 3) {
        cmd_vserial_usage(cdev);
        return VMM_EFAIL;
    }

    if (strcmp(argv[1], "bind") == 0) {
        return cmd_vserial_bind(cdev, argv[2]);
    } else if (strcmp(argv[1], "dump") == 0) {
        if (4 <= argc) {
            bcount = atoi(argv[3]);
        }

        return cmd_vserial_dump(cdev, argv[2], bcount);
    } else {
        cmd_vserial_usage(cdev);
        return VMM_EFAIL;
    }

    return VMM_OK;
}

static vmm_command_t cmd_vserial = {
    .name  = "vserial",
    .desc  = "virtual serial port commands",
    .usage = cmd_vserial_usage,
    .exec  = cmd_vserial_exec,
};

static int __init cmd_vserial_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_vserial);
}

static void __exit cmd_vserial_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_vserial);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
