/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   PS/2 Mouse Driver Header (AetherOS-64)
   ============================================================== */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

/* Public APIs */
void mouse_init(void);
void mouse_handler(void);

/* Getters */
int mouse_get_x(void);
int mouse_get_y(void);
uint8_t mouse_get_buttons(void);

/* Setters */
void mouse_set_position(int x, int y);

#endif /* MOUSE_H */
