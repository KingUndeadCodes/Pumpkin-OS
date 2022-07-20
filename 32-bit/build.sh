#!/bin/bash
function build {
    clear
    export PATH=$PATH:/usr/local/i386elfgcc/bin
    echo -e "\033[0;93mPumpkin OS installer.\033[0m"
    echo -ne "\033[1;33mCompiling...\033[0m"
    nasm bootLoader.asm -f bin -o Pump.bin
    nasm empty_end.asm -f bin -o empty_end.bin
    nasm Kernel-Entry.asm -f elf -o Kernel-Entry.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 p-kernel.cpp -o kernel.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/std/string.cpp -o string.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/std/text.cpp -o text.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/idt/idt.cpp -o idt.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/idt/isr.cpp -o isr.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/idt/irq.cpp -o irq.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/kb/kb.cpp -o kb.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/std/stdlib.cpp -o stdlib.o
    i386-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/audio/speaker.cpp -o speaker.o
    i386-elf-ld -o kernel.bin -Ttext 0x1000 Kernel-Entry.o kernel.o text.o idt.o isr.o irq.o kb.o string.o speaker.o stdlib.o --oformat binary
    cat Pump.bin kernel.bin > short.bin
    cat short.bin empty_end.bin > image.bin
    echo -e "   done"
    echo -e "\033[1;32mCompiled successfully!\033[0m"
    echo -e "\033[1;33mRunning QEMU...\033[0m"
    qemu-system-x86_64 -drive format=raw,file=image.bin,if=floppy -vga std
    # mkfs.ext2 -c image.bin
    # fdisk -u -l image.bin
    # strings image.bin
    # dd if=/dev/zero of=osimage_formated.bin bs=512 count=2880 >/dev/null
    # dd if=os_image.bin of=osimage_formated.bin conv=notrunc >/dev/null
    echo -ne "\033[1;33mCleaning up...\033[0m"
    rm isr.o image.bin kb.o Kernel-Entry.o kernel.bin Pump.bin kernel.o text.o idt.o irq.o empty_end.bin short.bin string.o speaker.o stdlib.o
    echo -e " done"
    echo -e "\033[1;32mFinished!\033[0m" | sudo tee /dev/kmsg
    return
}

build
