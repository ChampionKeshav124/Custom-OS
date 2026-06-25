/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VGA Text Mode Screen Driver Implementation (AetherOS-64)
   ============================================================== */

#include "vga.h"
#include "io.h"

/*
 * DESIGN: We write to BOTH the physical VGA RAM at 0xB8000 AND the
 * GUI shadow buffer (terminal_buffer in gui.c).
 * - 0xB8000 guarantees text is always visible on screen even if VESA/GUI fails.
 * - terminal_buffer lets the GUI console window render the same text in graphics mode.
 */
extern uint16_t terminal_buffer[80 * 25];
static uint16_t* const phys_vga   = (uint16_t*)0xB8000; /* Physical VGA hardware RAM */
static uint16_t* const vga_buffer = terminal_buffer;     /* GUI shadow buffer         */

/* Current software cursor position */
static int cursor_x = 0;
static int cursor_y = 0;

/* vga_set_cursor: Updates both software and hardware cursor coordinates */
void vga_set_cursor(int row, int col) {
    // Synchronize software coordinates so subsequent prints align correctly
    cursor_x = col;
    cursor_y = row;

    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);

    // Write the low byte of the cursor offset to the VGA registers
    outb(0x3D4, 0x0F);                  // Select Cursor Location Low Register (index 15)
    outb(0x3D5, (uint8_t)(pos & 0xFF)); // Write low byte of offset

    // Write the high byte of the cursor offset
    outb(0x3D4, 0x0E);                  // Select Cursor Location High Register (index 14)
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF)); // Write high byte of offset
}

/* vga_clear_screen: Clears both the physical VGA buffer and GUI shadow buffer */
void vga_clear_screen(void) {
    uint8_t attribute = vga_color_byte(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint16_t blank_cell = (uint16_t)((attribute << 8) | ' ');

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        phys_vga[i]   = blank_cell;   /* Write to real screen hardware */
        vga_buffer[i] = blank_cell;   /* Write to GUI shadow buffer    */
    }

    cursor_x = 0;
    cursor_y = 0;
    vga_set_cursor(cursor_y, cursor_x);
}

/* vga_scroll: Scrolls both the physical VGA buffer and the GUI shadow buffer up one row */
static void vga_scroll(void) {
    uint8_t attribute = vga_color_byte(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint16_t blank_cell = (uint16_t)((attribute << 8) | ' ');

    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            int dst = (row - 1) * VGA_WIDTH + col;
            int src = row * VGA_WIDTH + col;
            phys_vga[dst]   = phys_vga[src];
            vga_buffer[dst] = vga_buffer[src];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++) {
        int last = (VGA_HEIGHT - 1) * VGA_WIDTH + col;
        phys_vga[last]   = blank_cell;
        vga_buffer[last] = blank_cell;
    }
}

/* vga_print_char: Output a single character to BOTH physical VGA and GUI shadow buffer */
void vga_print_char(char c, uint8_t color) {
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            int index = cursor_y * VGA_WIDTH + cursor_x;
            uint16_t cell = (uint16_t)((color << 8) | ' ');
            phys_vga[index]   = cell;
            vga_buffer[index] = cell;
        }
    }
    else if (c == '\r') {
        cursor_x = 0;
    }
    else if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    }
    else {
        int index = cursor_y * VGA_WIDTH + cursor_x;
        uint16_t cell = (uint16_t)((color << 8) | c);
        phys_vga[index]   = cell;   /* Directly visible on screen */
        vga_buffer[index] = cell;   /* Visible in GUI console window */
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
        cursor_y = VGA_HEIGHT - 1;
    }

    vga_set_cursor(cursor_y, cursor_x);
}

/* vga_print_string: Prints a null-terminated string to the screen */
void vga_print_string(const char* str, uint8_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        vga_print_char(str[i], color);
    }
}

/* vga_print_newline: Convenience function to print a newline */
void vga_print_newline(void) {
    vga_print_char('\n', vga_color_byte(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/* vga_print_integer: Prints a 64-bit unsigned integer to the screen in the specified base (e.g. 10 or 16) */
void vga_print_integer(uint64_t val, int base, uint8_t color) {
    char buf[64];
    int i = 0;

    // Handle 0 explicitly
    if (val == 0) {
        vga_print_char('0', color);
        return;
    }

    // Convert digits to characters (in reverse order)
    while (val > 0) {
        uint64_t rem = val % base;
        buf[i++] = (rem < 10) ? (char)('0' + rem) : (char)('A' + rem - 10);
        val /= base;
    }

    // Print digits in reverse order (correct direction)
    for (int j = i - 1; j >= 0; j--) {
        vga_print_char(buf[j], color);
    }
}
