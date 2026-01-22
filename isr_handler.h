#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "global.h"
#include "keyboard.h"

void isr_handler(interrupt_frame_t* frame);

void ExecptionHandler(interrupt_frame_t* frame);
void IRQHandler(interrupt_frame_t* frame);

// Exception Handlers
void DivideByZeroHandler(interrupt_frame_t* frame);
void DebugHandler(interrupt_frame_t* frame);
void NMIHandler(interrupt_frame_t* frame);
void BreakpointHandler(interrupt_frame_t* frame);
void OverflowHandler(interrupt_frame_t* frame);
void BoundRangeExceededHandler(interrupt_frame_t* frame);
void InvalidOpcodeHandler(interrupt_frame_t* frame);
void DeviceNotAvailableHandler(interrupt_frame_t* frame);
void DoubleFaultHandler(interrupt_frame_t* frame);
void CoprocessorSegmentOverrunHandler(interrupt_frame_t* frame);
void InvalidTSSHandler(interrupt_frame_t* frame);
void SegmentNotPresentHandler(interrupt_frame_t* frame);
void StackSegmentFaultHandler(interrupt_frame_t* frame);
void GeneralProtectionFaultHandler(interrupt_frame_t* frame);
void PageFaultHandler(interrupt_frame_t* frame);
void FloatingPointExceptionHandler(interrupt_frame_t* frame);
void AlignmentCheckHandler(interrupt_frame_t* frame);
void CoprocessorErrorHandler(interrupt_frame_t* frame);
void SIMDFloatingPointExceptionHandler(interrupt_frame_t* frame);
void VirtualizationExceptionHandler(interrupt_frame_t* frame);
void ControlProtectionExceptionHandler(interrupt_frame_t* frame);
void UnknownExceptionHandler(interrupt_frame_t* frame);

// IRQ Handlers
void TimerHandler();
void KeyboardHandler();

