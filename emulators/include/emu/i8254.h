/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file i8254.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Definitions related to i8254 PIT
 *
 * This and other PIT related files have largely been adapted
 * from Qemu hw/timer/i8254*
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 */

#ifndef _I8254_EMU_H
#define _I8254_EMU_H

#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_types.h>

#define PIT_FREQ 1193182

typedef struct pit_channel_info {
    int gate;
    int mode;
    int initial_count;
    int out;
} pit_channel_info_t;

typedef struct pit_channel_state {
    int               count; /* can be 65536 */
    uint16_t          latched_count;
    uint8_t           count_latched;
    uint8_t           status_latched;
    uint8_t           status;
    uint8_t           read_state;
    uint8_t           write_state;
    uint8_t           write_latch;
    uint8_t           rw_mode;
    uint8_t           mode;
    uint8_t           bcd;  /* not supported */
    uint8_t           gate; /* timer start */
    int64_t           count_load_time;
    /* irq handling */
    int64_t           next_transition_time;
    vmm_timer_event_t irq_timer;
    vmm_spinlock_t    channel_lock;
    uint32_t          irq;
    uint32_t          irq_disabled;
    struct vmm_guest *guest;
} pit_channel_state_t;

typedef struct pit_common_state {
    pit_channel_state_t channels[3];
} pit_common_state_t;

int     pit_get_out(pit_channel_state_t *s, int64_t current_time);
int64_t pit_get_next_transition_time(pit_channel_state_t *s, int64_t current_time);
void    pit_get_channel_info_common(pit_common_state_t *s, pit_channel_state_t *sc, pit_channel_info_t *info);
void    pit_reset_common(pit_common_state_t *s);
// void pit_set_gate(ISADevice *dev, int channel, int val);
// void pit_get_channel_info(ISADevice *dev, int channel, PITChannelInfo *info);

#endif /* !_I8254_EMU_H */
