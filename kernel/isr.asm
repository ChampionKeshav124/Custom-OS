; ==============================================================
; AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
; YouTube Series: "I Built My Own Operating System From Scratch"
; Milestone 10: Fault Gates & CPU Exceptions (AetherOS-64)
; ==============================================================

[bits 64]               ; Specify 64-bit instructions

; Export ISR stubs
global keyboard_handler_asm
global pit_handler_asm
global mouse_handler_asm
extern keyboard_handler
extern mouse_handler
extern exception_handler
extern schedule_next

; ==============================================================
; EXCEPTION HANDLING STUBS (VECTORS 0 - 31)
; ==============================================================

; Macro for exceptions that DO NOT push an error code (we push a dummy 0)
%macro ISR_NO_ERR_CODE 1
global isr%1
isr%1:
    push qword 0        ; Dummy error code
    push qword %1       ; Interrupt vector number
    jmp isr_common_stub
%endmacro

; Macro for exceptions that DO push an error code automatically
%macro ISR_ERR_CODE 1
global isr%1
isr%1:
    ; Error code is already pushed by the CPU
    push qword %1       ; Interrupt vector number
    jmp isr_common_stub
%endmacro

; Generate stubs 0 to 31
ISR_NO_ERR_CODE 0   ; Division by Zero
ISR_NO_ERR_CODE 1   ; Debug
ISR_NO_ERR_CODE 2   ; Non-Maskable Interrupt
ISR_NO_ERR_CODE 3   ; Breakpoint
ISR_NO_ERR_CODE 4   ; Overflow
ISR_NO_ERR_CODE 5   ; Bound Range Exceeded
ISR_NO_ERR_CODE 6   ; Invalid Opcode
ISR_NO_ERR_CODE 7   ; Device Not Available
ISR_ERR_CODE    8   ; Double Fault
ISR_NO_ERR_CODE 9   ; Coprocessor Segment Overrun
ISR_ERR_CODE    10  ; Invalid TSS
ISR_ERR_CODE    11  ; Segment Not Present
ISR_ERR_CODE    12  ; Stack-Segment Fault
ISR_ERR_CODE    13  ; General Protection Fault
ISR_ERR_CODE    14  ; Page Fault
ISR_NO_ERR_CODE 15  ; Reserved
ISR_NO_ERR_CODE 16  ; x87 Floating-Point Exception
ISR_ERR_CODE    17  ; Alignment Check
ISR_NO_ERR_CODE 18  ; Machine Check
ISR_NO_ERR_CODE 19  ; SIMD Floating-Point Exception
ISR_NO_ERR_CODE 20  ; Virtualization Exception
ISR_NO_ERR_CODE 21  ; Reserved
ISR_NO_ERR_CODE 22  ; Reserved
ISR_NO_ERR_CODE 23  ; Reserved
ISR_NO_ERR_CODE 24  ; Reserved
ISR_NO_ERR_CODE 25  ; Reserved
ISR_NO_ERR_CODE 26  ; Reserved
ISR_NO_ERR_CODE 27  ; Reserved
ISR_NO_ERR_CODE 28  ; Reserved
ISR_NO_ERR_CODE 29  ; Reserved
ISR_ERR_CODE    30  ; Security Exception
ISR_NO_ERR_CODE 31  ; Reserved

; ==============================================================
; UNIFIED ISR COMMON STUB
; ==============================================================
isr_common_stub:
    ; 1. Save all general-purpose registers (preserves current execution state)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Pass pointer to the register structure (rsp) as the 1st argument (rdi) to C
    mov rdi, rsp
    call exception_handler

    ; 3. Restore all general-purpose registers in reverse order
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 4. Pop the interrupt number and error code from the stack
    add rsp, 16

    ; 5. Return from interrupt (popping RIP, CS, RFLAGS, RSP, SS)
    iretq

; ==============================================================
; KEYBOARD HARDWARE INTERRUPT (IRQ 1)
; ==============================================================
keyboard_handler_asm:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call keyboard_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; ==============================================================
; TIMER INTERRUPT HANDLER (IRQ 0) - Preemptive Context Switch
; ==============================================================
pit_handler_asm:
    ; 1. Push all general-purpose registers to save current context
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Pass current stack pointer (rsp) as first parameter (rdi) to schedule_next
    mov rdi, rsp
    call schedule_next

    ; 3. Switch CPU stack pointer to the next thread's stack (returned in rax)
    mov rsp, rax

    ; 4. Send End of Interrupt (EOI) command (0x20) to Master PIC command port (0x20)
    mov al, 0x20
    out 0x20, al

    ; 5. Pop all general-purpose registers from the new thread's stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 6. Return from timer interrupt to the new thread's execution stream
    iretq

; ==============================================================
; MOUSE HARDWARE INTERRUPT (IRQ 12)
; ==============================================================
mouse_handler_asm:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call mouse_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

