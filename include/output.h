#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>

void print_header(const char *target, const char *device,
                  const char *fstype, unsigned long long bs,
                  unsigned long long total, int raw, int do_write, int do_read);
void print_progress(FILE *fp, const char *label, unsigned long long done,
                    unsigned long long total, double elapsed);
void print_result(const char *label, unsigned long long bytes, double elapsed);
void print_verify_result(unsigned long long total_blocks,
                         unsigned long long passed, unsigned long long failed);

#endif
