#include "std/std.h"

#define TAIL_BUF_SZ 4096

static long parse_ulong(const char* s) {
    long v = 0;
    int seen = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        v = v * 10 + (*s - '0');
        seen = 1;
        s++;
    }
    return seen ? v : -1;
}

// Stream tail: buffer the last n_lines lines from a non-seekable fd.
// Each line is bounded by TAIL_BUF_SZ; longer lines are truncated.
static void tail_stream(long fd, long n_lines) {
    if (n_lines <= 0) return;
    if (n_lines > 1024) n_lines = 1024;

    char* ring = (char*) sys_mmap((unsigned long)n_lines * TAIL_BUF_SZ);
    unsigned long* lens = (unsigned long*) sys_mmap((unsigned long)n_lines * sizeof(unsigned long));
    if (!ring || !lens) return;

    long head = 0;     // index of oldest line slot
    long count = 0;    // lines currently buffered
    unsigned long cur_len = 0;
    char* cur = ring + head * TAIL_BUF_SZ;

    // Seed `cur` to the slot we will fill next (==slot at index (head+count)).
    long slot = 0;
    cur = ring + slot * TAIL_BUF_SZ;

    char rbuf[1024];
    for (;;) {
        long n = sys_read(fd, rbuf, sizeof(rbuf));
        if (n <= 0) break;
        for (long i = 0; i < n; i++) {
            char ch = rbuf[i];
            if (cur_len < TAIL_BUF_SZ) cur[cur_len++] = ch;
            if (ch == '\n') {
                lens[slot] = cur_len;
                if (count < n_lines) {
                    count++;
                } else {
                    head = (head + 1) % n_lines;
                }
                slot = (head + count) % n_lines;
                cur = ring + slot * TAIL_BUF_SZ;
                cur_len = 0;
            }
        }
    }
    // Trailing bytes without newline form a final partial line.
    if (cur_len > 0) {
        lens[slot] = cur_len;
        if (count < n_lines) count++;
        else head = (head + 1) % n_lines;
    }

    for (long k = 0; k < count; k++) {
        long s = (head + k) % n_lines;
        sys_write(1, ring + s * TAIL_BUF_SZ, lens[s]);
    }

    sys_munmap(ring, (unsigned long)n_lines * TAIL_BUF_SZ);
    sys_munmap(lens, (unsigned long)n_lines * sizeof(unsigned long));
}

// Seekable tail: walk backwards from EOF in chunks until we have counted
// n_lines newlines, then dump from there to EOF.
static int tail_seekable(long fd, long n_lines) {
    long size = sys_lseek(fd, 0, SEEK_END);
    if (size < 0) return -1;
    if (size == 0) return 0;

    long pos = size;
    long want = n_lines;

    // Skip a single trailing newline so the last line counts as one.
    long start = 0;
    char chunk[1024];

    while (pos > 0) {
        long want_read = (pos >= (long)sizeof(chunk)) ? (long)sizeof(chunk) : pos;
        pos -= want_read;
        if (sys_lseek(fd, pos, SEEK_SET) < 0) return -1;
        long got = sys_read(fd, chunk, (unsigned long)want_read);
        if (got <= 0) return -1;
        for (long i = got - 1; i >= 0; i--) {
            int is_last = (pos + i == size - 1);
            if (chunk[i] == '\n' && !is_last) {
                if (--want == 0) {
                    start = pos + i + 1;
                    goto found;
                }
            }
        }
    }
    start = 0;

found:
    if (sys_lseek(fd, start, SEEK_SET) < 0) return -1;
    char buf[1024];
    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        sys_write(1, buf, (unsigned long)n);
    }
    return 0;
}

int main(int argc, char** argv) {
    long n_lines = 10;
    int i = 1;
    if (i < argc && argv[i][0] == '-' && argv[i][1] == 'n') {
        const char* num;
        if (argv[i][2]) {
            num = &argv[i][2];
            i++;
        } else if (i + 1 < argc) {
            num = argv[i + 1];
            i += 2;
        } else {
            st_puts("usage: tail [-n N] [file...]\n");
            return 1;
        }
        long v = parse_ulong(num);
        if (v < 0) { st_puts("tail: invalid count\n"); return 1; }
        n_lines = v;
    }

    if (i >= argc) {
        tail_stream(0, n_lines);
        return 0;
    }

    int rc = 0;
    int multi = (argc - i) > 1;
    for (int j = i; j < argc; j++) {
        long fd = sys_open(argv[j], O_RDONLY, 0);
        if (fd < 0) {
            st_puts("tail: ");
            st_puts(argv[j]);
            st_puts(": cannot open\n");
            rc = 1;
            continue;
        }
        if (multi) {
            if (j > i) sys_write(1, "\n", 1);
            st_puts("==> ");
            st_puts(argv[j]);
            st_puts(" <==\n");
        }
        if (tail_seekable(fd, n_lines) < 0) {
            // Fallback for unseekable fds (pipes, ttys, etc).
            tail_stream(fd, n_lines);
        }
        sys_close(fd);
    }
    return rc;
}
