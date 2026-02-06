#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <uapi/linux/types.h>
#include <vmm_types.h>

#define __user
#define __init_refok

#ifndef __KERNEL__
#define __KERNEL__
#endif

#ifdef __KERNEL__

/* bsd */
// typedef unsigned char  uint8_t;
// typedef unsigned short u_short;
// typedef uint32_t       u_int;
// typedef uint64_t  uint64_t;

// /* sysv */
// typedef unsigned char  unchar;
// typedef unsigned short ushort;
// typedef uint32_t       uint;

// typedef unsigned short umode_t;

#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__

// typedef unsigned char      uint8_t;
// typedef unsigned short     uint16_t;
// typedef uint32_t           uint32_t;
// typedef char               __s8;
// typedef short              __s16;
// typedef int                int32_t;
// typedef uint64_t uint64_t;
// typedef long long          __s64;

// typedef uint8_t  u_int8_t;
// typedef __s8  int8_t;
// typedef uint16_t u_int16_t;
// typedef __s16 int16_t;
// typedef uint32_t u_int32_t;
// typedef int32_t int32_t;

#endif /* !(__BIT_TYPES_DEFINED__) */

// typedef uint8_t  uint8_t;
// typedef uint16_t uint16_t;
// typedef uint32_t uint32_t;

// #if defined(__GNUC__)
// typedef uint64_t uint64_t;
// typedef uint64_t u_int64_t;
// typedef __s64 int64_t;
// #endif

/* this is a special 64bit data type that is 8-byte aligned */
#define aligned_u64  uint64_t __attribute__((aligned(8)))
#define aligned_be64 __be64 __attribute__((aligned(8)))
#define aligned_le64 __le64 __attribute__((aligned(8)))

/**
 * The type used for indexing onto a disc or disc partition.
 *
 * Linux always considers sectors to be 512 bytes long independently
 * of the devices real block size.
 *
 * blkcnt_t is the type of the inode's block count.
 */
#ifdef CONFIG_LBDAF
typedef uint64_t sector_t;
typedef uint64_t blkcnt_t;
#else
typedef uint64_t sector_t;
typedef uint64_t blkcnt_t;
#endif

/*
 * The type of an index into the pagecache.  Use a #define so asm/types.h
 * can override it.
 */
#ifndef pgoff_t
#define pgoff_t uint64_t
#endif

#endif /* __KERNEL__ */

/*
 * Below are truly Linux-specific types that should never collide with
 * any application/library that wants linux/types.h.
 */

#ifdef __CHECKER__
#define __bitwise__ __attribute__((bitwise))
#else
#define __bitwise__
#endif
#ifdef __CHECK_ENDIAN__
#define __bitwise __bitwise__
#else
#define __bitwise
#endif

typedef uint16_t __bitwise __le16;
typedef uint16_t __bitwise __be16;
typedef uint32_t __bitwise __le32;
typedef uint32_t __bitwise __be32;
typedef uint64_t __bitwise __le64;
typedef uint64_t __bitwise __be64;

typedef uint16_t __bitwise __sum16;
typedef uint32_t __bitwise __wsum;

#ifdef __KERNEL__
typedef unsigned __bitwise__ gfp_t;
typedef unsigned __bitwise__ fmode_t;

#ifdef CONFIG_PHYS_ADDR_T_64BIT
typedef uint64_t phys_addr_t;
#else
typedef uint32_t phys_addr_t;
#endif

#endif /* __KERNEL__ */
#endif /* _LINUX_TYPES_H */
