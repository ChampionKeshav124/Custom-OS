/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   GUI Desktop Manager Header (AetherOS-64)
   ============================================================== */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>

/* Window structure */
typedef struct {
    int x;
    int y;
    int w;
    int h;
    const char* title;
    int active;
    int minimized;
    int maximized;
    int prev_x;
    int prev_y;
    int prev_w;
    int prev_h;
    int closed;
} gui_window_t;

/* Public GUI APIs */
void gui_init(void);
void gui_draw(void);
void gui_handle_key(char c);

/* Getters for terminal mapping */
uint16_t* gui_get_terminal_buffer(void);

#endif /* GUI_H */
