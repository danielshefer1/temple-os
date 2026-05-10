#include "std/std.h"

static long parse_long(const char* s) {
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

// Map a few common signal names. Only a tiny subset of POSIX — covers what
// the kernel actually delivers today.
static int parse_signo(const char* s) {
    if (s[0] >= '0' && s[0] <= '9') {
        long v = parse_long(s);
        return (v >= 0 && v < 64) ? (int)v : -1;
    }
    if (st_strcmp(s, "INT")  == 0 || st_strcmp(s, "SIGINT")  == 0) return SIGINT;
    if (st_strcmp(s, "TERM") == 0 || st_strcmp(s, "SIGTERM") == 0) return SIGTERM;
    if (st_strcmp(s, "ALRM") == 0 || st_strcmp(s, "SIGALRM") == 0) return SIGALRM;
    if (st_strcmp(s, "CHLD") == 0 || st_strcmp(s, "SIGCHLD") == 0) return SIGCHLD;
    if (st_strcmp(s, "WINCH") == 0 || st_strcmp(s, "SIGWINCH") == 0) return SIGWINCH;
    return -1;
}

int main(int argc, char** argv) {
    int signo = SIGTERM;
    int first_pid = 1;

    if (argc < 2) {
        st_puts("usage: kill [-N|-NAME] <pid...>\n");
        return 1;
    }

    if (argv[1][0] == '-' && argv[1][1] != 0) {
        signo = parse_signo(&argv[1][1]);
        if (signo < 0) {
            st_puts("kill: unknown signal\n");
            return 1;
        }
        first_pid = 2;
    }

    if (first_pid >= argc) {
        st_puts("kill: no pid given\n");
        return 1;
    }

    int rc = 0;
    for (int i = first_pid; i < argc; i++) {
        long pid = parse_long(argv[i]);
        if (pid < 0) {
            st_puts("kill: ");
            st_puts(argv[i]);
            st_puts(": invalid pid\n");
            rc = 1;
            continue;
        }
        long r = sys_kill(pid, signo);
        if (r < 0) {
            st_puts("kill: ");
            st_puts(argv[i]);
            st_puts(": no such process\n");
            rc = 1;
        }
    }
    return rc;
}
