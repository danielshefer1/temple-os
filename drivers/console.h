#pragma once
#include "includes.h"

// Abstract "screen" console. The serial port (COM1, in vga.c) is *always*
// written to as a debug mirror; this interface is for the user-visible
// display surface. Until an implementation registers itself (M3 wires up
// the framebuffer console), active_console is NULL and screen writes are
// no-ops.
//
// Color is encoded as a 4-bit index into a 16-color palette (CGA-style).
// SGR-style 24-bit color comes later through set_truecolor; backends that
// don't support it fall back to a 16-color approximation.

typedef struct console {
    void (*putc)     (struct console* c, char ch);
    void (*clear)    (struct console* c);
    void (*set_attr) (struct console* c, uint8_t fg, uint8_t bg);
    void (*set_cursor)(struct console* c, uint64_t row, uint64_t col);
    uint64_t rows;
    uint64_t cols;
    void* priv;
} console_t;

extern console_t* active_console;

// No-op when no backend is registered. Safe to call from any context.
void console_putc(char c);
void console_puts(const char* s);
void console_write(const char* s, uint64_t len);
void console_clear(void);
void console_set_attr(uint8_t fg, uint8_t bg);
void console_set_cursor(uint64_t row, uint64_t col);

// Backends call this once they're ready to receive output.
void console_register(console_t* c);
