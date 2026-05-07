// All keyboard input now flows through drivers/tty.c. The legacy
// PushKeyboardBuffer / GetInputUntilKey / kscanf / FlushBuffer code that
// drove the unsynchronized global console_buffer was removed when the
// per-task fd table started routing reads through the tty's file_ops. If
// you need kernel-side line input again, call tty_read on console_tty.
