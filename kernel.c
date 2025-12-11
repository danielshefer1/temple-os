#include <stdint.h>

void kmain(void) {
    // Kernel main function
    unsigned char *VGA_MEM = (unsigned char *)0xB8000;
    const char *message = "Hello, Kernel World!";
    for (int i = 0; message[i] != '\0'; i++) {
        VGA_MEM[i * 2] = message[i];       // Character byte
        VGA_MEM[i * 2 + 1] = 0x07;         // Attribute byte (light grey on black)
    }
}