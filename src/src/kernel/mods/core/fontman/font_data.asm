[bits 32]
section .rodata

; Embeds the vendored TTF font file directly into the kernel binary's
; .rodata section. See docs/DOCS.md ("Font Rendering System") for why
; this is incbin rather than a generated C byte array, and why there's
; deliberately no separate size label (read the size in C++ via pointer
; subtraction of the start/end symbols instead).
;
; incbin's path is resolved relative to nasm's CWD (the Makefile invokes
; it from src/src/kernel/), NOT relative to this .asm file's own
; location -- confirmed empirically, see docs/DOCS.md. The font asset
; itself lives in src/bin/ alongside this project's other shipped binary
; assets (test.mp3, main.elf, etc.), not under mods/ -- it's data, not
; kernel source, even though (unlike those other src/bin/ files) it's
; embedded directly into the kernel binary at build time rather than
; mcopy'd onto the floppy image as its own file.
global _binary_font_ttf_start
global _binary_font_ttf_end

_binary_font_ttf_start:
    incbin "../../bin/fonts/Cousine-Regular.ttf"
_binary_font_ttf_end:
