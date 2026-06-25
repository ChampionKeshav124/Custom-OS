/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VESA graphics driver header (AetherOS-64)
   ============================================================== */

#ifndef VESA_H
#define VESA_H

#include <stdint.h>

/* Multiboot2 Framebuffer Info Tag */
struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
} __attribute__((packed));

/* Public Drawing APIs */
void vesa_init(uint32_t mb_magic, uintptr_t mb_addr);
void vesa_clear(uint32_t color);
void vesa_put_pixel(int x, int y, uint32_t color);
void vesa_draw_rect(int x, int y, int w, int h, uint32_t color);
void vesa_draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void vesa_draw_char(char c, int x, int y, uint32_t color);
void vesa_draw_string(const char* str, int x, int y, uint32_t color);
void vesa_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void vesa_flush(void);

/* Getters & Setters */
uint32_t vesa_get_width(void);
uint32_t vesa_get_height(void);
uint32_t* vesa_get_backbuffer(void);
void vesa_set_resolution(uint32_t width, uint32_t height);
int  vesa_is_ready(void);   /* Returns 1 if framebuffer was mapped, 0 otherwise */

#endif /* VESA_H */
