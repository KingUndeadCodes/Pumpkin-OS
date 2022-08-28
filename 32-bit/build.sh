#!/bin/bash
function build {
    clear
    echo -e "\033[0;93mPumpkin OS installer.\033[0m"
    echo -ne "\033[1;33mCompiling...\033[0m"
    nasm bootLoader.asm -f bin -o Pump.bin
    nasm empty_end.asm -f bin -o empty_end.bin
    nasm Kernel-Entry.asm -f elf -o Kernel-Entry.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 p-kernel.cpp -o kernel.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/std/string.cpp -o string.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/std/text.cpp -o text.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/idt/idt.cpp -o idt.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/idt/isr.cpp -o isr.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/idt/irq.cpp -o irq.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/kb/kb.cpp -o kb.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/std/stdlib.cpp -o stdlib.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/audio/speaker.cpp -o speaker.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/pit/pit.cpp -o timer.o
    x86_64-elf-g++ -fpermissive -ffreestanding -m32 -g -c -w -I mods/std/include -O2 mods/dev/pci/pci.cpp -o pci.o
    x86_64-elf-ld -m elf_i386 -o kernel.bin -Ttext 0x1000 Kernel-Entry.o kernel.o text.o idt.o isr.o irq.o kb.o string.o speaker.o stdlib.o timer.o pci.o --oformat binary
    cat Pump.bin kernel.bin > short.bin
    cat short.bin empty_end.bin > image.bin
    echo -e "   done"
    echo -e "\033[1;32mCompiled successfully!\033[0m"
    echo -e "\033[1;33mRunning QEMU...\033[0m"
    qemu-system-x86_64 -drive format=raw,file=image.bin,if=floppy -vga std
    echo -ne "\033[1;33mCleaning up...\033[0m"
    rm isr.o image.bin kb.o Kernel-Entry.o kernel.bin Pump.bin kernel.o text.o idt.o irq.o empty_end.bin short.bin string.o speaker.o stdlib.o timer.o pci.o
    echo -e " done"
    echo -e "\033[1;32mFinished!\033[0m"
    return
}

build
