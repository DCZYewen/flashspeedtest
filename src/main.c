#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bench.h"
#include "io.h"
#include "output.h"
#include "parse.h"
#include "safety.h"

#define VERSION "1.0.1"
#define DEFAULT_BS  (1ULL * 1024 * 1024)
#define DEFAULT_SZ  (256ULL * 1024 * 1024)

typedef struct {
    int raw;
    int do_write;
    int do_read;
    int do_verify;
    int skip_confirm;
    int progress;
    int count;
    unsigned long long bs;
    unsigned long long sz;
    const char *target;
} config_t;

static volatile sig_atomic_t got_signal = 0;
static char *g_tmpfile = NULL;
static int g_is_raw = 0;

static void signal_handler(int sig)
{
    (void)sig;
    got_signal = 1;
}

static void cleanup_tmpfile(void)
{
    if (g_tmpfile && !g_is_raw) {
        unlink(g_tmpfile);
        free(g_tmpfile);
        g_tmpfile = NULL;
    }
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "flashspeedtest v%s\n"
        "\n"
        "Usage: %s [OPTIONS] <target>\n"
        "\n"
        "Target:\n"
        "  /path/to/mount      Test filesystem read/write speed (safe)\n"
        "  -raw /dev/sdX       Test raw block device speed (destructive!)\n"
        "\n"
        "Options:\n"
        "  -bs <size>          Block size (default: 1M)\n"
        "  -sz <size>          Total test size (default: 256M)\n"
        "  -w                  Write test only\n"
        "  -r                  Read test only\n"
        "  -rw                 Read + write test (default)\n"
        "  -verify             Verify data integrity after write\n"
        "  -count <n>          Number of iterations (default: 1)\n"
        "  -progress           Show live progress bar (adds I/O overhead)\n"
        "  -y                  Skip confirmations (for scripts)\n"
        "  -h, --help          Show this help\n"
        "\n"
        "Size suffixes: K, M, G\n"
        "\n"
        "Examples:\n"
        "  %s /mnt/usbdrv\n"
        "  %s -raw /dev/sdc\n"
        "  %s -bs 4K -sz 64M /mnt/sdcard\n",
        VERSION, prog, prog, prog, prog);
}

static int parse_args(int argc, char **argv, config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->bs = DEFAULT_BS;
    cfg->sz = DEFAULT_SZ;
    cfg->count = 1;
    cfg->do_write = 1;
    cfg->do_read = 1;

    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-raw") == 0) {
            cfg->raw = 1;
            i++;
        } else if (strcmp(argv[i], "-bs") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Error: -bs requires argument\n"); return -1; }
            if (parse_size(argv[i + 1], &cfg->bs) < 0) { fprintf(stderr, "Error: invalid -bs value\n"); return -1; }
            i += 2;
        } else if (strcmp(argv[i], "-sz") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Error: -sz requires argument\n"); return -1; }
            if (parse_size(argv[i + 1], &cfg->sz) < 0) { fprintf(stderr, "Error: invalid -sz value\n"); return -1; }
            i += 2;
        } else if (strcmp(argv[i], "-w") == 0) {
            cfg->do_write = 1; cfg->do_read = 0;
            i++;
        } else if (strcmp(argv[i], "-r") == 0) {
            cfg->do_read = 1; cfg->do_write = 0;
            i++;
        } else if (strcmp(argv[i], "-rw") == 0) {
            cfg->do_write = 1; cfg->do_read = 1;
            i++;
        } else if (strcmp(argv[i], "-verify") == 0) {
            cfg->do_verify = 1;
            i++;
        } else if (strcmp(argv[i], "-count") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Error: -count requires argument\n"); return -1; }
            cfg->count = atoi(argv[i + 1]);
            if (cfg->count < 1) { fprintf(stderr, "Error: -count must be >= 1\n"); return -1; }
            i += 2;
        } else if (strcmp(argv[i], "-y") == 0) {
            cfg->skip_confirm = 1;
            i++;
        } else if (strcmp(argv[i], "-progress") == 0) {
            cfg->progress = 1;
            i++;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            return -1;
        } else {
            cfg->target = argv[i];
            i++;
        }
    }

    if (!cfg->target) {
        fprintf(stderr, "Error: no target specified\n");
        return -1;
    }

    if (cfg->bs == 0 || cfg->sz == 0) {
        fprintf(stderr, "Error: block size and total size must be > 0\n");
        return -1;
    }

    return 0;
}

static int resolve_target(config_t *cfg, char *device, size_t devlen,
                          char *fstype, size_t fslen)
{
    struct stat st;
    if (stat(cfg->target, &st) < 0) {
        fprintf(stderr, "Error: cannot stat '%s': %s\n",
                cfg->target, strerror(errno));
        return -1;
    }

    static char resolved[PATH_MAX];
    if (!realpath(cfg->target, resolved)) {
        fprintf(stderr, "Error: cannot resolve '%s': %s\n",
                cfg->target, strerror(errno));
        return -1;
    }
    cfg->target = resolved;

    if (cfg->raw) {
        if (!S_ISBLK(st.st_mode)) {
            fprintf(stderr, "Error: -raw mode requires a block device\n");
            return -1;
        }
        snprintf(device, devlen, "%s", cfg->target);
        fstype[0] = '\0';
    } else {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: target must be a directory (mount point)\n");
            return -1;
        }

        FILE *f = fopen("/proc/mounts", "r");
        if (!f) {
            fprintf(stderr, "Error: cannot open /proc/mounts\n");
            return -1;
        }

        char line[1024];
        size_t best_len = 0;
        while (fgets(line, sizeof(line), f)) {
            char *saveptr = NULL;
            char *dev = strtok_r(line, " ", &saveptr);
            char *mnt = strtok_r(NULL, " ", &saveptr);
            char *fs = strtok_r(NULL, " ", &saveptr);
            if (!dev || !mnt || !fs)
                continue;

            size_t mlen = strlen(mnt);
            if (strncmp(cfg->target, mnt, mlen) == 0 &&
                (cfg->target[mlen] == '/' || cfg->target[mlen] == '\0' ||
                 strcmp(mnt, "/") == 0) &&
                mlen > best_len) {
                snprintf(device, devlen, "%s", dev);
                snprintf(fstype, fslen, "%s", fs);
                best_len = mlen;
            }
        }
        fclose(f);

        if (best_len == 0) {
            fprintf(stderr, "Warning: '%s' not found in /proc/mounts\n",
                    cfg->target);
            device[0] = '\0';
            fstype[0] = '\0';
        }
    }

    return 0;
}

static unsigned long long clamp_size(unsigned long long requested,
                                     unsigned long long available,
                                     const char *label)
{
    if (available > 0 && requested > available) {
        fprintf(stderr, "Warning: requested %s (%llu) exceeds available (%llu), clamping\n",
                label, requested, available);
        return available;
    }
    return requested;
}

static int run_single(config_t *cfg, int iteration)
{
    char device[PATH_MAX] = {0};
    char fstype[64] = {0};
    int fd = -1;
    void *buf = NULL;
    int ret = 1;
    char *tmpfile = NULL;

    if (resolve_target(cfg, device, sizeof(device), fstype, sizeof(fstype)) < 0)
        return 1;

    if (cfg->raw) {
        if (check_protected_device(device))
            return 1;

        int mnt = check_mounted(device);
        if (mnt > 0) {
            fprintf(stderr, "Error: device '%s' is currently mounted:\n", device);
            return 1;
        } else if (mnt < 0) {
            fprintf(stderr, "Warning: cannot check mount status\n");
        }

        if (confirm_raw_mode(device, cfg->sz, cfg->skip_confirm) < 0)
            return 1;
    }

    print_header(cfg->target, device, fstype, cfg->bs, cfg->sz,
                 cfg->raw, cfg->do_write, cfg->do_read);

    if (cfg->raw) {
        fd = open_target_direct(device, 1);
        if (fd < 0)
            return 1;

        unsigned long long devsz = get_device_size(fd);
        if (devsz == 0) {
            fprintf(stderr, "Error: cannot determine device size\n");
            close(fd);
            return 1;
        }
        cfg->sz = clamp_size(cfg->sz, devsz, "test size");
    } else {
        char pathbuf[512];
        snprintf(pathbuf, sizeof(pathbuf), "%s/.flashspeedtest_XXXXXX", cfg->target);

        fd = mkstemp(pathbuf);
        if (fd < 0) {
            fprintf(stderr, "Error: cannot create temp file in '%s': %s\n",
                    cfg->target, strerror(errno));
            return 1;
        }

        tmpfile = strdup(pathbuf);
        g_tmpfile = tmpfile;

        /* Re-open with O_DIRECT */
        close(fd);
        fd = open(tmpfile, O_DIRECT | O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "Error: cannot reopen temp file with O_DIRECT: %s\n",
                    strerror(errno));
            unlink(tmpfile);
            free(tmpfile);
            g_tmpfile = NULL;
            return 1;
        }

        unsigned long long free_space = get_file_free_space(cfg->target);
        cfg->sz = clamp_size(cfg->sz, free_space, "test size");
    }

    buf = alloc_aligned(cfg->bs, 4096);
    if (!buf) {
        fprintf(stderr, "Error: cannot allocate aligned buffer\n");
        goto cleanup;
    }

    if (iteration > 0)
        fprintf(stderr, "\n--- Iteration %d/%d ---\n", iteration + 1, cfg->count);

    bench_result_t wres = {0}, rres = {0};
    verify_result_t vres = {0};

    if (cfg->do_write) {
        if (bench_write(fd, buf, cfg->bs, cfg->sz, cfg->progress, &wres) < 0) {
            fprintf(stderr, "Write test failed after %.1f MB\n",
                    (double)wres.bytes / (1024.0 * 1024.0));
            goto cleanup;
        }
        print_result("Write:", wres.bytes, wres.elapsed);
    }

    if (cfg->do_read) {
        if (cfg->do_write) {
            lseek(fd, 0, SEEK_SET);
        } else if (!cfg->raw) {
            /* Read-only FS mode: pre-allocate file with posix_fallocate */
            if (posix_fallocate(fd, 0, cfg->sz) != 0) {
                fprintf(stderr, "Error: posix_fallocate failed: %s\n",
                        strerror(errno));
                goto cleanup;
            }
            /* Fill with known pattern */
            if (getrandom(buf, cfg->bs, 0) != (ssize_t)cfg->bs) {
                fprintf(stderr, "Error: getrandom failed\n");
                goto cleanup;
            }
            unsigned long long written = 0;
            while (written < cfg->sz) {
                unsigned long long chunk = cfg->bs;
                if (written + chunk > cfg->sz)
                    chunk = cfg->sz - written;
                ssize_t wret = pwrite(fd, buf, chunk, written);
                if (wret < 0) {
                    fprintf(stderr, "Error: pwrite failed during read-only fill\n");
                    goto cleanup;
                }
                written += wret;
            }
            fdatasync(fd);
            lseek(fd, 0, SEEK_SET);
        }

        if (bench_read(fd, buf, cfg->bs, cfg->sz, cfg->progress, &rres) < 0) {
            fprintf(stderr, "Read test failed after %.1f MB\n",
                    (double)rres.bytes / (1024.0 * 1024.0));
            goto cleanup;
        }
        print_result("Read:", rres.bytes, rres.elapsed);
    }

    if (cfg->do_verify) {
        lseek(fd, 0, SEEK_SET);
        if (bench_verify(fd, buf, cfg->bs, cfg->sz, &vres) < 0) {
            fprintf(stderr, "Verify test failed\n");
            goto cleanup;
        }
        print_verify_result(vres.total_blocks, vres.passed, vres.failed);
    }

    ret = 0;

cleanup:
    if (buf)
        free(buf);
    if (fd >= 0)
        close(fd);
    if (tmpfile) {
        unlink(tmpfile);
        free(tmpfile);
        g_tmpfile = NULL;
    }
    return ret;
}

int main(int argc, char **argv)
{
    config_t cfg;
    if (parse_args(argc, argv, &cfg) < 0) {
        usage(argv[0]);
        return 1;
    }

    if (cfg.raw && check_privilege() < 0)
        return 1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    g_is_raw = cfg.raw;
    atexit(cleanup_tmpfile);

    int exit_code = 0;
    for (int i = 0; i < cfg.count; i++) {
        if (got_signal) {
            fprintf(stderr, "\nInterrupted.\n");
            break;
        }
        int ret = run_single(&cfg, i);
        if (ret != 0) {
            exit_code = 2;
            break;
        }
    }

    return exit_code;
}
