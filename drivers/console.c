#include "console.h"

console_t* active_console = NULL;

void console_register(console_t* c) {
    active_console = c;
}

void console_putc(char ch) {
    console_t* c = active_console;
    if (c && c->putc) c->putc(c, ch);
}

void console_puts(const char* s) {
    console_t* c = active_console;
    if (!c || !c->putc) return;
    while (*s) c->putc(c, *s++);
}

void console_write(const char* s, uint64_t len) {
    console_t* c = active_console;
    if (!c || !c->putc) return;
    for (uint64_t i = 0; i < len; i++) c->putc(c, s[i]);
}

void console_clear(void) {
    console_t* c = active_console;
    if (c && c->clear) c->clear(c);
}

void console_set_attr(uint8_t fg, uint8_t bg) {
    console_t* c = active_console;
    if (c && c->set_attr) c->set_attr(c, fg, bg);
}

void console_set_cursor(uint64_t row, uint64_t col) {
    console_t* c = active_console;
    if (c && c->set_cursor) c->set_cursor(c, row, col);
}
