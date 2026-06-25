/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VESA graphics driver implementation (AetherOS-64)
   ============================================================== */

#include "vesa.h"
#include "font.h"
#include "vga.h"

/* VESA Framebuffer State */
static uint64_t framebuffer_addr = 0;
static uint32_t framebuffer_pitch = 0;
static uint32_t framebuffer_width = 0;
static uint32_t framebuffer_height = 0;
static uint8_t framebuffer_bpp = 0;

/* Backbuffer for double buffering to prevent tearing (1024x768 max) */
#define MAX_WIDTH 1024
#define MAX_HEIGHT 768
static uint32_t backbuffer[MAX_WIDTH * MAX_HEIGHT];

/* vesa_show_error_screen: Paints a VGA text-mode error banner at 0xB8000 when VESA fails */
static void vesa_show_error_screen(void) {
    /*
     * Write directly to physical 0xB8000 VGA text RAM.
     * Each cell = 2 bytes: high byte = attribute (color), low byte = character.
     * 0x4F = white text on RED background.  0x0E = yellow on black.
     */
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    const uint8_t RED  = 0x4F;  /* white on red    */
    const uint8_t YEL  = 0x0E;  /* yellow on black */
    const uint8_t CYN  = 0x0B;  /* cyan on black   */
    const uint8_t GRY  = 0x07;  /* grey on black   */

    /* Clear 80x25 screen with red background */
    for (int i = 0; i < 80 * 25; i++) {
        vga[i] = (uint16_t)((RED << 8) | ' ');
    }

    /* Helper: write a string at (row, col) with given attribute */
    #define WR(row, col, attr, str) do { \
        const char* _s = (str); \
        for (int _i = 0; _s[_i]; _i++) \
            vga[(row)*80+(col)+_i] = (uint16_t)(((attr)<<8)|(unsigned char)_s[_i]); \
    } while(0)

    WR(0, 0,  RED, "================================================================================" );
    WR(1, 0,  RED, "                     !!!  AETHEROS VIDEO ERROR  !!!                            " );
    WR(2, 0,  RED, "================================================================================" );
    WR(4, 2,  YEL, "VESA / Linear Framebuffer could NOT be initialised."                            );
    WR(5, 2,  GRY, "GRUB did not provide a Multiboot2 framebuffer tag."                             );
    WR(7, 2,  CYN, "CAUSE:   VirtualBox display adapter may not support this VESA mode."           );
    WR(8, 2,  CYN, "         Required: 1024 x 768 x 32 bpp linear framebuffer."                    );
    WR(10, 2, YEL, "FIX OPTIONS (try one):");
    WR(11, 4, GRY, "1. In VirtualBox -> Settings -> Display -> change controller to VMSVGA");
    WR(12, 4, GRY, "   then restart the VM.");
    WR(13, 4, GRY, "2. Boot the 'AetherOS (Safe / Text Mode)' entry in the GRUB menu.");
    WR(14, 4, GRY, "3. Increase video RAM to 128 MB in VirtualBox Display settings.");
    WR(16, 2, YEL, "FALLBACK: AetherOS text shell is still running below. Type commands:");
    WR(17, 4, CYN, "help | about | version | meminfo | clear | reboot");
    WR(19, 0, RED, "--------------------------------------------------------------------------------");
    WR(20, 2, GRY, "Framebuffer addr : 0x00000000  (not mapped)");
    WR(21, 2, GRY, "GRUB magic       : 0x36d76289  (Multiboot2)");
    WR(23, 0, RED, "================================================================================");
    WR(24, 2, RED, "  AetherOS-64 v0.1 — Kernel alive. Graphics failed. Text fallback active.     ");

    #undef WR
}

/* vesa_init: Parses the Multiboot2 tags to locate and initialize the linear framebuffer */
void vesa_init(uint32_t mb_magic, uintptr_t mb_addr) {
    if (mb_magic != 0x36d76289) {
        vesa_show_error_screen();
        return; // Invalid magic
    }

    /* Iterate through Multiboot2 tags to locate the framebuffer tag (type == 8) */
    struct multiboot_tag {
        uint32_t type;
        uint32_t size;
    };

    struct multiboot_tag* tag = (struct multiboot_tag*)(mb_addr + 8);
    struct multiboot_tag_framebuffer* fb_tag = 0;

    while (tag->type != 0) {
        if (tag->type == 8) {
            fb_tag = (struct multiboot_tag_framebuffer*)tag;
            break;
        }
        // Jump to next tag, aligned to 8 bytes
        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }

    if (fb_tag != 0) {
        framebuffer_addr   = fb_tag->framebuffer_addr;
        framebuffer_pitch  = fb_tag->framebuffer_pitch;
        framebuffer_width  = fb_tag->framebuffer_width;
        framebuffer_height = fb_tag->framebuffer_height;
        framebuffer_bpp    = fb_tag->framebuffer_bpp;
    } else {
        /* GRUB did not provide a framebuffer — show the red error screen */
        vesa_show_error_screen();
    }
}

/* vesa_clear: Clears the backbuffer with a solid color */
void vesa_clear(uint32_t color) {
    uint32_t size = framebuffer_width * framebuffer_height;
    if (size > MAX_WIDTH * MAX_HEIGHT) {
        size = MAX_WIDTH * MAX_HEIGHT;
    }
    for (uint32_t i = 0; i < size; i++) {
        backbuffer[i] = color;
    }
}

/* vesa_put_pixel: Plots a single pixel on the backbuffer */
void vesa_put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)framebuffer_width && y >= 0 && y < (int)framebuffer_height) {
        backbuffer[y * framebuffer_width + x] = color;
    }
}

/* vesa_draw_rect: Draws a filled rectangle */
void vesa_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int cy = y; cy < y + h; cy++) {
        for (int cx = x; cx < x + w; cx++) {
            vesa_put_pixel(cx, cy, color);
        }
    }
}

/* vesa_draw_rect_outline: Draws an empty rectangle border */
void vesa_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    // Top & Bottom
    for (int cx = x; cx < x + w; cx++) {
        vesa_put_pixel(cx, y, color);
        vesa_put_pixel(cx, y + h - 1, color);
    }
    // Left & Right
    for (int cy = y; cy < y + h; cy++) {
        vesa_put_pixel(x, cy, color);
        vesa_put_pixel(x + w - 1, cy, color);
    }
}

/* vesa_draw_char: Draws an 8x8 character from our custom matrix font */
void vesa_draw_char(char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) {
        c = ' ';
    }
    int idx = c - 32;
    for (int row = 0; row < 8; row++) {
        uint8_t row_bits = font8x8[idx][row];
        for (int col = 0; col < 8; col++) {
            if (row_bits & (1 << (7 - col))) {
                vesa_put_pixel(x + col, y + row, color);
            }
        }
    }
}

/* vesa_draw_string: Draws a null-terminated string */
void vesa_draw_string(const char* str, int x, int y, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        vesa_draw_char(str[i], x + i * 8, y, color);
    }
}

/* vesa_draw_line: Draws a line using Bresenham's line algorithm */
void vesa_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0;
    if (dx < 0) dx = -dx;
    int dy = y1 - y0;
    if (dy < 0) dy = -dy;
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        vesa_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* vesa_flush: Flushes the backbuffer to the physical linear framebuffer */
void vesa_flush(void) {
    if (framebuffer_addr == 0) {
        return;
    }

    uint8_t* dest_line = (uint8_t*)(uintptr_t)framebuffer_addr;
    uint8_t* src_line = (uint8_t*)backbuffer;
    uint32_t line_bytes = framebuffer_width * 4;

    for (uint32_t y = 0; y < framebuffer_height; y++) {
        uint32_t* d = (uint32_t*)dest_line;
        uint32_t* s = (uint32_t*)src_line;
        for (uint32_t x = 0; x < framebuffer_width; x++) {
            d[x] = s[x];
        }
        dest_line += framebuffer_pitch;
        src_line += line_bytes;
    }
}

/* Getters */
uint32_t vesa_get_width(void) {
    return framebuffer_width;
}

uint32_t vesa_get_height(void) {
    return framebuffer_height;
}

uint32_t* vesa_get_backbuffer(void) {
    return backbuffer;
}

void vesa_set_resolution(uint32_t width, uint32_t height) {
    if (width <= MAX_WIDTH && height <= MAX_HEIGHT && width > 0 && height > 0) {
        framebuffer_width = width;
        framebuffer_height = height;
    }
}

/* vesa_is_ready: Returns 1 if VESA framebuffer was successfully initialised, 0 otherwise */
int vesa_is_ready(void) {
    return (framebuffer_addr != 0) ? 1 : 0;
}
