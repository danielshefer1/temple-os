#include "keyboard.h"

void PushKeyboardBuffer(input_buffer_t* buffer, char c) {
    buffer->buffer[buffer->head].c = c;
    buffer->buffer[buffer->head].time = timer_ticks;
    buffer->head = (buffer->head + 1) % buffer->size;
}

bool isInTuple(tuple_t* tuple, uint32_t value) {
    return (value == tuple->first || value == tuple->second);
}

void FlushBuffer(input_buffer_t* buffer) {
    buffer->head = 0;
    buffer->tail = 0;
}

uint32_t GetInputUntilKey(input_buffer_t* buffer, char* user_buffer, uint32_t max_read, uint32_t ms_back, tuple_t* keys) {
    uint32_t curr_time = timer_ticks, idx = buffer->tail, tmp_idx = 0, end = buffer->head;

    while (curr_time - ms_back > buffer->buffer[idx].time && idx != end) {
        idx = (idx + 1) % buffer->size; 
    }

    while (idx != end) {
        if (isInTuple(keys, buffer->buffer[idx].c)) {
            if (tmp_idx >= max_read - 1) user_buffer[tmp_idx] = '\0';
            else user_buffer[tmp_idx] = buffer->buffer[idx].c;

            kprintf(user_buffer);
            idx++;
            buffer->tail = idx;
            return idx;
        }
        if (buffer->buffer[buffer->tail].c == '\b') {
            if (tmp_idx == 0) user_buffer[0] = '\0';
            else {
                tmp_idx--;
                user_buffer[tmp_idx] = '\0';
            }
        }
        else if (tmp_idx < max_read) {
            user_buffer[tmp_idx] = buffer->buffer[buffer->tail].c;
            tmp_idx++;
        }
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
            if (isInTuple(keys, buffer->buffer[buffer->tail].c)) {
                user_buffer[tmp_idx] = '\0';
                putchar(buffer->buffer[buffer->tail].c, GREY_COLOR);
                return tmp_idx;
            }
            if (buffer->buffer[buffer->tail].c == '\b') {
                if (tmp_idx == 0) user_buffer[0] = '\0';
                else {
                    tmp_idx--;
                    user_buffer[tmp_idx] = '\0';
                }
            }
            else if (tmp_idx < max_read) {
                user_buffer[tmp_idx] = buffer->buffer[buffer->tail].c;
                tmp_idx++;
            }
            putchar(buffer->buffer[buffer->tail].c, GREY_COLOR);
            buffer->tail++;
        }
    }
}

void kscanf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char nums_buffer[20];
    char* p1;
    uint32_t* p2;
    tuple_t num_triggers;
    num_triggers.first = ' ';
    num_triggers.second = '\n';
    tuple_t str_triggers;
    str_triggers.first = '\0';
    str_triggers.second = '\n';

    memset(nums_buffer, 0, sizeof(nums_buffer));

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format) {
            case 'c':
                p1 = va_arg(args, char*);
                GetInputUntilKey(&console_buffer, nums_buffer, 1, KEYBOARD_MS_BACK, &num_triggers);
                *p1 = nums_buffer[0];
                memset(nums_buffer, 0, sizeof(nums_buffer));
                break;
            case 's':
                p1 = va_arg(args, char*);
                GetInputUntilKey(&console_buffer, p1, -1, KEYBOARD_MS_BACK, &str_triggers);
                break;
            case 'd':
                p2 = va_arg(args, uint32_t*);
                GetInputUntilKey(&console_buffer, nums_buffer, sizeof(nums_buffer) / sizeof(nums_buffer[0]), KEYBOARD_MS_BACK, &num_triggers);
                *p2 = atoi(nums_buffer, 10);
                memset(nums_buffer, 0, sizeof(nums_buffer));
                break;
            case 'x':
                p2 = va_arg(args, uint32_t*);
                GetInputUntilKey(&console_buffer, nums_buffer, sizeof(nums_buffer) / sizeof(nums_buffer[0]), KEYBOARD_MS_BACK, &num_triggers);
                *p2 = atoi(nums_buffer, 10);
                memset(nums_buffer, 0, sizeof(nums_buffer));
                break;
            default:
                break;
            }
        }
        format++;
        FlushBuffer(&console_buffer);
    }
    
    va_end(args);
}

void InitConsoleBuffer() {
    console_buffer.head = 0;
    console_buffer.tail = 0;
    console_buffer.size = CONSOLE_BUFFER_SIZE;
    console_buffer.buffer = kmalloc(sizeof(timed_key_t) * CONSOLE_BUFFER_SIZE);
}