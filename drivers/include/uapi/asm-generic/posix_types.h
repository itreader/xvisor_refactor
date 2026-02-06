#ifndef __ASM_GENERIC_POSIX_TYPES_H
#define __ASM_GENERIC_POSIX_TYPES_H

#ifndef __kernel_long_t
typedef long     __kernel_long_t;
typedef uint64_t __kernel_ulong_t;
#endif

/*
 * anything below here should be completely generic
 */
typedef __kernel_long_t __kernel_time_t;

#endif /* __ASM_GENERIC_POSIX_TYPES_H */
