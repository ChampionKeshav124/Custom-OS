/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VGA Text Mode Screen Driver Header (AetherOS-64)
   ============================================================== */

#ifndef VGA_H
#define VGA_H

#include <stdint.h>

/* VGA Text Mode Dimensions */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* Standard VGA Colors (for foreground & background) */
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GREY    0x7
#define VGA_COLOR_DARK_GREY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

/* Helper to combine foreground and background colors into a single attribute byte */
static inline uint8_t vga_color_byte(uint8_t fg, uint8_t bg) {
    return (uint8_t)((bg << 4) | fg);
}

/* Public APIs */

/* vga_clear_screen: Fills the entire screen with space characters and a black background */
void vga_clear_screen(void);

/* vga_print_char: Prints a single character at the current cursor position */
void vga_print_char(char c, uint8_t color);

/* vga_print_string: Prints a null-terminated string using the specified color attribute */
void vga_print_string(const char* str, uint8_t color);

/* vga_print_integer: Prints a 64-bit integer to the screen in a specified base */
void vga_print_integer(uint64_t val, int base, uint8_t color);

/* vga_print_newline: Advances the cursor position to the start of the next line */
void vga_print_newline(void);

/* vga_set_cursor: Directly positions the hardware blinking cursor at (row, col) */
void vga_set_cursor(int row, int col);

#endif /* VGA_H */
