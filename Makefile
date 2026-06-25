# ==============================================================
# AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
# Makefile for Milestone 8: Project Structure + ISO (AetherOS-64)
# ==============================================================

ASM = nasm
CC = gcc
LD = ld

# Compiler flags for x86_64 freestanding C kernel
# -Iinclude: Include search path for our header files
CFLAGS = -m64 -ffreestanding -O2 -mno-red-zone -mno-sse -mno-sse2 -Iinclude -c

# Linker flags
# We link all object files together into a standard 64-bit ELF executable
LDFLAGS = -m elf_x86_64 -T linker.ld

all: AetherOS.iso

# Build directory creation target
build_dirs:
	mkdir -p build
	mkdir -p isodir/boot/grub

# Compile NASM bootloader object (64-bit ELF)
build/boot.o: boot/boot.asm | build_dirs
	$(ASM) -f elf64 boot/boot.asm -o build/boot.o

# Compile C kernel to an object file
build/kernel.o: kernel/kernel.c include/vga.h include/splash.h include/idt.h include/shell.h include/pmm.h include/pit.h include/task.h | build_dirs
	$(CC) $(CFLAGS) kernel/kernel.c -o build/kernel.o

# Compile VGA screen driver to an object file
build/vga.o: drivers/vga.c include/vga.h include/io.h | build_dirs
	$(CC) $(CFLAGS) drivers/vga.c -o build/vga.o

# Compile Splash screen driver to an object file
build/splash.o: boot/splash.c include/splash.h include/vga.h include/io.h | build_dirs
	$(CC) $(CFLAGS) boot/splash.c -o build/splash.o

# Compile Interrupt Descriptor Table (IDT) to an object file
build/idt.o: kernel/idt.c include/idt.h include/io.h include/vga.h | build_dirs
	$(CC) $(CFLAGS) kernel/idt.c -o build/idt.o

# Compile Keyboard driver to an object file
build/keyboard.o: drivers/keyboard.c include/keyboard.h include/vga.h include/io.h include/shell.h | build_dirs
	$(CC) $(CFLAGS) drivers/keyboard.c -o build/keyboard.o

# Compile Physical Memory Manager to an object file
build/pmm.o: kernel/pmm.c include/pmm.h include/vga.h | build_dirs
	$(CC) $(CFLAGS) kernel/pmm.c -o build/pmm.o

# Compile PIT Timer Driver to an object file
build/pit.o: kernel/pit.c include/pit.h include/io.h | build_dirs
	$(CC) $(CFLAGS) kernel/pit.c -o build/pit.o

# Compile Task Scheduler to an object file
build/task.o: kernel/task.c include/task.h include/pmm.h include/pit.h | build_dirs
	$(CC) $(CFLAGS) kernel/task.c -o build/task.o

# Compile Shell v1 to an object file
build/shell.o: kernel/shell.c include/shell.h include/vga.h | build_dirs
	$(CC) $(CFLAGS) kernel/shell.c -o build/shell.o

# Compile Assembly ISR wrapper object
build/isr.o: kernel/isr.asm | build_dirs
	$(ASM) -f elf64 kernel/isr.asm -o build/isr.o

# Compile VESA graphics driver to an object file
build/vesa.o: drivers/vesa.c include/vesa.h include/font.h | build_dirs
	$(CC) $(CFLAGS) drivers/vesa.c -o build/vesa.o

# Compile Mouse driver to an object file
build/mouse.o: drivers/mouse.c include/mouse.h include/io.h | build_dirs
	$(CC) $(CFLAGS) drivers/mouse.c -o build/mouse.o

# Compile GUI to an object file
build/gui.o: drivers/gui.c include/gui.h include/vesa.h include/mouse.h include/pmm.h include/pit.h include/task.h | build_dirs
	$(CC) $(CFLAGS) drivers/gui.c -o build/gui.o

# Link all object files into the 64-bit ELF kernel binary
build/aetheros.bin: build/boot.o build/kernel.o build/vga.o build/splash.o build/idt.o build/keyboard.o build/pmm.o build/pit.o build/task.o build/shell.o build/isr.o build/vesa.o build/mouse.o build/gui.o
	$(LD) $(LDFLAGS) $^ -o $@

# Build the rescue ISO directory and run grub-mkrescue to package the CD-ROM ISO
AetherOS.iso: build/aetheros.bin isodir/boot/grub/grub.cfg
	# Copy the compiled ELF kernel binary into the staging directory
	cp build/aetheros.bin isodir/boot/aetheros.bin
	# Generate the bootable ISO file via GRUB2 utility
	grub-mkrescue -o AetherOS.iso isodir

# Clean build artifacts
clean:
	rm -rf build
	rm -f isodir/boot/aetheros.bin
	rm -f AetherOS.iso
