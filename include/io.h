#ifndef IO_H
#define IO_H

#include <stddef.h>

void *alloc_aligned(size_t size, size_t alignment);
int open_target_direct(const char *path, int raw);
unsigned long long get_device_size(int fd);
unsigned long long get_file_free_space(const char *path);

#endif
