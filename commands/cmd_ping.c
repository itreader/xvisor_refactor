/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file cmd_ping.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Implementation of ping command
 */

#include <libs/mathlib.h>
#include <libs/netstack.h>
#include <libs/stringlib.h>
#include <net/vmm_protocol.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command ping"
#define MODULE_AUTHOR    "Sukanto Ghosh"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_ping_init
#define MODULE_EXIT      cmd_ping_exit

static void cmd_ping_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage: ");
    vmm_cdev_printf(cdev, "   ping <ipaddr> [<count>] [<size>]\n");
}

static int cmd_ping_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    uint16_t                   sent, rcvd, count = 1, size = 56;
    struct netstack_echo_reply reply;
    char                       ip_addr_str[20];
    uint32_t                   rtt_usecs, rtt_msecs;
    uint64_t                   min_rtt = -1, max_rtt = 0, avg_rtt = 0;
    uint8_t                    ipaddr[4];

    if ((argc < 2) || (argc > 4)) {
        cmd_ping_usage(cdev);
        return VMM_EFAIL;
    }

    if (argc > 2) {
        count = atoi(argv[2]);
    }

    if (argc > 3) {
        size = atoi(argv[3]);
    }

    str2ipaddr(ipaddr, argv[1]);

    vmm_cdev_printf(cdev, "PING (%s) %d(%zu) bytes of data.\n", argv[1], size, (size + IP4_HLEN + ICMP_HLEN));

    netstack_prefetch_arp_mapping(ipaddr);

    for (sent = 0, rcvd = 0; sent < count; sent++) {
        if (!netstack_send_echo(ipaddr, size, sent, &reply)) {
            if (reply.rtt < min_rtt) {
                min_rtt = reply.rtt;
            }

            if (reply.rtt > max_rtt) {
                max_rtt = reply.rtt;
            }

            avg_rtt += reply.rtt;
            rtt_msecs = udiv64(reply.rtt, 1000);
            rtt_usecs = umod64(reply.rtt, 1000);
            ip4addr_to_str(ip_addr_str, (const uint8_t *)&reply.ripaddr);
            vmm_cdev_printf(
                cdev,
                "%d bytes from %s: seq=%d "
                "ttl=%d time=%d.%03dms\n",
                reply.len, ip_addr_str, reply.seqno, reply.ttl, rtt_msecs, rtt_usecs);
            rcvd++;
        }
    }

    if (min_rtt == -1) {
        min_rtt = 0;
    }

    if (rcvd) {
        avg_rtt = udiv64(avg_rtt, rcvd);
    } else {
        avg_rtt = 0;
    }

    vmm_cdev_printf(cdev, "\n----- %s ping statistics -----\n", argv[1]);
    vmm_cdev_printf(cdev, "%d packets transmitted, %d packets received\n", sent, rcvd);
    vmm_cdev_printf(cdev, "round-trip min/avg/max = ");
    rtt_msecs = udiv64(min_rtt, 1000);
    rtt_usecs = umod64(min_rtt, 1000);
    vmm_cdev_printf(cdev, "%d.%03d/", rtt_msecs, rtt_usecs);
    rtt_msecs = udiv64(avg_rtt, 1000);
    rtt_usecs = umod64(avg_rtt, 1000);
    vmm_cdev_printf(cdev, "%d.%03d/", rtt_msecs, rtt_usecs);
    rtt_msecs = udiv64(max_rtt, 1000);
    rtt_usecs = umod64(max_rtt, 1000);
    vmm_cdev_printf(cdev, "%d.%03d ms\n", rtt_msecs, rtt_usecs);

    return VMM_OK;
}

static vmm_command_t cmd_ping = {
    .name  = "ping",
    .desc  = "ping target machine on network",
    .usage = cmd_ping_usage,
    .exec  = cmd_ping_exec,
};

static int __init cmd_ping_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_ping);
}

static void __exit cmd_ping_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_ping);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
