#ifndef MYOS_VFS_H
#define MYOS_VFS_H

#include <stdint.h>

#define VFS_NAME_MAX UINT64_C(64)
#define VFS_TMPFS_MAX_FILES UINT64_C(16)
#define VFS_TMPFS_FILE_CAPACITY UINT64_C(1024)

struct vfs_file {
    const uint8_t *data;
    uint64_t size;
};

int vfs_mount_newc(const void *archive, uint64_t length);
int vfs_open(const char *path, struct vfs_file *file);
int vfs_get_entry(uint64_t index, char *name, uint64_t name_capacity, uint64_t *size);
int vfs_tmpfs_create(const char *path);
int vfs_tmpfs_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length);
int vfs_tmpfs_remove(const char *path);
uint64_t vfs_file_count(void);
uint64_t vfs_total_size(void);

#endif
