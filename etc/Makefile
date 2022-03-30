AS      =nasm
CC      =/usr/local/i386elfgcc/bin/i386-elf-g++
LD      =/usr/local/i386elfgcc/bin/i386-elf-ld
CFLAGS  =-fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -o2
LDFLAGS =-o kernel.bin -Ttext 0x1000

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
    $(CC) $(CFLAGS) mods/dev/kb.cpp -o kb.o
    $(LD) $(LDFLAGS) Kernel-Entry.o kernel.o text.o idt.o isr.o irq.o kb.o string.o --oformat binary
    cat Pump.bin kernel.bin > short.bin
    cat short.bin empty_end.bin > image.bin

clean:
    rm isr.o image.bin kb.o Kernel-Entry.o kernel.bin Pump.bin kernel.o text.o idt.o irq.o empty_end.bin short.bin string.o
