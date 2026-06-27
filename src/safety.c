#include "safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int check_privilege(void)
{
    if (geteuid() == 0)
        return 0;

    int ret = system("sudo -n true 2>/dev/null");
    if (ret == 0)
        return 0;

    fprintf(stderr, "Error: root or sudoer privilege required\n");
    return -1;
}

int check_mounted(const char *device)
{
    FILE *f = fopen("/proc/mounts", "r");
    if (!f)
        return -1;

    char line[1024];
    int found = 0;
    size_t devlen = strlen(device);

    while (fgets(line, sizeof(line), f)) {
        char *saveptr = NULL;
        char *tok = strtok_r(line, " ", &saveptr);
        if (!tok)
            continue;

        if (strncmp(tok, device, devlen) == 0) {
            if (tok[devlen] == '\0' || tok[devlen] >= '0') {
                char *mountpoint = strtok_r(NULL, " ", &saveptr);
                if (mountpoint) {
                    fprintf(stderr, "  mounted at: %s\n", mountpoint);
                    found = 1;
                }
            }
        }
    }
    fclose(f);
    return found ? 1 : 0;
}

int check_protected_device(const char *device)
{
    static const char *protected[] = {
        "/dev/sda", "/dev/nvme0n1", NULL
    };

    for (int i = 0; protected[i]; i++) {
        if (strcmp(device, protected[i]) == 0) {
            fprintf(stderr, "Error: '%s' is a protected device (likely system disk)\n",
                    device);
            return 1;
        }
    }

    FILE *f = fopen("/proc/mounts", "r");
    if (!f)
        return 0;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *saveptr = NULL;
        char *tok = strtok_r(line, " ", &saveptr);
        if (!tok)
            continue;
        char *mountpoint = strtok_r(NULL, " ", &saveptr);
        if (mountpoint && strcmp(mountpoint, "/") == 0) {
            if (strncmp(tok, device, strlen(device)) == 0) {
                fprintf(stderr, "Error: '%s' contains root filesystem\n", device);
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

static void format_size(char *buf, size_t buflen, unsigned long long bytes)
{
    if (bytes >= 1024ULL * 1024 * 1024)
        snprintf(buf, buflen, "%.1f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ULL * 1024)
        snprintf(buf, buflen, "%.1f MB", (double)bytes / (1024.0 * 1024));
    else
        snprintf(buf, buflen, "%.1f KB", (double)bytes / 1024.0);
}

int confirm_raw_mode(const char *device, unsigned long long size, int skip_confirm)
{
    if (skip_confirm)
        return 0;

    char sizestr[64];
    format_size(sizestr, sizeof(sizestr), size);

    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  WARNING: RAW BLOCK DEVICE MODE\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  Device: %s\n", device);
    fprintf(stderr, "  Size:   %s\n", sizestr);
    fprintf(stderr, "  ALL DATA ON THIS DEVICE WILL BE DESTROYED!\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\nProceed? [y/N]: ");

    char answer[16];
    if (!fgets(answer, sizeof(answer), stdin))
        return -1;
    if (answer[0] != 'y' && answer[0] != 'Y')
        return -1;

    fprintf(stderr, "Type the full device name (%s) to confirm: ", device);
    if (!fgets(answer, sizeof(answer), stdin))
        return -1;

    answer[strcspn(answer, "\n")] = '\0';
    if (strcmp(answer, device) != 0) {
        fprintf(stderr, "Error: device name mismatch. Aborted.\n");
        return -1;
    }

    return 0;
}
