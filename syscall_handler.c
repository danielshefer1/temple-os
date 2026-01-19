#include "syscall_handler.h"

void syscall_handler(interrupt_frame* frame) {
    uint32_t cs = frame->cs;
    if (cs == 0x08) return;

    uint32_t syscall_id = frame->eax, ret;

    switch (syscall_id) {
        case EXIT_SYSCALL:
            ret = ExitHandler();
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

int32_t WriteHandler(interrupt_frame* frame) {
    char* pointer = (char*) frame->ebx;
    int32_t length = frame->ecx;
    return print_str_SYSCALL(pointer, GREY_COLOR, length);
}

int32_t ReadHandler(interrupt_frame* frame) {
    char* buffer = (char*) frame->ebx;
    Tuple* triggers = (Tuple*) frame->ecx;
    int32_t max_read = frame->edx;
    return GetInputUntilKey(&console_buffer, buffer, max_read, KEYBOARD_MS_BACK, triggers);
}

int32_t MmapHandler(interrupt_frame* frame) {

    int32_t size = frame->ebx;
    void* ret = RequestBuddy(size);
    return (int32_t) ret;
}

int32_t MunmapHandler(interrupt_frame* frame) {
    int32_t addr = frame->ebx;
    FreeBuddy((void*) addr);
    return 1;
}

int32_t UnknownSysCall() {
    kprintf("This is not a known SYSCALL");
    return -1;
}

int32_t FlushBufferHandler() {
    FlushBuffer(&console_buffer);
    return 1;
}

int32_t ExitHandler() {
    CliHelper();
    HltHelper();
    return 1;
}