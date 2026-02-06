#/**
# Copyright (c) 2010 Anup Patel.
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
# @brief list of command objects to be build
# */

commands-objs-$(CONFIG_CMD_VERSION)+= cmd_version.o
commands-objs-$(CONFIG_CMD_SLEEP)+= cmd_sleep.o
commands-objs-$(CONFIG_CMD_ECHO)+= cmd_echo.o
commands-objs-$(CONFIG_CMD_RESET)+= cmd_reset.o
commands-objs-$(CONFIG_CMD_SHUTDOWN)+= cmd_shutdown.o
commands-objs-$(CONFIG_CMD_HOST)+= cmd_host.o
commands-objs-$(CONFIG_CMD_IOMMU)+= cmd_iommu.o
commands-objs-$(CONFIG_CMD_DEVTREE)+= cmd_device_tree.o
commands-objs-$(CONFIG_CMD_VCPU)+= cmd_vcpu.o
commands-objs-$(CONFIG_CMD_GUEST)+= cmd_guest.o
commands-objs-$(CONFIG_CMD_MEMORY)+= cmd_memory.o
commands-objs-$(CONFIG_CMD_SHMEM)+= cmd_share_memory.o
commands-objs-$(CONFIG_CMD_THREAD)+= cmd_thread.o
commands-objs-$(CONFIG_CMD_CHARDEV)+= cmd_char_device.o
commands-objs-$(CONFIG_CMD_STDIO)+= cmd_stdio.o
commands-objs-$(CONFIG_CMD_HEAP)+= cmd_heap.o
commands-objs-$(CONFIG_CMD_WALLCLOCK)+= cmd_wall_clock.o
commands-objs-$(CONFIG_CMD_MODULE)+= cmd_module.o
commands-objs-$(CONFIG_CMD_PROFILE)+= cmd_profile.o

commands-objs-$(CONFIG_CMD_VMSG)+= cmd_vmsg.o
commands-objs-$(CONFIG_CMD_VSERIAL)+= cmd_vserial.o
commands-objs-$(CONFIG_CMD_VDISK)+= cmd_virtual_disk.o
commands-objs-$(CONFIG_CMD_VDISPLAY)+= cmd_virtual_display.o
commands-objs-$(CONFIG_CMD_VINPUT)+= cmd_virtual_input.o
commands-objs-$(CONFIG_CMD_VSCREEN)+= cmd_virtual_screen.o

commands-objs-$(CONFIG_CMD_CLK)+= cmd_clock.o
commands-objs-$(CONFIG_CMD_RTCDEV)+= cmd_rtcdev.o
commands-objs-$(CONFIG_CMD_INPUT)+= cmd_input.o
commands-objs-$(CONFIG_CMD_GPIO)+= cmd_gpio.o
commands-objs-$(CONFIG_CMD_FB)+= cmd_frame_buffer.o
commands-objs-$(CONFIG_CMD_FB_BACKLIGHT)+= cmd_backlight.o
commands-objs-$(CONFIG_CMD_BLOCK_DEVICE)+= cmd_block_device.o
commands-objs-$(CONFIG_CMD_RBD)+= cmd_ram_backed_device.o
commands-objs-$(CONFIG_CMD_FLASH)+= cmd_flash.o
commands-objs-$(CONFIG_CMD_I2C)+= cmd_i2c.o
commands-objs-$(CONFIG_CMD_SPI_DEV)+= cmd_spi_device.o

commands-objs-$(CONFIG_CMD_NET)+= cmd_net.o
commands-objs-$(CONFIG_CMD_IPCONFIG)+= cmd_ipconfig.o
commands-objs-$(CONFIG_CMD_PING)+= cmd_ping.o
commands-objs-$(CONFIG_CMD_MII)+= cmd_mii.o

commands-objs-$(CONFIG_CMD_VSDAEMON)+= cmd_vsdaemon.o
commands-objs-$(CONFIG_CMD_VFS)+= cmd_vfs.o
commands-objs-$(CONFIG_CMD_WBOXTEST)+= cmd_wboxtest.o
