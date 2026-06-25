/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Physical Memory Manager Implementation (AetherOS-64)
   ============================================================== */

#include "pmm.h"
#include "vga.h"

/* Multiboot2 specific structures */
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
};

/* Symbols defined in linker.ld representing kernel bounds */
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/* PMM State Variables */
static uint8_t* pmm_bitmap = 0;
static uint32_t total_blocks = 0;
static uint32_t free_blocks = 0;
static uint32_t bitmap_size = 0;

/* Bitmap Helper Functions */
static void pmm_set_bit(uint32_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static void pmm_clear_bit(uint32_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static int pmm_test_bit(uint32_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

/* pmm_init: Parses the Multiboot2 structure, sizes memory, and maps/reserves blocks */
void pmm_init(uint32_t mb_magic, uintptr_t mb_addr) {
    // 1. Assert magic validity
    if (mb_magic != 0x36d76289) {
        vga_print_string("PMM: Error - Invalid Multiboot2 magic number: 0x", vga_color_byte(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        vga_print_integer(mb_magic, 16, vga_color_byte(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        vga_print_newline();
        return;
    }

    // 2. Fetch the size of the Multiboot2 information structure (first 4 bytes)
    uint32_t mb_size = *(uint32_t*)mb_addr;

    // 3. Iterate through Multiboot2 tags to locate the memory map tag (type == 6)
    struct multiboot_tag* tag = (struct multiboot_tag*)(mb_addr + 8);
    struct multiboot_tag_mmap* mmap = 0;

    while (tag->type != 0) {
        if (tag->type == 6) {
            mmap = (struct multiboot_tag_mmap*)tag;
            break;
        }
        // Jump to next tag, aligned to 8 bytes
        tag = (struct multiboot_tag*)((uintptr_t)tag + ((tag->size + 7) & ~7));
    }

    if (mmap == 0) {
        vga_print_string("PMM: Error - Memory map tag not found!\n", vga_color_byte(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        return;
    }

    // 4. Scan the memory map to find the highest physical RAM address
    uint32_t num_entries = (mmap->size - sizeof(struct multiboot_tag_mmap)) / mmap->entry_size;
    uint64_t max_memory = 0;

    for (uint32_t i = 0; i < num_entries; i++) {
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)((uintptr_t)mmap->entries + i * mmap->entry_size);
        // type 1 = Usable RAM
        if (entry->type == 1) {
            uint64_t end_addr = entry->addr + entry->len;
            if (end_addr > max_memory) {
                max_memory = end_addr;
            }
        }
    }

    // 5. Calculate block counts and bitmap sizing
    // We identity-map the first 4GB, so cap memory detection at 4GB for safety
    if (max_memory > 4ULL * 1024 * 1024 * 1024) {
        max_memory = 4ULL * 1024 * 1024 * 1024;
    }

    total_blocks = (uint32_t)(max_memory / PMM_BLOCK_SIZE);
    bitmap_size = total_blocks / 8;

    // Place the bitmap aligned to 4KB page boundary after the kernel end
    pmm_bitmap = (uint8_t*)(((uintptr_t)_kernel_end + 4095) & ~4095);

    // Default: Mark all blocks as reserved (1 = reserved/used)
    for (uint32_t i = 0; i < bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    // 6. Free usable memory blocks in the bitmap
    for (uint32_t i = 0; i < num_entries; i++) {
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)((uintptr_t)mmap->entries + i * mmap->entry_size);
        if (entry->type == 1) {
            uint32_t start_block = (uint32_t)(entry->addr / PMM_BLOCK_SIZE);
            uint32_t num_blocks = (uint32_t)(entry->len / PMM_BLOCK_SIZE);
            for (uint32_t b = 0; b < num_blocks; b++) {
                uint32_t block_index = start_block + b;
                if (block_index < total_blocks) {
                    pmm_clear_bit(block_index);
                }
            }
        }
    }

    // 7. Explicitly protect/reserve essential system regions (set bits to 1)
    
    // A. First 1MB of memory (contains BIOS tables, page tables, VGA buffer)
    uint32_t end_1mb_block = 0x100000 / PMM_BLOCK_SIZE;
    for (uint32_t b = 0; b < end_1mb_block; b++) {
        pmm_set_bit(b);
    }

    // B. Kernel image and the PMM allocation bitmap
    uintptr_t kernel_start_phys = (uintptr_t)_kernel_start;
    uintptr_t bitmap_end_phys = (uintptr_t)pmm_bitmap + bitmap_size;
    uint32_t start_kernel_block = (uint32_t)(kernel_start_phys / PMM_BLOCK_SIZE);
    uint32_t num_kernel_blocks = (uint32_t)(((bitmap_end_phys - kernel_start_phys) + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE);
    for (uint32_t b = 0; b < num_kernel_blocks; b++) {
        pmm_set_bit(start_kernel_block + b);
    }

    // C. Multiboot2 information structure
    uintptr_t mb_start_phys = mb_addr;
    uintptr_t mb_end_phys = mb_addr + mb_size;
    uint32_t start_mb_block = (uint32_t)(mb_start_phys / PMM_BLOCK_SIZE);
    uint32_t num_mb_blocks = (uint32_t)(((mb_end_phys - mb_start_phys) + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE);
    for (uint32_t b = 0; b < num_mb_blocks; b++) {
        pmm_set_bit(start_mb_block + b);
    }

    // 8. Count the final free blocks
    free_blocks = 0;
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!pmm_test_bit(i)) {
            free_blocks++;
        }
    }

    // 9. Output PMM Initialization confirmation
    vga_print_string("PMM: Physical Memory Manager initialized successfully.\n", vga_color_byte(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    vga_print_string("PMM: Total Detected Blocks: ", vga_color_byte(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_print_integer(total_blocks, 10, vga_color_byte(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_print_string(" (", vga_color_byte(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_print_integer(total_blocks / 256, 10, vga_color_byte(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    vga_print_string(" MB)\n", vga_color_byte(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
}

/* pmm_alloc_block: Finds the first free bit, sets it to 1, and returns its physical address */
void* pmm_alloc_block(void) {
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!pmm_test_bit(i)) {
            pmm_set_bit(i);
            free_blocks--;
            return (void*)((uintptr_t)i * PMM_BLOCK_SIZE);
        }
    }
    return 0; // Out of memory
}

/* pmm_free_block: Clears the bit for a given address, returning the block to the free pool */
void pmm_free_block(void* addr) {
    uint32_t block = (uint32_t)((uintptr_t)addr / PMM_BLOCK_SIZE);
    if (block < total_blocks) {
        if (pmm_test_bit(block)) {
            pmm_clear_bit(block);
            free_blocks++;
        }
    }
}

/* API Getters for stats */
uint32_t pmm_get_total_blocks(void) {
    return total_blocks;
}

uint32_t pmm_get_free_blocks(void) {
    return free_blocks;
}

uint32_t pmm_get_used_blocks(void) {
    return total_blocks - free_blocks;
}
