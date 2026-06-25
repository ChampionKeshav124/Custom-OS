/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   64-bit Interrupt Descriptor Table Header (AetherOS-64)
   ============================================================== */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* A 64-bit IDT Gate Descriptor (16 bytes total) */
struct IDTEntry {
    uint16_t offset_low;      // Offset bits 0..15
    uint16_t selector;        // Code Segment selector in GDT (0x08 for CODE_SEG_64)
    uint8_t  ist;             // Interrupt Stack Table offset (usually 0, unused)
    uint8_t  types_attr;      // Gate Type and Attributes (e.g. 0x8E = Present, Ring 0, Int Gate)
    uint16_t offset_mid;      // Offset bits 16..31
    uint32_t offset_high;     // Offset bits 32..63
    uint32_t reserved;        // Reserved, must be 0
} __attribute__((packed));

/* Structure matching the physical LIDT argument (10 bytes total) */
struct IDTPointer {
    uint16_t limit;           // Size of the IDT table minus 1
    uint64_t base;            // Direct virtual address of the IDT array
} __attribute__((packed));

/* Structure representing the CPU registers pushed during an ISR */
struct interrupt_frame {
    // Pushed by isr_common_stub (in reverse push order)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // Pushed by ISR stub macro
    uint64_t interrupt_number;
    uint64_t error_code;      // Real error code or dummy 0
    
    // Pushed automatically by CPU on interrupt
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

/* idt_init: Initializes the IDT table, remaps the PIC, sets handlers, and loads LIDT */
void idt_init(void);

/* Declare all 32 exception handlers generated in isr.asm */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void mouse_handler_asm(void);

#endif /* IDT_H */
