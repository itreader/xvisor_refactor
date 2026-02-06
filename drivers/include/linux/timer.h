#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <vmm_timer.h>

#include <linux/jiffies.h>
#include <linux/list.h>

#define timer_list vmm_timer_event

static inline void setup_timer(struct timer_list *tl, void (*handler)(struct timer_list *), uint64_t data)
{
    INIT_TIMER_EVENT(tl, handler, (void *)data);
}

static inline int mod_timer(struct timer_list *tl, uint64_t delay)
{
    uint64_t nsecs = delay;
    nsecs          = nsecs * (1000000000 / HZ);
    return vmm_timer_event_start(tl, nsecs);
}

static inline int del_timer(struct timer_list *tl)
{
    return vmm_timer_event_stop(tl);
}

static inline int del_timer_sync(struct timer_list *tl)
{
    return vmm_timer_event_stop(tl);
}

#endif
