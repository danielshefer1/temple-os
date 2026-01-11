#include "int_handler.h"

static idt_entry idt[256];
static idt_ptr idtr;

static void* handlers[] = {
    (void*)isr_stub_0,
    (void*)isr_stub_1,
    (void*)isr_stub_2,
    (void*)isr_stub_3,
    (void*)isr_stub_4,
    (void*)isr_stub_5,
    (void*)isr_stub_6,
    (void*)isr_stub_7,
    (void*)isr_stub_8,
    (void*)isr_stub_9,
    (void*)isr_stub_10,
    (void*)isr_stub_11,
    (void*)isr_stub_12,
    (void*)isr_stub_13,
    (void*)isr_stub_14,
    0,
    (void*)isr_stub_16,
    (void*)isr_stub_17,
    (void*)isr_stub_18,
    (void*)isr_stub_19,
    (void*)isr_stub_20,
    (void*)isr_stub_21
};
static uint32_t num_handlers = sizeof(handlers) / sizeof(handlers[0]);

void SetIDTEntry(uint32_t offset, uint16_t sel, uint8_t present, uint8_t privilege, uint8_t type, uint32_t idx) {
    idt_entry* entry = &idt[idx];

    entry->base_low = offset & 0xFFFF;
    entry->sel = sel;
    entry->reserved = 0;

    entry->gate_type = type;
    entry->storage_segment = 0;
    entry->privilege = privilege;
    entry->present = present;

    entry->base_high = (offset >> 16) & 0xFFFF;
}

void CheckIDT() {
    idt_ptr current_idtr;
    __asm__ volatile("sidt %0" : "=m"(current_idtr));
    
    kprintf("IDTR Base: 0x%x\n", current_idtr.base);
    kprintf("IDTR Limit: 0x%x\n", current_idtr.limit);
    kprintf("Expected Base: 0x%x\n", (uint32_t)&idt);  // or physical if needed
    kprintf("Expected Limit: 0x%x\n", sizeof(idt) - 1);
}

void InitIDT() {
    for (uint32_t i = 0; i < num_handlers; i++) {
        if (handlers[i] != 0) {
            SetIDTEntry((uint32_t)handlers[i], 0x08, 1, 0, 0xE, i);
        }
        else {
            // Unused entry
            SetIDTEntry(0, 0x08, 0, 0, 0xE, i);
        }
    }
    for (uint32_t i = num_handlers; i < 256; i++) {
        // Unused entry
        SetIDTEntry(0, 0x08, 0, 0, 0xE, i);
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt;
    LoadIDTHelper(&idtr);
    StiHelper();

    CheckIDT();
}

void isr_handler(interrupt_frame* frame) {
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
        default:
            UnknownExceptionHandler(frame);
            break;
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