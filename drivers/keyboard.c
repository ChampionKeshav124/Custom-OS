/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   VGA PS/2 Keyboard Driver (AetherOS-64)
   ============================================================= */

#include "keyboard.h"
#include "vga.h"
#include "io.h"
#include "shell.h"

/* Standard US QWERTY Keyboard Scan Code Set 1 mapping table.
   Maps key press scan codes (0x01 - 0x7F) to ASCII.
   Keys without direct ASCII representation are mapped to 0. */
static const char keyboard_map[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', /* 0x00 - 0x09 */
    '9', '0', '-', '=', '\b',                         /* 0x0A - 0x0E (0x0E = Backspace) */
    '\t',                                             /* 0x0F = Tab */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', /* 0x10 - 0x19 */
    '[', ']', '\n',                                   /* 0x1A - 0x1C (0x1C = Enter) */
    0,                                                /* 0x1D = Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 0x1E - 0x27 */
    '\'', '`', 0,                                     /* 0x28 - 0x2A (0x2A = Left Shift) */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.',/* 0x2B - 0x34 */
    '/', 0,                                           /* 0x35 - 0x36 (0x36 = Right Shift) */
    '*',                                              /* 0x37 = Keypad * */
    0,                                                /* 0x38 = Alt */
    ' ',                                              /* 0x39 = Space */
    0,                                                /* 0x3A = Caps Lock */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                     /* 0x3B - 0x44 = F1 - F10 (Unused) */
    0,                                                /* 0x45 = Num Lock */
    0,                                                /* 0x46 = Scroll Lock */
    0, 0, 0,                                          /* 0x47 - 0x49 = Home, Up, PgUp */
    '-',                                              /* 0x4A = Keypad - */
    0, 0, 0,                                          /* 0x4B - 0x4D = Left, Center, Right */
    '+',                                              /* 0x4E = Keypad + */
    0, 0, 0, 0,                                       /* 0x4F - 0x52 = End, Down, PgDn, Ins */
    0, 0, 0, 0,                                       /* 0x53 - 0x56 = Del, SysReq, Unused, Unused */
    0,                                                /* 0x57 = F11 */
    0,                                                /* 0x58 = F12 */
    0                                                 /* Rest are 0 */
};

/* keyboard_handler: Read key scan code, translate to ASCII, and print */
void keyboard_handler(void) {
    // 1. Read scan code from the PS/2 keyboard data port 0x60
    uint8_t scancode = inb(0x60);

    // 2. Send End of Interrupt (EOI) to PIC command port 0x20.
    // This tells the PIC that the interrupt has been handled and it can
    // send further interrupts.
    outb(0x20, 0x20);

    // 3. Filter out key release (break) codes.
    // Key releases have bit 7 set (scancode >= 0x80).
    if (scancode & 0x80) {
        return;
    }

    // 4. Translate scan code to ASCII
    if (scancode < 128) {
        char ascii = keyboard_map[scancode];
        
        // Route character to GUI manager
        if (ascii != 0) {
            extern void gui_handle_key(char c);
            gui_handle_key(ascii);
        }
    }
}
