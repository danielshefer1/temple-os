#include "std/std.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    st_puts("builtin commands:\n");
    st_puts("  help                       - this message\n");
    st_puts("  clear                      - clear the screen\n");
    st_puts("  pwd                        - print working directory\n");
    st_puts("  cd <path>                  - change directory (shell builtin)\n");
    st_puts("  exit                       - exit the shell (builtin)\n");
    st_puts("  ls [path]                  - list directory entries\n");
    st_puts("  cat [file...]              - print files (or stdin) to stdout\n");
    st_puts("  echo <args>                - print args\n");
    st_puts("  mkdir <path>               - create directory\n");
    st_puts("  rmdir <path>               - remove empty directory\n");
    st_puts("  rm <path...>               - remove file(s)\n");
    st_puts("  mv <src> <dst>             - rename file or directory\n");
    st_puts("  cp <src> <dst>             - copy file\n");
    st_puts("  touch <path>               - create empty file if missing\n");
    st_puts("  ln -s <target> <link>      - create symbolic link\n");
    st_puts("  readlink <link>            - print symlink target\n");
    st_puts("  stat <path>                - print file metadata\n");
    st_puts("  truncate <path> <length>   - set file size\n");
    st_puts("  sleep <seconds>            - sleep N seconds\n");
    st_puts("  kill [-N] <pid...>         - send signal (default TERM)\n");
    st_puts("  sync                       - flush filesystem buffers\n");
    st_puts("  true / false               - exit 0 / exit 1\n");
    st_puts("  hello                      - run the M8/M9 self-test program\n");
    st_puts("  shutdown                   - flush root fs and power off\n");
    st_puts("\nshell features: pipes |, redirection < > >>\n");
    return 0;
}
