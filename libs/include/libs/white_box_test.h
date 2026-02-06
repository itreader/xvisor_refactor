/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file white_box_test.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief white-box testing library interface
 */

#ifndef __WBOXTEST_H__
#define __WBOXTEST_H__

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_types.h>

#define WBOXTEST_IPRIORITY (1)

struct vmm_char_device;

struct wboxtest_group {
    /* list head */
    double_list_t head;

    /* group name */
    char name[VMM_FIELD_NAME_SIZE];

    /* number of test under this group */
    uint32_t test_count;

    /* list of tests under this group */
    double_list_t test_list;
};

struct white_box_test {
    /* list head */
    double_list_t head;

    /* group pointer */
    struct wboxtest_group *group;

    /* test name */
    char name[VMM_FIELD_NAME_SIZE];

    /* operations */
    int (*setup)(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu);
    int (*run)(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu);
    void (*cleanup)(struct white_box_test *test, vmm_char_device_t *cdev);
};

/** Iterate over each white_box_test group */
void wboxtest_group_iterate(void (*iter)(struct wboxtest_group *group, void *data), void *data);

/** Iterate over each white_box_test */
void wboxtest_iterate(void (*iter)(struct white_box_test *test, void *data), void *data);

/** Run one or more white_box_test groups */
void wboxtest_run_groups(vmm_char_device_t *cdev, uint32_t iterations, int group_count, char **group_names);

/** Run one or more wboxtests */
void wboxtest_run_tests(vmm_char_device_t *cdev, uint32_t iterations, int test_count, char **test_names);

/** Run all wboxtests */
void wboxtest_run_all(vmm_char_device_t *cdev, uint32_t iterations);

/** Register white_box_test */
int wboxtest_register(const char *group_name, struct white_box_test *test);

/** Unregister white_box_test */
void wboxtest_unregister(struct white_box_test *test);

#endif /* __WBOXTEST_H__ */
