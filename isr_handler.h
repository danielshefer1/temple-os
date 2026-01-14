#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "global.h"
#include "keyboard.h"

void isr_handler(interrupt_frame* frame);

void ExecptionHandler(interrupt_frame* frame);
void IRQHandler(interrupt_frame* frame);

// Exception Handlers
void DivideByZeroHandler(interrupt_frame* frame);
void DebugHandler(interrupt_frame* frame);
void NMIHandler(interrupt_frame* frame);
void BreakpointHandler(interrupt_frame* frame);
void OverflowHandler(interrupt_frame* frame);
void BoundRangeExceededHandler(interrupt_frame* frame);
void InvalidOpcodeHandler(interrupt_frame* frame);
void DeviceNotAvailableHandler(interrupt_frame* frame);
void DoubleFaultHandler(interrupt_frame* frame);
void CoprocessorSegmentOverrunHandler(interrupt_frame* frame);
void InvalidTSSHandler(interrupt_frame* frame);
void SegmentNotPresentHandler(interrupt_frame* frame);
void StackSegmentFaultHandler(interrupt_frame* frame);
void GeneralProtectionFaultHandler(interrupt_frame* frame);
void PageFaultHandler(interrupt_frame* frame);
void FloatingPointExceptionHandler(interrupt_frame* frame);
void AlignmentCheckHandler(interrupt_frame* frame);
void CoprocessorErrorHandler(interrupt_frame* frame);
void SIMDFloatingPointExceptionHandler(interrupt_frame* frame);
void VirtualizationExceptionHandler(interrupt_frame* frame);
void ControlProtectionExceptionHandler(interrupt_frame* frame);
void UnknownExceptionHandler(interrupt_frame* frame);

// IRQ Handlers
void TimerHandler(interrupt_frame* frame);
void KeyboardHandler(interrupt_frame* frame);