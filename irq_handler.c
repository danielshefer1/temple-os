#include "irq_handler.h"

void irq_handler(interrupt_frame_t* frame) {
    lapic[0xB0 / 4] = 0;

    switch (frame->int_no) {
        case 32:
            TimerHandler();
            return;
        default:
            kprintf("Unkown Interrupt just fired, interrupt number: %d\n", frame->int_no);
    }
}

void TimerHandler() {
    timer_ticks++;
}