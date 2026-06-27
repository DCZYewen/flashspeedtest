#include "output.h"
#include <math.h>
#include <unistd.h>

#define PROGRESS_WIDTH 30

static int is_tty(void)
{
    static int cached = -1;
    if (cached < 0)
        cached = isatty(STDERR_FILENO);
    return cached;
}

void print_header(const char *target, const char *device,
                  const char *fstype, unsigned long long bs,
                  unsigned long long total, int raw, int do_write, int do_read)
{
    (void)raw;
    const char *modestr;
    if (do_write && do_read)
        modestr = "Sequential Read+Write";
    else if (do_write)
        modestr = "Sequential Write";
    else
        modestr = "Sequential Read";

    fprintf(stderr, "\n");
    fprintf(stderr, "flashspeedtest v1.0.1\n");
    fprintf(stderr, "Target:     %s", target);
    if (device && device[0])
        fprintf(stderr, " (%s, %s)", fstype ? fstype : "raw", device);
    fprintf(stderr, "\n");
    fprintf(stderr, "Block size: ");
    if (bs >= 1024 * 1024)
        fprintf(stderr, "%lluM", bs / (1024 * 1024));
    else if (bs >= 1024)
        fprintf(stderr, "%lluK", bs / 1024);
    else
        fprintf(stderr, "%llu", bs);
    fprintf(stderr, "\n");
    fprintf(stderr, "Total size: ");
    if (total >= 1024ULL * 1024 * 1024)
        fprintf(stderr, "%.0fG", (double)total / (1024.0 * 1024 * 1024));
    else
        fprintf(stderr, "%.0fM", (double)total / (1024.0 * 1024));
    fprintf(stderr, "\n");
    fprintf(stderr, "Mode:       %s\n", modestr);
    fprintf(stderr, "\n");
}

void print_progress(FILE *fp, const char *label, unsigned long long done,
                    unsigned long long total, double elapsed)
{
    double pct = total > 0 ? (double)done / total * 100.0 : 0;
    int filled = (int)(pct / 100.0 * PROGRESS_WIDTH);
    if (filled > PROGRESS_WIDTH)
        filled = PROGRESS_WIDTH;

    double speed = elapsed > 0 ? (double)done / elapsed / (1024.0 * 1024.0) : 0;
    double done_mb = (double)done / (1024.0 * 1024.0);
    double total_mb = (double)total / (1024.0 * 1024.0);

    if (is_tty()) {
        fprintf(fp, "\r%s %.0f/%.0f MB  [", label, done_mb, total_mb);
        for (int i = 0; i < PROGRESS_WIDTH; i++)
            fprintf(fp, "%c", i < filled ? '=' : ' ');
        fprintf(fp, "] %3.0f%%  %.1f MB/s", pct, speed);
        fflush(fp);
    } else {
        static int last_pct = -1;
        int pct_int = (int)pct;
        if (pct_int % 10 == 0 && pct_int > 0 && pct_int != last_pct) {
            last_pct = pct_int;
            fprintf(fp, "%s %d%% (%.1f MB/s)\n", label, pct_int, speed);
        }
        if (done >= total)
            last_pct = -1;
    }
}

void print_result(const char *label, unsigned long long bytes, double elapsed)
{
    double mb = (double)bytes / (1024.0 * 1024.0);
    double speed = elapsed > 0 ? mb / elapsed : 0;
    fprintf(stderr, "  %-6s %7.1f MB/s (%.0f MB in %.2fs)\n",
            label, speed, mb, elapsed);
}

void print_verify_result(unsigned long long total_blocks,
                         unsigned long long passed,
                         unsigned long long failed)
{
    fprintf(stderr, "  Verify: %llu/%llu blocks", passed, total_blocks);
    if (failed > 0)
        fprintf(stderr, " (%llu FAILED)", failed);
    else
        fprintf(stderr, " OK");
    fprintf(stderr, "\n");
}
