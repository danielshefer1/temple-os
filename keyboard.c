#include "keyboard.h"

static buffer_queue console_buffer;

void PushKeyboardBuffer(char c) {
    // PlaceHolder
}

void InitConsoleBuffer() {
    console_buffer.head = 0;
    console_buffer.tail = 0;
    console_buffer.size = CONSOLE_BUFFER_SIZE;
    console_buffer.buffer = kmalloc(sizeof(char) * CONSOLE_BUFFER_SIZE);
}