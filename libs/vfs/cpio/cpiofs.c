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
 * @file cpiofs.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief CPIO filesystem driver
 *
 * CPIO is well-known archive format. It is widely used by
 * Linux kernel for setting up contents of its initramfs/initrd.
 *
 * The below code implements a CPIO filesystem driver for read-only
 * purpose since CPIO is an archive format.
 */

#include <libs/stringlib.h>
#include <libs/vfs.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "CPIO Filesystem Driver"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VFS_IPRIORITY + 1)
#define MODULE_INIT      cpiofs_init
#define MODULE_EXIT      cpiofs_exit

struct cpio_newc_header {
    uint8_t c_magic[6];
    uint8_t c_ino[8];
    uint8_t c_mode[8];
    uint8_t c_uid[8];
    uint8_t c_gid[8];
    uint8_t c_nlink[8];
    uint8_t c_mtime[8];
    uint8_t c_filesize[8];
    uint8_t c_devmajor[8];
    uint8_t c_devminor[8];
    uint8_t c_rdevmajor[8];
    uint8_t c_rdevminor[8];
    uint8_t c_namesize[8];
    uint8_t c_check[8];
} __packed;

/*
 * Helper routines
 */

static bool get_next_token(const char *path, const char *prefix, char *result)
{
    int         l;
    const char *p, *q;

    if (!path || !prefix || !result) {
        return FALSE;
    }

    if (*path == '/') {
        path++;
    }

    if (*prefix == '/') {
        prefix++;
    }

    l = strlen(prefix);

    if (strncmp(path, prefix, l) != 0) {
        return FALSE;
    }

    p = &path[l];

    if (*p == '\0') {
        return FALSE;
    }

    if (*p == '/') {
        p++;
    }

    if (*p == '\0') {
        return FALSE;
    }

    q = strchr(p, '/');

    if (q) {
        if (*(q + 1) != '\0') {
            return FALSE;
        }

        l = q - p;
    } else {
        l = strlen(p);
    }

    memcpy(result, p, l);
    result[l] = '\0';

    return TRUE;
}

static bool check_path(const char *path, const char *prefix, const char *name)
{
    int l;

    if (!path || !prefix || !name) {
        return FALSE;
    }

    if (path[0] == '/') {
        path++;
    }

    if (prefix[0] == '/') {
        prefix++;
    }

    l = strlen(prefix);

    if (l && (strncmp(path, prefix, l) != 0)) {
        return FALSE;
    }

    path += l;

    if (path[0] == '/') {
        path++;
    }

    if (strcmp(path, name) != 0) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Mount point operations
 */

static int cpiofs_mount(struct mount *m, const char *dev, uint32_t flags)
{
    uint64_t                read_count;
    struct cpio_newc_header header;

    if (dev == NULL) {
        return VMM_ERR_INVALID;
    }

    if (vmm_block_device_total_size(m->m_device) <= sizeof(struct cpio_newc_header)) {
        return VMM_ERR_FAIL;
    }

    read_count = vmm_block_device_read(m->m_device, (uint8_t *)(&header), 0, sizeof(struct cpio_newc_header));

    if (read_count != sizeof(struct cpio_newc_header)) {
        return VMM_ERR_IO;
    }

    if (strncmp((const char *)header.c_magic, "070701", 6) != 0) {
        return VMM_ERR_INVALID;
    }

    m->m_flags        = MOUNT_RDONLY; /* We treat CPIO filesystem as read-only */
    m->m_root->v_data = NULL;
    m->m_data         = NULL;

    return VMM_OK;
}

static int cpiofs_unmount(struct mount *m)
{
    m->m_data = NULL;

    return VMM_OK;
}

static int cpiofs_msync(struct mount *m)
{
    /* Not required (read-only filesystem) */
    return VMM_OK;
}

static int cpiofs_vget(struct mount *m, struct vnode *v)
{
    /* Not required */
    return VMM_OK;
}

static int cpiofs_vput(struct mount *m, struct vnode *v)
{
    /* Not required */
    return VMM_OK;
}

/*
 * Vnode operations
 */

static size_t cpiofs_read(struct vnode *v, loff_t off, void *buf, size_t len)
{
    uint64_t toff;
    size_t   size = 0;

    if (v->v_type != VREG) {
        return 0;
    }

    if (off >= v->v_size) {
        return 0;
    }

    size = len;

    if ((v->v_size - off) < size) {
        size = v->v_size - off;
    }

    toff = (uint64_t)((uint64_t)(v->v_data));
    size = vmm_block_device_read(v->v_mount->m_device, (uint8_t *)buf, (toff + off), size);

    return size;
}

static size_t cpiofs_write(struct vnode *v, loff_t off, void *buf, size_t len)
{
    /* Not required (read-only filesystem) */
    return 0;
}

static int cpiofs_truncate(struct vnode *v, loff_t off)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

static int cpiofs_sync(struct vnode *v)
{
    /* Not required (read-only filesystem) */
    return VMM_OK;
}

static int cpiofs_readdir(struct vnode *dv, loff_t off, struct dirent *d)
{
    struct cpio_newc_header header;
    char                    path[VFS_MAX_PATH];
    char                    name[VFS_MAX_NAME];
    uint32_t                size, name_size, mode;
    uint64_t                toff = 0, rd;
    char                    buf[9];
    int                     i = 0;

    while (1) {
        rd = vmm_block_device_read(dv->v_mount->m_device, (uint8_t *)&header, toff, sizeof(struct cpio_newc_header));

        if (!rd) {
            return VMM_ERR_IO;
        }

        if (strncmp((const char *)&header.c_magic, "070701", 6) != 0) {
            return VMM_ERR_NOENT;
        }

        buf[8] = '\0';

        memcpy(buf, &header.c_filesize, 8);
        size = strtoul((const char *)buf, NULL, 16);

        memcpy(buf, &header.c_namesize, 8);
        name_size = strtoul((const char *)buf, NULL, 16);

        memcpy(buf, &header.c_mode, 8);
        mode = strtoul((const char *)buf, NULL, 16);

        rd   = vmm_block_device_read(dv->v_mount->m_device, (uint8_t *)path, toff + sizeof(struct cpio_newc_header), name_size);

        if (!rd) {
            return VMM_ERR_IO;
        }

        if ((size == 0) && (mode == 0) && (name_size == 11) && (strncmp(path, "TRAILER!!!", 10) == 0)) {
            return VMM_ERR_NOENT;
        }

        toff += sizeof(struct cpio_newc_header);
        toff += (((name_size + 1) & ~3) + 2) + size;
        toff = (toff + 3) & ~3;

        if (path[0] == '.') {
            continue;
        }

        if (!get_next_token(path, dv->v_path, name)) {
            continue;
        }

        if (i++ == off) {
            toff = 0;
            break;
        }
    }

    if ((mode & 00170000) == 0140000) {
        d->d_type = DT_SOCK;
    } else if ((mode & 00170000) == 0120000) {
        d->d_type = DT_LNK;
    } else if ((mode & 00170000) == 0100000) {
        d->d_type = DT_REG;
    } else if ((mode & 00170000) == 0060000) {
        d->d_type = DT_BLK;
    } else if ((mode & 00170000) == 0040000) {
        d->d_type = DT_DIR;
    } else if ((mode & 00170000) == 0020000) {
        d->d_type = DT_CHR;
    } else if ((mode & 00170000) == 0010000) {
        d->d_type = DT_FIFO;
    } else {
        d->d_type = DT_REG;
    }

    strncpy(d->d_name, name, VFS_MAX_NAME - 1);
    d->d_name[VFS_MAX_NAME - 1] = '\0';

    d->d_off                    = off;
    d->d_reclen                 = 1;

    return 0;
}

static int cpiofs_lookup(struct vnode *dv, const char *name, struct vnode *v)
{
    struct cpio_newc_header header;
    char                    path[VFS_MAX_PATH];
    uint32_t                size, name_size, mode, mtime;
    uint64_t                off = 0, rd;
    uint8_t                 buf[9];

    while (1) {
        rd = vmm_block_device_read(dv->v_mount->m_device, (uint8_t *)&header, off, sizeof(struct cpio_newc_header));

        if (!rd) {
            return VMM_ERR_IO;
        }

        if (strncmp((const char *)header.c_magic, "070701", 6) != 0) {
            return VMM_ERR_NOENT;
        }

        buf[8] = '\0';

        memcpy(buf, &header.c_filesize, 8);
        size = strtoul((const char *)buf, NULL, 16);

        memcpy(buf, &header.c_namesize, 8);
        name_size = strtoul((const char *)buf, NULL, 16);

        memcpy(buf, &header.c_mode, 8);
        mode = strtoul((const char *)buf, NULL, 16);

        memcpy(buf, &header.c_mtime, 8);
        mtime = strtoul((const char *)buf, NULL, 16);

        rd    = vmm_block_device_read(dv->v_mount->m_device, (uint8_t *)path, off + sizeof(struct cpio_newc_header), name_size);

        if (!rd) {
            return VMM_ERR_IO;
        }

        if ((size == 0) && (mode == 0) && (name_size == 11) && (strncmp(path, "TRAILER!!!", 10) == 0)) {
            return VMM_ERR_NOENT;
        }

        if ((path[0] != '.') && check_path(path, dv->v_path, name)) {
            break;
        }

        off += sizeof(struct cpio_newc_header);
        off += (((name_size + 1) & ~3) + 2) + size;
        off = (off + 3) & ~0x3;
    }

    v->v_atime = mtime;
    v->v_mtime = mtime;
    v->v_ctime = mtime;

    v->v_mode  = 0;

    if ((mode & 00170000) == 0140000) {
        v->v_type = VSOCK;
        v->v_mode |= S_IFSOCK;
    } else if ((mode & 00170000) == 0120000) {
        v->v_type = VLNK;
        v->v_mode |= S_IFLNK;
    } else if ((mode & 00170000) == 0100000) {
        v->v_type = VREG;
        v->v_mode |= S_IFREG;
    } else if ((mode & 00170000) == 0060000) {
        v->v_type = VBLK;
        v->v_mode |= S_IFBLK;
    } else if ((mode & 00170000) == 0040000) {
        v->v_type = VDIR;
        v->v_mode |= S_IFDIR;
    } else if ((mode & 00170000) == 0020000) {
        v->v_type = VCHR;
        v->v_mode |= S_IFCHR;
    } else if ((mode & 00170000) == 0010000) {
        v->v_type = VFIFO;
        v->v_mode |= S_IFIFO;
    } else {
        v->v_type = VREG;
    }

    v->v_mode |= (mode & 00400) ? S_IRUSR : 0;
    v->v_mode |= (mode & 00200) ? S_IWUSR : 0;
    v->v_mode |= (mode & 00100) ? S_IXUSR : 0;
    v->v_mode |= (mode & 00040) ? S_IRGRP : 0;
    v->v_mode |= (mode & 00020) ? S_IWGRP : 0;
    v->v_mode |= (mode & 00010) ? S_IXGRP : 0;
    v->v_mode |= (mode & 00004) ? S_IROTH : 0;
    v->v_mode |= (mode & 00002) ? S_IWOTH : 0;
    v->v_mode |= (mode & 00001) ? S_IXOTH : 0;

    v->v_size = size;

    off += sizeof(struct cpio_newc_header);
    off += (((name_size + 1) & ~3) + 2);
    v->v_data = (void *)((uint64_t)off);

    return 0;
}

static int cpiofs_create(struct vnode *dv, const char *filename, uint32_t mode)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

static int cpiofs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

static int cpiofs_rename(struct vnode *sv, const char *sname, struct vnode *v, struct vnode *dv, const char *dname)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

static int cpiofs_mkdir(struct vnode *dv, const char *name, uint32_t mode)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

static int cpiofs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

static int cpiofs_chmod(struct vnode *v, uint32_t mode)
{
    /* Not allowed (read-only filesystem) */
    return VMM_ERR_FAIL;
}

/* cpiofs filesystem */
static struct filesystem cpiofs = {
    .name     = "cpio",

    /* Mount point operations */
    .mount    = cpiofs_mount,
    .unmount  = cpiofs_unmount,
    .msync    = cpiofs_msync,
    .vget     = cpiofs_vget,
    .vput     = cpiofs_vput,

    /* Vnode operations */
    .read     = cpiofs_read,
    .write    = cpiofs_write,
    .truncate = cpiofs_truncate,
    .sync     = cpiofs_sync,
    .readdir  = cpiofs_readdir,
    .lookup   = cpiofs_lookup,
    .create   = cpiofs_create,
    .remove   = cpiofs_remove,
    .rename   = cpiofs_rename,
    .mkdir    = cpiofs_mkdir,
    .rmdir    = cpiofs_rmdir,
    .chmod    = cpiofs_chmod,
};

static int __init cpiofs_init(void)
{
    return vfs_filesystem_register(&cpiofs);
}

static void __exit cpiofs_exit(void)
{
    vfs_filesystem_unregister(&cpiofs);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
