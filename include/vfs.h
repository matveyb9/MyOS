#ifndef MYOS_VFS_H
#define MYOS_VFS_H

#include <stdint.h>

#define VFS_NAME_MAX UINT64_C(64)
#define VFS_PATH_MAX UINT64_C(112)
#define VFS_PATH_DEPTH_MAX UINT64_C(8)
#define VFS_TMPFS_MAX_FILES UINT64_C(32)
#define VFS_TMPFS_FILE_CAPACITY UINT64_C(4096)
#define VFS_PERSIST_MAX_FILES UINT64_C(128)
#define VFS_PERSIST_FILE_CAPACITY (UINT64_C(8) * UINT64_C(1024) * UINT64_C(1024))
#define VFS_PERSIST_OPEN_CAPACITY (UINT64_C(128) * UINT64_C(1024))
#define VFS_PERSIST_EXTENT_MAX UINT64_C(6)
#define VFS_PERSIST_NAME_CAPACITY VFS_NAME_MAX
#define VFS_IO_CHUNK UINT64_C(128)

#define VFS_OBJECT_REGULAR UINT64_C(1)
#define VFS_OBJECT_DIRECTORY UINT64_C(2)
#define VFS_OBJECT_SYMBOLIC_LINK UINT64_C(3)
#define VFS_OBJECT_VIRTUAL UINT64_C(4)

struct vfs_file {
    const uint8_t *data;
    uint64_t size;
    uint64_t type;
};

struct vfs_directory_entry {
    char name[VFS_NAME_MAX];
    uint64_t size;
    uint64_t type;
};

int vfs_mount_newc(const void *archive, uint64_t length);
int vfs_mount_persistent(void);
int vfs_open(const char *path, struct vfs_file *file);
int vfs_read(const char *path, uint64_t offset, uint8_t *data, uint64_t length, uint64_t *read_length);
int vfs_list(const char *path, uint64_t index, struct vfs_directory_entry *entry);
int vfs_create_file(const char *path);
int vfs_create_directory(const char *path);
int vfs_write_file(const char *path, uint64_t offset, const uint8_t *data, uint64_t length);
int vfs_remove_object(const char *path);
int vfs_rename_object(const char *source, const char *target);
int vfs_move_object(const char *source, const char *target);
int vfs_get_entry(uint64_t index, char *name, uint64_t name_capacity, uint64_t *size);

/* Temporary compatibility wrappers for the legacy syscall ABI. */
int vfs_tmpfs_create(const char *path);
int vfs_tmpfs_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length);
int vfs_tmpfs_remove(const char *path);
int vfs_persistent_create(const char *path);
int vfs_persistent_write(const char *path, uint64_t offset, const uint8_t *data, uint64_t length);
int vfs_persistent_remove(const char *path);
uint64_t vfs_file_count(void);
uint64_t vfs_total_size(void);

#endif
