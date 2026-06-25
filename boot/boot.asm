; ==============================================================
; AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
; YouTube Series: "I Built My Own Operating System From Scratch"
; Milestone 8: GRUB Multiboot2 Entry & 64-bit Handoff (AetherOS-64)
; ==============================================================

[bits 32]               ; GRUB boots the CPU into 32-bit Protected Mode

; ==============================================================
; MULTIBOOT2 HEADER (Must be within the first 8192 bytes of ELF, aligned to 8 bytes)
; ==============================================================
section .multiboot_header
align 8
multiboot_header_start:
    dd 0xe85250d6       ; Magic number (Multiboot2 identifier)
    dd 0                ; Architecture (0 = 32-bit Protected Mode i386)
    dd multiboot_header_end - multiboot_header_start ; Header length
    ; Checksum: magic + arch + length + checksum must equal 0
    dd 0x100000000 - (0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))

    ; Framebuffer tag request (type = 5, size = 20)
    align 8
    dw 5                ; Type = 5
    dw 0                ; Flags (0 = required)
    dd 20               ; Size = 20
    dd 1024             ; Width
    dd 768              ; Height
    dd 32               ; Depth (32-bit color)

    ; End tag (required by Multiboot2 specification)
    align 8
    dw 0                ; Type
    dw 0                ; Flags
    dd 8                ; Size
multiboot_header_end:

; ==============================================================
; KERNEL ENTRY POINT
; ==============================================================
section .text
global _start
extern kernel_main
extern _bss_start
extern _bss_end

_start:
    ; Save magic number (eax) in ESI before it is overwritten
    mov esi, eax

    ; Clear BSS section (from _bss_start to _bss_end)
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; Initialize the stack pointer to our custom stack area in .bss
    mov esp, stack_top

    ; 1. Set Up 64-bit Page Tables (Identity Map 0 - 4GB)
    ; Clear 24KB of page table area (from physical address 0x1000 to 0x6FFF)
    mov edi, 0x1000     ; Destination address (PML4 base)
    xor eax, eax        ; Value to write (0)
    mov ecx, 6144       ; 6144 double-words = 24 KB
    rep stosd           ; Write zeros

    ; Set up PML4 table (at 0x1000) pointing to PDPT (at 0x2000)
    mov edi, 0x1000
    mov dword [edi], 0x2003      ; 0x2000 (PDPT base) | 3 (Present + Read/Write flags)

    ; Set up PDPT table (at 0x2000) pointing to PDs (at 0x3000, 0x4000, 0x5000, 0x6000)
    mov edi, 0x2000
    mov dword [edi], 0x3003      ; 0-1GB: PD at 0x3000 | 3
    mov dword [edi + 8], 0x4003  ; 1-2GB: PD at 0x4000 | 3
    mov dword [edi + 16], 0x5003 ; 2-3GB: PD at 0x5000 | 3
    mov dword [edi + 24], 0x6003 ; 3-4GB: PD at 0x6000 | 3

    ; Set up PD tables starting at 0x3000 pointing directly to physical memory addresses
    ; Using 2MB huge pages, we map 2048 entries to cover the first 4GB of physical RAM
    mov edi, 0x3000              ; PD base address
    mov eax, 0x00000083          ; Base address 0 | flags (Present + Read/Write + Page Size 2MB)
    mov ecx, 2048                ; 2048 entries
.map_pd:
    mov [edi], eax
    add edi, 8                   ; Each entry is 8 bytes
    add eax, 0x200000            ; Advance address by 2MB
    loop .map_pd

    ; Load Page Directory Base (PML4 address) into CR3 register
    mov eax, 0x1000
    mov cr3, eax

    ; 2. Enable Physical Address Extension (PAE) in CR4
    mov eax, cr4
    or eax, 1 << 5      ; Set PAE bit (bit 5)
    mov cr4, eax

    ; 3. Enable Long Mode in EFER MSR
    mov ecx, 0xC0000080 ; EFER MSR index
    rdmsr               ; Read MSR into EDX:EAX
    or eax, 1 << 8      ; Set Long Mode Enable (LME) bit (bit 8)
    wrmsr               ; Write back to EFER MSR

    ; 4. Enable Paging (PG) in CR0
    mov eax, cr0
    or eax, 1 << 31     ; Set Paging Enable (PG) bit (bit 31)
    mov cr0, eax        ; Paging is now active! The CPU is in Compatibility Mode.

    ; 5. Load 64-bit GDT and jump to 64-bit Long Mode
    lgdt [gdt_descriptor] ; Load GDT pointer
    jmp CODE_SEG_64:init_lm ; Far jump to clear pipeline and enter Long Mode

; ==============================================================
; 64-BIT LONG MODE
; ==============================================================
[bits 64]
init_lm:
    ; Clean segment registers (unused in 64-bit mode, set to 0 for safety)
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 6. Call C Kernel entry point with Multiboot parameters:
    ; magic in edi (from esi), addr in esi (from ebx)
    mov edi, esi
    mov esi, ebx
    call kernel_main

    ; Halt the CPU if the kernel returns
.halt:
    hlt
    jmp .halt

; ==============================================================
; GLOBAL DESCRIPTOR TABLE (GDT)
; ==============================================================
align 4
gdt_start:
    ; Null Descriptor
    dq 0

    ; 64-bit Code Segment Descriptor (Privilege Level 0, Code Exec/Read)
    dw 0                ; Limit (ignored)
    dw 0                ; Base low (ignored)
    db 0                ; Base middle (ignored)
    db 0x9a             ; Access Byte: Present, Ring 0, Code, Exec/Read
    db 0x20             ; Flags: Long Mode Code (L=1, D=0)
    db 0                ; Base high (ignored)

    ; 64-bit Data Segment Descriptor (Privilege Level 0, Data Read/Write)
    dw 0                ; Limit (ignored)
    dw 0                ; Base low (ignored)
    db 0                ; Base middle (ignored)
    db 0x92             ; Access Byte: Present, Ring 0, Data, Read/Write
    db 0                ; Flags
    db 0                ; Base high (ignored)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT Limit
    dd gdt_start                ; GDT Base Address

; Segment Selectors
CODE_SEG_64 equ 0x08
DATA_SEG_64 equ 0x10

; ==============================================================
; KERNEL STACK AREA
; ==============================================================
section .bss
align 16
stack_bottom:
    resb 16384          ; Reserve 16 KB stack space for the kernel
stack_top:
