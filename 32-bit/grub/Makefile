AS    	=i686-elf-as
CC      =i686-elf-g++
LD      =i686-elf-gcc
CFLAGS  =-fpermissive -ffreestanding -O2 -w -fno-exceptions -fno-rtti -I src/mods/std/include
LDFLAGS =-T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib

all: build clean 

# When running in docker, make sure the working dirrectory is 32-bit.

build:
	cp -r ../mods src/mods
	nasm src/boot.asm -f elf -o entry.o
	$(AS) src/boot.s -o boot.o
	$(CC) -c src/main.cpp $(CFLAGS) -o kernel.o
	$(CC) -c src/mods/std/text.cpp $(CFLAGS) -o text.o
	$(CC) -c src/mods/std/string.cpp $(CFLAGS) -o string.o
	$(CC) -c src/mods/dev/idt/idt.cpp $(CFLAGS) -o idt.o
	$(CC) -c src/mods/dev/idt/isr.cpp $(CFLAGS) -o isr.o
	$(CC) -c src/mods/dev/idt/irq.cpp $(CFLAGS) -o irq.o
	$(CC) -c src/mods/dev/kb/kb.cpp $(CFLAGS) -o kb.o
	$(CC) -c src/mods/dev/mouse/mouse.cpp $(CFLAGS) -o mouse.o
	$(CC) -c src/mods/dev/serial/serial.cpp $(CFLAGS) -o serial.o
	$(CC) -c src/mods/dev/paging/paging.cpp $(CFLAGS) -o paging.o
	$(CC) -c src/mods/std/stdlib.cpp $(CFLAGS) -o stdlib.o
	$(CC) -c src/mods/dev/audio/speaker.cpp $(CFLAGS) -o speaker.o
	$(CC) -c src/mods/dev/pit/pit.cpp $(CFLAGS) -o timer.o
	$(CC) -c src/mods/dev/pci/pci.cpp $(CFLAGS) -o pci.o
	$(CC) -c src/mods/dev/pci/drivers/rtl8139.cpp $(CFLAGS) -o rtl8139.o
	$(CC) -c src/mods/dev/serial/serial.cpp $(CFLAGS) -o serial.o
	$(CC) -c src/mods/dev/cmos/cmos.cpp $(CFLAGS) -o cmos.o
	$(CC) -c src/mods/std/time.cpp $(CFLAGS) -o time.o
	$(CC) -c src/mods/std/math.cpp $(CFLAGS) -o math.o
	$(CC) -c src/mods/std/tasking/tasking.cpp $(CFLAGS) -o tasking.o
	$(LD) $(LDFLAGS) boot.o entry.o kernel.o text.o idt.o isr.o irq.o kb.o mouse.o string.o math.o speaker.o paging.o stdlib.o timer.o cmos.o pci.o rtl8139.o time.o serial.o tasking.o -lgcc
	mkdir -p isodir/boot/grub
	rm -rf src/mods
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

run:
	qemu-system-x86_64 \
		-cdrom myos.iso \
		-m 1G \
		-netdev user,id=network0 \
		-device rtl8139,mac=52:54:00:12:34:56,netdev=network0 \
		-audiodev coreaudio,id=audio0 -machine pcspk-audiodev=audio0 \
		-serial stdio
clean:
	rm *.o
	rm *.bin
	rm -rf isodir