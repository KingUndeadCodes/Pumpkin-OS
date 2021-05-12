iamge.bin: short.bin empty_end.bin
	cat short.bin empty_end.bin > image.bin
short.bin: Pump.bin kernel.bin
	cat Pump.bin kernel.bin > short.bin
bootLoader.asm: Pump.bin
	nasm bootLoader.asm -f elf -o Pump.bin
kernel.bin: Kernel-Entry.o kernel.o
	/usr/local/i386elfgcc/bin/./i386-elf-ld -o kernel.bin -Ttext 0x1000 Kernel-Entry.o kernel.o --oformat binary
empty_end.bin: empty_end.asm
	nasm empty_end.asm -f elf -o empty_end.bin
Kernel-Entry.o: Kernel-Entry.asm
	nasm Kernel-Entry.asm -f elf -o Kernel-Entry.o
kernel.o: p-kernel.cpp
	/usr/local/i386elfgcc/bin/./i386-elf-gcc -ffreestanding -c p-kernel.cpp -o kernel.o
clean:
	rm Kernel-Entry.o kernel.bin Pump.bin kernel.o empty_end.bin short.bin
