/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Programmable Interval Timer (PIT) Driver (AetherOS-64)
   ============================================================== */

#include "pit.h"
#include "io.h"

static volatile uint64_t system_ticks = 0;

/* pit_init: Sets Channel 0 of PIT to Mode 3 (Square Wave) at the given frequency.
   The base frequency of the PIT oscillator is 1193182 Hz. */
void pit_init(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;

    // Send control byte (0x36) to PIT Command Register (0x43)
    // 0x36 = 00110110: Channel 0, Access mode lobyte/hibyte, Mode 3, Binary count
    outb(0x43, 0x36);

    // Send divisor to Channel 0 Data Register (0x40) - Low byte first, then high byte
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

/* pit_get_ticks: Returns the current tick count */
uint64_t pit_get_ticks(void) {
    return system_ticks;
}

/* pit_increment_ticks: Increments ticks, called by scheduler on every tick */
void pit_increment_ticks(void) {
    system_ticks++;
}
