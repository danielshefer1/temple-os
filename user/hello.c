#include "syscall_inline.h"

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

    sys_write(STDOUT_FILENO, "sleep start\n", 12);
    sys_sleep(500);
    sys_write(STDOUT_FILENO, "sleep done\n", 11);

    sys_exit(0);
}
