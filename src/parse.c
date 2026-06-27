#include "parse.h"
#include <errno.h>
#include <stdlib.h>

int parse_size(const char *str, unsigned long long *out)
{
    if (!str || !str[0])
        return -1;

    errno = 0;
    char *end = NULL;
    unsigned long long val = strtoull(str, &end, 10);
    if (errno == ERANGE)
        return -1;

    if (*end == '\0') {
        *out = val;
        return 0;
    }

    if (end[1] != '\0')
        return -1;

    switch (*end) {
    case 'K': case 'k':
        *out = val * 1024ULL;
        return 0;
    case 'M': case 'm':
        *out = val * 1024ULL * 1024ULL;
        return 0;
    case 'G': case 'g':
        *out = val * 1024ULL * 1024ULL * 1024ULL;
        return 0;
    default:
        return -1;
    }
}
