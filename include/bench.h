#ifndef BENCH_H
#define BENCH_H

#include <stddef.h>

typedef struct {
    double elapsed;
    unsigned long long bytes;
} bench_result_t;

typedef struct {
    unsigned long long total_blocks;
    unsigned long long passed;
    unsigned long long failed;
} verify_result_t;

int bench_write(int fd, void *buf, unsigned long long bs,
                unsigned long long total, int progress, bench_result_t *out);

int bench_read(int fd, void *buf, unsigned long long bs,
               unsigned long long total, int progress, bench_result_t *out);

int bench_verify(int fd, void *buf, unsigned long long bs,
                 unsigned long long total, verify_result_t *out);

#endif
