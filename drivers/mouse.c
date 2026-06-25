/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   PS/2 Mouse Driver Implementation (AetherOS-64)
   ============================================================== */

#include "mouse.h"
#include "io.h"

/* Mouse State */
static int mouse_x = 400; // Center of 1024x768 initially
static int mouse_y = 300;
static uint8_t mouse_buttons = 0;

/* Packet buffer for 3-byte PS/2 mouse packet */
static uint8_t mouse_packet[3];
static uint8_t mouse_cycle = 0;

/* PS/2 Controller Helper Functions */
static void mouse_wait_write(void) {
    // Bit 1 of status register (0x64) indicates if input buffer is full
    // We wait until it becomes 0 (buffer empty)
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

static void mouse_wait_read(void) {
    // Bit 0 of status register (0x64) indicates if output buffer is full (data ready to read)
    // We wait until it becomes 1 (data ready)
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(0x64) & 1) == 1) return;
    }
}

static void mouse_write_cmd(uint8_t write_to_aux_data) {
    mouse_wait_write();
    outb(0x64, 0xD4); // Tell controller we are writing to auxiliary/mouse port
    mouse_wait_write();
    outb(0x60, write_to_aux_data); // Send data
}

static uint8_t mouse_read_data(void) {
    mouse_wait_read();
    return inb(0x60);
}

/* mouse_init: Initializes the PS/2 controller and the auxiliary mouse device */
void mouse_init(void) {
    uint8_t status;

    // 1. Enable auxiliary mouse device
    mouse_wait_write();
    outb(0x64, 0xA8);

    // 2. Read Controller Command Byte
    mouse_wait_write();
    outb(0x64, 0x20); // Command: Read Command Byte
    mouse_wait_read();
    status = inb(0x60);

    // 3. Set Controller Command Byte (Enable IRQ 12, Enable Mouse Clock)
    status |= 0x02;   // Bit 1: Enable IRQ 12 interrupt on PS/2 mouse input
    status &= ~0x20;  // Bit 5: Clear disable mouse bit
    mouse_wait_write();
    outb(0x64, 0x60); // Command: Write Command Byte
    mouse_wait_write();
    outb(0x60, status);

    // 4. Reset mouse to default state
    mouse_write_cmd(0xF6); // Set Default Settings
    mouse_read_data();     // Read Ack (0xFA)

    // 5. Enable Packet Streaming
    mouse_write_cmd(0xF4); // Enable packet streaming
    mouse_read_data();     // Read Ack (0xFA)

    mouse_cycle = 0;
}

/* mouse_handler: Parses the 3-byte mouse packets on IRQ 12 */
void mouse_handler(void) {
    uint8_t val = inb(0x60);

    // Send EOI to both Master and Slave PICs (IRQ 12 is on Slave PIC)
    outb(0xA0, 0x20); // Slave PIC EOI
    outb(0x20, 0x20); // Master PIC EOI

    // Alignment check: the first byte of a valid packet must have bit 3 set to 1
    if (mouse_cycle == 0 && !(val & 0x08)) {
        return; // Out of sync, discard byte
    }

    mouse_packet[mouse_cycle++] = val;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        // Parse buttons (bit 0 = left, bit 1 = right, bit 2 = middle)
        mouse_buttons = mouse_packet[0] & 0x07;

        // Parse X & Y deltas (with sign extension based on bits 4 and 5 of byte 0)
        int x_delta = mouse_packet[1];
        int y_delta = mouse_packet[2];

        if (mouse_packet[0] & 0x10) { // X sign bit
            x_delta -= 256;
        }
        if (mouse_packet[0] & 0x20) { // Y sign bit
            y_delta -= 256;
        }

        // Update mouse position
        mouse_x += x_delta;
        mouse_y -= y_delta; // Y axis is inverted in screen coordinates

        // Cap to screen boundaries (assuming 1024x768)
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= 1024) mouse_x = 1023;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= 768) mouse_y = 767;
    }
}

/* Getters */
int mouse_get_x(void) {
    return mouse_x;
}

int mouse_get_y(void) {
    return mouse_y;
}

uint8_t mouse_get_buttons(void) {
    return mouse_buttons;
}

/* Setters */
void mouse_set_position(int x, int y) {
    mouse_x = x;
    mouse_y = y;
}
