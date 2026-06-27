#ifndef SAFETY_H
#define SAFETY_H

int check_privilege(void);
int check_mounted(const char *device);
int check_protected_device(const char *device);
int confirm_raw_mode(const char *device, unsigned long long size, int skip_confirm);

#endif
