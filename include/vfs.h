#ifndef MYOS_VFS_H
#define MYOS_VFS_H

#include <stdint.h>

struct vfs_file {
    const uint8_t *data;
    uint64_t size;
};

int vfs_mount_newc(const void *archive, uint64_t length);
int vfs_open(const char *path, struct vfs_file *file);
int vfs_get_entry(uint64_t index, char *name, uint64_t name_capacity, uint64_t *size);
uint64_t vfs_file_count(void);
uint64_t vfs_total_size(void);

#endif
