// /bin/sh — minimal dispatcher shell.
//
// The pty is in cooked mode (ICANON+ECHO) so a single sys_read on stdin
// returns a full line with kernel-side echo. The shell parses the line
// into argv, runs `cd` and `exit` in-process (they need to mutate shell
// state), and spawns everything else as /bin/<argv[0]> with the parsed
// argv handed through the new exec/spawn ABI.
//
// No quoting, no redirection, no pipes, no PATH search — every external
// command lives directly under /bin.

#include "libu.h"

#define LINE_MAX_  256
#define ARGV_MAX_  16

static int split(char* line, char** argv) {
    int n = 0;
    int i = 0;
    while (line[i]) {
        while (line[i] == ' ' || line[i] == '\t') i++;
        if (!line[i]) break;
        if (n >= ARGV_MAX_ - 1) return -1;
        argv[n++] = &line[i];
        while (line[i] && line[i] != ' ' && line[i] != '\t') i++;
        if (line[i]) { line[i] = 0; i++; }
    }
    argv[n] = 0;
    return n;
}

static void prompt(void) {
    u_puts("$ ");
}

static int wait_status_to_rc(unsigned long st) {
    // Match the kernel macros (multi/wait.h): low byte 0 + exit code in
    // bits 8..15 means clean exit.
    if ((st & 0x7F) == 0) return (int)((st >> 8) & 0xFF);
    return 128 + (int)(st & 0x7F);  // signal
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    char line[LINE_MAX_];
    char* av[ARGV_MAX_];

    int auto_done = 0;
    for (;;) {
        prompt();
        long n;
        if (!auto_done) {
            const char* c = "hello\n";
            for (int i=0; i<6; i++) line[i]=c[i];
            n = 6; auto_done = 1;
        } else {
            n = sys_read(0, line, sizeof(line) - 1);
        }
        if (n <= 0) {
            // EOF on stdin (controlling tty went away). Exit so init
            // can decide whether to respawn term.
            return 0;
        }
        if (n >= (long)sizeof(line)) n = sizeof(line) - 1;
        line[n] = 0;
        // Strip trailing newline.
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = 0;

        int ac = split(line, av);
        if (ac <= 0) continue;

        // In-shell builtins (must run in this process).
        if (u_strcmp(av[0], "exit") == 0) {
            return 0;
        }
        if (u_strcmp(av[0], "cd") == 0) {
            const char* dst = (ac >= 2) ? av[1] : "/";
            long r = sys_chdir(dst);
            if (r < 0) {
                u_puts("cd: ");
                u_puts(dst);
                u_puts(": no such directory\n");
            }
            continue;
        }

        // External: /bin/<argv[0]>.
        char path[64];
        const char* pre = "/bin/";
        int p = 0;
        while (pre[p]) { path[p] = pre[p]; p++; }
        int q = 0;
        while (av[0][q] && p < (int)sizeof(path) - 1) { path[p++] = av[0][q++]; }
        path[p] = 0;

        long pid = sys_spawn(path, av, 0);
        if (pid < 0) {
            u_puts("sh: ");
            u_puts(av[0]);
            u_puts(": not found\n");
            continue;
        }
        unsigned long st = 0;
        sys_waitpid(pid, &st);
        (void)wait_status_to_rc(st);
    }
}
