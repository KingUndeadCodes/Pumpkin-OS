nasm bootLoader.asm -f bin -o image.bin
qemu-system-x86_64 -drive format=raw,file=image.bin,if=floppy
rm image.bin

