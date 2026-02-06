/*
 * Copyright (c) 2016 Open Wide
 * Copyright (c) 2016 Institut de Recherche Technologique SystemX
 *
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *
 * @file mxc-hdmi-core.h
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 */
#ifndef __LINUX_MXC_HDMI_CORE_H_
#define __LINUX_MXC_HDMI_CORE_H_

uint8_t  hdmi_readb(uint32_t reg);
void     hdmi_writeb(uint8_t value, uint32_t reg);
void     hdmi_mask_writeb(uint8_t data, uint32_t addr, uint8_t shift, uint8_t mask);
uint32_t hdmi_read4(uint32_t reg);
void     hdmi_write4(uint32_t value, uint32_t reg);

void     hdmi_irq_init(void);
void     hdmi_irq_enable(int irq);
uint32_t hdmi_irq_disable(int irq);

void hdmi_set_sample_rate(uint32_t rate);
void hdmi_set_dma_mode(uint32_t dma_running);
void hdmi_init_clock_regenerator(void);
void hdmi_clock_regenerator_update_pixel_clock(uint32_t pixclock);

void hdmi_set_edid_cfg(struct mxc_edid_cfg *cfg);
void hdmi_get_edid_cfg(struct mxc_edid_cfg *cfg);

extern int mxc_hdmi_ipu_id;
extern int mxc_hdmi_disp_id;

void hdmi_set_registered(int registered);
int  hdmi_get_registered(void);

#if 0
int mxc_hdmi_register_audio(struct snd_pcm_substream *substream);
void mxc_hdmi_unregister_audio(struct snd_pcm_substream *substream);
#endif

uint32_t hdmi_set_cable_state(uint32_t state);
uint32_t hdmi_set_blank_state(uint32_t state);
int      mxc_hdmi_abort_stream(void);

int check_hdmi_state(void);

#endif
