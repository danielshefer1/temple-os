#include "keyboard.h"

void PushKeyboardBuffer(InputBuffer* buffer, char c) {
    buffer->buffer[buffer->head].c = c;
    buffer->buffer[buffer->head].time = timer_ticks;
    buffer->head = (buffer->head + 1) % buffer->size;
}

void scanfH(InputBuffer* buffer, char* user_buffer, uint32_t ms_back, char key) {
    uint32_t curr_time = timer_ticks, idx = buffer->tail, tmp_idx = 0, end = buffer->head;

    while (curr_time - ms_back > buffer->buffer[idx].time && idx != end) {
        idx = (idx + 1) % buffer->size; 
    }

    while (idx != end) {
        if (buffer->buffer[idx].c == key) {
            user_buffer[tmp_idx] = '\n';
            kprintf(user_buffer);
            idx++;
            buffer->tail = idx;
            return;
        }
        user_buffer[tmp_idx] = buffer->buffer[idx].c;
        tmp_idx++;
        idx++;
    }
    buffer->tail = buffer->head;
    kprintf(user_buffer);
    while (true) {
        while (buffer->tail == buffer->head) {
            HltHelper();
        }
        while (buffer->tail != buffer->head) {
            if (buffer->buffer[buffer->tail].c == key) {
                putchar('\n', GREY_COLOR);
                user_buffer[tmp_idx] = '\0';
                return;
            }
            user_buffer[tmp_idx] = buffer->buffer[buffer->tail].c;
            putchar(buffer->buffer[buffer->tail].c, GREY_COLOR);
            buffer->tail++;
            tmp_idx++;
        }
    }
}

void scanf(const char *format, void* pointer) {
    if (*format != '%') kerror("You Need to Input a Type Specifier!");

    char buffer[20];
    memset(buffer, 0, sizeof(buffer));
    uint32_t result;
    uint32_t* p_n;
    char* p_s;

    scanfH(&console_buffer, buffer, 10, '\n');
    format++;
    switch (*format) {
        case 'd':
            result = atoi(buffer, 10);
            p_n = (uint32_t*) pointer;
            *p_n = result;
            break;
        case 'x':
            result = atoi(buffer, 16);
            p_n = (uint32_t*) pointer;
            *p_n = result;
            break;
        case 's':
            p_s = (char*) pointer;
            cpystr(buffer, p_s);
            break;
        case 'c':
            p_s = (char*) pointer;
            *p_s = buffer[0];
            break;
    }
}

void InitConsoleBuffer() {
    console_buffer.head = 0;
    console_buffer.tail = 0;
    console_buffer.size = CONSOLE_BUFFER_SIZE;
    console_buffer.buffer = kmalloc(sizeof(TimedKey) * CONSOLE_BUFFER_SIZE);
}