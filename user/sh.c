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

// Run a single command via sys_spawn (no pipes, no redirs). Returns the
// exit code of the child.
static int run_simple(stage_t* st_) {
    char path[64];
    if (build_path(st_->argv[0], path, sizeof(path)) < 0) {
        st_puts("sh: command name too long\n");
        return 127;
    }
    long pid = sys_spawn(path, st_->argv, 0);
    if (pid < 0) {
        st_puts("sh: ");
        st_puts(st_->argv[0]);
        st_puts(": not found\n");
        return 127;
    }
    unsigned long st = 0;
    sys_waitpid(pid, &st);
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

    for (int i = 0; i < n; i++) {
        long pid = sys_fork();
        if (pid == 0) {
            // child
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
    }

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
    st_puts("\x1b[93m$ \x1b[0m");
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    char       line[LINE_MAX_];
    char       tokbuf[TOKBUF_MAX];
    char*      words[TOKENS_MAX];
    tok_kind_t kinds[TOKENS_MAX];
    stage_t    stages[STAGES_MAX];

    for (;;) {
        prompt();
        long n = sys_read(0, line, sizeof(line) - 1);
        if (n <= 0) {
            // EOF on stdin (controlling tty went away). Exit so init
            // can decide whether to respawn term.
            return 0;
        }
        if (n >= (long)sizeof(line)) n = sizeof(line) - 1;
        line[n] = 0;
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = 0;

        int ntoks = tokenize(line, tokbuf, words, kinds);
        if (ntoks <= 0) {
            if (ntoks < 0) st_puts("sh: line too long\n");
            continue;
        }

        const char* err_msg = 0;
        int n_stages = parse_pipeline(ntoks, words, kinds, stages, &err_msg);
        if (n_stages < 0) {
            st_puts("sh: ");
            st_puts(err_msg);
            sys_write(1, "\n", 1);
            continue;
        }

        // Builtins must run in-process. Only honor them when the shape is
        // a single stage with no redirs — otherwise the caller's pipe/redir
        // would be invisible to the builtin and surprising.
        int is_simple = (n_stages == 1
                         && stages[0].in_file == 0
                         && stages[0].out_file == 0);
        if (is_simple && try_builtin(&stages[0])) continue;

        if (is_simple) {
            (void) run_simple(&stages[0]);
        } else {
            (void) run_pipeline(stages, n_stages);
        }
    }
}
