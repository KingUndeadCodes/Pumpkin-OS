AS      =nasm
CC      =x86_64-elf-g++
LD      =x86_64-elf-ld
CFLAGS  =-fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include
LDFLAGS =-m elf_i386 -o kernel.bin -Ttext 0x7dfe

### REMOVE 'graphics.cpp' !!!

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
	$(CC) $(CFLAGS) mods/dev/mouse/mouse.cpp -o mouse.o
	$(CC) $(CFLAGS) mods/dev/paging/paging.cpp -o paging.o
	$(CC) $(CFLAGS) mods/std/stdlib.cpp -o stdlib.o
	$(CC) $(CFLAGS) mods/dev/audio/speaker.cpp -o speaker.o
	$(CC) $(CFLAGS) mods/dev/pit/pit.cpp -o timer.o
	$(CC) $(CFLAGS) mods/dev/pci/pci.cpp -o pci.o
	$(CC) $(CFLAGS) mods/dev/pci/drivers/rtl8139.cpp -o rtl8139.o
	$(CC) $(CFLAGS) mods/dev/serial/serial.cpp -o serial.o
	$(CC) $(CFLAGS) mods/dev/cmos/cmos.cpp -o cmos.o
	$(CC) $(CFLAGS) mods/std/time.cpp -o time.o
	$(CC) $(CFLAGS) mods/std/math.cpp -o math.o
	$(CC) $(CFLAGS) mods/std/tasking/tasking.cpp -o tasking.o
	$(CC) $(CFLAGS) mods/std/graphics.cpp -o graphics.o
	$(LD) $(LDFLAGS) Kernel-Entry.o graphics.o kernel.o text.o idt.o isr.o irq.o kb.o mouse.o string.o math.o speaker.o paging.o stdlib.o timer.o cmos.o pci.o rtl8139.o time.o serial.o tasking.o --oformat binary
	cat Pump.bin kernel.bin > short.bin
	cat short.bin empty_end.bin > image.bin
	cp image.bin image.img

# https://forum.osdev.org/viewtopic.php?f=1&t=26849
# https://github.com/szhou42/osdev/blob/master/qemu_run.sh
# https://forum.osdev.org/viewtopic.php?t=25157
# Network Device Flags:
# 		-netdev vmnet-shared,id=net
# 		-device rtl8139,mac=52:54:00:12:34:56,netdev=net

run:
	qemu-system-x86_64 \
		-m 1G \
		-netdev user,id=network0 \
		-device rtl8139,mac=52:54:00:12:34:56,netdev=network0 \
		-audiodev coreaudio,id=audio0 -machine pcspk-audiodev=audio0 \
		-drive format=raw,file=image.img,if=floppy \
		-device virtio-mouse \
		-serial stdio \
		-object filter-dump,id=network_filter_object,netdev=network0,file=dump.dat

clean:
	rm isr.o kb.o mouse.o Kernel-Entry.o kernel.bin Pump.bin kernel.o text.o idt.o irq.o empty_end.bin short.bin string.o speaker.o paging.o serial.o stdlib.o timer.o pci.o rtl8139.o cmos.o time.o tasking.o math.o graphics.o image.bin