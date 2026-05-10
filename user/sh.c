// /bin/sh — minimal shell with pipes and redirection.
//
// The pty is in cooked mode (ICANON+ECHO) so a single sys_read on stdin
// returns a full line with kernel-side echo. The shell tokenizes the line,
// splits on `|` into pipeline stages, scans each stage for `<file`,
// `>file`, `>>file` redirs, then either:
//   - runs `cd` / `exit` in-process when the line is a single stage with
//     no pipes or redirs (those builtins must mutate shell state), or
//   - sys_spawns a single external command (fast path for plain commands), or
//   - fork()/exec()s a pipeline with sys_pipe + sys_dup2 plumbing.
//
// No quoting, no `&` jobs, no PATH search — every external command lives
// directly under /bin.

#include "std/std.h"

#define LINE_MAX_     256
#define TOKBUF_MAX    320
#define TOKENS_MAX     64
#define ARGV_MAX_      16
#define STAGES_MAX      8
#define HIST_MAX       64
#define HIST_FILE     "/home/.sh_history"

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND,
} tok_kind_t;

typedef struct {
    char*       argv[ARGV_MAX_];
    int         argc;
    const char* in_file;
    const char* out_file;
    int         append;
} stage_t;

static int tokenize(const char* line, char* tokbuf,
                    char** words, tok_kind_t* kinds) {
    int n = 0, tb = 0, i = 0;
    while (line[i]) {
        char c = line[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        if (n >= TOKENS_MAX - 1) return -1;
        if (c == '|') { kinds[n] = TOK_PIPE;     words[n++] = 0; i++; continue; }
        if (c == '<') { kinds[n] = TOK_REDIR_IN; words[n++] = 0; i++; continue; }
        if (c == '>') {
            i++;
            if (line[i] == '>') { kinds[n] = TOK_REDIR_APPEND; words[n++] = 0; i++; }
            else                { kinds[n] = TOK_REDIR_OUT;    words[n++] = 0; }
            continue;
        }
        kinds[n]   = TOK_WORD;
        words[n++] = &tokbuf[tb];
        while (line[i] && line[i] != ' ' && line[i] != '\t' &&
               line[i] != '|' && line[i] != '<' && line[i] != '>') {
            if (tb >= TOKBUF_MAX - 1) return -1;
            tokbuf[tb++] = line[i++];
        }
        if (tb >= TOKBUF_MAX) return -1;
        tokbuf[tb++] = 0;
    }
    return n;
}

// Returns number of stages on success, -1 on parse error (with err_msg set).
// Stages are filled into `stages`.
static int parse_pipeline(int ntoks, char** words, tok_kind_t* kinds,
                          stage_t* stages, const char** err_msg) {
    int s = 0;
    stages[0].argc     = 0;
    stages[0].in_file  = 0;
    stages[0].out_file = 0;
    stages[0].append   = 0;

    for (int i = 0; i < ntoks; i++) {
        switch (kinds[i]) {
            case TOK_WORD:
                if (stages[s].argc >= ARGV_MAX_ - 1) {
                    *err_msg = "too many arguments";
                    return -1;
                }
                stages[s].argv[stages[s].argc++] = words[i];
                break;
            case TOK_PIPE:
                if (stages[s].argc == 0) {
                    *err_msg = "empty command in pipeline";
                    return -1;
                }
                stages[s].argv[stages[s].argc] = 0;
                s++;
                if (s >= STAGES_MAX) {
                    *err_msg = "too many pipeline stages";
                    return -1;
                }
                stages[s].argc     = 0;
                stages[s].in_file  = 0;
                stages[s].out_file = 0;
                stages[s].append   = 0;
                break;
            case TOK_REDIR_IN:
            case TOK_REDIR_OUT:
            case TOK_REDIR_APPEND: {
                if (i + 1 >= ntoks || kinds[i + 1] != TOK_WORD) {
                    *err_msg = "expected filename after redirection";
                    return -1;
                }
                if (kinds[i] == TOK_REDIR_IN) {
                    stages[s].in_file = words[i + 1];
                } else {
                    stages[s].out_file = words[i + 1];
                    stages[s].append   = (kinds[i] == TOK_REDIR_APPEND);
                }
                i++;
                break;
            }
        }
    }

    if (stages[s].argc == 0) {
        *err_msg = "empty command";
        return -1;
    }
    stages[s].argv[stages[s].argc] = 0;
    int n_stages = s + 1;

    // Validate redirs against pipe positions: middle stages can't have
    // their own stdin redir if there's a pipe upstream, and non-last
    // stages can't redirect stdout (the pipe would be ignored).
    for (int j = 0; j < n_stages; j++) {
        if (j > 0 && stages[j].in_file) {
            *err_msg = "stdin redirection conflicts with upstream pipe";
            return -1;
        }
        if (j < n_stages - 1 && stages[j].out_file) {
            *err_msg = "stdout redirection conflicts with downstream pipe";
            return -1;
        }
    }
    return n_stages;
}

// Build /bin/<name> in `out`. Returns 0 on success, -1 on overflow.
static int build_path(const char* name, char* out, unsigned long out_size) {
    const char* pre = "/bin/";
    unsigned long p = 0;
    while (pre[p]) { if (p >= out_size - 1) return -1; out[p] = pre[p]; p++; }
    unsigned long q = 0;
    while (name[q]) {
        if (p >= out_size - 1) return -1;
        out[p++] = name[q++];
    }
    out[p] = 0;
    return 0;
}

static int wait_status_to_rc(unsigned long st) {
    if (WIFEXITED(st))   return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return -1;
}

// Cached at startup. The shell runs in its own pgrp (== own pid, set by
// term's setsid call before exec'ing /bin/sh) and reclaims the foreground
// pgrp via TIOCSPGRP after each pipeline finishes.
static long sh_pgid_;

// Set the controlling pty's foreground pgrp. fd 0 is the slave; pty's
// TIOCSPGRP takes the pgid in the arg slot directly (not via pointer) —
// see drivers/pty.c:160-162.
static void give_terminal_to(long pgid) {
    sys_ioctl(0, TIOCSPGRP_U, (void*)(unsigned long)pgid);
}

// Run a single command (no pipes, no redirs). Forks, puts the child in its
// own pgrp, hands the pty foreground to it, waits, then reclaims foreground.
// Replaces the old sys_spawn fast path so Ctrl+C only signals the child
// (not the shell, which inherits — and ignores — SIGINT).
static int run_simple(stage_t* st_) {
    char path[64];
    if (build_path(st_->argv[0], path, sizeof(path)) < 0) {
        st_puts("sh: command name too long\n");
        return 127;
    }
    long pid = sys_fork();
    if (pid == 0) {
        // Child: own pgrp, restore default SIGINT (sh ignored it), exec.
        sys_setpgid(0, 0);
        sys_signal(SIGINT, SIG_DFL, 0);
        sys_exec(path, st_->argv, 0);
        st_puts("sh: ");
        st_puts(st_->argv[0]);
        st_puts(": not found\n");
        sys_exit(127);
    }
    if (pid < 0) {
        st_puts("sh: fork failed\n");
        return 127;
    }
    // Race-safe duplicate of the child's setpgid. POSIX guarantees one of
    // the two takes effect before exec.
    sys_setpgid(pid, pid);
    give_terminal_to(pid);
    unsigned long st = 0;
    sys_waitpid(pid, &st);
    give_terminal_to(sh_pgid_);
    return wait_status_to_rc(st);
}

// Apply this stage's redirs in the child, AFTER pipe-fd dup2 has set up
// stdin/stdout. Redirs override the pipe ends.
static int apply_redirs(stage_t* st_) {
    if (st_->in_file) {
        long fd = sys_open(st_->in_file, O_RDONLY, 0);
        if (fd < 0) {
            st_puts("sh: ");
            st_puts(st_->in_file);
            st_puts(": cannot open\n");
            return -1;
        }
        sys_dup2(fd, 0);
        sys_close(fd);
    }
    if (st_->out_file) {
        long flags = O_WRONLY | O_CREAT | (st_->append ? O_APPEND : O_TRUNC);
        long fd = sys_open(st_->out_file, flags, 0644);
        if (fd < 0) {
            st_puts("sh: ");
            st_puts(st_->out_file);
            st_puts(": cannot open\n");
            return -1;
        }
        sys_dup2(fd, 1);
        sys_close(fd);
    }
    return 0;
}

// Run a pipeline of N stages with fork+exec. Returns the exit code of the
// last stage.
static int run_pipeline(stage_t* stages, int n) {
    int pipes[STAGES_MAX - 1][2];
    long pids[STAGES_MAX];

    for (int i = 0; i < n - 1; i++) {
        if (sys_pipe(pipes[i]) < 0) {
            st_puts("sh: pipe failed\n");
            return 127;
        }
    }

    // Pipeline pgrp leader: set after the first fork, then every subsequent
    // child joins it. Children read this from their fork-time copy of
    // leader_pgid (0 for child #0 -> sys_setpgid(0,0) starts a new pgrp).
    long leader_pgid = 0;

    for (int i = 0; i < n; i++) {
        long pid = sys_fork();
        if (pid == 0) {
            // child: join (or, if first, start) the pipeline pgrp.
            sys_setpgid(0, leader_pgid);
            sys_signal(SIGINT, SIG_DFL, 0);
            if (i > 0) {
                sys_dup2(pipes[i - 1][0], 0);
            }
            if (i < n - 1) {
                sys_dup2(pipes[i][1], 1);
            }
            // close every pipe end inherited via fork
            for (int j = 0; j < n - 1; j++) {
                sys_close(pipes[j][0]);
                sys_close(pipes[j][1]);
            }
            if (apply_redirs(&stages[i]) < 0) {
                sys_exit(127);
            }
            char path[64];
            if (build_path(stages[i].argv[0], path, sizeof(path)) < 0) {
                st_puts("sh: command name too long\n");
                sys_exit(127);
            }
            sys_exec(path, stages[i].argv, 0);
            st_puts("sh: ");
            st_puts(stages[i].argv[0]);
            st_puts(": not found\n");
            sys_exit(127);
        }
        if (pid < 0) {
            st_puts("sh: fork failed\n");
            // Best-effort cleanup: close pipes, mark this and remaining
            // stages as failed (no children to wait for).
            for (int j = 0; j < n - 1; j++) {
                sys_close(pipes[j][0]);
                sys_close(pipes[j][1]);
            }
            for (int j = 0; j < i; j++) {
                unsigned long st = 0;
                sys_waitpid(pids[j], &st);
            }
            return 127;
        }
        pids[i] = pid;
        if (i == 0) leader_pgid = pid;
        // Race-safe parallel setpgid from the parent side.
        sys_setpgid(pid, leader_pgid);
    }

    // Hand the pty foreground to the pipeline pgrp before draining waits.
    give_terminal_to(leader_pgid);

    // Parent: close every pipe end.
    for (int j = 0; j < n - 1; j++) {
        sys_close(pipes[j][0]);
        sys_close(pipes[j][1]);
    }

    // Wait for all stages; report the last stage's status.
    int rc = 0;
    for (int i = 0; i < n; i++) {
        unsigned long st = 0;
        sys_waitpid(pids[i], &st);
        if (i == n - 1) rc = wait_status_to_rc(st);
    }

    give_terminal_to(sh_pgid_);
    return rc;
}

static int try_builtin(stage_t* st_) {
    // Returns 1 if handled (with side effects), 0 if not a builtin.
    if (st_strcmp(st_->argv[0], "exit") == 0) {
        sys_exit(0);
    }
    if (st_strcmp(st_->argv[0], "cd") == 0) {
        const char* dst = (st_->argc >= 2) ? st_->argv[1] : "/";
        long r = sys_chdir(dst);
        if (r < 0) {
            st_puts("cd: ");
            st_puts(dst);
            st_puts(": no such directory\n");
        }
        return 1;
    }
    return 0;
}

static void prompt(void) {
    char cwd[256];
    long r = sys_getcwd(cwd, sizeof(cwd));
    const char* path = (r > 0) ? cwd : "?";

    // Collapse a leading "/home" or "/home/..." to "~" (...).
    char collapsed[256];
    if (path[0] == '/' && path[1] == 'h' && path[2] == 'o' && path[3] == 'm' &&
        path[4] == 'e' && (path[5] == 0 || path[5] == '/')) {
        collapsed[0] = '~';
        int k = 1;
        int i = 5;
        while (path[i] && k < (int)sizeof(collapsed) - 1) {
            collapsed[k++] = path[i++];
        }
        collapsed[k] = 0;
        path = collapsed;
    }

    st_puts("\x1b[93m$ ");        // yellow dollar
    st_puts("\x1b[96m");          // bright cyan
    st_puts(path);
    st_puts(" \x1b[90m>\x1b[0m ");  // grey '>' then reset
}

// ---- history ring -------------------------------------------------------

// Most-recent entry is at hist[(head - 1) mod HIST_MAX]. count saturates
// at HIST_MAX once the ring wraps.
static char hist[HIST_MAX][LINE_MAX_];
static int  hist_count;
static int  hist_head;

static void hist_add(const char* s) {
    if (s[0] == 0) return;
    if (hist_count > 0) {
        int prev = (hist_head - 1 + HIST_MAX) % HIST_MAX;
        if (st_strcmp(hist[prev], s) == 0) return;
    }
    unsigned long i = 0;
    while (s[i] && i < LINE_MAX_ - 1) { hist[hist_head][i] = s[i]; i++; }
    hist[hist_head][i] = 0;
    hist_head = (hist_head + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++;
}

// back=1 -> most recent, back=2 -> previous, ... NULL once past oldest.
static const char* hist_get(int back) {
    if (back <= 0 || back > hist_count) return 0;
    int idx = (hist_head - back + HIST_MAX * 2) % HIST_MAX;
    return hist[idx];
}

static void hist_load(void) {
    long fd = sys_open(HIST_FILE, O_RDONLY, 0);
    if (fd < 0) return;
    char buf[1024];
    char cur[LINE_MAX_];
    int  cl = 0;
    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (long i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                cur[cl] = 0;
                hist_add(cur);
                cl = 0;
            } else if (cl < (int)sizeof(cur) - 1) {
                cur[cl++] = c;
            }
        }
    }
    if (cl > 0) { cur[cl] = 0; hist_add(cur); }
    sys_close(fd);
}

static void hist_persist_append(const char* s) {
    long fd = sys_open(HIST_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    sys_write(fd, s, st_strlen(s));
    sys_write(fd, "\n", 1);
    sys_close(fd);
}

// ---- line editor --------------------------------------------------------

static void emit(const char* s) { sys_write(1, s, st_strlen(s)); }
static void emit_n(const char* s, int n) { sys_write(1, s, (unsigned long)n); }

// Move cursor left by n columns using CSI D. n must be > 0.
static void cursor_left(int n) {
    if (n <= 0) return;
    char buf[16];
    int  k = 0;
    buf[k++] = 0x1b; buf[k++] = '[';
    char num[8];
    int  nn = 0;
    int  v = n;
    while (v > 0) { num[nn++] = (char)('0' + v % 10); v /= 10; }
    while (nn > 0) buf[k++] = num[--nn];
    buf[k++] = 'D';
    emit_n(buf, k);
}

static void cursor_right(int n) {
    if (n <= 0) return;
    char buf[16];
    int  k = 0;
    buf[k++] = 0x1b; buf[k++] = '[';
    char num[8];
    int  nn = 0;
    int  v = n;
    while (v > 0) { num[nn++] = (char)('0' + v % 10); v /= 10; }
    while (nn > 0) buf[k++] = num[--nn];
    buf[k++] = 'C';
    emit_n(buf, k);
}

// After an insert/delete: print the bytes from cur..len, erase the old
// trailing column with \x1b[K, then move cursor back over the tail.
static void redraw_tail(const char* line, int len, int cur) {
    int tail = len - cur;
    if (tail > 0) emit_n(line + cur, tail);
    emit("\x1b[K");
    if (tail > 0) cursor_left(tail);
}

// Repaint the whole input area: \r, prompt, line, \x1b[K. Cursor lands
// at end-of-line; caller must adjust if it wants cursor elsewhere.
static void repaint_line(const char* line, int len) {
    emit("\r");
    prompt();
    if (len > 0) emit_n(line, len);
    emit("\x1b[K");
}

// Reads a line with full editing in raw mode. Returns len, or -1 on EOF.
// The pty is assumed to already be in raw mode on entry.
static long read_line_edit(char* line, int cap) {
    int len = 0;
    int cur = 0;

    char saved_live[LINE_MAX_];
    int  saved_live_len = 0;
    int  hist_view      = 0;  // 0 = live edit; 1 = most recent; ...

    enum { GROUND, ESC1, CSI } state = GROUND;

    for (;;) {
        char b;
        long n = sys_read(0, &b, 1);
        if (n == 0) return -1;
        if (n < 0) {
            // Interrupted by SIGINT delivered to a non-ignoring sibling? sh
            // ignores SIGINT, so this shouldn't happen — but be safe.
            continue;
        }

        if (state == ESC1) {
            if (b == '[') { state = CSI; continue; }
            state = GROUND;
            continue;
        }
        if (state == CSI) {
            // Skip parameter bytes (digits and ';'). Stop at a final byte.
            if ((b >= '0' && b <= '9') || b == ';') continue;
            state = GROUND;
            switch (b) {
                case 'D':  // left
                    if (cur > 0) { cur--; emit("\x1b[D"); }
                    break;
                case 'C':  // right
                    if (cur < len) { cur++; emit("\x1b[C"); }
                    break;
                case 'A': {  // up: older
                    const char* h = hist_get(hist_view + 1);
                    if (!h) break;
                    if (hist_view == 0) {
                        // snapshot live edit before first nav
                        for (int i = 0; i < len; i++) saved_live[i] = line[i];
                        saved_live_len = len;
                    }
                    hist_view++;
                    int i = 0;
                    while (h[i] && i < cap - 1) { line[i] = h[i]; i++; }
                    len = i;
                    cur = len;
                    repaint_line(line, len);
                    break;
                }
                case 'B': {  // down: newer
                    if (hist_view == 0) break;
                    hist_view--;
                    if (hist_view == 0) {
                        for (int i = 0; i < saved_live_len; i++) line[i] = saved_live[i];
                        len = saved_live_len;
                    } else {
                        const char* h = hist_get(hist_view);
                        int i = 0;
                        while (h[i] && i < cap - 1) { line[i] = h[i]; i++; }
                        len = i;
                    }
                    cur = len;
                    repaint_line(line, len);
                    break;
                }
                case 'H':  // Home
                    if (cur > 0) { cursor_left(cur); cur = 0; }
                    break;
                case 'F':  // End
                    if (cur < len) { cursor_right(len - cur); cur = len; }
                    break;
                default: break;
            }
            continue;
        }

        // GROUND
        if (b == 0x1b) { state = ESC1; continue; }
        if (b == 0x1e) {
            // Soft newline: Shift+Enter from the terminal, or a '\n' in a
            // pasted clipboard. Insert a literal '\n' into the buffer at
            // the cursor and visually break to the next row, but do *not*
            // submit. The multi-line buffer is run as separate commands
            // when the user finally hits plain Enter (see main loop).
            if (len < cap - 1) {
                for (int i = len; i > cur; i--) line[i] = line[i - 1];
                line[cur] = '\n';
                len++;
                cur++;
                emit("\r\n");
            }
            continue;
        }
        if (b == '\r' || b == '\n') {
            emit("\n");
            line[len] = 0;
            return len;
        }
        if (b == 0x7f || b == 0x08) {
            if (cur > 0) {
                for (int i = cur - 1; i < len - 1; i++) line[i] = line[i + 1];
                cur--; len--;
                emit("\b");
                redraw_tail(line, len, cur);
            }
            continue;
        }
        if (b == 0x01) {  // Ctrl+A
            if (cur > 0) { cursor_left(cur); cur = 0; }
            continue;
        }
        if (b == 0x05) {  // Ctrl+E
            if (cur < len) { cursor_right(len - cur); cur = len; }
            continue;
        }
        if ((unsigned char)b >= 0x20 && (unsigned char)b < 0x7f) {
            if (len >= cap - 1) continue;
            for (int i = len; i > cur; i--) line[i] = line[i - 1];
            line[cur] = b;
            len++;
            emit_n(&b, 1);
            cur++;
            redraw_tail(line, len, cur);
            continue;
        }
        // any other control byte: ignore
    }
}

static void set_raw(void)    { sys_ioctl(0, TTY_IOCTL_SET_RAW,    0); }
static void set_cooked(void) { sys_ioctl(0, TTY_IOCTL_SET_COOKED, 0); }

// Run one command line (must be null-terminated, no embedded '\n').
// Lives here so the main loop can call it once per '\n'-separated segment
// from a multi-line buffer (Shift+Enter / pasted newlines).
static void exec_one(char* cmd,
                     char* tokbuf, char** words, tok_kind_t* kinds,
                     stage_t* stages) {
    int ntoks = tokenize(cmd, tokbuf, words, kinds);
    if (ntoks <= 0) {
        if (ntoks < 0) st_puts("sh: line too long\n");
        return;
    }
    const char* err_msg = 0;
    int n_stages = parse_pipeline(ntoks, words, kinds, stages, &err_msg);
    if (n_stages < 0) {
        st_puts("sh: ");
        st_puts(err_msg);
        sys_write(1, "\n", 1);
        return;
    }
    int is_simple = (n_stages == 1
                     && stages[0].in_file == 0
                     && stages[0].out_file == 0);
    if (is_simple && try_builtin(&stages[0])) return;
    if (is_simple) (void) run_simple(&stages[0]);
    else           (void) run_pipeline(stages, n_stages);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    char       line[LINE_MAX_];
    char       tokbuf[TOKBUF_MAX];
    char*      words[TOKENS_MAX];
    tok_kind_t kinds[TOKENS_MAX];
    stage_t    stages[STAGES_MAX];

    sys_mkdir("/home", 0755);
    sys_chdir("/home");

    // Job control: shell runs in its own pgrp (term's child did setsid before
    // exec, so pgid == pid). Ignore SIGINT so a Ctrl+C that lands on us
    // during the fork/tcsetpgrp race window — or while waiting at the
    // prompt — doesn't kill the shell. Children reset SIG_DFL before exec.
    sh_pgid_ = sys_getpid();
    sys_signal(SIGINT, SIG_IGN, 0);

    hist_load();

    for (;;) {
        prompt();
        set_raw();
        long n = read_line_edit(line, sizeof(line));
        set_cooked();
        if (n < 0) {
            // EOF on stdin (controlling tty went away). Exit so init
            // can decide whether to respawn term.
            return 0;
        }
        line[n] = 0;

        // The buffer may contain '\n' bytes (Shift+Enter or pasted
        // newlines). Walk it and run each segment as its own command. Each
        // non-empty segment is recorded as a separate history entry, the
        // same way bash does with bracketed paste.
        int start = 0;
        for (int i = 0; i <= (int)n; i++) {
            if (i == (int)n || line[i] == '\n') {
                int seg_len = i - start;
                if (seg_len > 0) {
                    char saved = line[i];
                    line[i] = 0;
                    char* seg = line + start;

                    int prev_head  = hist_head;
                    int prev_count = hist_count;
                    hist_add(seg);
                    if (hist_head != prev_head || hist_count != prev_count) {
                        hist_persist_append(seg);
                    }
                    exec_one(seg, tokbuf, words, kinds, stages);

                    line[i] = saved;
                }
                start = i + 1;
            }
        }
    }
}
