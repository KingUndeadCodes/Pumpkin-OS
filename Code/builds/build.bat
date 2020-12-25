nasm bootLoader.asm -f bin -o Pump.img
qemu-system-x86_64 Pump.img
del Pump.img
