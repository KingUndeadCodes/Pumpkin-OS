AS      =nasm
CC      =x86_64-elf-g++
LD      =x86_64-elf-ld
CFLAGS  =-fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -fno-exceptions -fno-rtti -mno-sse -mno-sse2 -mno-mmx -mfpmath=387 # -lrt -fno-pic -fno-pie
LDFLAGS =-m elf_i386 -Tkernel.ld -o kernel.bin
LDFLAGS_TAIL = --oformat binary

all: build clean

# 	$(CC) $(CFLAGS) mods/core/explorer/explorer.cpp -o explorer.o

build:
	$(AS) Kernel-Entry.asm -f elf -o Kernel-Entry.o
	$(CC) $(CFLAGS) p-kernel.cpp -o kernel.o
	$(CC) $(CFLAGS) mods/std/string.cpp -o string.o
	$(CC) $(CFLAGS) mods/std/ctype.cpp -o ctype.o
	$(CC) $(CFLAGS) mods/std/stdlib.cpp -o stdlib.o
	$(CC) $(CFLAGS) mods/std/time.cpp -o time.o
	$(CC) $(CFLAGS) mods/std/math.cpp -o math.o
	$(CC) $(CFLAGS) mods/std/logging.cpp -o logging.o
	$(CC) $(CFLAGS) mods/dev/tasking/tasking.cpp -o tasking.o
	$(CC) $(CFLAGS) mods/std/graphics.cpp -o graphics.o
	$(CC) $(CFLAGS) mods/std/stdio.cpp -o stdio.o
	$(CC) $(CFLAGS) mods/dev/idt/idt.cpp -o idt.o
	$(CC) $(CFLAGS) mods/dev/idt/isr.cpp -o isr.o
	$(CC) $(CFLAGS) mods/dev/idt/irq.cpp -o irq.o
	$(CC) $(CFLAGS) mods/dev/kb/kb.cpp -o kb.o
	$(CC) $(CFLAGS) mods/dev/mouse/mouse.cpp -o mouse.o
	$(CC) $(CFLAGS) mods/dev/paging/paging.cpp -o paging.o
	$(CC) $(CFLAGS) mods/dev/paging/pat/pat.cpp -o pat.o
	$(CC) $(CFLAGS) mods/dev/pit/pit.cpp -o timer.o
	$(CC) $(CFLAGS) mods/dev/pci/pci.cpp -o pci.o
	$(CC) $(CFLAGS) mods/dev/pci/drivers/rtl8139.cpp -o rtl8139.o
	$(CC) $(CFLAGS) mods/dev/pci/drivers/ac97.cpp -o ac97.o
	$(CC) $(CFLAGS) mods/dev/serial/serial.cpp -o serial.o
	$(CC) $(CFLAGS) mods/dev/cmos/cmos.cpp -o cmos.o
	$(CC) $(CFLAGS) mods/dev/vbe/vbe.cpp -o vbe.o
	$(CC) $(CFLAGS) mods/dev/ramfs/ramfs.cpp -o ramfs.o
	$(CC) $(CFLAGS) mods/dev/memory/memory.cpp -o memory.o
	$(CC) $(CFLAGS) mods/dev/memory/allocator.cpp -o allocator.o
	$(CC) $(CFLAGS) mods/dev/syscall/syscall.cpp -o syscall.o
	$(CC) $(CFLAGS) mods/dev/net/arp.cpp -o arp.o
	$(CC) $(CFLAGS) mods/dev/net/ip.cpp -o ip.o
	$(CC) $(CFLAGS) mods/dev/vfs/vfs.cpp -o vfs.o
	$(CC) $(CFLAGS) mods/dev/chorus/chorus.cpp -o chorus.o
	$(CC) $(CFLAGS) mods/dev/chorus/wav.cpp -o wav.o
	$(CC) $(CFLAGS) mods/dev/chorus/minimp3/minimp3.cpp -o minimp3.o
	$(CC) $(CFLAGS) mods/dev/chorus/mp3.cpp -o mp3.o
	$(CC) $(CFLAGS) mods/dev/elf/elf.cpp -o elf.o
	$(CC) $(CFLAGS) mods/core/wingman/suite/explorer/explorer.cpp -o explorerWingman.o
	$(CC) $(CFLAGS) mods/core/wingman/suite/message/message.cpp -o messageWingman.o
	$(CC) $(CFLAGS) mods/core/wingman/window.cpp -o window.o
	$(CC) $(CFLAGS) mods/core/wingman/surface.cpp -o surface.o
	$(CC) $(CFLAGS) mods/core/wingman/manager.cpp -o manager.o
	$(CC) $(CFLAGS) mods/core/wingman/wingman.cpp -o wingman.o
	$(CC) $(CFLAGS) mods/core/wingman/cursor.cpp -o cursor.o
	$(LD) $(LDFLAGS) Kernel-Entry.o kernel.o graphics.o idt.o isr.o irq.o kb.o mouse.o ctype.o string.o math.o memory.o allocator.o vfs.o paging.o pat.o stdlib.o timer.o cmos.o pci.o rtl8139.o ac97.o time.o serial.o logging.o tasking.o vbe.o stdio.o syscall.o arp.o ip.o wav.o chorus.o minimp3.o mp3.o elf.o ramfs.o window.o surface.o manager.o wingman.o cursor.o explorerWingman.o messageWingman.o $(LDFLAGS_TAIL)
# truncate -s 131072 kernel.bin
# cat kernel.bin empty_end.bin > kernel2.bin
# mv kernel2.bin kernel.bin

# https://forum.osdev.org/viewtopic.php?f=1&t=26849
# https://github.com/szhou42/osdev/blob/master/qemu_run.sh
# https://forum.osdev.org/viewtopic.php?t=25157

clean:
	rm chorus.o wav.o minimp3.o mp3.o ip.o arp.o allocator.o pat.o isr.o kb.o mouse.o vfs.o memory.o logging.o Kernel-Entry.o kernel.o idt.o irq.o ctype.o string.o paging.o serial.o stdlib.o timer.o pci.o rtl8139.o ac97.o cmos.o time.o tasking.o math.o graphics.o vbe.o ramfs.o stdio.o elf.o syscall.o window.o surface.o manager.o cursor.o explorerWingman.o messageWingman.o wingman.o # image.bin
#  kernel.bin short.bin 