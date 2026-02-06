#ifndef _LINUX_SEQ_FILE_H
#define _LINUX_SEQ_FILE_H

#include <vmm_char_device.h>
#include <vmm_stdio.h>

#define seq_file              vmm_char_device

#define seq_printf(s, msg...) vmm_cdev_printf(s, msg)

#define seq_putc(s, ch)       vmm_cdev_putc(s, ch)

#define seq_puts(s, str)      vmm_cdev_puts(s, str)

#endif
