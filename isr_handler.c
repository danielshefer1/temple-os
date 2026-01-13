#include "isr_handler.h"

static char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

static bool shift_pressed = false;

void isr_handler(interrupt_frame* frame) {
    uint32_t int_no = frame->int_no;
    if (int_no >= 32 && int_no < 48) {
        IRQHandler(frame);
    } else {
        ExecptionHandler(frame);
    }
}

void ExecptionHandler(interrupt_frame* frame) {
    uint32_t int_no = frame->int_no;

    switch (int_no) {
        case 0:
            DivideByZeroHandler(frame);
            break;
        case 1:
            DebugHandler(frame);
            break;
        case 2:
            NMIHandler(frame);
            break;
        case 3:
            BreakpointHandler(frame);
            break;
        case 4:
            OverflowHandler(frame);
            break;
        case 5:
            BoundRangeExceededHandler(frame);
            break;
        case 6:
            InvalidOpcodeHandler(frame);
            break;
        case 7:
            DeviceNotAvailableHandler(frame);
            break;
        case 8:
            DoubleFaultHandler(frame);
            break;
        case 9:
            CoprocessorSegmentOverrunHandler(frame);
            break;
        case 10:
            InvalidTSSHandler(frame);
            break;
        case 11:
            SegmentNotPresentHandler(frame);
            break;
        case 12:
            StackSegmentFaultHandler(frame);
            break;
        case 13:
            GeneralProtectionFaultHandler(frame);
            break;
        case 14:
            PageFaultHandler(frame);
            break;
        case 16:
            FloatingPointExceptionHandler(frame);
            break;
        case 17:
            AlignmentCheckHandler(frame);
            break;
        case 18:
            CoprocessorErrorHandler(frame);
            break;
        case 19:
            SIMDFloatingPointExceptionHandler(frame);
            break;
        case 20:
            VirtualizationExceptionHandler(frame);
            break;
        case 21:
            ControlProtectionExceptionHandler(frame);
            break;
    }
}

void IRQHandler(interrupt_frame* frame) {
    uint32_t int_no = frame->int_no;

    switch (int_no) {
        case 32:
            TimerHandler(frame);
            break;
        case 33:
            KeyboardHandler(frame);
            break;
        default:
            UnknownExceptionHandler(frame);
            break;
    }
}

void TimerHandler(interrupt_frame* frame) {
    kprintf("Timer Interrupt\n");
}

void KeyboardHandler(interrupt_frame* frame) {
    uint8_t scancode = inb(0x60);
    uint8_t presscode = scancode & 0x7F;
    bool is_release = scancode & 0x80;
    char c = kbd_us[presscode];

    if  (presscode == LEFT_SHIFT_MAKE_SCANCODE || presscode == RIGHT_SHIFT_MAKE_SCANCODE) {
        shift_pressed = !is_release;
        if (shift_pressed) kprintf("Shift Is Pressed!\n");
        else kprintf("Shift Is Released!\n");
        return;
    }

    if (scancode & 0x80) {
        kprintf("Character %c Was Released!\n", c);
    } else {
        if (c >= 'a' && c <= 'z') {
            if (shift_pressed) c -= 32;
        }
        kprintf("Character %c Was Pressed!\n", c);
    }
}

void DivideByZeroHandler(interrupt_frame* frame) {
    kerror("Exception 0: Divide by Zero at EIP: 0x%x\n", frame->eip);
}

void DebugHandler(interrupt_frame* frame) {
    kerror("Exception 1: Debug Trap at EIP: 0x%x\n", frame->eip);
}

void NMIHandler(interrupt_frame* frame) {
    kerror("Exception 2: Non-Maskable Interrupt at EIP: 0x%x\n", frame->eip);
}

void BreakpointHandler(interrupt_frame* frame) {
    kerror("Exception 3: Breakpoint at EIP: 0x%x\n", frame->eip);
}

void OverflowHandler(interrupt_frame* frame) {
    kerror("Exception 4: Overflow at EIP: 0x%x\n", frame->eip);
}

void BoundRangeExceededHandler(interrupt_frame* frame) {
    kerror("Exception 5: BOUND Range Exceeded at EIP: 0x%x\n", frame->eip);
}

void InvalidOpcodeHandler(interrupt_frame* frame) {
    kerror("Exception 6: Invalid Opcode at EIP: 0x%x\n", frame->eip);
}

void DeviceNotAvailableHandler(interrupt_frame* frame) {
    kerror("Exception 7: Device Not Available at EIP: 0x%x\n", frame->eip);
}

void DoubleFaultHandler(interrupt_frame* frame) {
    kerror("Exception 8: Double Fault (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void CoprocessorSegmentOverrunHandler(interrupt_frame* frame) {
    kerror("Exception 9: Coprocessor Segment Overrun at EIP: 0x%x\n", frame->eip);
}

void InvalidTSSHandler(interrupt_frame* frame) {
    kerror("Exception 10: Invalid TSS (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void SegmentNotPresentHandler(interrupt_frame* frame) {
    kerror("Exception 11: Segment Not Present (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void StackSegmentFaultHandler(interrupt_frame* frame) {
    kerror("Exception 12: Stack-Segment Fault (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void GeneralProtectionFaultHandler(interrupt_frame* frame) {
    kerror("Exception 13: General Protection Fault (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void PageFaultHandler(interrupt_frame* frame) {
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));
    kerror("Exception 14: Page Fault (Error Code: 0x%x) at Address: 0x%x, EIP: 0x%x\n", frame->err_code, faulting_addr, frame->eip);
}

void FloatingPointExceptionHandler(interrupt_frame* frame) {
    kerror("Exception 16: x87 Floating-Point Exception at EIP: 0x%x\n", frame->eip);
}

void AlignmentCheckHandler(interrupt_frame* frame) {
    kerror("Exception 17: Alignment Check (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void CoprocessorErrorHandler(interrupt_frame* frame) {
    kerror("Exception 18: Machine Check at EIP: 0x%x\n", frame->eip);
}

void SIMDFloatingPointExceptionHandler(interrupt_frame* frame) {
    kerror("Exception 19: SIMD Floating-Point Exception at EIP: 0x%x\n", frame->eip);
}

void VirtualizationExceptionHandler(interrupt_frame* frame) {
    kerror("Exception 20: Virtualization Exception at EIP: 0x%x\n", frame->eip);
}

void ControlProtectionExceptionHandler(interrupt_frame* frame) {
    kerror("Exception 21: Control Protection Exception (Error Code: 0x%x) at EIP: 0x%x\n", frame->err_code, frame->eip);
}

void UnknownExceptionHandler(interrupt_frame* frame) {
    kerror("Unknown Exception %d at EIP: 0x%x\n", frame->int_no, frame->eip);
}

