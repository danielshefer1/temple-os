#include "syscall_handler.h"

void syscall_handler(interrupt_frame* frame) {
    uint32_t cs = frame->cs;
    if (cs == 0x08) return;

    uint32_t syscall_id = frame->eax, ret;
    StiHelper();

    switch (syscall_id) {
        case HLT_SYSCALL:
            ret = HltSYSCALLHandler();
            break;
        case WRITE_SYSCALL:
            ret =  WriteHandler(frame);
            break;
        case READ_SYSCALL:
            ret = ReadHandler(frame);
            break;
        case FLUSH_BUFFER_SYSCALL:
            ret = FlushBufferHandler();
            break;
        case MMAP_SYSCALL:
            ret = MmapHandler(frame);
            break;
        case MUNMAP_SYSCALL:
            ret = MunmapHandler(frame);
            break;
        default:
            ret = UnknownSysCall();
    }
    frame->eax = ret;
}

uint32_t WriteHandler(interrupt_frame* frame) {
    char* pointer = (char*) frame->ebx;
    uint32_t length = frame->ecx;
    return print_str_SYSCALL(pointer, GREY_COLOR, length);
}

uint32_t ReadHandler(interrupt_frame* frame) {
    char* buffer = (char*) frame->ebx;
    Tuple* triggers = (Tuple*) frame->ecx;
    uint32_t max_read = frame->edx;
    return GetInputUntilKey(&console_buffer, buffer, max_read, KEYBOARD_MS_BACK, triggers);
}

uint32_t MmapHandler(interrupt_frame* frame) {

    uint32_t size = frame->ebx;
    void* ret = RequestBuddy(size);
    return (uint32_t) ret;
}

uint32_t MunmapHandler(interrupt_frame* frame) {
    uint32_t addr = frame->ebx;
    FreeBuddy((void*) addr);
    return 1;
}

uint32_t UnknownSysCall() {
    kprintf("This is not a known SYSCALL");
    return -1;
}

uint32_t FlushBufferHandler() {
    FlushBuffer(&console_buffer);
    return 1;
}

uint32_t HltSYSCALLHandler() {
    CliHelper();
    HltHelper();
    return 1;
}