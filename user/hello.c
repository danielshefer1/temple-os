#include "syscall_inline.h"
#include "sys/wait.h"

static const char start_m[]  = "starting\n";
static const char child_m[]  = "child running\n";
static const char wait_m[]   = "parent reaped child, status=";
static const char nl_m[]     = "\n";
static const char loop_done[] = "loop done, all children reaped\n";

__attribute__((used)) const char* volatile msg_ptr = start_m;

static unsigned long my_strlen(const char* s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

// Tiny base-10 itoa into a fixed buffer. Returns length written.
static unsigned long itoa10(unsigned long v, char* buf) {
    char tmp[24];
    unsigned long n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (char)(v % 10); v /= 10; }
    for (unsigned long i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = '\n';
    return n + 1;
}

void _start(void) {
    sys_write(STDOUT_FILENO, msg_ptr, my_strlen(msg_ptr));

    // Loop: fork + waitpid 5 times. Verifies waitpid blocks and unblocks
    // correctly, propagates exit codes, and (crucially) doesn't leak
    // user-AS pages — without the drain_pending_reap fix this would burn
    // ~16 KB+ of user pages per iteration.
    for (long iter = 1; iter <= 5; iter++) {
        long pid = sys_fork();
        if (pid == 0) {
            sys_write(STDOUT_FILENO, child_m, my_strlen(child_m));
            sys_exit(iter * 10);   // distinctive per-child exit code
        }

        unsigned long status = 0;
        long reaped = sys_waitpid(pid, &status);
        (void)reaped;

        sys_write(STDOUT_FILENO, wait_m, my_strlen(wait_m));
        char buf[32];
        unsigned long n = itoa10(status, buf);
        sys_write(STDOUT_FILENO, buf, n);
    }

    sys_write(STDOUT_FILENO, loop_done, my_strlen(loop_done));

    // /dev/tty exercise: open the chardev by path, write through it. Goes
    // entirely through the new devfs path: namei -> ext2 inode (CHARDEV) ->
    // vfs_open dispatches to devfs_lookup(4, 0) -> tty_fops with
    // private_data = &console_tty.
    static const char devtty_path[] = "/dev/tty";
    static const char devtty_msg[]  = "hello via /dev/tty\n";
    static const char devtty_fail[] = "open /dev/tty failed\n";
    long fd = sys_open(devtty_path, O_RDWR, 0);
    if (fd < 0) {
        sys_write(STDOUT_FILENO, devtty_fail, my_strlen(devtty_fail));
        char ebuf[32];
        unsigned long en = itoa10((unsigned long)(-fd), ebuf);
        sys_write(STDOUT_FILENO, ebuf, en);
    } else {
        sys_write(fd, devtty_msg, my_strlen(devtty_msg));
        sys_close(fd);
    }

    // /dev/null: writes succeed, reads return 0 (EOF).
    {
        static const char ok[]   = "/dev/null ok\n";
        static const char fail[] = "/dev/null FAIL\n";
        long nfd = sys_open("/dev/null", O_RDWR, 0);
        char tmp[8];
        long w = (nfd < 0) ? -1 : sys_write(nfd, "abcd", 4);
        long r = (nfd < 0) ? -1 : sys_read_for_test_(nfd, tmp, sizeof(tmp));
        if (nfd >= 0) sys_close(nfd);
        sys_write(STDOUT_FILENO, (nfd >= 0 && w == 4 && r == 0) ? ok : fail,
                  my_strlen(nfd >= 0 && w == 4 && r == 0 ? ok : fail));
    }

    // /dev/zero: reads return all zeros; writes are silently dropped.
    {
        static const char ok[]   = "/dev/zero ok\n";
        static const char fail[] = "/dev/zero FAIL\n";
        long zfd = sys_open("/dev/zero", O_RDWR, 0);
        char tmp[16];
        for (unsigned long i = 0; i < sizeof(tmp); i++) tmp[i] = (char)0xAB;
        long r = (zfd < 0) ? -1 : sys_read_for_test_(zfd, tmp, sizeof(tmp));
        int allzero = 1;
        for (unsigned long i = 0; i < sizeof(tmp); i++) if (tmp[i] != 0) { allzero = 0; break; }
        if (zfd >= 0) sys_close(zfd);
        sys_write(STDOUT_FILENO, (zfd >= 0 && r == (long)sizeof(tmp) && allzero) ? ok : fail,
                  my_strlen(zfd >= 0 && r == (long)sizeof(tmp) && allzero ? ok : fail));
    }

    // /dev/ram0: write a pattern, lseek(0), read back, compare.
    {
        static const char ok[]   = "/dev/ram0 ok\n";
        static const char fail[] = "/dev/ram0 FAIL fd=";
        long rfd = sys_open("/dev/ram0", O_RDWR, 0);
        const char pattern[] = "RAM-DEV-PATTERN";
        char tmp[sizeof(pattern)];
        long w = -1, s = -1, r = -1;
        int good = 0;
        if (rfd >= 0) {
            w = sys_write(rfd, pattern, sizeof(pattern));
            s = sys_lseek_for_test_(rfd, 0, 0 /*SEEK_SET*/);
            r = sys_read_for_test_(rfd, tmp, sizeof(tmp));
            good = (w == (long)sizeof(pattern) && r == (long)sizeof(tmp));
            for (unsigned long i = 0; good && i < sizeof(pattern); i++) {
                if (tmp[i] != pattern[i]) { good = 0; break; }
            }
            sys_close(rfd);
        }
        if (good) {
            sys_write(STDOUT_FILENO, ok, my_strlen(ok));
        } else {
            sys_write(STDOUT_FILENO, fail, my_strlen(fail));
            char eb[64];
            unsigned long n;
            n = itoa10((unsigned long)rfd, eb); sys_write(STDOUT_FILENO, eb, n);
            sys_write(STDOUT_FILENO, " w=", 3);
            n = itoa10((unsigned long)w, eb);   sys_write(STDOUT_FILENO, eb, n);
            sys_write(STDOUT_FILENO, " s=", 3);
            n = itoa10((unsigned long)s, eb);   sys_write(STDOUT_FILENO, eb, n);
            sys_write(STDOUT_FILENO, " r=", 3);
            n = itoa10((unsigned long)r, eb);   sys_write(STDOUT_FILENO, eb, n);
        }
    }

    // mknod: create /dev/foo (chardev pointing at the tty), open it,
    // write through it. Verifies the syscall + on-disk dev_id encoding.
    {
        static const char ok[]   = "mknod /dev/foo ok\n";
        static const char fail[] = "mknod /dev/foo FAIL\n";
        // Best-effort cleanup if a previous boot left the node behind.
        sys_unlink_for_test_("/dev/foo");
        long mr = sys_mknod("/dev/foo", S_IFCHR_T, 0666, UMKDEV(4, 0));
        long mfd = (mr < 0) ? -1 : sys_open("/dev/foo", O_RDWR, 0);
        long w = (mfd < 0) ? -1 : sys_write(mfd, "via foo\n", 8);
        if (mfd >= 0) sys_close(mfd);
        sys_unlink_for_test_("/dev/foo");
        sys_write(STDOUT_FILENO, (mr >= 0 && mfd >= 0 && w == 8) ? ok : fail,
                  my_strlen(mr >= 0 && mfd >= 0 && w == 8 ? ok : fail));
    }

    // mmap: 3 pages, write+read pattern, then munmap.
    {
        static const char ok[]   = "mmap ok\n";
        static const char fail[] = "mmap FAIL\n";
        unsigned long sz = 3 * 4096;
        char* p = (char*)sys_mmap(sz);
        int good = ((long)p > 0);
        if (good) {
            for (unsigned long i = 0; i < sz; i++) p[i] = (char)(i & 0xFF);
            for (unsigned long i = 0; i < sz; i++) {
                if (p[i] != (char)(i & 0xFF)) { good = 0; break; }
            }
            if (sys_munmap(p, sz) != 0) good = 0;
        }
        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
    }

    // mmap + fork: parent writes a pattern, forks; child verifies its copy
    // and unmaps; parent waits, then unmaps independently. Verifies that
    // clone_user_pml4 deep-copies mmap'd pages and that munmap in one task
    // doesn't pull pages out from under the other.
    {
        static const char ok[]   = "mmap fork ok\n";
        static const char fail[] = "mmap fork FAIL\n";
        unsigned long sz = 2 * 4096;
        char* p = (char*)sys_mmap(sz);
        int good = ((long)p > 0);
        if (good) {
            for (unsigned long i = 0; i < sz; i++) p[i] = (char)((i + 7) & 0xFF);
            long pid = sys_fork();
            if (pid == 0) {
                int child_ok = 1;
                for (unsigned long i = 0; i < sz; i++) {
                    if (p[i] != (char)((i + 7) & 0xFF)) { child_ok = 0; break; }
                }
                p[0] = 'X';
                sys_munmap(p, sz);
                sys_exit(child_ok ? 42 : 1);
            }
            unsigned long status = 0;
            sys_waitpid(pid, &status);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 42) good = 0;
            if (good && p[0] != (char)(7 & 0xFF)) good = 0;
            if (sys_munmap(p, sz) != 0) good = 0;
        }
        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
    }

    // mmap negative tests.
    {
        static const char ok[]   = "mmap neg ok\n";
        static const char fail[] = "mmap neg FAIL\n";
        int good = 1;
        if ((long)sys_mmap(0) >= 0) good = 0;
        if (sys_munmap((void*)0, 4096) >= 0) good = 0;
        char* p2 = (char*)sys_mmap(4096);
        if ((long)p2 <= 0) good = 0;
        else {
            if (sys_munmap(p2 + 1, 4096) >= 0) good = 0;
            if (sys_munmap(p2, 0) >= 0) good = 0;
            if (sys_munmap(p2, 4096) != 0) good = 0;
        }
        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
    }

    sys_write(STDOUT_FILENO, "sleep start\n", 12);
    sys_sleep(500);
    sys_write(STDOUT_FILENO, "sleep done\n", 11);

    // ---- M8 PR A tests ----

    // Test 1: anonymous pipe + fork. Child writes 3 bytes; parent reads them.
    {
        static const char ok[]   = "pipe ok\n";
        static const char fail[] = "pipe FAIL\n";
        int fds[2] = { -1, -1 };
        long pr = sys_pipe(fds);
        int good = (pr == 0 && fds[0] >= 3 && fds[1] >= 3);
        if (good) {
            long pid = sys_fork();
            if (pid == 0) {
                sys_close(fds[0]);
                sys_write(fds[1], "abc", 3);
                sys_close(fds[1]);
                sys_exit(0);
            }
            sys_close(fds[1]);
            char b[8] = {0};
            long r = sys_read_for_test_(fds[0], b, sizeof(b));
            sys_close(fds[0]);
            unsigned long st = 0;
            sys_waitpid(pid, &st);
            if (!(r == 3 && b[0] == 'a' && b[1] == 'b' && b[2] == 'c')) good = 0;
        }
        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
    }

    // Test 2: dup2-based stdout redirection. Child dup2(fds[1], 1) and
    // writes via fd 1; parent reads from fds[0].
    {
        static const char ok[]   = "dup2 ok\n";
        static const char fail[] = "dup2 FAIL\n";
        int fds[2] = { -1, -1 };
        long pr = sys_pipe(fds);
        int good = (pr == 0);
        if (good) {
            long pid = sys_fork();
            if (pid == 0) {
                sys_close(fds[0]);
                sys_dup2(fds[1], 1);
                sys_close(fds[1]);
                sys_write(1, "via stdout\n", 11);
                sys_exit(0);
            }
            sys_close(fds[1]);
            char b[16] = {0};
            long r = sys_read_for_test_(fds[0], b, sizeof(b));
            sys_close(fds[0]);
            unsigned long st = 0;
            sys_waitpid(pid, &st);
            if (r != 11) good = 0;
            const char* exp = "via stdout\n";
            for (int i = 0; good && i < 11; i++) if (b[i] != exp[i]) good = 0;
        }
        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
    }

    // Test 3: FIFO. Create /myfifo (the data fs has no /tmp), fork two
    // children — reader and writer — and assert end-to-end transfer.
    {
        static const char ok[]   = "fifo ok\n";
        static const char fail[] = "fifo FAIL\n";
        sys_unlink_for_test_("/myfifo");
        long mr = sys_mknod("/myfifo", S_IFIFO_T, 0666, 0);
        int good = (mr == 0);
        unsigned long ws = 0, rs = 0;
        if (good) {
            long wpid = sys_fork();
            if (wpid == 0) {
                long fd = sys_open("/myfifo", O_WRONLY, 0);
                long w = (fd >= 0) ? sys_write(fd, "hello", 5) : fd;
                if (fd >= 0) sys_close(fd);
                sys_exit((fd >= 0 && w == 5) ? 0 : (long)(-fd));
            }
            long rpid = sys_fork();
            if (rpid == 0) {
                long fd = sys_open("/myfifo", O_RDONLY, 0);
                char b[8] = {0};
                long n = (fd >= 0) ? sys_read_for_test_(fd, b, 5) : -1;
                if (fd >= 0) sys_close(fd);
                int rc = (n == 5 && b[0] == 'h' && b[4] == 'o') ? 0 : 99;
                sys_exit(rc);
            }
            sys_waitpid(wpid, &ws);
            sys_waitpid(rpid, &rs);
            if (!(WIFEXITED(ws) && WEXITSTATUS(ws) == 0 &&
                  WIFEXITED(rs) && WEXITSTATUS(rs) == 0)) good = 0;
        }
        sys_unlink_for_test_("/myfifo");
        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
        (void)mr; (void)ws; (void)rs;
    }

    // Test 4: wait status macros. Two children — one killed by SIGINT
    // (default action: terminate), one cleanly exit(7).
    {
        static const char ok[]   = "wait macros ok\n";
        static const char fail[] = "wait macros FAIL\n";
        int good = 1;

        long pid = sys_fork();
        if (pid == 0) {
            // Tight syscall loop so SIGINT is delivered on return-to-user.
            for (;;) (void)sys_getpid();
        }
        sys_kill(pid, SIGINT);
        unsigned long st = 0;
        sys_waitpid(pid, &st);
        if (!(WIFSIGNALED(st) && WTERMSIG(st) == SIGINT)) good = 0;

        long pid2 = sys_fork();
        if (pid2 == 0) sys_exit(7);
        unsigned long st2 = 0;
        sys_waitpid(pid2, &st2);
        if (!(WIFEXITED(st2) && WEXITSTATUS(st2) == 7)) good = 0;

        sys_write(STDOUT_FILENO, good ? ok : fail,
                  my_strlen(good ? ok : fail));
    }

    // Test 5: TIOCGWINSZ on stdin (the tty). Just sanity-check that the
    // call succeeds and returns non-zero geometry.
    {
        static const char ok[]   = "winsize ok ";
        static const char fail[] = "winsize FAIL\n";
        winsize_t ws = {0};
        long r = sys_ioctl(0, TIOCGWINSZ, &ws);
        int good = (r == 0 && ws.ws_col != 0 && ws.ws_row != 0);
        if (good) {
            sys_write(STDOUT_FILENO, ok, my_strlen(ok));
            char b[24];
            unsigned long n;
            n = itoa10(ws.ws_col, b); b[n-1] = 'x';   // overwrite the trailing '\n'
            sys_write(STDOUT_FILENO, b, n);
            n = itoa10(ws.ws_row, b);
            sys_write(STDOUT_FILENO, b, n);
        } else {
            sys_write(STDOUT_FILENO, fail, my_strlen(fail));
        }
    }

    sys_exit(0);
}
