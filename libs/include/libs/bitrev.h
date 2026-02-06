/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Code from Linux kernel 3.16, originally from
 * Akinobu Mita <akinobu.mita@gmail.com>.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file bitrev.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Bit ordering reversal functions
 */
#ifndef __BITREV_H__
#define __BITREV_H__

#include <vmm_types.h>

extern uint8_t const byte_rev_table[256];

static inline uint8_t bitrev8(uint8_t byte)
{
    return byte_rev_table[byte];
}

extern uint16_t bitrev16(uint16_t in);
extern uint32_t bitrev32(uint32_t in);

#endif /* __BITREV_H__ */
