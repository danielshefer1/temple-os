#include "user_app.h"

__attribute__((section(".text.entry")))
void main() {
    uint32_t test;
    char test1[20];
    printf("Input a number and phrase: ");
    scanf("%d %s", &test, test1);
    printf("Number: %d\nPhrase: %s\n", test, test1);
    exit();
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char str[20], c;
    uint32_t num;
    
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            // Check for width specifier FIRST
            uint32_t min_width = 0;
            while (*format >= '0' && *format <= '9') {
                min_width = min_width * 10 + (*format - '0');
                format++;
            }
            
            
            
            switch (*format) {
            case 'c':
                c = (char)va_arg(args, uint32_t);
                write(&c, 1);
                break;
            case 's':
                char* pointer = va_arg(args, char*);
                write(pointer, UINT32_MAX);
                break;
            case 'd':
                num = va_arg(args, uint32_t);
                itoa(num, str, 10, min_width);
                write(str, UINT32_MAX);
                break;
            case 'x':
                num = va_arg(args, uint32_t);
                str[0] = '0';
                str[1] = 'x';
                itoa(num, &str[2], 16, min_width);
                write(str, UINT32_MAX);
                break;
            case '%':
                c = '%';
                write(&c, 1);
                break;
            default:
                break;
            }
        } else {
            write(format, 1);
        }
        format++;
    }
    
    va_end(args);
}

void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width) {
    char* ptr = str;
    uint32_t tmp_value, count = 0;

    if (value == 0) {
        *ptr++ = '0';
        count++;
    } 
    else {
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789ABCDEF"[tmp_value - value * base];
        count++;
    } while (value);
    }

    while (count < min_width) {
        *ptr++ = '0';
        count++;
    }

    *ptr = '\0';

    flip_str(str);
}

void flip_str(char* str) {
    char* start = str;
    char* end = str;

    while (*end != '\0') {
        end++;
    }
    end--;

    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
}

bool isdigit(char c) {
    if (c < '0' || c > '9') return false;
    return true;
}
bool isuppercasealpha(char c) {
    if (c < 'A' || c > 'F') return false;
    return true;
}
bool islowercasealpha(char c) {
    if (c < 'a' || c > 'f') return false;
    return true;
}

uint32_t char_to_digit(char c) {
    if (isdigit(c)) return c - '0';
    if (islowercasealpha(c)) return c - 'a' + 10;
    if (isuppercasealpha(c)) return c - 'A' + 10;
    return 0xFF;
}

uint32_t atoi(char* str, uint32_t base) {
    uint32_t result = 0, digit;
    char* ptr = str;

    while (*ptr != '\0') {
        result *= base;
        digit = char_to_digit(*ptr);
        if (digit == 0xFF) break;
        result += digit;
        ptr++;
    }
    return result;
}

void scanf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char nums_buffer[20];
    char* p1;
    uint32_t* p2;
    Tuple num_triggers;
    num_triggers.first = ' ';
    num_triggers.second = '\n';
    Tuple str_triggers;
    str_triggers.first = '\0';
    str_triggers.second = '\n';

    memset(nums_buffer, 0, sizeof(nums_buffer));

    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format) {
            case 'c':
                p1 = va_arg(args, char*);
                read(p1, &str_triggers, 1);
                *p1 = nums_buffer[0];
                memset(nums_buffer, 0, sizeof(nums_buffer));
                break;
            case 's':
                p1 = va_arg(args, char*);
                read(p1, &str_triggers, UINT32_MAX);
                break;
            case 'd':
                p2 = va_arg(args, uint32_t*);
                read(nums_buffer, &num_triggers, 20);
                *p2 = atoi(nums_buffer, 10);
                memset(nums_buffer, 0, sizeof(nums_buffer));
                break;
            case 'x':
                p2 = va_arg(args, uint32_t*);
                read(nums_buffer, &num_triggers, 20);
                *p2 = atoi(nums_buffer, 16);
                memset(nums_buffer, 0, sizeof(nums_buffer));
                break;
            default:
                break;
            }
        }
        format++;
        flush_consle_buffer();
    }
    va_end(args);
}

void memset(void* address, uint8_t value, uint32_t size) {
    uint32_t reminder = size % 4;
    size -= reminder;
    size /= 4;
    uint32_t value_32 = value + (value << 8) + (value << 16) + (value << 24);

    for(uint32_t i = 0; i < size; i++) {
        ((uint32_t*) address)[i] = value_32;
    }

    for(uint32_t i = 0; i < reminder; i++) {
        ((uint8_t*) address)[size * 4 +i] = value;
    }
}
