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
 * @file netstack.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Stack Interface APIs
 */

#ifndef __NETSTACK_H_
#define __NETSTACK_H_

#include <net/vmm_mbuf.h>
#include <net/vmm_net.h>
#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <vmm_error.h>
#include <vmm_types.h>

#define NETSTACK_IPRIORITY (VMM_NET_CLASS_IPRIORITY + 1)

/**
 * Structure containing the ICMP_ECHO_REPLY parameters
 */
struct netstack_echo_reply {
    uint8_t  ripaddr[4];
    uint8_t  ttl;
    uint16_t len;
    uint16_t seqno;
    uint64_t rtt;
};

/**
 * Possible socket types
 */
enum netstack_socket_type {
    NETSTACK_SOCKET_UNK = 0,
    NETSTACK_SOCKET_TCP = 1,
    NETSTACK_SOCKET_UDP = 2,
};

/**
 * Generic socket wrapper
 */
struct netstack_socket {
    uint8_t                   ipaddr[4];
    uint16_t                  port;
    enum netstack_socket_type type;
    void *private;
};

/**
 * Generic socket buffer wrapper
 * Note: sometimes incoming data in fragment to chain of buffer and
 * underly network stack will receive a chain of buffer
 */
struct netstack_socket_buf {
    void    *data;
    uint16_t len;
    void *private;
};

/**
 * Get the name of network stack.
 */
char *netstack_get_name(void);

/**
 * Obtain IP-settings using DHCP request.
 */
int netstack_dhcp_request(void);

/**
 * Set IP-address of the host.
 */
int netstack_set_ipaddr(uint8_t *ipaddr);

/**
 * Get IP-address of the host.
 */
int netstack_get_ipaddr(uint8_t *ipaddr);

/**
 * Set IP-netmask of the host.
 */
int netstack_set_ipmask(uint8_t *ipmask);

/**
 * Get IP-netmask of the host.
 */
int netstack_get_ipmask(uint8_t *ipmask);

/**
 * Set Gateway IP-address of the host.
 */
int netstack_set_gatewayip(uint8_t *ipaddr);

/**
 * Get Gateway IP-address of the host.
 */
int netstack_get_gatewayip(uint8_t *ipaddr);

/**
 * Get HW-address of the host.
 */
int netstack_get_hwaddr(uint8_t *hwaddr);

/**
 *  Generates an ICMP echo request to a remote host and blocks for
 *  sometime till the reply is received.
 *
 *  @ipaddr - IP address of the remote host
 *  @size - size of the payload inside the ICMP msg
 *  @seqno - sequence-no to be used in the request
 *  @reply - on success, this stores the parameters of echo_reply
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_send_echo(uint8_t *ipaddr, uint16_t size, uint16_t seqno, struct netstack_echo_reply *reply);

/**
 *  This is meant for network-stacks which do not support reliable
 *  arp output processing.
 *
 *  e.g. In case of uIP, if there is no ARP mapping for the destination
 *  ipaddr of an outgoing packet, an ARP request is sent out but the
 *  original packet is discarded. In such cases this hint will allow to
 *  either refresh an existing ARP entry or prefetch the required ARP
 *  mapping (by sending out ARP-request) to avoid discards.
 *
 *  @ipaddr - IP address whose ARP mapping is to be prefetched/refreshed
 */
void netstack_prefetch_arp_mapping(uint8_t *ipaddr);

/**
 *  Allocate or create a new socket.
 *
 *  @type - type of socket
 *
 *  returns pointer to newly created socket
 */
struct netstack_socket *netstack_socket_alloc(enum netstack_socket_type type);

/**
 *  Bind a socket with a port number.
 *
 *  @sk - pointer to socket
 *  @ipaddr - Allowable IP addresses. NULL means any IP address
 *  @port - port number on which we will listen
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_bind(struct netstack_socket *sk, uint8_t *ipaddr, uint16_t port);

/**
 *  Connect socket to a remote host.
 *
 *  @sk - pointer to socket
 *  @ipaddr - IP address of remote host
 *  @port - port number of remote host
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_connect(struct netstack_socket *sk, uint8_t *ipaddr, uint16_t port);

/**
 *  Disconnect a socket from remote host
 *
 *  @sk - pointer to socket
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_disconnect(struct netstack_socket *sk);

/**
 *  Listen or wait for incoming connection to a socket.
 *
 *  @sk - pointer to socket
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_listen(struct netstack_socket *sk);

/**
 *  Accept a new incoming connection from a listening socket.
 *
 *  @sk - pointer to socket
 *  @new_sk - pointer to socket for new connection.
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_accept(struct netstack_socket *sk, struct netstack_socket **new_sk);

/**
 *  Close a listening socket.
 *  OR
 *  Close an accepted socket.
 *
 *  @sk - pointer to socket
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_close(struct netstack_socket *sk);

/**
 *  Free alloced socket
 *  OR
 *  Free accepted socket.
 *
 *  @sk - pointer to socket
 */
void netstack_socket_free(struct netstack_socket *sk);

/**
 *  Recieve data from a socket to socket buffer.
 *
 *  @sk - pointer to socket
 *  @buf - pointer to socket buffer
 *  @timeout - timeout in milliseconds (<=0 means wait forever)
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_recv(struct netstack_socket *sk, struct netstack_socket_buf *buf, int timeout);

/**
 *  Traverse chain of socket buffer.
 *
 *  @buf - pointer to socket buffer
 *
 *  returns
 *    VMM_OK - success
 *    VMM_ETIMEDOUT - upon receive timeout
 *    VMM_Exxxx - failure (this is no futher buffers)
 */
int netstack_socket_nextbuf(struct netstack_socket_buf *buf);

/**
 *  Free the socket buffer.
 *
 *  @buf - pointer to socket buffer
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure (this is no futher buffers)
 */
void netstack_socket_freebuf(struct netstack_socket_buf *buf);

/**
 *  Write data to a socket
 *
 *  @sk - pointer to socket
 *  @data - pointer to data
 *  @len  - length of data
 *
 *  returns
 *    VMM_OK - success
 *    VMM_Exxxx - failure
 */
int netstack_socket_write(struct netstack_socket *sk, void *data, uint16_t len);

#endif /* __VMM_NETSTACK_H_ */
