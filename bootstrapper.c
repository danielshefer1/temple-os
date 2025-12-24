#include <stdint.h>

#define VGA_BUFFER ((volatile char*)0xB8000)

__attribute__((section(".bootstrap")))
void print_str_bootstrap(uint32_t idx, char msg[]) {
    while (msg[idx] != '\0') {
        VGA_BUFFER[idx * 2] = msg[idx];
        VGA_BUFFER[idx * 2 + 1] = 0x07;
        idx++;
    }   
}


__attribute__((section(".bootstrap")))
void clear_screen_bootstrap() {
    for (uint32_t i = 0; i < 80 * 25 * 2; i++) {
        VGA_BUFFER[i] = 0;
    }
}

__attribute__((section(".bootstrap")))
void itoa_bootstrap(uint32_t value, char* str, uint32_t base, uint32_t min_width) {
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
}

__attribute__((section(".bootstrap")))
void bootstrap_kmain() {
    clear_screen_bootstrap();

    char msg[] = "I hate paging";

    print_str_bootstrap(0, msg);
    uint32_t pd_addr = 0x10000;

    char pd_addr_str[20];

    clear_screen_bootstrap();

    


}