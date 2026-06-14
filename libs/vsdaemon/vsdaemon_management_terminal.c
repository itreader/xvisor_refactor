/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vsdaemon_management_terminal.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon management_terminal transport implementation
 */

#include <libs/fifo.h>
#include <libs/vsdaemon.h>
#include <vmm_char_device.h>
#include <vmm_command_manager.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "vsdaemon management_terminal transport"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VSDAEMON_IPRIORITY + 1)
#define MODULE_INIT      vsdaemon_management_terminal_init
#define MODULE_EXIT      vsdaemon_management_terminal_exit

struct vsdaemon_management_terminal {
    /* pointer to original vsdaemon */
    struct vsdaemon *vsd;

    /* command buffer */
    char cmds[CONFIG_VSDAEMON_MTERM_CMD_WIDTH];

    /* dummy character device */
    vmm_char_device_t cdev;

    /* rx fifo and completion */
    struct fifo     *rx_fifo;
    vmm_completion_t rx_avail;

#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
    /* stdio history */
    struct vmm_history history;
#endif
};

static uint32_t vsdaemon_management_terminal_char_device_write(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t __unused *off, bool sleep)
{
    struct vsdaemon_management_terminal *vmanagement_terminal;

    if (!(cdev && src && cdev->private)) {
        return 0;
    }

    vmanagement_terminal = cdev->private;

    return vmm_vserial_send(vmanagement_terminal->vsd->vser, src, len);
}

static uint32_t vsdaemon_management_terminal_char_device_read(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t __unused *off, bool sleep)
{
    uint32_t                             i;
    struct vsdaemon_management_terminal *vmanagement_terminal;

    if (!(cdev && dest && cdev->private)) {
        return 0;
    }

    vmanagement_terminal = cdev->private;

    if (sleep) {
        for (i = 0; i < len; i++) {
            while (!fifo_dequeue(vmanagement_terminal->rx_fifo, &dest[i])) {
                vmm_completion_wait(&vmanagement_terminal->rx_avail);
            }
        }
    } else {
        for (i = 0; i < len; i++) {
            if (!fifo_dequeue(vmanagement_terminal->rx_fifo, &dest[i])) {
                break;
            }
        }
    }

    return i;
}

static bool vsdaemon_management_terminal_cmd_filter(vmm_char_device_t *cdev, int argc, char **argv)
{
    if ((argc > 1) && !strcmp(argv[0], "vserial") && (!strcmp(argv[1], "bind") || !strcmp(argv[1], "dump"))) {
        /* Filter out "vserial bind" and "vserial dump" commands */
        return TRUE;
    }

    return FALSE;
}

static void vsdaemon_management_terminal_receive_char(struct vsdaemon *vsd, uint8_t ch)
{
    struct vsdaemon_management_terminal *vmanagement_terminal = vsdaemon_transport_get_data(vsd);

    fifo_enqueue(vmanagement_terminal->rx_fifo, &ch, FALSE);

    vmm_completion_complete(&vmanagement_terminal->rx_avail);
}

static int vsdaemon_management_terminal_main_loop(struct vsdaemon *vsd)
{
    size_t                               cmds_len;
    struct vsdaemon_management_terminal *vmanagement_terminal = vsdaemon_transport_get_data(vsd);

    while (1) {
        vmm_cdev_printf(&vmanagement_terminal->cdev, "XVisor# ");

        memset(vmanagement_terminal->cmds, 0, sizeof(vmanagement_terminal->cmds));

        /* Get command string */
#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
        vmm_cgets(
            &vmanagement_terminal->cdev, vmanagement_terminal->cmds, CONFIG_VSDAEMON_MTERM_CMD_WIDTH, '\n', &vmanagement_terminal->history, TRUE);
#else
        vmm_cgets(&vmanagement_terminal->cdev, vmanagement_terminal->cmds, CONFIG_VSDAEMON_MTERM_CMD_WIDTH, '\n', NULL, TRUE);
#endif

        /* Process command string */
        cmds_len = strlen(vmanagement_terminal->cmds);

        if (cmds_len > 0) {
            if (vmanagement_terminal->cmds[cmds_len - 1] == '\r') {
                vmanagement_terminal->cmds[cmds_len - 1] = '\0';
            }

            vmm_command_manager_execute_cmdstr(&vmanagement_terminal->cdev, vmanagement_terminal->cmds, vsdaemon_management_terminal_cmd_filter);
        }
    }

    return VMM_OK;
}

static int vsdaemon_management_terminal_setup(struct vsdaemon *vsd, int argc, char **argv)
{
    struct vsdaemon_management_terminal *vmanagement_terminal;

    vmanagement_terminal = vmm_zalloc(sizeof(*vmanagement_terminal));

    if (!vmanagement_terminal) {
        return VMM_ERR_NOMEM;
    }

    vmanagement_terminal->vsd = vsd;

    strncpy(vmanagement_terminal->cdev.name, vsd->name, sizeof(vmanagement_terminal->cdev.name));
    vmanagement_terminal->cdev.read    = vsdaemon_management_terminal_char_device_read;
    vmanagement_terminal->cdev.write   = vsdaemon_management_terminal_char_device_write;
    vmanagement_terminal->cdev.private = vmanagement_terminal;

    vmanagement_terminal->rx_fifo      = fifo_alloc(1, CONFIG_VSDAEMON_MTERM_CMD_WIDTH);

    if (!vmanagement_terminal->rx_fifo) {
        vmm_free(vmanagement_terminal);
        return VMM_ERR_NOMEM;
    }

    INIT_COMPLETION(&vmanagement_terminal->rx_avail);

#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
    INIT_HISTORY(&vmanagement_terminal->history, CONFIG_VSDAEMON_MTERM_HISTORY_SIZE, CONFIG_VSDAEMON_MTERM_CMD_WIDTH);
#endif

    vsdaemon_transport_set_data(vsd, vmanagement_terminal);

    return VMM_OK;
}

static void vsdaemon_management_terminal_cleanup(struct vsdaemon *vsd)
{
    struct vsdaemon_management_terminal *vmanagement_terminal = vsdaemon_transport_get_data(vsd);

    vsdaemon_transport_set_data(vsd, NULL);

#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
    CLEANUP_HISTORY(&vmanagement_terminal->history);
#endif

    fifo_free(vmanagement_terminal->rx_fifo);

    vmm_free(vmanagement_terminal);
}

static struct vsdaemon_transport management_terminal = {
    .name         = "management_terminal",
    .setup        = vsdaemon_management_terminal_setup,
    .cleanup      = vsdaemon_management_terminal_cleanup,
    .main_loop    = vsdaemon_management_terminal_main_loop,
    .receive_char = vsdaemon_management_terminal_receive_char,
};

static int __init vsdaemon_management_terminal_init(void)
{
    return vsdaemon_transport_register(&management_terminal);
}

static void __exit vsdaemon_management_terminal_exit(void)
{
    vsdaemon_transport_unregister(&management_terminal);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
