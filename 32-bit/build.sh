# DEFENITIONS
comp=/usr/local/i386elfgcc/bin/./i386-elf-gcc
link=/usr/local/i386elfgcc/bin/./i386-elf-ld
qemu=qemu-system-x86_64

# ASSEMBLERS, COMPILERS AND LINKERS
${comp} -ffreestanding -c p-kernel.cpp -o kernel.o
nasm Kernel-Entry.asm -f elf -o Kernel-Entry.o
${link} -o kernel.bin -Ttext 0x1000 kernel.o Kernel-Entry.o --oformat binary
nasm bootLoader.asm -f bin -o Pump.bin
cat Pump.bin kernel.bin > image.bin

# VIRTUAL MACHINE AND DELETE TEMP FILES
rm Kernel-Entry.o kernel.bin Pump.bin kernel.o
${qemu} -drive format=raw,file=image.bin,if=floppy

