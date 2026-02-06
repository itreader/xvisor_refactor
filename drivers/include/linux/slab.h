#ifndef _LINUX_SLAB_H_
#define _LINUX_SLAB_H_

#include <libs/stringlib.h>
#include <vmm_heap.h>
#include <vmm_types.h>

#include <linux/bug.h>
#include <linux/gfp.h>

#define kmalloc_track_caller kmalloc

typedef uint32_t gfp_t;

static inline void *kmalloc(size_t size, gfp_t flags)
{
    return vmm_malloc(size);
}

static inline void *kzalloc(size_t size, gfp_t flags)
{
    return vmm_zalloc(size);
}

static inline void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
    return vmm_malloc(n * size);
}

static inline void *kcalloc(size_t n, size_t size, gfp_t flags)
{
    void *ret = kmalloc_array(n, size, flags);

    if (ret) {
        memset(ret, 0, n * size);
    }

    return ret;
}

static inline void kfree(const void *ptr)
{
    vmm_free((void *)ptr);
}

static inline size_t ksize(const void *ptr)
{
    return vmm_alloc_size(ptr);
}

static inline void *krealloc(const void *p, size_t new_size, uint32_t flags)
{
    size_t ks;
    void  *ret;

    if (!new_size) {
        kfree((void *)p);
        return NULL;
    }

    ks = ksize(p);

    if (ks >= new_size) {
        return (void *)p;
    }

    ret = vmm_malloc(new_size);

    if (ret && p) {
        memcpy(ret, p, ks);
    }

    return ret;
}

#endif /* _LINUX_SLAB_H_ */
