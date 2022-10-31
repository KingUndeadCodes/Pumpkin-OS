AS      =nasm
CC      =x86_64-elf-g++
LD      =x86_64-elf-ld
CFLAGS  =-fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include
LDFLAGS =-m elf_i386 -o kernel.bin -Ttext 0x1000

all: build clean

build:
	$(AS) bootLoader.asm -f bin -o Pump.bin
	$(AS) empty_end.asm -f bin -o empty_end.bin
	$(AS) Kernel-Entry.asm -f elf -o Kernel-Entry.o
	$(CC) $(CFLAGS) p-kernel.cpp -o kernel.o
	$(CC) $(CFLAGS) mods/std/string.cpp -o string.o
	$(CC) $(CFLAGS) mods/std/text.cpp -o text.o
	$(CC) $(CFLAGS) mods/dev/idt/idt.cpp -o idt.o
	$(CC) $(CFLAGS) mods/dev/idt/isr.cpp -o isr.o
	$(CC) $(CFLAGS) mods/dev/idt/irq.cpp -o irq.o
	$(CC) $(CFLAGS) mods/dev/kb/kb.cpp -o kb.o
	$(CC) $(CFLAGS) mods/dev/paging/paging.cpp -o paging.o
	$(CC) $(CFLAGS) mods/std/stdlib.cpp -o stdlib.o
	$(CC) $(CFLAGS) mods/dev/audio/speaker.cpp -o speaker.o
	$(CC) $(CFLAGS) mods/dev/pit/pit.cpp -o timer.o
	$(CC) $(CFLAGS) mods/dev/pci/pci.cpp -o pci.o
	$(CC) $(CFLAGS) mods/dev/cmos/cmos.cpp -o cmos.o
	$(CC) $(CFLAGS) mods/std/time.cpp -o time.o
	$(CC) $(CFLAGS) mods/std/tasking/tasking.cpp -o tasking.o
	$(CC) $(CFLAGS) mods/ports/sha256/sha256.c -o hash.o
	$(LD) $(LDFLAGS) Kernel-Entry.o kernel.o text.o idt.o isr.o irq.o kb.o string.o speaker.o paging.o stdlib.o timer.o pci.o cmos.o time.o tasking.o hash.o --oformat binary
	cat Pump.bin kernel.bin > short.bin
	cat short.bin empty_end.bin > image.bin

# mkdisk:
#	qemu-img create pumpkin.img 64M
# 	-vga std -serial stdio -d int

run:
	qemu-system-x86_64 -m 256m -device rtl8139,mac=52:54:00:12:34:56 -audiodev coreaudio,id=audio0 -machine pcspk-audiodev=audio0 -drive format=raw,file=image.bin,if=floppy

clean:
	rm isr.o kb.o Kernel-Entry.o kernel.bin Pump.bin kernel.o text.o idt.o irq.o empty_end.bin short.bin string.o speaker.o paging.o stdlib.o timer.o pci.o cmos.o time.o hash.o tasking.o