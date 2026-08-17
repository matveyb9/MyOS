#ifndef MYOS_INITRAMFS_H
#define MYOS_INITRAMFS_H

#include <stdint.h>

struct limine_module_response;

int initramfs_init(const struct limine_module_response *modules);
uint64_t initramfs_size(void);
uint64_t initramfs_file_count(void);
int initramfs_has_init(void);
int initramfs_start_init(void);

#endif
