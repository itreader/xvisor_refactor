/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file roothubdesc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for virtual Root Hub.
 *
 * This header is largely adapted from u-boot sources:
 * <u-boot>/include/usbroothubdes.h
 *
 * USB virtual root hub descriptors
 *
 * (C) Copyright 2014
 * Stephen Warren swarren@wwwdotorg.org
 *
 * Based on ohci-hcd.c
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __USB_ROOTHUBDESC_H__
#define __USB_ROOTHUBDESC_H__

#include <vmm_types.h>

/* Device descriptor */
static uint8_t root_hub_dev_desc[] = {
    0x12,       /* uint8_t  bLength; */
    0x01,       /* uint8_t  bDescriptorType; Device */
    0x10,       /* uint16_t bcdUSB; v1.1 */
    0x01, 0x09, /* uint8_t  bDeviceClass; HUB_CLASSCODE */
    0x00,       /* uint8_t  bDeviceSubClass; */
    0x00,       /* uint8_t  bDeviceProtocol; */
    0x08,       /* uint8_t  bMaxPacketSize0; 8 Bytes */
    0x00,       /* uint16_t idVendor; */
    0x00, 0x00, /* uint16_t idProduct; */
    0x00, 0x00, /* uint16_t bcdDevice; */
    0x00, 0x01, /* uint8_t  iManufacturer; */
    0x02,       /* uint8_t  iProduct; */
    0x03,       /* uint8_t  iSerialNumber; */
    0x01,       /* uint8_t  bNumConfigurations; */
};

/* Configuration descriptor */
static uint8_t root_hub_config_desc[] = {
    0x09,       /* uint8_t  bLength; */
    0x02,       /* uint8_t  bDescriptorType; Configuration */
    0x19,       /* uint16_t wTotalLength; */
    0x00, 0x01, /* uint8_t  bNumInterfaces; */
    0x01,       /* uint8_t  bConfigurationValue; */
    0x00,       /* uint8_t  iConfiguration; */
    0x40,       /* uint8_t  bmAttributes;
                 *       Bit 7: Bus-powered
                 *       6: Self-powered,
                 *       5 Remote-wakwup,
                 *       4..0: resvd
                 */
    0x00,       /* uint8_t  MaxPower; */
    /* interface */
    0x09, /* uint8_t  if_bLength; */
    0x04, /* uint8_t  if_bDescriptorType; Interface */
    0x00, /* uint8_t  if_bInterfaceNumber; */
    0x00, /* uint8_t  if_bAlternateSetting; */
    0x01, /* uint8_t  if_bNumEndpoints; */
    0x09, /* uint8_t  if_bInterfaceClass; HUB_CLASSCODE */
    0x00, /* uint8_t  if_bInterfaceSubClass; */
    0x00, /* uint8_t  if_bInterfaceProtocol; */
    0x00, /* uint8_t  if_iInterface; */
    /* endpoint */
    0x07,       /* uint8_t  ep_bLength; */
    0x05,       /* uint8_t  ep_bDescriptorType; Endpoint */
    0x81,       /* uint8_t  ep_bEndpointAddress; IN Endpoint 1 */
    0x03,       /* uint8_t  ep_bmAttributes; Interrupt */
    0x02,       /* uint16_t ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
    0x00, 0xff, /* uint8_t  ep_bInterval; 255 ms */
};

#ifdef WANT_USB_ROOT_HUB_HUB_DESC
static uint8_t root_hub_hub_desc[] = {
    0x09,       /* uint8_t  bLength; */
    0x29,       /* uint8_t  bDescriptorType; Hub-descriptor */
    0x02,       /* uint8_t  bNbrPorts; */
    0x00,       /* uint16_t wHubCharacteristics; */
    0x00, 0x01, /* uint8_t  bPwrOn2pwrGood; 2ms */
    0x00,       /* uint8_t  bHubContrCurrent; 0 mA */
    0x00,       /* uint8_t  DeviceRemovable; *** 7 Ports max *** */
    0xff,       /* uint8_t  PortPwrCtrlMask; *** 7 ports max *** */
};
#endif

static uint8_t root_hub_str_index0[] = {
    0x04, /* uint8_t  bLength; */
    0x03, /* uint8_t  bDescriptorType; String-descriptor */
    0x09, /* uint8_t  lang ID */
    0x04, /* uint8_t  lang ID */
};

static uint8_t root_hub_str_index1[] = {
    14,   /* uint8_t  bLength; */
    0x03, /* uint8_t  bDescriptorType; String-descriptor */
    'X',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'v',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'i',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    's',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'o',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'r',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
};

static uint8_t root_hub_str_index2[] = {
    32,   /* uint8_t  bLength; */
    0x03, /* uint8_t  bDescriptorType; String-descriptor */
    'X',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'v',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'i',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    's',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'o',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'r',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    ' ',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'R',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'o',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'o',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    't',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    ' ',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'H',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'u',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    'b',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
};

static uint8_t root_hub_str_index3[] = {
    10,   /* uint8_t  bLength; */
    0x03, /* uint8_t  bDescriptorType; String-descriptor */
    '0',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    '0',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    '0',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
    '0',  /* uint8_t  Unicode */
    0,    /* uint8_t  Unicode */
};

#endif
