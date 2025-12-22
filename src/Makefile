AS = nasm
CC = x86_64-elf-g++
LD = x86_64-elf-ld
MAKE = make

CLIBPATH = ../../32-bit/mods/std/include/

CFLAGS = -fpermissive -ffreestanding -m32 -g -c -w -fno-exceptions -fno-rtti -I $(CLIBPATH)
LDFLAGS = -m elf_i386 -Ttext 0x8000

# https://github.com/nanobyte-dev/nanobyte_os/blob/videos/part3/Makefile

STAGE0 = stage0.bin
STAGE1 = stage1.bin
STAGE2 = stage2.bin
KERNEL = src/kernel/kernel.bin
IMAGE = image.bin # Must be a ".bin" binary file

all: $(IMAGE)

$(STAGE0): src/boot/stage0.asm
	$(AS) -f bin $< -Isrc/boot/include -o $@

$(STAGE1): src/boot/stage1.asm
	$(AS) -f bin $< -Isrc/boot/include -o $@

entry.o: src/boot/loader/entry.asm
	$(AS) -f elf $< -o $@

# (08/08/2025) NOTE: If things break, remove the `-Os` flag from the following targets.
# > main.o
# > floppy.o
# > libboot.o

main.o: src/boot/loader/main.cpp
	$(CC) $(CFLAGS) -Os -o $@ $<

floppy.o: src/boot/loader/floppy.cpp
	$(CC) $(CFLAGS) -Os -o $@ $<

libboot.o: src/boot/loader/libboot.cpp
	$(CC) $(CFLAGS) -Os -o $@ $<

$(KERNEL):
	$(MAKE) -C $(dir $(KERNEL))

$(STAGE2): entry.o main.o libboot.o floppy.o
	$(LD) $(LDFLAGS) entry.o libboot.o main.o floppy.o --oformat binary -o $@
	truncate -s 8192 $@

$(IMAGE): $(STAGE0) $(STAGE1) $(STAGE2) $(KERNEL)
	dd if=/dev/zero of=$(IMAGE) bs=512 count=2880
	mkfs.fat -F 12 -R 18 $(IMAGE)
	dd if=$(STAGE0) of=$(IMAGE) bs=512 seek=0 conv=notrunc
	dd if=$(STAGE1) of=$(IMAGE) bs=512 seek=1 conv=notrunc
	dd if=$(STAGE2) of=$(IMAGE) bs=512 seek=2 conv=notrunc
	mcopy -i $(IMAGE) $(KERNEL) "::kernel.bin"
	mcopy -i $(IMAGE) bin/test.wav "::test.wav"
	mcopy -i $(IMAGE) bin/oh.wav "::oh.wav"
	mcopy -i $(IMAGE) bin/main.elf "::main.elf"
	mcopy -i $(IMAGE) bin/test.txt "::test.txt"
# cat $(STAGE0) $(STAGE1) $(STAGE2) $(KERNEL) > $(IMAGE)

# mcopy -i $(IMAGE) oh.wav "::oh.wav"
# mcopy -i $(IMAGE) hey.wav "::hey.wav"

run: $(IMAGE)
	qemu-system-x86_64 \
		-m 1G \
		-netdev socket,id=network0,udp=127.0.0.1:12345,localaddr=127.0.0.1:12346 \
  		-device rtl8139,mac=52:54:00:12:34:56,netdev=network0 \
		-fda $(IMAGE) \
		-device virtio-mouse \
		-serial stdio \
		-object filter-dump,id=network_filter_object,netdev=network0,file=dump.dat \
		-audiodev coreaudio,id=snd0,out.frequency=48000,out.channels=2,out.format=s16 \
		-device AC97,audiodev=snd0

# clean: 
# 	rm -f $(filter-out $(IMAGE), $(wildcard *.bin)) *.o $(KERNEL)

clean: 
	rm -f *.bin *.o $(IMAGE) $(KERNEL)

.PHONY: all boot kernel link clean run
