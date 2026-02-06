/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_threads.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of hypervisor threads.
 */
#ifndef __VMM_THREADS_H__
#define __VMM_THREADS_H__

#include <vmm_manager.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_THREAD_MAX_PRIORITY    VMM_VCPU_MAX_PRIORITY
#define VMM_THREAD_MIN_PRIORITY    VMM_VCPU_MIN_PRIORITY
#define VMM_THREAD_DEF_PRIORITY    VMM_VCPU_DEF_PRIORITY
#define VMM_THREAD_DEF_TIME_SLICE  VMM_VCPU_DEF_TIME_SLICE
#define VMM_THREAD_DEF_DEADLINE    VMM_VCPU_DEF_DEADLINE
#define VMM_THREAD_DEF_PERIODICITY VMM_VCPU_DEF_PERIODICITY

enum vmm_thread_states {
    VMM_THREAD_STATE_CREATED  = 0,
    VMM_THREAD_STATE_RUNNING  = 1,
    VMM_THREAD_STATE_SLEEPING = 2,
    VMM_THREAD_STATE_STOPPED  = 3
};

typedef struct vmm_thread {
    double_list_t head;              /* thread list head */
    vmm_vcpu_t   *vcpu_on_thread;    /* vcpu on which thread runs */
    int (*thread_func)(void *udata); /* thread functions */
    void    *tdata;                  /* data passed to thread
                                      * function on execution */
    int      thread_ret_value;       /* thread return value */
    uint64_t thread_nanoseconds;     /* thread time slice in nanoseconds */
    uint64_t thread_deadline;        /* thread deadline in nanoseconds */
    uint64_t thread_periodicity;     /* thread periodicity in nanoseconds */
} vmm_thread_t;

/** Start a thread */
int vmm_threads_start(vmm_thread_t *thread_info);

/** Stop a thread */
int vmm_threads_stop(vmm_thread_t *thread_info);

/** Put a thread to sleep */
int vmm_threads_sleep(vmm_thread_t *thread_info);

/** Wakeup a thread */
int vmm_threads_wakeup(vmm_thread_t *thread_info);

/** Retrive thread id */
uint32_t vmm_threads_get_id(vmm_thread_t *thread_info);

/** Retrive thread priority */
uint8_t vmm_threads_get_priority(vmm_thread_t *thread_info);

/** Retrive thread name */
int vmm_threads_get_name(char *dst, vmm_thread_t *thread_info);

/** Retrive thread state */
int vmm_threads_get_state(vmm_thread_t *thread_info);

/** Retrive host CPU assigned to given thread */
int vmm_threads_get_hcpu(vmm_thread_t *thread_info, uint32_t *host_cpu);

/** Update host CPU assigned to given thread */
int vmm_thread_set_hcpu(vmm_thread_t *thread_info, uint32_t host_cpu);

/** Retrive host CPU affinity of given thread */
const vmm_cpumask_t *vmm_threads_get_affinity(vmm_thread_t *thread_info);

/** Update host CPU affinity of given thread */
int vmm_threads_set_affinity(vmm_thread_t *thread_info, const vmm_cpumask_t *cpu_mask);

/** Retrive thread instance from thread id */
vmm_thread_t *vmm_threads_id2thread(uint32_t thread_id);

/** Retrive thread instance from thread index */
vmm_thread_t *vmm_threads_index2thread(int index);

/** Count number of threads */
uint32_t vmm_threads_count(void);

/** Create a new thread with explicitly specified deadline and periodicity.
 *  This is more real-time friendly API so that users can specify deadline
 *  and periodicity for a VCPU.
 */
vmm_thread_t *vmm_threads_create_rt(
    const char *thread_name, int (*thread_fn)(void *udata), void *thread_data, uint8_t thread_priority, uint64_t thread_nsecs,
    uint64_t thread_deadline, uint64_t thread_periodicity);

/** Create a new thread */
static inline vmm_thread_t *vmm_threads_create(
    const char *thread_name, int (*thread_fn)(void *udata), void *thread_data, uint8_t thread_priority, uint64_t thread_nsecs)
{
    return vmm_threads_create_rt(thread_name, thread_fn, thread_data, thread_priority, thread_nsecs, thread_nsecs * 10, thread_nsecs * 100);
}

/** Destroy a thread */
int vmm_threads_destroy(vmm_thread_t *thread_info);

/** Intialize hypervisor threads */
int vmm_threads_init(void);

#endif /* __VMM_THREADS_H__ */
