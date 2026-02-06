/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_rtcdev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of rtcdev command
 */

#include <drv/rtc.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command rtcdev"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (RTC_DEVICE_CLASS_IPRIORITY + 1)
#define MODULE_INIT      cmd_rtcdev_init
#define MODULE_EXIT      cmd_rtcdev_exit

static void cmd_rtcdev_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   rtcdev help\n");
    vmm_cdev_printf(cdev, "   rtcdev list\n");
    vmm_cdev_printf(cdev, "   rtcdev sync_wall_clock <rtc_name>\n");
    vmm_cdev_printf(cdev, "   rtcdev sync_device <rtc_name>\n");
    vmm_cdev_printf(cdev, "   rtcdev get_time <rtc_name>\n");
    vmm_cdev_printf(
        cdev, "   rtcdev set_time <rtc_name> "
              "<hour>:<min>:<sec> <day> <month> <year>\n");
    vmm_cdev_printf(cdev, "Note:\n");
    vmm_cdev_printf(
        cdev, "   RTC devices keep track of "
              "time in UTC/GMT timezone only\n");
    vmm_cdev_printf(cdev, "   <hour>    = any value between 0..23\n");
    vmm_cdev_printf(cdev, "   <minute>  = any value between 0..59\n");
    vmm_cdev_printf(cdev, "   <second>  = any value between 0..59\n");
    vmm_cdev_printf(cdev, "   <day>     = any value between 0..31\n");
    vmm_cdev_printf(
        cdev, "   <month>   = Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|"
              "Sep|Oct|Nov|Dec\n");
    vmm_cdev_printf(cdev, "   <year>    = any value greater than 1970\n");
}

static int cmd_rtcdev_list_iter(struct rtc_device *rd, void *data)
{
    int                rc;
    char               path[256];
    vmm_char_device_t *cdev = data;

    if (rd->dev.parent && rd->dev.parent->of_node) {
        rc = vmm_device_tree_getpath(path, sizeof(path), rd->dev.parent->of_node);

        if (rc) {
            vmm_snprintf(path, sizeof(path), "----- (error %d)", rc);
        }
    } else {
        strcpy(path, "-----");
    }

    vmm_cdev_printf(cdev, " %-24s %-53s\n", rd->name, path);

    return VMM_OK;
}

static void cmd_rtcdev_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-24s %-53s\n", "Name", "Device Path");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    rtc_device_iterate(NULL, cdev, cmd_rtcdev_list_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_rtcdev_sync_wall_clock(vmm_char_device_t *cdev, const char *name)
{
    int                rc;
    struct rtc_device *rtc = rtc_device_find(name);

    if (!rtc) {
        vmm_cdev_printf(cdev, "Error: cannot find rtc %s\n", name);
        return VMM_EFAIL;
    }

    rc = rtc_device_sync_wall_clock(rtc);

    if (rc) {
        vmm_cdev_printf(
            cdev,
            "Error: sync_wall_clock failed "
            "for rtc %s\n",
            name);
        return rc;
    }

    return VMM_OK;
}

static int cmd_rtcdev_sync_device(vmm_char_device_t *cdev, const char *name)
{
    int                rc;
    struct rtc_device *rtc = rtc_device_find(name);

    if (!rtc) {
        vmm_cdev_printf(cdev, "Error: cannot find rtc %s\n", name);
        return VMM_EFAIL;
    }

    rc = rtc_device_sync_device(rtc);

    if (rc) {
        vmm_cdev_printf(cdev, "Error: sync_device failed for rtc %s\n", name);
        return rc;
    }

    return VMM_OK;
}

static int cmd_rtcdev_get_time(vmm_char_device_t *cdev, const char *name)
{
    int                rc;
    struct rtc_time    tm;
    struct rtc_device *rtc = rtc_device_find(name);

    if (!rtc) {
        vmm_cdev_printf(cdev, "Error: cannot find rtc %s\n", name);
        return VMM_EFAIL;
    }

    rc = rtc_device_get_time(rtc, &tm);

    if (rc) {
        vmm_cdev_printf(cdev, "Error: get_time failed for rtc %s\n", name);
        return rc;
    }

    switch (tm.tm_week_of_day) {
        case 0:
            vmm_cdev_printf(cdev, "%s ", "Sun");
            break;

        case 1:
            vmm_cdev_printf(cdev, "%s ", "Mon");
            break;

        case 2:
            vmm_cdev_printf(cdev, "%s ", "Tue");
            break;

        case 3:
            vmm_cdev_printf(cdev, "%s ", "Wed");
            break;

        case 4:
            vmm_cdev_printf(cdev, "%s ", "Thu");
            break;

        case 5:
            vmm_cdev_printf(cdev, "%s ", "Fri");
            break;

        case 6:
            vmm_cdev_printf(cdev, "%s ", "Sat");
            break;

        default:
            vmm_cdev_printf(cdev, "Error: Invalid day of week\n");
    };

    switch (tm.tm_month) {
        case 0:
            vmm_cdev_printf(cdev, "%s ", "Jan");
            break;

        case 1:
            vmm_cdev_printf(cdev, "%s ", "Feb");
            break;

        case 2:
            vmm_cdev_printf(cdev, "%s ", "Mar");
            break;

        case 3:
            vmm_cdev_printf(cdev, "%s ", "Apr");
            break;

        case 4:
            vmm_cdev_printf(cdev, "%s ", "May");
            break;

        case 5:
            vmm_cdev_printf(cdev, "%s ", "Jun");
            break;

        case 6:
            vmm_cdev_printf(cdev, "%s ", "Jul");
            break;

        case 7:
            vmm_cdev_printf(cdev, "%s ", "Aug");
            break;

        case 8:
            vmm_cdev_printf(cdev, "%s ", "Sep");
            break;

        case 9:
            vmm_cdev_printf(cdev, "%s ", "Oct");
            break;

        case 10:
            vmm_cdev_printf(cdev, "%s ", "Nov");
            break;

        case 11:
            vmm_cdev_printf(cdev, "%s ", "Dec");
            break;

        default:
            vmm_cdev_printf(cdev, "Error: Invalid month\n");
    };

    vmm_cdev_printf(cdev, "%2d %d:%d:%d UTC %d", tm.tm_month_of_day, tm.tm_hour, tm.tm_minute, tm.tm_second, tm.tm_year + 1900);

    vmm_cdev_printf(cdev, "\n");

    return VMM_OK;
}

static int cmd_rtcdev_set_time(vmm_char_device_t *cdev, const char *name, int targc, char **targv)
{
    int                rc;
    char              *s;
    struct rtc_time    tm;
    struct rtc_device *rtc = rtc_device_find(name);

    if (!rtc) {
        vmm_cdev_printf(cdev, "Error: cannot find rtc %s\n", name);
        return VMM_EFAIL;
    }

    s            = targv[0];
    rc           = 0;
    tm.tm_hour   = 0;
    tm.tm_minute = 0;
    tm.tm_second = 0;

    while (*s) {
        if (*s == ':') {
            rc++;
        } else if ('0' <= *s && *s <= '9') {
            switch (rc) {
                case 0:
                    tm.tm_hour = tm.tm_hour * 10 + (*s - '0');
                    break;

                case 1:
                    tm.tm_minute = tm.tm_minute * 10 + (*s - '0');
                    break;

                case 2:
                    tm.tm_second = tm.tm_second * 10 + (*s - '0');
                    break;

                default:
                    break;
            };
        }

        s++;
    }

    rc                 = 0;
    tm.tm_month_of_day = atoi(targv[1]);
    str2lower(targv[2]);

    if (strcmp(targv[2], "jan") == 0) {
        tm.tm_month = 0;
    } else if (strcmp(targv[2], "feb") == 0) {
        tm.tm_month = 1;
    } else if (strcmp(targv[2], "mar") == 0) {
        tm.tm_month = 2;
    } else if (strcmp(targv[2], "apr") == 0) {
        tm.tm_month = 3;
    } else if (strcmp(targv[2], "may") == 0) {
        tm.tm_month = 4;
    } else if (strcmp(targv[2], "jun") == 0) {
        tm.tm_month = 5;
    } else if (strcmp(targv[2], "jul") == 0) {
        tm.tm_month = 6;
    } else if (strcmp(targv[2], "aug") == 0) {
        tm.tm_month = 7;
    } else if (strcmp(targv[2], "sep") == 0) {
        tm.tm_month = 8;
    } else if (strcmp(targv[2], "oct") == 0) {
        tm.tm_month = 9;
    } else if (strcmp(targv[2], "nov") == 0) {
        tm.tm_month = 10;
    } else if (strcmp(targv[2], "dec") == 0) {
        tm.tm_month = 11;
    } else {
        tm.tm_month = atoi(targv[2]);
    }

    tm.tm_year = atoi(targv[3]) - 1900;

    if (!rtc_valid_tm(&tm)) {
        vmm_cdev_printf(cdev, "Error: invalid date-time\n");
        return VMM_EFAIL;
    }

    rc = rtc_device_set_time(rtc, &tm);

    if (rc) {
        vmm_cdev_printf(cdev, "Error: set_time failed for rtc %s\n", name);
        return rc;
    }

    return VMM_OK;
}

static int cmd_rtcdev_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_rtcdev_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_rtcdev_list(cdev);
            return VMM_OK;
        }
    }

    if (argc < 3) {
        cmd_rtcdev_usage(cdev);
        return VMM_EFAIL;
    }

    if (strcmp(argv[1], "sync_wall_clock") == 0) {
        return cmd_rtcdev_sync_wall_clock(cdev, argv[2]);
    } else if (strcmp(argv[1], "sync_device") == 0) {
        return cmd_rtcdev_sync_device(cdev, argv[2]);
    } else if (strcmp(argv[1], "get_time") == 0) {
        return cmd_rtcdev_get_time(cdev, argv[2]);
    } else if ((strcmp(argv[1], "set_time") == 0) && argc == 7) {
        return cmd_rtcdev_set_time(cdev, argv[2], argc - 3, &argv[3]);
    }

    cmd_rtcdev_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_rtcdev = {
    .name  = "rtcdev",
    .desc  = "rtc device commands",
    .usage = cmd_rtcdev_usage,
    .exec  = cmd_rtcdev_exec,
};

static int __init cmd_rtcdev_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_rtcdev);
}

static void __exit cmd_rtcdev_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_rtcdev);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
