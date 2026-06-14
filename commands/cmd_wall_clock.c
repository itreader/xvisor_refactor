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
 * @file cmd_wall_clock.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of wall_clock command
 */

#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_wall_clock.h>

#define MODULE_DESC      "Command wall_clock"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_wall_clock_init
#define MODULE_EXIT      cmd_wall_clock_exit

static void cmd_wall_clock_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   wall_clock help\n");
    vmm_cdev_printf(cdev, "   wall_clock get_time\n");
    vmm_cdev_printf(
        cdev, "   wall_clock set_time <hour>:<min>:<sec> "
              "<day> <month> <year> [+/-<tz_hour>:<tz_min>]\n");
    vmm_cdev_printf(cdev, "   wall_clock get_timezone\n");
    vmm_cdev_printf(cdev, "   wall_clock set_timezone +/-<tz_hour>:<tz_min>\n");
    vmm_cdev_printf(cdev, "Note:\n");
    vmm_cdev_printf(cdev, "   <hour>    = any value between 0..23\n");
    vmm_cdev_printf(cdev, "   <minute>  = any value between 0..59\n");
    vmm_cdev_printf(cdev, "   <second>  = any value between 0..59\n");
    vmm_cdev_printf(cdev, "   <day>     = any value between 0..31\n");
    vmm_cdev_printf(
        cdev, "   <month>   = Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|"
              "Sep|Oct|Nov|Dec\n");
    vmm_cdev_printf(cdev, "   <year>    = any value greater than 1970\n");
    vmm_cdev_printf(cdev, "   <tz_hour> = timezone hour\n");
    vmm_cdev_printf(cdev, "   <tz_min>  = timezone minutes\n");
}

static int cmd_wall_clock_get_time(vmm_char_device_t *cdev)
{
    int              rc;
    vmm_time_info_t  ti;
    vmm_time_value_t tv;
    vmm_timezone_t   tz;

    rc = vmm_wall_clock_get_timeofday(&tv, &tz);

    if (rc) {
        vmm_cdev_printf(cdev, "Error: get_time failed\n");
        return rc;
    }

    vmm_wall_clock_mkinfo(tv.tv_sec, 0, &ti);

    switch (ti.tm_week_of_day) {
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

    switch (ti.tm_month) {
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

    vmm_cdev_printf(cdev, "%2d %d:%d:%d ", ti.tm_month_of_day, ti.tm_hour, ti.tm_minute, ti.tm_second);

    if (tz.tz_minutes_greenwich == 0) {
        vmm_cdev_printf(cdev, "UTC ");
    } else if (tz.tz_minutes_greenwich < 0) {
        tz.tz_minutes_greenwich *= -1;
        vmm_cdev_printf(cdev, "UTC-%d:%d ", tz.tz_minutes_greenwich / 60, tz.tz_minutes_greenwich % 60);
    } else {
        vmm_cdev_printf(cdev, "UTC+%d:%d ", tz.tz_minutes_greenwich / 60, tz.tz_minutes_greenwich % 60);
    }

    vmm_cdev_printf(cdev, "%ld", ti.tm_year + 1900);

    vmm_cdev_printf(cdev, "\n");

    return VMM_OK;
}

static int cmd_wall_clock_set_time(vmm_char_device_t *cdev, int targc, char **targv)
{
    int              rc;
    char            *s;
    vmm_time_info_t  ti;
    vmm_time_value_t tv;
    vmm_timezone_t   tz;

    if (targc > 4) {
        s                       = targv[4];
        rc                      = 0;
        tz.tz_minutes_greenwich = 0;
        tz.tz_dsttime           = 0;

        if (*s == '-' || *s == '+') {
            s++;
        }

        while (*s) {
            if (*s == ':') {
                rc++;
            } else if ('0' <= *s && *s <= '9') {
                switch (rc) {
                    case 0:
                        tz.tz_dsttime = tz.tz_dsttime * 10 + (*s - '0');
                        break;

                    case 1:
                        tz.tz_minutes_greenwich = tz.tz_minutes_greenwich * 10 + (*s - '0');
                        break;

                    default:
                        break;
                };
            }

            s++;
        }

        rc = 0;
        tz.tz_minutes_greenwich += tz.tz_dsttime * 60;
        tz.tz_dsttime = 0;
        s             = targv[4];

        if (*s == '-') {
            tz.tz_minutes_greenwich *= -1;
        }

        if ((rc = vmm_wall_clock_set_timezone(&tz))) {
            vmm_cdev_printf(cdev, "Error: set_timezone failed\n");
            return rc;
        }
    }

    s            = targv[0];
    rc           = 0;
    ti.tm_hour   = 0;
    ti.tm_minute = 0;
    ti.tm_second = 0;

    while (*s) {
        if (*s == ':') {
            rc++;
        } else if ('0' <= *s && *s <= '9') {
            switch (rc) {
                case 0:
                    ti.tm_hour = ti.tm_hour * 10 + (*s - '0');
                    break;

                case 1:
                    ti.tm_minute = ti.tm_minute * 10 + (*s - '0');
                    break;

                case 2:
                    ti.tm_second = ti.tm_second * 10 + (*s - '0');
                    break;

                default:
                    break;
            };
        }

        s++;
    }

    rc                 = 0;
    ti.tm_month_of_day = atoi(targv[1]);
    str2lower(targv[2]);

    if (strcmp(targv[2], "jan") == 0) {
        ti.tm_month = 0;
    } else if (strcmp(targv[2], "feb") == 0) {
        ti.tm_month = 1;
    } else if (strcmp(targv[2], "mar") == 0) {
        ti.tm_month = 2;
    } else if (strcmp(targv[2], "apr") == 0) {
        ti.tm_month = 3;
    } else if (strcmp(targv[2], "may") == 0) {
        ti.tm_month = 4;
    } else if (strcmp(targv[2], "jun") == 0) {
        ti.tm_month = 5;
    } else if (strcmp(targv[2], "jul") == 0) {
        ti.tm_month = 6;
    } else if (strcmp(targv[2], "aug") == 0) {
        ti.tm_month = 7;
    } else if (strcmp(targv[2], "sep") == 0) {
        ti.tm_month = 8;
    } else if (strcmp(targv[2], "oct") == 0) {
        ti.tm_month = 9;
    } else if (strcmp(targv[2], "nov") == 0) {
        ti.tm_month = 10;
    } else if (strcmp(targv[2], "dec") == 0) {
        ti.tm_month = 11;
    } else {
        ti.tm_month = atoi(targv[2]);
        /* Directly entered month will have range 1-12. */
        ti.tm_month--;
    }

    ti.tm_year = atoi(targv[3]) - 1900;

    tv.tv_sec  = vmm_wall_clock_mktime(ti.tm_year + 1900, ti.tm_month + 1, ti.tm_month_of_day, ti.tm_hour, ti.tm_minute, ti.tm_second);
    tv.tv_nsec = 0;

    if ((rc = vmm_wall_clock_set_local_time(&tv))) {
        vmm_cdev_printf(cdev, "Error: set_local_time failed\n");
        return rc;
    }

    return VMM_OK;
}

static int cmd_wall_clock_get_timezone(vmm_char_device_t *cdev)
{
    int            rc;
    vmm_timezone_t tz;

    if ((rc = vmm_wall_clock_get_timezone(&tz))) {
        vmm_cdev_printf(cdev, "Error: get_timezone failed\n");
        return rc;
    }

    if (tz.tz_minutes_greenwich == 0) {
        vmm_cdev_printf(cdev, "UTC\n");
    } else if (tz.tz_minutes_greenwich < 0) {
        tz.tz_minutes_greenwich *= -1;
        vmm_cdev_printf(cdev, "UTC-%d:%d\n", tz.tz_minutes_greenwich / 60, tz.tz_minutes_greenwich % 60);
    } else {
        vmm_cdev_printf(cdev, "UTC+%d:%d\n", tz.tz_minutes_greenwich / 60, tz.tz_minutes_greenwich % 60);
    }

    return VMM_OK;
}

static int cmd_wall_clock_set_timezone(vmm_char_device_t *cdev, char *tzstr)
{
    int            rc;
    char          *s;
    vmm_timezone_t tz;

    s                       = tzstr;
    rc                      = 0;
    tz.tz_minutes_greenwich = 0;
    tz.tz_dsttime           = 0;

    if (*s == '-' || *s == '+') {
        s++;
    }

    while (*s) {
        if (*s == ':') {
            rc++;
        } else if ('0' <= *s && *s <= '9') {
            switch (rc) {
                case 0:
                    tz.tz_dsttime = tz.tz_dsttime * 10 + (*s - '0');
                    break;

                case 1:
                    tz.tz_minutes_greenwich = tz.tz_minutes_greenwich * 10 + (*s - '0');
                    break;

                default:
                    break;
            };
        }

        s++;
    }

    rc = 0;
    tz.tz_minutes_greenwich += tz.tz_dsttime * 60;
    tz.tz_dsttime = 0;
    s             = tzstr;

    if (*s == '-') {
        tz.tz_minutes_greenwich *= -1;
    }

    if ((rc = vmm_wall_clock_set_timezone(&tz))) {
        vmm_cdev_printf(cdev, "Error: set_timezone failed\n");
        return rc;
    }

    return VMM_OK;
}

static int cmd_wall_clock_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_wall_clock_usage(cdev);
            return VMM_OK;
        }
    }

    if (argc < 2) {
        cmd_wall_clock_usage(cdev);
        return VMM_ERR_FAIL;
    }

    if (strcmp(argv[1], "get_time") == 0) {
        return cmd_wall_clock_get_time(cdev);
    } else if ((strcmp(argv[1], "set_time") == 0) && argc >= 6) {
        return cmd_wall_clock_set_time(cdev, argc - 2, &argv[2]);
    } else if (strcmp(argv[1], "get_timezone") == 0) {
        return cmd_wall_clock_get_timezone(cdev);
    } else if ((strcmp(argv[1], "set_timezone") == 0) && argc == 3) {
        return cmd_wall_clock_set_timezone(cdev, argv[2]);
    }

    cmd_wall_clock_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_wall_clock = {
    .name  = "wall_clock",
    .desc  = "wall-clock commands",
    .usage = cmd_wall_clock_usage,
    .exec  = cmd_wall_clock_exec,
};

static int __init cmd_wall_clock_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_wall_clock);
}

static void __exit cmd_wall_clock_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_wall_clock);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
