/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Physical Memory Manager Header (AetherOS-64)
   ============================================================== */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>

/* Page frame size (4KB standard on x86_64) */
#define PMM_BLOCK_SIZE 4096

/* Public API Functions */
void pmm_init(uint32_t mb_magic, uintptr_t mb_addr);
void* pmm_alloc_block(void);
void pmm_free_block(void* addr);

uint32_t pmm_get_total_blocks(void);
uint32_t pmm_get_free_blocks(void);
uint32_t pmm_get_used_blocks(void);

#endif /* PMM_H */
