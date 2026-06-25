/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   YouTube Series: "I Built My Own Operating System From Scratch"
   Milestone 7: Extended Shell with Argument Parsing (AetherOS-64)
   ============================================================== */

#include "shell.h"
#include "vga.h"
#include "io.h"
#include "pmm.h"

#define BUFFER_SIZE 256

/* Static buffer to collect typed characters */
static char input_buffer[BUFFER_SIZE];
static int input_len = 0;

/* strcmp: Custom string comparison function since standard library string.h is not available */
static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* reboot_system: Hardware reboot via the keyboard controller, falling back to a triple fault */
static void reboot_system(void) {
    uint8_t status;

    // 1. Wait for the keyboard controller (8042) input buffer to be empty.
    // We poll status port 0x64 until bit 1 (Input Buffer Full) is 0.
    do {
        status = inb(0x64);
    } while (status & 0x02);

    // 2. Send the reset CPU command (0xFE) to the keyboard controller command register 0x64.
    // This pulses the physical CPU reset pin.
    outb(0x64, 0xFE);

    // 3. Fallback: If the keyboard controller reset fails (e.g. on some virtualizers),
    // we force a CPU Triple Fault.
    // We load an invalid IDT (limit=0, base=0) and trigger a software interrupt (int 3).
    // The CPU fails to find any IDT, escalates to double fault, then triple fault, forcing reset.
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = {0, 0};

    __asm__ volatile(
        "lidt %0\n\t"
        "int $3"
        :
        : "m"(null_idt)
    );
}

/* shell_init: Resets the shell buffer and renders the initial prompt */
void shell_init(void) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        input_buffer[i] = 0;
    }
    input_len = 0;

    vga_print_string("AetherOS> ", vga_color_byte(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
}

/* shell_execute: Compares parsed command name and runs the command with arguments */
static void shell_execute(const char* cmd, const char* args) {
    uint8_t color_success = vga_color_byte(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    uint8_t color_info    = vga_color_byte(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    uint8_t color_error   = vga_color_byte(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    uint8_t color_cyan    = vga_color_byte(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t color_white   = vga_color_byte(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    // 1. HELP command
    if (strcmp(cmd, "help") == 0) {
        vga_print_string("AetherOS-64 Shell Help Menu:\n", color_cyan);
        vga_print_string("  help          - Display this help information\n", color_info);
        vga_print_string("  about         - Show operating system details\n", color_info);
        vga_print_string("  version       - Print OS version information\n", color_info);
        vga_print_string("  meminfo       - Display physical memory allocation stats\n", color_info);
        vga_print_string("  fault-div     - Trigger CPU Division by Zero Exception (#DE)\n", color_info);
        vga_print_string("  fault-pf      - Trigger CPU Page Fault Exception (#PF)\n", color_info);
        vga_print_string("  echo [text]   - Print the text argument to screen\n", color_info);
        vga_print_string("  clear         - Clear the screen buffer\n", color_info);
        vga_print_string("  reboot        - Hard restart the computer\n", color_info);
    }
    // 2. ABOUT command
    else if (strcmp(cmd, "about") == 0) {
        vga_print_string("AetherOS v0.1.0-alpha\n", color_success);
        vga_print_string("Architecture: x86_64 Long Mode\n", color_info);
        vga_print_string("Created by   : Expert OS Engineer & Beginner Mentee\n", color_info);
        vga_print_string("Description  : Written from scratch in assembly and C!\n", color_info);
    }
    // 3. VERSION command
    else if (strcmp(cmd, "version") == 0) {
        vga_print_string("AetherOS v0.1.0-alpha (64-bit Long Mode | El Torito CD-ROM ISO)\n", color_cyan);
    }
    // 4. MEMINFO command
    else if (strcmp(cmd, "meminfo") == 0) {
        uint32_t total = pmm_get_total_blocks();
        uint32_t free = pmm_get_free_blocks();
        uint32_t used = pmm_get_used_blocks();

        vga_print_string("AetherOS-64 PMM Memory Info:\n", color_cyan);
        
        vga_print_string("  Total RAM: ", color_info);
        vga_print_integer(total / 256, 10, color_white);
        vga_print_string(" MB (", color_info);
        vga_print_integer(total, 10, color_white);
        vga_print_string(" pages)\n", color_info);

        vga_print_string("  Used RAM:  ", color_info);
        vga_print_integer(used * 4, 10, color_white);
        vga_print_string(" KB (", color_info);
        vga_print_integer(used, 10, color_white);
        vga_print_string(" pages)\n", color_info);

        vga_print_string("  Free RAM:  ", color_info);
        vga_print_integer(free / 256, 10, color_white);
        vga_print_string(" MB (", color_info);
        vga_print_integer(free, 10, color_white);
        vga_print_string(" pages)\n", color_info);
    }
    // 5. FAULT-DIV command
    else if (strcmp(cmd, "fault-div") == 0) {
        vga_print_string("Triggering Division by Zero Exception...\n", color_error);
        volatile int a = 0;
        volatile int b = 5 / a;
        (void)b;
    }
    // 6. FAULT-PF command
    else if (strcmp(cmd, "fault-pf") == 0) {
        vga_print_string("Triggering Page Fault Exception...\n", color_error);
        // Writing above 1GB (0x40000000), which is outside our mapped memory range
        volatile uint64_t* ptr = (volatile uint64_t*)0x500000000;
        *ptr = 0x1234;
    }
    // 7. ECHO command
    else if (strcmp(cmd, "echo") == 0) {
        // Just print the arguments passed after the command
        vga_print_string(args, color_white);
        vga_print_newline();
    }
    // 8. CLEAR command
    else if (strcmp(cmd, "clear") == 0) {
        vga_clear_screen();
    }
    // 9. REBOOT command
    else if (strcmp(cmd, "reboot") == 0) {
        vga_print_string("Rebooting system...\n", color_error);
        reboot_system();
    }
    // 10. UNKNOWN command
    else {
        vga_print_string("Command not found: ", color_error);
        vga_print_string(cmd, color_error);
        vga_print_newline();
    }
}

/* shell_input_char: Called by the keyboard driver when an ASCII key is pressed */
void shell_input_char(char c) {
    uint8_t color_text = vga_color_byte(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    // Case A: Enter key pressed
    if (c == '\n') {
        vga_print_newline();
        input_buffer[input_len] = '\0'; // Null terminate string
        
        // 1. Argument Parsing (Tokenization)
        // Find the first space in the input buffer to separate command and arguments
        char* cmd = input_buffer;
        char* args = "";
        char* space = 0;

        for (int i = 0; input_buffer[i] != '\0'; i++) {
            if (input_buffer[i] == ' ') {
                space = &input_buffer[i];
                break;
            }
        }

        // If a space was found, split the string into command and arguments
        if (space != 0) {
            *space = '\0';        // Terminate command string at the space
            args = space + 1;     // Arguments start after the space

            // Skip any additional leading spaces in the argument string
            while (*args == ' ') {
                args++;
            }
        }

        // 2. Execute command
        if (strcmp(cmd, "") != 0) {
            shell_execute(cmd, args);
        }
        
        // 3. Reset shell state and print new prompt (unless clear was executed)
        if (strcmp(cmd, "clear") != 0) {
            shell_init();
        } else {
            // If clear was called, we just reset the buffer indices silently
            for (int i = 0; i < BUFFER_SIZE; i++) {
                input_buffer[i] = 0;
            }
            input_len = 0;
        }
    }
    // Case B: Backspace key pressed
    else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            input_buffer[input_len] = '\0';
            vga_print_char('\b', color_text); // Echo backspace to screen
        }
    }
    // Case C: Normal printable ASCII characters
    else if (c >= 32 && c <= 126) {
        if (input_len < BUFFER_SIZE - 1) {
            input_buffer[input_len] = c;
            input_len++;
            input_buffer[input_len] = '\0';
            vga_print_char(c, color_text);
        }
    }
}
