/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VESA Graphical Loading Screen (AetherOS-64)
   ============================================================== */

#include "splash.h"
#include "vesa.h"

/* Simple busy-wait delay helper */
static void delay(volatile uint32_t n) {
    while (n--) {
        __asm__ volatile("nop");
    }
}

/* splash_run: Orchestrates a graphical Windows 11-style boot loading screen */
void splash_run(void) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    
    // Background color: deep slate blue-black
    uint32_t bg_color = 0x0A0B0E;
    
    // 8 loading dot circular positions (offsets from center)
    int px[] = {0, 14, 20, 14, 0, -14, -20, -14};
    int py[] = {-20, -14, 0, 14, 20, 14, 0, -14};
    
    // 60-frame loading animation
    for (int frame = 0; frame < 60; frame++) {
        // Clear background
        vesa_clear(bg_color);
        
        // 1. Draw Windows 11-style Grid Logo (Centered at 512, 300)
        int cx = 512;
        int cy = 300;
        int size = 36;
        int gap = 6;
        uint32_t logo_color = 0x00E5FF; // Cyber Cyan
        
        // Top-Left
        vesa_draw_rect(cx - size - gap/2, cy - size - gap/2, size, size, logo_color);
        // Top-Right
        vesa_draw_rect(cx + gap/2, cy - size - gap/2, size, size, logo_color);
        // Bottom-Left
        vesa_draw_rect(cx - size - gap/2, cy + gap/2, size, size, logo_color);
        // Bottom-Right
        vesa_draw_rect(cx + gap/2, cy + gap/2, size, size, logo_color);
        
        // Tagline text
        vesa_draw_string("AetherOS", cx - 4 * 8, cy + 60, 0xFFFFFF);
        
        // 2. Draw Rotating Loading Circle (Centered at 512, 480)
        int lcx = 512;
        int lcy = 480;
        
        // Draw 5 dots of the trailing loading tail
        for (int k = 0; k < 5; k++) {
            int idx = (frame + k) % 8;
            int dx = px[idx];
            int dy = py[idx];
            
            uint32_t dot_color = 0x42464D; // Tail end (dim grey)
            int dot_size = 3;
            
            if (k == 4) {
                dot_color = 0x00E5FF; // Head (Vibrant Cyan)
                dot_size = 5;
            } else if (k == 3) {
                dot_color = 0x84FFFF; // Bright Cyan
                dot_size = 4;
            } else if (k == 2) {
                dot_color = 0x82B1FF; // Light Blue
                dot_size = 4;
            } else if (k == 1) {
                dot_color = 0x78909C; // Grey
                dot_size = 3;
            }
            
            // Draw dot centered on offset
            vesa_draw_rect(lcx + dx - dot_size/2, lcy + dy - dot_size/2, dot_size, dot_size, dot_color);
        }
        
        // 3. Draw booting status messages (Centered at 512, 540)
        const char* status_text = "Preparing system...";
        if (frame < 15) {
            status_text = "Initializing physical memory manager...";
        } else if (frame < 30) {
            status_text = "Loading kernel subsystems & drivers...";
        } else if (frame < 45) {
            status_text = "Starting task scheduler & workers...";
        } else {
            status_text = "Launching interactive desktop environment...";
        }
        
        // Find text length to center it
        int len = 0;
        while (status_text[len] != '\0') len++;
        vesa_draw_string(status_text, 512 - (len * 4), 540, 0x8A9099);
        
        // Flush buffer to display
        vesa_flush();
        
        // Delay for frame rate timing (~30-40ms)
        delay(1800000);
    }
    
    // Clear screen to black before exiting splash
    vesa_clear(0x000000);
    vesa_flush();
}
