/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   64-bit Interrupt Descriptor Table (IDT) Code (AetherOS-64)
   ============================================================== */

#include "idt.h"
#include "io.h"
#include "vga.h"

/* Declare the assembly interrupt handler stubs from isr.asm */
extern void keyboard_handler_asm(void);
extern void pit_handler_asm(void);
extern void mouse_handler_asm(void);

/* Exception Names array for descriptive panic messages */
static const char* exception_names[] = {
    "Division by Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt (#NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved Exception",
    "x87 Floating-Point Exception (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception (#SX)",
    "Reserved"
};

/* The 64-bit Interrupt Descriptor Table has 256 gates */
static struct IDTEntry idt[256];
static struct IDTPointer idt_ptr;

/* idt_set_gate: Fills out a 16-byte 64-bit IDT gate descriptor */
static void idt_set_gate(uint8_t vector, void* handler, uint8_t attributes) {
    uint64_t handler_addr = (uint64_t)handler;

    idt[vector].offset_low  = (uint16_t)(handler_addr & 0xFFFF);
    idt[vector].selector    = 0x08; // Code segment selector in our GDT (CODE_SEG_64)
    idt[vector].ist         = 0;    // Unused (Interrupt Stack Table)
    idt[vector].types_attr  = attributes; // Attributes (0x8E = Interrupt Gate, Present, Ring 0)
    idt[vector].offset_mid  = (uint16_t)((handler_addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((handler_addr >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;    // Must be 0
}

/* remap_pic: Remaps the hardware interrupts (IRQs) to prevent conflicts with CPU exceptions.
   - Master PIC: Command port 0x20, Data port 0x21.
   - Slave PIC: Command port 0xA0, Data port 0xA1.
   Remaps IRQ 0-7 to vectors 0x20-0x27, and IRQ 8-15 to vectors 0x28-0x2F. */
static void remap_pic(void) {
    // 1. Initialization Command Words (ICW) sequence
    outb(0x20, 0x11);           // Start initialization of Master PIC
    outb(0xA0, 0x11);           // Start initialization of Slave PIC

    // 2. Set Vector Offsets (ICW2)
    outb(0x21, 0x20);           // Master PIC vectors mapped to 0x20-0x27 (32-39)
    outb(0xA1, 0x28);           // Slave PIC vectors mapped to 0x28-0x2F (40-47)

    // 3. Connect Master and Slave (ICW3)
    outb(0x21, 0x04);           // Master PIC: Slave is attached to IRQ 2
    outb(0xA1, 0x02);           // Slave PIC: Cascade identity (2)

    // 4. Set Environmental Mode (ICW4)
    outb(0x21, 0x01);           // Set 8086 mode for Master
    outb(0xA1, 0x01);           // Set 8086 mode for Slave

    // 5. Mask/Unmask Interrupts (OCW1)
    // Master PIC mask: 0xF8 (binary 11111000) -> Enable IRQ0 (timer), IRQ1 (keyboard), and IRQ2 (cascade to Slave PIC)
    // Slave PIC mask:  0xEF (binary 11101111) -> Enable IRQ12 (mouse), mask all others
    outb(0x21, 0xF8);
    outb(0xA1, 0xEF);
}

/* idt_init: Entry point for setting up interrupts and exceptions */
void idt_init(void) {
    // 1. Remap the Programmable Interrupt Controller
    remap_pic();

    // 2. Clear all IDT entries to 0 (default unhandled state)
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low  = 0;
        idt[i].selector    = 0;
        idt[i].ist         = 0;
        idt[i].types_attr  = 0;
        idt[i].offset_mid  = 0;
        idt[i].offset_high = 0;
        idt[i].reserved    = 0;
    }

    // 3. Register our 32 assembly CPU exception handlers (vectors 0 to 31)
    // Attribute 0x8E = Present, Privilege Ring 0, Interrupt Gate type
    void* exception_stubs[32] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, exception_stubs[i], 0x8E);
    }

    // 4. Register our assembly timer handler stub at vector 0x20 (IRQ 0)
    idt_set_gate(0x20, pit_handler_asm, 0x8E);

    // 5. Register our assembly keyboard handler stub at vector 0x21 (IRQ 1)
    idt_set_gate(0x21, keyboard_handler_asm, 0x8E);

    // 6. Register our assembly mouse handler stub at vector 0x2C (IRQ 12)
    idt_set_gate(0x2C, mouse_handler_asm, 0x8E);

    // 6. Setup the LIDT pointer structure
    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base  = (uint64_t)&idt;

    // 7. Load the IDT into the CPU's IDTR register
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    // 8. Enable CPU interrupts
    __asm__ volatile("sti");
}

/* exception_handler: Unified C handler for all CPU exceptions. 
   Paints a Red Screen of Death, dumps CPU registers, prints CR2 on page faults, and halts. */
void exception_handler(struct interrupt_frame* frame) {
    // 1. Disable all interrupts to prevent nested panics
    __asm__ volatile("cli");

    // 2. Clear the screen with a solid RED background (attribute 0x4F: white on red)
    uint8_t panic_color = vga_color_byte(VGA_COLOR_WHITE, VGA_COLOR_RED);
    uint16_t* const vga_buffer = (uint16_t*)0xB8000;
    uint16_t blank_cell = (uint16_t)((panic_color << 8) | ' ');
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank_cell;
    }

    // Reset the cursor coordinates to (0, 0)
    vga_set_cursor(0, 0);

    // 3. Render header
    vga_print_string("================================================================================\n", panic_color);
    vga_print_string("                       !!! KERNEL PANIC: CPU EXCEPTION !!!                      \n", panic_color);
    vga_print_string("================================================================================\n", panic_color);

    // 4. Render exception detail
    vga_print_string("Exception Vector: ", panic_color);
    vga_print_integer(frame->interrupt_number, 10, panic_color);
    if (frame->interrupt_number < 32) {
        vga_print_string(" - ", panic_color);
        vga_print_string(exception_names[frame->interrupt_number], panic_color);
    }
    vga_print_newline();

    vga_print_string("Error Code:       0x", panic_color);
    vga_print_integer(frame->error_code, 16, panic_color);
    vga_print_newline();

    // 5. Special handler: If it's a Page Fault (vector 14), read CR2 (contains the faulting memory address)
    if (frame->interrupt_number == 14) {
        uint64_t cr2_val;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2_val));
        vga_print_string("Faulting Addr:    0x", panic_color);
        vga_print_integer(cr2_val, 16, panic_color);
        vga_print_newline();
    }

    vga_print_string("--------------------------------------------------------------------------------\n", panic_color);
    vga_print_string("CPU Register Dump:\n", panic_color);

    // Row A: RIP, CS, RFLAGS
    vga_print_string("  RIP: 0x", panic_color);
    vga_print_integer(frame->rip, 16, panic_color);
    vga_print_string("   CS: 0x", panic_color);
    vga_print_integer(frame->cs, 16, panic_color);
    vga_print_string("   RFLAGS: 0x", panic_color);
    vga_print_integer(frame->rflags, 16, panic_color);
    vga_print_newline();

    // Row B: RSP, SS
    vga_print_string("  RSP: 0x", panic_color);
    vga_print_integer(frame->rsp, 16, panic_color);
    vga_print_string("   SS: 0x", panic_color);
    vga_print_integer(frame->ss, 16, panic_color);
    vga_print_newline();

    // Row C: RAX, RBX, RCX, RDX
    vga_print_string("  RAX: 0x", panic_color);
    vga_print_integer(frame->rax, 16, panic_color);
    vga_print_string("  RBX: 0x", panic_color);
    vga_print_integer(frame->rbx, 16, panic_color);
    vga_print_string("  RCX: 0x", panic_color);
    vga_print_integer(frame->rcx, 16, panic_color);
    vga_print_string("  RDX: 0x", panic_color);
    vga_print_integer(frame->rdx, 16, panic_color);
    vga_print_newline();

    // Row D: RSI, RDI, RBP
    vga_print_string("  RSI: 0x", panic_color);
    vga_print_integer(frame->rsi, 16, panic_color);
    vga_print_string("  RDI: 0x", panic_color);
    vga_print_integer(frame->rdi, 16, panic_color);
    vga_print_string("  RBP: 0x", panic_color);
    vga_print_integer(frame->rbp, 16, panic_color);
    vga_print_newline();

    // Row E: R8-R11
    vga_print_string("  R8:  0x", panic_color);
    vga_print_integer(frame->r8, 16, panic_color);
    vga_print_string("  R9:  0x", panic_color);
    vga_print_integer(frame->r9, 16, panic_color);
    vga_print_string("  R10: 0x", panic_color);
    vga_print_integer(frame->r10, 16, panic_color);
    vga_print_string("  R11: 0x", panic_color);
    vga_print_integer(frame->r11, 16, panic_color);
    vga_print_newline();

    // Row F: R12-R15
    vga_print_string("  R12: 0x", panic_color);
    vga_print_integer(frame->r12, 16, panic_color);
    vga_print_string("  R13: 0x", panic_color);
    vga_print_integer(frame->r13, 16, panic_color);
    vga_print_string("  R14: 0x", panic_color);
    vga_print_integer(frame->r14, 16, panic_color);
    vga_print_string("  R15: 0x", panic_color);
    vga_print_integer(frame->r15, 16, panic_color);
    vga_print_newline();

    vga_print_string("--------------------------------------------------------------------------------\n", panic_color);
    vga_print_string("                       AetherOS Halted. Please reboot the VM.                   \n", panic_color);
    vga_print_string("================================================================================\n", panic_color);

    // 6. Halt the CPU forever
    while (1) {
        __asm__ volatile("hlt");
    }
}
