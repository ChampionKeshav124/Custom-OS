/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VGA PS/2 Keyboard Driver Header (AetherOS-64)
   ============================================================= */

#ifndef KEYBOARD_H
#define KEYBOARD_H

/* keyboard_handler: Read key scan code from port 0x60, translate it to ASCII,
   echo it to the screen via the VGA driver, and acknowledge the PIC.
   Called by keyboard_handler_asm in isr.asm. */
void keyboard_handler(void);

#endif /* KEYBOARD_H */
