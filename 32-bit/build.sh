function build {
   /usr/local/i386elfgcc/bin/./i386-elf-gcc -ffreestanding -c p-kernel.cpp -o kernel.o
   nasm Kernel-Entry.asm -f elf -o Kernel-Entry.o
   nasm empty_end.asm -f bin -o empty_end.bin
   /usr/local/i386elfgcc/bin/./i386-elf-ld -o kernel.bin -Ttext 0x1000 Kernel-Entry.o kernel.o --oformat binary
   nasm bootLoader.asm -f bin -o Pump.bin
   cat Pump.bin kernel.bin > short.bin
   cat short.bin empty_end.bin > image.bin
   rm Kernel-Entry.o kernel.bin Pump.bin kernel.o
   qemu-system-x86_64 -drive format=raw,file=image.bin,if=floppy
   rm image.bin empty_end.bin short.bin
}

build now
