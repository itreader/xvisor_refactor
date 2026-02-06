/**
 * Copyright (c) 2013 Jean-Christophe Dubois
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
 * @file vmm_load_balancer.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor load balancer
 */

#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_load_balancer.h>
#include <vmm_manager.h>
#include <vmm_mutex.h>
#include <vmm_threads.h>
#include <vmm_timer.h>

#define LOAD_BALANCER_PRIORITY  VMM_VCPU_DEF_PRIORITY
#define LOAD_BALANCER_TIMESLICE VMM_VCPU_DEF_TIME_SLICE
#define LOAD_BALANCER_PERIOD    (CONFIG_LOAD_BALANCER_PERIOD_SECS * 1000000000ULL)

struct vmm_load_balancer_ctrl {
    vmm_mutex_t                    curr_algo_lock;
    struct vmm_load_balancer_algo *curr_algo;
    vmm_mutex_t                    algo_list_lock;
    double_list_t                  algo_list;
    vmm_completion_t               load_balancer_cmpl;
    vmm_thread_t                  *load_balancer_thread;
};

static struct vmm_load_balancer_ctrl lbctrl;

static int load_balancer_main(void *data)
{
    uint64_t tstamp;

    while (1) {
        tstamp = LOAD_BALANCER_PERIOD;
        vmm_completion_wait_timeout(&lbctrl.load_balancer_cmpl, &tstamp);

        if (vmm_cpumask_weight(cpu_online_mask) < 2) {
            continue;
        }

        vmm_mutex_lock(&lbctrl.curr_algo_lock);

        if (lbctrl.curr_algo && lbctrl.curr_algo->balance) {
            lbctrl.curr_algo->balance(lbctrl.curr_algo);
        }

        vmm_mutex_unlock(&lbctrl.curr_algo_lock);
    }

    return VMM_OK;
}

struct vmm_load_balancer_algo *vmm_load_balancer_current_algo(void)
{
    struct vmm_load_balancer_algo *ret;

    vmm_mutex_lock(&lbctrl.curr_algo_lock);
    ret = lbctrl.curr_algo;
    vmm_mutex_unlock(&lbctrl.curr_algo_lock);

    return ret;
}

static struct vmm_load_balancer_algo *__load_balancer_best_algo(void)
{
    uint32_t                       best_rating;
    struct vmm_load_balancer_algo *algo, *best_algo;

    best_rating = 0;
    best_algo   = NULL;
    list_for_each_entry(algo, &lbctrl.algo_list, head)
    {
        if (best_rating < algo->rating) {
            best_rating = algo->rating;
            best_algo   = algo;
        }
    }

    return best_algo;
}

int vmm_load_balancer_register_algo(struct vmm_load_balancer_algo *lbalgo)
{
    int                            rc = VMM_OK;
    bool                           found;
    struct vmm_load_balancer_algo *algo, *best_algo;

    /* Sanity checks */
    if (!lbalgo) {
        return VMM_EFAIL;
    }

    /* Lock algo list */
    vmm_mutex_lock(&lbctrl.algo_list_lock);

    /* Registered algo instance should not be present in algo list */
    found = FALSE;
    list_for_each_entry(algo, &lbctrl.algo_list, head)
    {
        if (algo == lbalgo) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&lbctrl.algo_list_lock);
        return VMM_EEXIST;
    }

    /* Add registered algo instance to algo list */
    INIT_LIST_HEAD(&lbalgo->head);
    list_add_tail(&lbalgo->head, &lbctrl.algo_list);

    /* Find best algo */
    best_algo = __load_balancer_best_algo();

    /* Update current algo */
    vmm_mutex_lock(&lbctrl.curr_algo_lock);

    if (best_algo && lbctrl.curr_algo != best_algo) {
        if (best_algo->start) {
            rc = best_algo->start(best_algo);
        }

        if (rc == VMM_OK) {
            if (lbctrl.curr_algo && lbctrl.curr_algo->stop) {
                lbctrl.curr_algo->stop(lbctrl.curr_algo);
            }

            lbctrl.curr_algo = best_algo;
        }
    }

    vmm_mutex_unlock(&lbctrl.curr_algo_lock);

    /* Unlock algo list */
    vmm_mutex_unlock(&lbctrl.algo_list_lock);

    return rc;
}

int vmm_load_balancer_unregister_algo(struct vmm_load_balancer_algo *lbalgo)
{
    int                            rc = VMM_OK;
    bool                           found;
    struct vmm_load_balancer_algo *algo, *best_algo;

    /* Sanity checks */
    if (!lbalgo || !lbalgo->balance) {
        return VMM_EFAIL;
    }

    /* Lock algo list */
    vmm_mutex_lock(&lbctrl.algo_list_lock);

    /* Unregistered algo instance should be present in algo list */
    found = FALSE;
    list_for_each_entry(algo, &lbctrl.algo_list, head)
    {
        if (algo == lbalgo) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&lbctrl.algo_list_lock);
        return VMM_ENOTAVAIL;
    }

    /* Update current algo */
    vmm_mutex_lock(&lbctrl.curr_algo_lock);

    if (lbctrl.curr_algo == lbalgo) {
        if (lbctrl.curr_algo->stop) {
            lbctrl.curr_algo->stop(lbctrl.curr_algo);
        }

        lbctrl.curr_algo = NULL;
    }

    vmm_mutex_unlock(&lbctrl.curr_algo_lock);

    /* Remove from algo list */
    list_del(&lbalgo->head);

    /* Find best algo */
    best_algo = __load_balancer_best_algo();

    /* Update current algo */
    vmm_mutex_lock(&lbctrl.curr_algo_lock);

    if (best_algo && lbctrl.curr_algo != best_algo) {
        if (best_algo->start) {
            rc = best_algo->start(best_algo);
        }

        if (rc == VMM_OK) {
            if (lbctrl.curr_algo && lbctrl.curr_algo->stop) {
                lbctrl.curr_algo->stop(lbctrl.curr_algo);
            }

            lbctrl.curr_algo = best_algo;
        }
    }

    vmm_mutex_unlock(&lbctrl.curr_algo_lock);

    /* Unlock algo list */
    vmm_mutex_unlock(&lbctrl.algo_list_lock);

    return rc;
}

int __init vmm_load_balancer_init(void)
{
    int rc;

    /* Initialize load_balancer current algo */
    INIT_MUTEX(&lbctrl.curr_algo_lock);
    lbctrl.curr_algo = NULL;

    /* Initialize load_balancer algo list */
    INIT_MUTEX(&lbctrl.algo_list_lock);
    INIT_LIST_HEAD(&lbctrl.algo_list);

    /* Initialize load_balancer completion */
    INIT_COMPLETION(&lbctrl.load_balancer_cmpl);

    /* Create load_balancer thread with default time slice */
    lbctrl.load_balancer_thread = vmm_threads_create("load_balancer", load_balancer_main, NULL, LOAD_BALANCER_PRIORITY, LOAD_BALANCER_TIMESLICE);

    if (!lbctrl.load_balancer_thread) {
        return VMM_EFAIL;
    }

    /* Set load_balancer thread affinity to this cpu */
    if ((rc = vmm_threads_set_affinity(lbctrl.load_balancer_thread, vmm_cpumask_of(vmm_smp_processor_id())))) {
        return rc;
    }

    /* Start load_balancer thread */
    if ((rc = vmm_threads_start(lbctrl.load_balancer_thread))) {
        return rc;
    }

    return VMM_OK;
}
