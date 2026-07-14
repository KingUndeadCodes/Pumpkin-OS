# Pumpkin OS

A hobby x86 (32-bit protected mode) operating system built from a custom bootloader on up — no GRUB, no ring 3. Everything runs in ring 0, single address space, TempleOS-style, by design.

## Cloning

This repo uses a git submodule (minimp3), so clone with `--recurse-submodules`:

```sh
git clone --recurse-submodules https://www.github.com/KingUndeadCodes/Pumpkin-OS
```

If you already cloned without it, fetch the submodule with:

```sh
git submodule update --init --recursive
```

## Features

- **Custom bootloader** — 3-stage (`stage0`/`stage1`/`stage2`), reads a FAT12 floppy image and hands off to the kernel in protected mode.
- **Wingman** — a custom windowing/GUI framework: composited windows with z-order and click-to-focus, a message-box dialog widget with dynamic buttons, and a file explorer.
- **VESA/VBE graphics** — linear framebuffer graphics via BGA/VBE.
- **PS/2 keyboard and mouse.**
- **PCI drivers** — RTL8139 (NIC) and AC97 (audio codec).
- **Networking** — ARP, IP, UDP.
- **Storage** — a RAM-backed filesystem (RAMFS) behind a small VFS layer, with `fopen`/`fread`/`fwrite`-style stdio on top.
- **ELF loading and execution** — loads and runs `ET_REL`/relocatable ELF binaries off disk.
- **Audio playback** — WAV and MP3 (via the vendored `minimp3` submodule) playback through the AC97 codec.
- **Preemptive multitasking** — a round-robin scheduler exists (`mods/dev/tasking`) but is currently not enabled; see [docs/TODO.md](docs/TODO.md) for what's gating turning it on.
- **Fault handling** — recoverable vs. fatal exception handling, with register/stack-trace diagnostics over serial and a real panic screen for fatal faults.

## Building and running

Requires an `i686-elf`/`x86_64-elf` cross-compiler toolchain, `nasm`, `mtools` (`mcopy`), and `qemu-system-x86_64`. A Docker environment with the toolchain preconfigured is provided in [docker/Dockerfile](docker/Dockerfile).

```sh
cd src
make          # builds stage0/stage1/stage2 + kernel, assembles the floppy image
make run      # boots the image in QEMU (networking + AC97 audio + virtio mouse)
```

## Project layout

```
src/src/boot/     — bootloader (stage0/stage1 real-mode asm, stage2 protected-mode loader)
src/src/kernel/   — the kernel
  mods/core/      — Wingman GUI framework (windows, manager, widgets, file explorer)
  mods/dev/       — drivers and subsystems (PCI, VBE, IDT/ISR, networking, VFS, tasking, ...)
  mods/std/       — freestanding libc-ish support (stdlib, string, stdio, ...)
  mods/ports/     — vendored third-party code (minimp3, via git submodule)
```