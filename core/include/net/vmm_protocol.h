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
 * @file vmm_protocol.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Helper utils for various network protocols
 *
 * Portions of this file have been adapted from linux source header
 * include/linux/etherdevice.h which is licensed under GPLv2:
 *
 * Original authors:
 *  Ross Biro
 *  Fred N. van Kempen <waltje@uWalt.NL.Mugnet.ORG>
 *  Alan Cox <gw4pts@gw4pts.ampr.org>
 */

#ifndef __VMM_PROTOCOL_H_
#define __VMM_PROTOCOL_H_

#include <libs/stringlib.h>
#include <vmm_host_io.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_types.h>

/**
 * is_zero_ether_addr - Determine if give Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 */
static inline int is_zero_ether_addr(const uint8_t *addr)
{
    return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

/**
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline int is_multicast_ether_addr(const uint8_t *addr)
{
    return 0x01 & addr[0];
}

/**
 * is_local_ether_addr - Determine if the Ethernet address is locally-assigned one (IEEE 802).
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a local address.
 */
static inline int is_local_ether_addr(const uint8_t *addr)
{
    return 0x02 & addr[0];
}

/**
 * is_broadcast_ether_addr - Determine if the Ethernet address is broadcast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is the broadcast address.
 */
static inline int is_broadcast_ether_addr(const uint8_t *addr)
{
    return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}

/**
 * is_unicast_ether_addr - Determine if the Ethernet address is unicast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a unicast address.
 */
static inline int is_unicast_ether_addr(const uint8_t *addr)
{
    return !is_multicast_ether_addr(addr);
}

/**
 * is_valid_ether_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * Return true if the address is valid.
 */
static inline int is_valid_ether_addr(const uint8_t *addr)
{
    /* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
     * explicitly check for it here. */
    return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

static inline void get_random_bytes(uint8_t *buf, int len)
{
    uint64_t tstamp;
    int      off = 0;

    while (len > 0) {
        tstamp = vmm_timer_timestamp();

        if (len < sizeof(uint64_t)) {
            memcpy(buf + off, &tstamp, len);
            off += len;
            len -= len;
        } else {
            memcpy(buf + off, &tstamp, sizeof(uint64_t));
            off += sizeof(uint64_t);
            len -= sizeof(uint64_t);
        }
    }
}

/**
 * random_ether_addr - Generate software assigned random Ethernet address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
static inline void random_ether_addr(uint8_t *addr)
{
    get_random_bytes(addr, 6);
    addr[0] &= 0xfe; /* clear multicast bit */
    addr[0] |= 0x02; /* set local assignment bit (IEEE802) */
}

/**
 * compare_ether_addr - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two ethernet addresses, returns 0 if equal
 */
static inline unsigned compare_ether_addr(const uint8_t *addr1, const uint8_t *addr2)
{
    const uint16_t *a = (const uint16_t *)addr1;
    const uint16_t *b = (const uint16_t *)addr2;

    return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

/**
 * ethaddr_to_str - Convert an ethernet address to string
 *
 * @str: Destination resultant string
 * @addr: ethernet address in network byte order (bug-endian)
 *
 * Returns str
 */
static inline char *ethaddr_to_str(char *str, const uint8_t *addr)
{
    vmm_sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return str;
}

/**
 * ipddr_to_str - Convert an ipv4 address to string
 *
 * @str: Destination resultant string
 * @addr: IPv4 address in network byte order (bug-endian)
 *
 * Returns str
 */
static inline char *ip4addr_to_str(char *str, const uint8_t *addr)
{
    vmm_sprintf(str, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
    return str;
}

struct eth_header {
    uint8_t  dstmac[6];
    uint8_t  srcmac[6];
    uint16_t ethertype;
    uint8_t  payload[0];
} __packed;

#define ETHER_HLEN                 (sizeof(struct eth_header))

#define ether_srcmac(ether_frame)  (((struct eth_header *)(ether_frame))->srcmac)
#define ether_dstmac(ether_frame)  (((struct eth_header *)(ether_frame))->dstmac)
#define ether_type(ether_frame)    vmm_be16_to_cpu(((struct eth_header *)(ether_frame))->ethertype)
#define ether_payload(ether_frame) (((struct eth_header *)(ether_frame))->payload)

struct ip_header {
    uint8_t  vhl;
    uint8_t  tos;
    uint16_t len;
    uint16_t ipid;
    uint16_t ipoffset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t ipchksum;
    uint8_t  srcipaddr[4];
    uint8_t  dstipaddr[4];
    uint8_t  payload[0];
} __packed;

#define IP4_HLEN              (sizeof(struct ip_header))

#define ip_srcaddr(ip_frame)  (((struct ip_header *)(ip_frame))->srcipaddr)
#define ip_dstaddr(ip_frame)  (((struct ip_header *)(ip_frame))->dstipaddr)
#define ip_ttl(ip_frame)      (((struct ip_header *)(ip_frame))->ttl)
#define ip_protocol(ip_frame) (((struct ip_header *)(ip_frame))->protocol)
#define ip_len(ip_frame)      vmm_be16_to_cpu(((struct ip_header *)(ip_frame))->len)
#define ip_chksum(ip_frame)   vmm_be16_to_cpu(((struct ip_header *)(ip_frame))->ipchksum)
#define ip_payload(ip_frame)  (((struct ip_header *)(ip_frame))->payload)

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    uint8_t  payload[0];
} __packed;

#define ICMP_HLEN                 (sizeof(struct icmp_header))

#define icmp_type(icmp_frame)     (((struct icmp_header *)(icmp_frame))->type)
#define icmp_code(icmp_frame)     (((struct icmp_header *)(icmp_frame))->code)
#define icmp_checksum(icmp_frame) vmm_be16_to_cpu(((struct icmp_header *)(icmp_frame))->checksum)
#define icmp_id(icmp_frame)       vmm_be16_to_cpu(((struct icmp_header *)(icmp_frame))->id)
#define icmp_sequence(icmp_frame) vmm_be16_to_cpu(((struct tcp_header *)(icmp_frame))->sequence)
#define icmp_payload(icmp_frame)  (((struct icmp_header *)(icmp_frame))->payload)

struct tcp_header {
    uint16_t srcport;
    uint16_t dstport;
    uint32_t sequence;
    uint32_t acknumber;
    uint16_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
    uint8_t  payload[0];
} __packed;

#define TCP_HLEN                 (sizeof(struct tcp_header))

#define tcp_srcport(tcp_frame)   vmm_be16_to_cpu(((struct tcp_header *)(tcp_frame))->srcport)
#define tcp_dstport(tcp_frame)   vmm_be16_to_cpu(((struct tcp_header *)(tcp_frame))->dstport)
#define tcp_sequence(tcp_frame)  vmm_be32_to_cpu(((struct tcp_header *)(tcp_frame))->sequence)
#define tcp_acknumber(tcp_frame) vmm_be32_to_cpu(((struct tcp_header *)(tcp_frame))->acknumber)
#define tcp_flags(tcp_frame)     vmm_be16_to_cpu(((struct tcp_header *)(tcp_frame))->flags)
#define tcp_window(tcp_frame)    vmm_be16_to_cpu(((struct tcp_header *)(tcp_frame))->window)
#define tcp_checksum(tcp_frame)  vmm_be16_to_cpu(((struct tcp_header *)(tcp_frame))->checksum)
#define tcp_urgent(tcp_frame)    vmm_be16_to_cpu(((struct tcp_header *)(tcp_frame))->urgent)
#define tcp_payload(tcp_frame)   (((struct tcp_header *)(tcp_frame))->payload)

struct arp_header {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint8_t  spa[4];
    uint8_t  tha[6];
    uint8_t  tpa[4];
} __packed;

#define ARP_HLEN             (sizeof(struct arp_header))

#define arp_htype(arp_frame) vmm_be16_to_cpu(((struct arp_header *)(arp_frame))->htype)
#define arp_ptype(arp_frame) vmm_be16_to_cpu(((struct arp_header *)(arp_frame))->ptype)
#define arp_hlen(arp_frame)  (((struct arp_header *)(arp_frame))->hlen)
#define arp_plen(arp_frame)  (((struct arp_header *)(arp_frame))->plen)
#define arp_oper(arp_frame)  vmm_be16_to_cpu(((struct arp_header *)(arp_frame))->oper)
#define arp_sha(arp_frame)   (((struct arp_header *)(arp_frame))->sha)
#define arp_spa(arp_frame)   (((struct arp_header *)(arp_frame))->spa)
#define arp_tha(arp_frame)   (((struct arp_header *)(arp_frame))->tha)
#define arp_tpa(arp_frame)   (((struct arp_header *)(arp_frame))->tpa)

#define IN_CLASSA(a)         ((((uint8_t *)(a))[0] & 0x80) == 0)
#define IN_CLASSB(a)         ((((uint8_t *)(a))[0] & 0xc0) == 0x80)
#define IN_CLASSC(a)         ((((uint8_t *)(a))[0] & 0xe0) == 0xc0)
#define IN_CLASSD(a)         ((((uint8_t *)(a))[0] & 0xf0) == 0xe0)
#define IN_MULTICAST(a)      IN_CLASSD(a)
#define IN_EXPERIMENTAL(a)   ((((uint8_t *)(a))[0] & 0xf0) == 0xf0)
#define IN_BADCLASS(a)       IN_EXPERIMENTAL((a))

static inline bool ipv4_is_zeronet(const uint8_t *addr)
{
    return (*addr == 0x00000000);
}

#ifndef htonl
#define htonl vmm_cpu_to_be32
#define ntohl htonl
#endif

static inline int ipv4_class_netmask(const uint8_t *addr, uint8_t *mask)
{
    if (!ipv4_is_zeronet(addr)) {
        if (IN_CLASSA(addr)) {
            mask[0] = 0xff;
            mask[1] = mask[2] = mask[3] = 0;
        } else if (IN_CLASSB(addr)) {
            mask[0] = mask[1] = 0xff;
            mask[2] = mask[3] = 0;
        } else if (IN_CLASSC(addr)) {
            mask[0] = mask[1] = mask[2] = 0xff;
            mask[3]                     = 0;
        } else { /* Something else, probably a multicast. */
            return -1;
        }
    }

    return 0;
}

#endif /* __VMM_PROTOCOL_H_ */
