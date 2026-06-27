#include "io.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

void *alloc_aligned(size_t size, size_t alignment)
{
    void *buf = NULL;
    if (posix_memalign(&buf, alignment, size) != 0)
        return NULL;
    return buf;
}

int open_target_direct(const char *path, int raw)
{
    int flags = O_DIRECT | O_RDWR;
    if (!raw)
        flags |= O_CREAT | O_TRUNC;

    int fd = open(path, flags, 0600);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open '%s': %s\n",
                path, strerror(errno));
        return -1;
    }
    return fd;
}

unsigned long long get_device_size(int fd)
{
    unsigned long long size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) < 0)
        return 0;
    return size;
}

unsigned long long get_file_free_space(const char *path)
{
    struct statvfs st;
    if (statvfs(path, &st) < 0)
        return 0;
    return (unsigned long long)st.f_bsize * st.f_bavail;
}
