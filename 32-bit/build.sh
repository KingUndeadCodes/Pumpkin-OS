function build {
   clear
   echo -ne "\033[1;33mCompiling...\033[0m"
   /usr/local/i386elfgcc/bin/./i386-elf-gcc -ffreestanding -m32 -g -c -w p-kernel.cpp -o kernel.o
   nasm Kernel-Entry.asm -f elf -o Kernel-Entry.o
   nasm empty_end.asm -f bin -o empty_end.bin
   /usr/local/i386elfgcc/bin/./i386-elf-ld -o kernel.bin -Ttext 0x1000 kernel.o Kernel-Entry.o --oformat binary
   nasm bootLoader.asm -f bin -o Pump.bin
   cat Pump.bin kernel.bin > short.bin
   cat short.bin empty_end.bin > image.bin
   echo "   done"
   echo -ne "\033[1;33mCleaning up...\033[0m"
   rm Kernel-Entry.o kernel.bin Pump.bin kernel.o
   echo " done"
   echo -e "\033[1;32mCompiled successfully!\033[0m"
   echo -e "\033[1;33mRunning QEMU...\033[0m"
   qemu-system-i386 -drive format=raw,file=image.bin,if=floppy -vga std
   echo -ne "\033[1;33mCleaning up...\033[0m"
   rm image.bin empty_end.bin short.bin
   echo " done"
}

build
