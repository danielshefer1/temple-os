#pragma once

// POSIX wait status decoders. The kernel encodes child status into a 16-bit
// word (see multi/signal.c and interrupts/syscall_handler.c::ExitHandler):
//   bits 0..6  : terminating signo (0 if cleanly exited)
//   bit  7     : reserved for "core dumped" (always 0)
//   bits 8..15 : low byte of exit code if WIFEXITED
//
// 0x7F in the low 7 bits would mean WIFSTOPPED, which we don't produce yet.

#define WIFEXITED(s)   (((s) & 0x7F) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xFF)
#define WIFSIGNALED(s) (((s) & 0x7F) != 0 && ((s) & 0x7F) != 0x7F)
#define WTERMSIG(s)    ((s) & 0x7F)
