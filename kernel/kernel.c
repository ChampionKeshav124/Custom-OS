/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   YouTube Series: "I Built My Own Operating System From Scratch"
   Milestone 6: Shell v1 Integration (AetherOS-64)
   ============================================================== */

#include "vga.h"
#include "splash.h"
#include "idt.h"
#include "shell.h"
#include "pmm.h"
#include "pit.h"
#include "task.h"
#include "vesa.h"
#include "gui.h"
#include "mouse.h"

/* Global thread tick counters for multitasking telemetry */
volatile uint32_t thread1_ticks = 0;
volatile uint32_t thread2_ticks = 0;

/* Background worker thread 1: increments a tick counter */
void worker_thread_1(void) {
    while (1) {
        thread1_ticks++;
        
        // Busy-wait delay (~200ms in VM)
        for (volatile uint32_t i = 0; i < 8000000; i++) {
            __asm__("nop");
        }
    }
}

/* Background worker thread 2: increments a tick counter */
void worker_thread_2(void) {
    while (1) {
        thread2_ticks++;
        
        // Busy-wait delay (~300ms in VM)
        for (volatile uint32_t i = 0; i < 12000000; i++) {
            __asm__("nop");
        }
    }
}

/* kernel_main: The entry point of our C kernel, called by boot.asm */
void kernel_main(unsigned long magic, unsigned long addr) {
    // Copy Multiboot2 info structure to a safe location in kernel memory
    // to prevent it from being overwritten by the PMM bitmap.
    static uint8_t mb_info_copy[8192];
    if (magic == 0x36d76289 && addr != 0) {
        uint32_t mb_size = *(uint32_t*)addr;
        if (mb_size > sizeof(mb_info_copy)) {
            mb_size = sizeof(mb_info_copy);
        }
        for (uint32_t i = 0; i < mb_size; i++) {
            mb_info_copy[i] = ((uint8_t*)addr)[i];
        }
        addr = (uintptr_t)mb_info_copy;
    }

    // 1. Initialize the Physical Memory Manager (PMM)
    pmm_init((uint32_t)magic, (uintptr_t)addr);
    *((volatile uint16_t*)0xB809E) = 0x0F41; // 'A'

    // Try to initialise VESA graphics — this also draws the error screen if it fails
    vesa_init((uint32_t)magic, (uintptr_t)addr);

    if (vesa_is_ready()) {
        // VESA success: bring up the full GUI desktop
        gui_init();
        mouse_init();

        // 2. Initialize multitasking and create background worker threads
        task_init();
        *((volatile uint16_t*)0xB809E) = 0x0F42; // 'B'
        task_create(worker_thread_1);
        *((volatile uint16_t*)0xB809E) = 0x0F43; // 'C'
        task_create(worker_thread_2);
        *((volatile uint16_t*)0xB809E) = 0x0F44; // 'D'

        // 3. Initialize the Programmable Interval Timer at 100 Hz
        pit_init(100);
        *((volatile uint16_t*)0xB809E) = 0x0F45; // 'E'

        // 4. Run the cinematic loading screen
        splash_run();
        *((volatile uint16_t*)0xB809E) = 0x0F46; // 'F'

        // 5. Initialize the IDT, PIC remapping, and enable CPU interrupts
        idt_init();

        // 6. Initialize the interactive text shell (shown in GUI console window)
        shell_init();

        // 7. GUI render loop: redraw desktop and wait for interrupts
        while (1) {
            gui_draw();
            __asm__ volatile("hlt");
        }
    } else {
        /*
         * VESA FALLBACK MODE
         * The error screen is already displayed by vesa_init().
         * Set up the IDT so keyboard interrupts are delivered, then drop into
         * the text shell — the user can type commands directly in VGA text mode.
         */
        task_init();
        pit_init(100);
        idt_init();
        shell_init();   /* Prints "AetherOS> " prompt to physical 0xB8000 VGA RAM */

        /* Infinite loop: keyboard IRQs drive shell_input_char, hlt saves CPU */
        while (1) {
            __asm__ volatile("hlt");
        }
    }
}
