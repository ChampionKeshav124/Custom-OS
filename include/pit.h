/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Programmable Interval Timer (PIT) Header (AetherOS-64)
   ============================================================== */

#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/* pit_init: Initializes the PIT Channel 0 to generate interrupts at frequency */
void pit_init(uint32_t frequency);

/* pit_get_ticks: Returns the current system ticks since boot */
uint64_t pit_get_ticks(void);

/* pit_increment_ticks: Increments the system tick count */
void pit_increment_ticks(void);

#endif /* PIT_H */
