#include "isr_handler.h"
#include "cpu_local.h"
#include "scheduler.h"



void isr_handler(interrupt_frame_t* frame) {

    uint64_t int_no = frame->int_no;
    if (int_no < 32) {
        ExecptionHandler(frame);
    }
    else if (int_no == 32) {
        PitTimer();
    }
}

void PitTimer() {
    pit_timer_fired = true;
}

void ExecptionHandler(interrupt_frame_t* frame) {
    uint64_t int_no = frame->int_no;

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
    cpu_local_t* c = this_cpu();
    if (frame->cs == 0x23) {
        // User-mode fault: kill the offending task and keep the kernel running.
        // task_exit() marks current ZOMBIE and schedules; the next schedule()
        // on this CPU reaps the kstack + task_t via drain_pending_reap.
        kerror("KILL cpu=%d int=%d rip=%x task=%s\n",
               c ? (uint64_t)c->cpu_index : (uint64_t)-1,
               frame->int_no, frame->rip,
               (c && c->current) ? c->current->name : "?");
        task_exit();
    }
    // Kernel-mode fault: kernel state is suspect, halt this CPU. Calling end()
    // would unmount the root FS and recursively fault, clobbering the printout.
    kerror("PANIC cpu=%d int=%d rip=%x cs=%x task=%s\n",
           c ? (uint64_t)c->cpu_index : (uint64_t)-1,
           frame->int_no, frame->rip, frame->cs,
           (c && c->current) ? c->current->name : "?");
    while (1) { __asm__ volatile("cli; hlt"); }
}


void DivideByZeroHandler(interrupt_frame_t* frame) {
    kerror("Exception 0: Divide by Zero at RIP: %x\n", frame->rip);
}

void DebugHandler(interrupt_frame_t* frame) {
    kerror("Exception 1: Debug Trap at rip: %x\n", frame->rip);
}

void NMIHandler(interrupt_frame_t* frame) {
    kerror("Exception 2: Non-Maskable Interrupt at rip: %x\n", frame->rip);
}

void BreakpointHandler(interrupt_frame_t* frame) {
    kerror("Exception 3: Breakpoint at rip: %x\n", frame->rip);
}

void OverflowHandler(interrupt_frame_t* frame) {
    kerror("Exception 4: Overflow at rip: %x\n", frame->rip);
}

void BoundRangeExceededHandler(interrupt_frame_t* frame) {
    kerror("Exception 5: BOUND Range Exceeded at rip: %x\n", frame->rip);
}

void InvalidOpcodeHandler(interrupt_frame_t* frame) {
    kerror("Exception 6: Invalid Opcode at rip: %x\n", frame->rip);
}

void DeviceNotAvailableHandler(interrupt_frame_t* frame) {
    kerror("Exception 7: Device Not Available at rip: %x\n", frame->rip);
}

void DoubleFaultHandler(interrupt_frame_t* frame) {
    kerror("Exception 8: Double Fault (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void CoprocessorSegmentOverrunHandler(interrupt_frame_t* frame) {
    kerror("Exception 9: Coprocessor Segment Overrun at rip: %x\n", frame->rip);
}

void InvalidTSSHandler(interrupt_frame_t* frame) {
    kerror("Exception 10: Invalid TSS (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void SegmentNotPresentHandler(interrupt_frame_t* frame) {
    kerror("Exception 11: Segment Not Present (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void StackSegmentFaultHandler(interrupt_frame_t* frame) {
    kerror("Exception 12: Stack-Segment Fault (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void GeneralProtectionFaultHandler(interrupt_frame_t* frame) {
    kerror("Exception 13: General Protection Fault (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void PageFaultHandler(interrupt_frame_t* frame) {
    uint64_t faulting_addr = 0;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));
    kerror("Exception 14: Page Fault (Error Code: %x) at Address: %x, rip: %x\n", frame->err_code, faulting_addr, frame->rip);
}

void FloatingPointExceptionHandler(interrupt_frame_t* frame) {
    kerror("Exception 16: x87 Floating-Point Exception at rip: %x\n", frame->rip);
}

void AlignmentCheckHandler(interrupt_frame_t* frame) {
    kerror("Exception 17: Alignment Check (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void CoprocessorErrorHandler(interrupt_frame_t* frame) {
    kerror("Exception 18: Machine Check at rip: %x\n", frame->rip);
}

void SIMDFloatingPointExceptionHandler(interrupt_frame_t* frame) {
    kerror("Exception 19: SIMD Floating-Point Exception at rip: %x\n", frame->rip);
}

void VirtualizationExceptionHandler(interrupt_frame_t* frame) {
    kerror("Exception 20: Virtualization Exception at rip: %x\n", frame->rip);
}

void ControlProtectionExceptionHandler(interrupt_frame_t* frame) {
    kerror("Exception 21: Control Protection Exception (Error Code: %x) at rip: %x\n", frame->err_code, frame->rip);
}

void UnknownExceptionHandler(interrupt_frame_t* frame) {
    kerror("Unknown Exception %d at rip: %x\n", frame->int_no, frame->rip);
}

