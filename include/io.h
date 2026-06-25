/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Hardware Port I/O Helpers (AetherOS-64)
   ============================================================== */

#ifndef IO_H
#define IO_H

#include <stdint.h>

/* outb: Writes an 8-bit byte value to a specified I/O port.
   - port: The 16-bit hardware port address (e.g., 0x3D4 for VGA index register)
   - val: The 8-bit data value to write */
static inline void outb(uint16_t port, uint8_t val) {
    // "outb %0, %1" sends the register contents of eax/al (%0) to I/O port (%1)
    // "a" specifies using the AL/EAX register for value
    // "Nd" specifies that the port can be a constant or in DX register
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* inb: Reads an 8-bit byte value from a specified I/O port.
   - port: The 16-bit hardware port address
   - returns: The 8-bit value read from the port */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    // "inb %1, %0" reads from I/O port (%1) into register AL/EAX (%0)
    // "=a" specifies storing the result from AL/EAX into variable 'ret'
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif /* IO_H */
