#/**
# Copyright (c) 2012 Anup Patel.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of video_terminal_emulate objects to be build
# */

libs-objs-$(CONFIG_VIDEO_TERMINAL_EMULATE)+= video_terminal_emulate/video_terminal_emulate_lib.o

video_terminal_emulate_lib-y+= video_terminal_emulate.o
video_terminal_emulate_lib-y+= video_terminal_emulate_font.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_6x11)+= video_terminal_emulate_font_6x11.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_7x14)+= video_terminal_emulate_font_7x14.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_8x8)+= video_terminal_emulate_font_8x8.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_8x16)+= video_terminal_emulate_font_8x16.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_10x18)+= video_terminal_emulate_font_10x18.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_ACORN_8x8)+= video_terminal_emulate_font_acorn_8x8.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_MINI_8x8)+= video_terminal_emulate_font_mini_4x6.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_PEARL_8x8)+= video_terminal_emulate_font_pearl_8x8.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_SUN8x16)+= video_terminal_emulate_font_sun8x16.o
video_terminal_emulate_lib-$(CONFIG_VIDEO_TERMINAL_EMULATE_FONT_SUN12x22)+= video_terminal_emulate_font_sun12x22.o

%/video_terminal_emulate_lib.o: $(foreach obj,$(video_terminal_emulate_lib-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/video_terminal_emulate_lib.dep: $(foreach dep,$(video_terminal_emulate_lib-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

