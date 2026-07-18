# Pumpkin-OS — TODO

Standing checklist of things to add/fix, ordered by recommended priority
(top = do next). Ring-0-only is an intentional design choice (like
TempleOS), not something any of these notes are trying to walk back.

---

## Priority 1 — Do next

Small, self-contained, no dependency on tasking. Ordered by recommended
sequence.

### 1. Fix the AC97/chorus playback race — DONE

`mods/dev/pci/drivers/ac97.cpp`, `mods/dev/chorus/chorus.cpp` —
`ac97_refill_fragment()` and `ac97_complete_descriptor()` (IRQ path) now
guard their whole bodies with `enter_critical()`/`exit_critical()`, and
`play_sound_with_refilling_buffer()` (mainline) guards its
`sound_buffer_refilling_info` field-init block plus the fragment-preload
loop's `last_filled_buffer` write (kept separate from the `fill_buffer()`
callback itself, which does real decode work and stays unguarded so
interrupts aren't held off for its whole duration). Kept intentionally
minimal per this item's own original scoping; a bigger fix would also
reorder `AC97StopPlayback()` to run before the mainline mutations instead
of after, shrinking the race window further — noted in DOCS.md, not done
here.

### 2. Distinguish recoverable vs. fatal exceptions in the fault handler — DONE

### 3. Finish wiring up MessageBox — DONE

### 4. Validate ELF header offsets/counts before trusting them — DONE

### 5. Fix the hardcoded WAV buffer size in the file explorer — DONE

### 6. Fix `init_phys_allocator()` skipping usable memory regions — DONE

`mods/dev/memory/memory.cpp` (`queryMemoryMap`) — was calling
`init_phys_allocator()` from inside the SMAP-entry loop, gated on
`baseCount++ == 1`, so only the *second* usable region was ever passed
in (alone), skipping the first entirely and ignoring any past the
second. Fixed by collecting every usable region into a fixed-size stack
array (`MAX_USABLE_REGIONS = 32`, a stack array since this runs before
either the heap or the physical allocator exists) while iterating, then
calling `init_phys_allocator()` once after the loop with the real count
— using the interface it already had, just never was given more than
one region through. Also fixed a small adjacent bug while in there:
the "usable memory" log line was passing `INFO` as the `bool time_show`
positional argument instead of `false, INFO` like the other three
branches.

### 7. Fix the RTL8139 TX-wait operator-precedence bug — DONE

`mods/dev/pci/drivers/rtl8139.cpp` (`RTL8139_SEND_PACKET`) —
`while (transmit_ok & (1 << 15) == 0)` had `==` binding tighter than `&`,
so it always evaluated to `transmit_ok & 0` (always false); the loop
body never ran, so "wait for TX complete" was a no-op. Fixed to
`while ((transmit_ok & (1 << 15)) == 0)`. Since the loop had never
actually executed before, its `Logging::log(...)` call (real
`fopen`/`fread`/`malloc`/`fwrite`/`fclose` file I/O — `Logging::capturing`
is `true` from boot onward) had never actually fired either; activating
the loop without addressing that would've turned a slow/stuck TX into a
filesystem-I/O spin loop. Moved the log call to fire once, only if a
wait is actually needed, instead of once per poll iteration.

### 8. Null-check `node` in `fseek()` — DONE

`mods/std/stdio.cpp` `fseek()` — added `if (!node) return -1;` right
after fetching `file_table[fd].node`, matching the check `fread`/`fwrite`
already had in the same file. `fseek()` on a bad or already-closed fd now
fails gracefully instead of null-dereferencing `node->size`.

### 9. Fix `Logging::log()`'s missing NUL-termination — DONE

`mods/dev/logging/logging.cpp` (was `mods/std/logging.cpp` at the time —
see `docs/DOCS.md` for the later relocation) — both `Logging::log()` and `Logging::flush()`
(same bug, one function apart — `flush()` hands its own unterminated
`fread()` buffer straight to `log()`) read into a `malloc`'d buffer and
treated it as a NUL-terminated string without `fread()` ever adding a
NUL and without the buffer being zeroed first. Fixed by allocating one
extra byte, capturing `fread()`'s actual return value, and
NUL-terminating at exactly that offset instead of relying on `strlen()`
to find where the real data ends. Also fixed a latent empty-file
underflow in `log()`'s "does the file already end in a newline" check
(`strlen(buffer) - 1` on an empty read wrapped `size_t` to `SIZE_MAX`;
replaced with a `bytes_read > 0` guard).

### 10. Fix missing newlines in early boot log messages — DONE

Found via a screenshot of the on-screen `Terminal` right before the
Wingman switch: every early boot message ("Interupts Enabled!", "Paging
Enabled!", "Memory pool initialized!", "Keyboard Enabled!", "Mouse
Enabled!", "PIT Enabled!", "Checking for PCI devices...", each "Found
PCI ..." line) ran together on one line with no separator at all, since
none of the `Logging::log(...)` calls that produce them
(`p-kernel.cpp:255-264`, `mods/dev/pci/pci.cpp:238`'s `sprintf` format
string) included a trailing `\n`. `Terminal::write()` itself handles
`\n` correctly (confirmed by tracing it and by the "Welcome to
PumpkinOS" banner, which does include `\n` and rendered on its own
lines) -- the messages themselves were just missing it. Added `\n` to
each. Also fixed the same issue in `mods/dev/pci/drivers/rtl8139.cpp`'s
`Logging::log("[RTL8139] Waiting for transmit_ok ...")` call from item
7, found while checking for other call sites with the same bug.
`p-kernel.cpp:227`'s `Logging::log("Tasking Enabled!")` has the same
issue but is inside a commented-out/disabled block, so left alone.

### 11. Fix the `serialDevice` `LogDevice` function-pointer type mismatch

Found while chasing item 10's Terminal bug, not yet fixed.
`p-kernel.cpp`'s `LogDevice serialDevice = { .log = &serial_write_string };`
assigns a 3-parameter function (`const char*, bool, enum Types`) to a
field typed `void (*log)(const char* message)` — default arguments don't
change a function's pointer type, so this is a genuine signature
mismatch. It compiles silently only because of `-fpermissive -w` in the
build flags. Every `Logging::propagate()` call ends up invoking
`serial_write_string` through a 1-argument-shaped call site, so the
function's own body reads its 2nd/3rd parameters (`time_show`, `Type`)
as whatever garbage happens to be on the stack at those offsets — a real
ABI violation. `terminalDevice`'s `&terminal_write` is fine (its actual
signature is `void terminal_write(const char* str)`, matching exactly).
Fix is presumably a thin `void serial_log_adapter(const char* message) {
serial_write_string(message); }`-style wrapper for `serialDevice` to
point at instead, matching the pattern that already works for
`terminal_write`.

### 12. Add a way to move windows around the screen — DONE

`mods/core/wingman/wingman.cpp` (`mouseFunctionWindowManager`) — a
press-edge landing in the top 30px of a window (`WINGMAN_DRAG_HANDLE_HEIGHT`)
starts a drag; `offsetX`/`offsetY` update every packet while the button
stays held, and content dispatch (`handleMouse()`) is skipped entirely
during a drag so it can't also register as a click underneath. Works
generically for every window type (implemented at the WindowManager
dispatch level, not per-widget) since neither `MessageBox` nor
`FileManager` draws anything interactive that high up today.

### 13. Implement `elf_lookup_symbol()` — DONE

`mods/dev/elf/elf.cpp` — was stubbed to always `return NULL`.
Originally filed under "lower priority" in the full-codebase audit below,
but re-assessed (2026-07-09, after an external peer review of
`docs/FULL.md`) as more consequential than that framing suggested: the
real ceiling on what ELF programs can do isn't the syscall count (only 5
exist: `open`/`read`/`write`/`close`/`exit`), it's that a program can't
call into *any* kernel-exported function, full stop, regardless of how
many syscalls exist — every program has to be entirely self-contained.
Fixing symbol resolution unlocks reuse of whatever's already
kernel-exported in one pass, rather than needing a new syscall number
added one at a time for each thing a program might want to call.

Implemented (2026-07-12) as a static `elf_exports[]` table of
`{name, address}` pairs in `elf.cpp`, linearly scanned via `strcmp`.
Covers everything actually *implemented* (not just declared) in
`mods/std/`: the malloc family, the string.h set, the stdio.h set, and
the 5 syscall wrappers. Deliberately excludes `ftell()`, which is
declared in `mods/std/include/stdio.h` but has no implementation
anywhere in the kernel — exporting it would have exposed a dangling
function pointer, since `--oformat binary` linking doesn't fail on
undefined symbols. See `docs/DOCS.md` ("`mods/dev/elf/elf.cpp` —
`elf_lookup_symbol()`") for the full rationale. Build-verified only
(`make clean && make build`, kernel.bin produced with no errors);
not yet boot-tested against a real ELF program that references one of
these exports by name.

### 14. Formalize the bootloader→kernel interface — DONE

New item (2026-07-09), surfaced by an external peer review of
`docs/FULL.md` — not previously tracked anywhere. `src/boot/loader/main.cpp`'s
`read_file_frontend` (a closure-like wrapper around the bootloader's own
FAT12 reader, bound to the already-open boot disk state) gets handed to
the kernel as its `read_file` function-pointer argument to
`kernel_main()` — this is how the kernel keeps reading arbitrary files
off the boot floppy post-boot without needing its own independent FAT12
implementation.

Traced end to end (2026-07-13) and found a real hazard, not just a
documentation gap: the pointer used to cross from
`src/boot/loader/main.cpp` into `src/boot/loader/entry.asm` via a
hand-computed absolute address (`READ_FUNCTION_ADDRESS`, a `#define`
chain in the C++ file) that `entry.asm` referenced as a bare, disconnected
hex literal (`0x3FFBF8`) — two files, two languages, one number, nothing
tying them together. A future change to `KERNEL_LOCATION` or any buffer
size ahead of it would silently hand the kernel a garbage pointer with no
build error. Fixed by replacing the hand-computed address with a real
linked symbol (`g_read_file_ptr`) referenced by name from both sides —
the linker now resolves it for real, so the two files can't drift apart
unnoticed.

The memory-ownership question from the original write-up turned out to
already be a non-issue, on closer inspection: `init_phys_allocator()`
already protects both boot-owned memory regions today, as an *incidental*
side effect of its reservation sweep starting at address `0` rather than
at the kernel's own load address — not something anyone wrote with
stage2 in mind, and not documented as a dependency until now. No new
allocator code was added (nothing is actually broken), but this
dependency is now written down explicitly. See `docs/DOCS.md`
("Bootloader → kernel `read_file` handoff") for the full trace, including
a verification note about a misleading intermediate result hit while
checking this (an ELF-format test link gave a different, wrong address
than the real `--oformat binary` build — worth reading if anyone
re-verifies this by hand later).

---

## Priority 2 — Before tasking (Proposal 1) can be turned on

Real multitasking is proposed in `agile-napping-stallman.md` (each ELF run
becomes its own preemptible task). Given an explicit go-ahead (2026-07-08),
Phase 1 and Phase 2 are now done — see "Full tasking proposal" further
down for the whole plan; Phase 3 (blocking syscalls block the task, not
the CPU) hasn't been started.

All Phase 0 hardening items below are done.

- [x] `enter_critical()`/`exit_critical()` primitive — implemented in
      `mods/dev/port.cpp`: saves `EFLAGS.IF` (`pushf`) then `cli`; restore
      only fires `sti` if the saved flag says interrupts were previously
      on, which is what makes nested calls safe.
- [x] Guard `malloc`/`free` (`mods/std/stdlib.cpp`) — the free-list
      walk/splice in `malloc()` and `free()`, plus the walk in
      `get_kmalloc_free_bytes()`, are now wrapped in
      `enter_critical()`/`exit_critical()`.
- [x] Guard the physical page bitmap (`mods/dev/memory/allocator.cpp`) —
      `alloc_phys_page()`/`free_phys_page()` now wrap their bit flip in
      `enter_critical()`/`exit_critical()`, and `alloc_phys_pages()` takes
      one critical section for its whole scan/rollback loop (nesting is
      safe, so this is what actually makes the multi-page request atomic
      as a unit, not just each individual page). Also fixed a real
      pre-existing bug found while doing this: the non-contiguous-page
      rollback branch was freeing `base + i * PAGE_SIZE` instead of the
      actual over-fetched page at `addr`, which are only equal when
      contiguity *didn't* break — i.e. it never freed the right page.
- [x] Guard VFS/ramfs pool bookkeeping (`mods/dev/vfs/vfs.cpp`,
      `mods/dev/ramfs/ramfs.cpp`, `file_table[]` in `stdio.cpp`) —
      `stdio.cpp`'s `alloc_fd()` now claims the slot (`in_use = 1`) inside
      the same critical section as the scan that found it, instead of
      leaving a gap between "found a free slot" and "claimed it" for a
      second `fopen()` to hand out the same fd; `fclose()`'s `in_use = 0`
      is guarded too. `vfs.cpp`'s `vfs_mount()` redoes its limit/
      double-mount checks atomically with the `mounts[]` write, and
      `vfs_find_mount()`'s scan over the same array is guarded so it can't
      observe a torn update. `ramfs.cpp` (rewritten since this TODO item
      was written, so it's malloc'd linked lists rather than fixed pool
      arrays now, but the same hazard shape applies): `ramfs_create_node()`
      and `ramfs_delete_node()` guard their whole duplicate-check/splice
      and unlink sequences, `ramfs_readdir()`/`ramfs_finddir()` guard their
      list walks, and `ramfs_read()`/`ramfs_write()` guard their per-file
      block-list traversal/growth.
- [x] Make `sched_lock`/`sched_unlock` actually atomic
      (`mods/dev/tasking/tasking.cpp`) — both now guard their
      increment/decrement of `g_sched_lock` with `enter_critical()`/
      `exit_critical()`, so a timer tick landing mid-update can't see a
      torn value or lose an update. `scheduler_on_tick()`'s own read of
      `g_sched_lock` needs no separate guard, since it only ever runs
      from inside the IRQ0 handler itself.
- [x] AC97/chorus playback race — promoted to Priority 1 item 1, since
      it's a real bug independent of tasking, not just a prerequisite.

---

## Priority 3 — Explicitly deferred past tasking (not blockers)

- ~~Keyboard focus routing for multiple tasks blocked on stdin~~ — DONE
  (2026-07-14), see "Recently completed" below. The window-manager-focus
  version of this (routing based on which *window* is focused) is still
  genuinely deferred, since ELF programs don't have windows at all today
  — that's a real, separate, bigger feature (see the "Recently completed"
  writeup for the scoping discussion).
- Moving mouse/keyboard IRQ work out of interrupt context (the cursor
  tearing problem) — real task-blocking gives us the primitive to do this
  properly later, but it's a separate change.
- Per-task memory isolation (ring 3, per-task page tables) — out of scope,
  same boundary as everything else. Ring 0 only, by design.

---

## Miscellaneous — parked, revisit later

### `KERNEL_LOCATION` (`0x400000`) is independently hardcoded in three places

Found (2026-07-13) while working item #14 — same class of hazard as that
item's `READ_FUNCTION_ADDRESS` bug, but worse blast radius. `0x400000` is
defined three separate times with no build-time link between the copies:

- `src/boot/loader/main.cpp:5` — `#define KERNEL_LOCATION 0x400000`
  (where the bootloader writes `KERNEL.BIN` and jumps to start it)
- `src/boot/loader/entry.asm:5` — `KERNEL_LOCATION equ 0x400000` (second
  independent copy of the same value)
- `src/kernel/kernel.ld:6` — `. = 0x400000;` (tells the linker what base
  address the *compiled kernel itself* assumes for every internal
  absolute symbol/global/jump)

If these three ever drift apart, the bootloader loads the kernel image
to one address while the kernel's own code was linked assuming a
different one — unlike the `read_file` pointer bug (one broken function
pointer), this would make essentially every symbol reference in the
entire kernel binary wrong at once. Realistically an instant
triple-fault or silent garbage execution, not something with a
debuggable symptom.

Also noticed in passing: `src/boot/stage1.asm:7` defines its own
`KERNEL_LOCATION equ 0x8000` — same name, unrelated meaning (that one's
where *stage2* loads, not the kernel). Different value, never
cross-referenced against the other three, but a landmine for anyone
reading these files side by side and assuming the name means one thing
everywhere.

Not fixed yet — genuinely harder than the `read_file` fix was, since
`kernel.ld` is a linker script, not a compilable/assemblable unit, so
there's no `extern`/symbol reference that reaches into it the way
`entry.asm` could reference `main.cpp`'s `g_read_file_ptr`. Real fix
would mean running `kernel.ld` through the C preprocessor at build time
(`cpp -P` before `ld -T`) so all three files `#include`/`%include` one
shared header defining the address once — a legitimate, common technique
for exactly this problem, but a Makefile change, not just a source edit.
Cheap fallback (comment-only cross-reference in all three spots) doesn't
actually prevent the drift, just makes it easier to notice by hand.

### Characters repeat when typing into a running ELF program

Reported behavior (2026-07-08): run an ELF program (e.g. `main.asm` at
the project root, which prints a prompt, reads a line via `sys_read`,
then echoes it back) and typed characters arrive doubled — typing
"hello" produces something like "hheellllooo" in the echoed output. The
program's own printed text isn't duplicated, only what was typed.

Investigated at length (interrupt/task-switch mechanics in `idt.asm`,
`tasking.asm`, `syscall.asm`) without finding a confirmed root cause —
the nested-interrupt stack unwinding for a blocking syscall preempted by
the timer looks self-contained/symmetric on inspection. The leading
suspect, fixed as of this entry: `kb_add_event()` (`mods/dev/kb/kb.cpp`)
had no deduplication for an already-registered callback function
pointer (unlike `irq_install_handler()` elsewhere in this codebase,
which explicitly guards against this). `stdin_read_line()`
(`mods/dev/syscall/syscall.cpp`) registers `stdin_kb_callback` once per
blocking read; if that registration were ever duplicated, every keystroke
would get appended to the shared stdin buffer twice via
`kb_run_events()` invoking both entries. See `docs/DOCS.md` for the fix.

Also found while investigating (likely harmless on its own, but a real
bug): `syscall.asm`'s `syscall_handler` pushed `gs,fs,es,ds` but popped
them shifted by one position (`pop es; pop fs; pop gs; add esp,4`),
leaving `ds` never explicitly restored. Stack depth balanced correctly
(so `iret` landed at the right EIP), and in this ring-0-only
flat-memory-model kernel all segments are likely `0x10` everywhere
anyway, so this probably wasn't visibly harmful on its own — left unfixed
at the time, noted for follow-up.

**Fixed (2026-07-14)**, as a side effect of chasing a *different* bug
(tasking Phase 3's `task_block()` boot-test failure — see the Full
tasking proposal section below). Corrected the pop sequence to mirror
the pushes exactly (`pop ds; pop es; pop fs; pop gs`, no trailing
`add esp, 4`). Turned out not to be the actual cause of that other bug,
but it was a real, confirmed mismatch regardless and is now fixed.

**Informally confirmed via boot-testing (2026-07-14)**, though not as a
dedicated test of this specific issue: while boot-testing tasking Phase 3
(see below), typed "bob" into `MAIN.ELF`'s blocking `sys_read` prompt via
QEMU monitor `sendkey` and got back exactly "bob", not a doubled
"bbooatb"-style result. That's real evidence the dedup fix works for the
basic single-task case, but it wasn't a targeted test of the original
concurrent-registration scenario (e.g. two programs racing) — worth a
dedicated pass if this ever resurfaces.

### Cursor jumps back to its pre-launch position when an ELF program exits

Reported behavior: run a program from the file explorer, move the mouse
while it's running, and once it exits the cursor snaps back to wherever it
was when the program was launched instead of reflecting where the mouse
actually is.

Root cause understood, fix not yet landed: `elf_run()`
(`mods/dev/elf/elf.cpp`) calls the program's entry point as a plain,
blocking call from inside the mouse IRQ's handler chain, which is an
interrupt gate (`IF` cleared on entry). Nothing re-enables interrupts for
a plain run (unlike the MP3-playback branch right above the `.elf` branch
in `mods/core/wingman/suite/explorer/explorer.cpp`, which explicitly does
`asm volatile("sti")` before its own blocking wait) — so the PS/2 mouse's
relative-motion packets aren't just delayed while a program runs, they're
dropped outright, and the driver's tracked position genuinely never
changes for the run's whole duration. Adding a matching `sti` before
`elf_run(entry)` was tried and did **not** fix the symptom (reason still
unclear), so it was reverted rather than left in as a partial/no-op fix
that also would have widened exposure to the still-unguarded concurrency
hazards below (letting mouse/keyboard IRQs interleave with arbitrary ELF
execution before Phase 0 hardening is complete is its own risk).

Worth another look once Priority 2's Phase 0 hardening (physical page
bitmap, VFS/ramfs bookkeeping) is further along, since any real fix here
likely involves interrupts being safely live during a program's run
anyway.

**Update (2026-07-08):** Phase 2 of the tasking proposal replaced
`elf_run()` with non-blocking `elf_spawn()` — the mouse IRQ handler chain
that used to call `elf_run()` and block for the program's whole lifetime
now just spawns a task and returns almost immediately. That was the root
cause described above, so this bug may simply be gone as a side effect,
not a targeted fix. Not confirmed — worth re-testing specifically rather
than assuming it's resolved.

### Full-codebase performance/architecture audit (2026-07-07)

A 3-way parallel audit (boot/memory/interrupts, drivers/networking,
ELF/syscalls/stdlib/explorer) plus a review of the GUI/compositing code
already worked on this session. The four real bugs it surfaced were
promoted straight to Priority 1 (items 6-9); everything below is
performance/architecture follow-up, parked for later.

#### Concurrency gaps beyond Phase 0

Same "shared mutable state touched from both IRQ and mainline, no guard"
shape as the AC97 race and the already-hardened malloc/allocator/VFS/
ramfs/scheduler-lock work — just in places that pass didn't cover:

- `mods/std/stdio.cpp` — `fseek()`/`fread()`/`fwrite()` mutate
  `file_table[fd].position` and `node->size` with no
  `enter_critical()`/`exit_critical()`, even though `alloc_fd()`/
  `fclose()` in the same file already got one. Reachable from the
  syscall path too.
- `mods/dev/syscall/syscall.cpp` — `syscall_fd_table[]` and the
  `stdin_buf_*`/`stdin_line_done` globals are touched by
  `sys_read`/`sys_write`/`sys_open`/`sys_close` *and* by a keyboard-IRQ
  callback, unguarded.
- `mods/dev/kb/kb.cpp` / `mods/dev/mouse/mouse.cpp` — the fixed callback
  arrays are mutated by `kb_add_event`/`kb_remove_event` from mainline
  while iterated from IRQ context with no guard; `kb_remove_event`
  clearing `.callback`/`.id` non-atomically mid-iteration is a real race.
  (`kb_add_event()`'s separate dedup gap — no check for an already-
  registered callback, unlike `irq_install_handler()` — is now fixed;
  see Miscellaneous, "Characters repeat when typing into a running ELF
  program". `mouse_add_event()` still has the identical dedup gap,
  not yet fixed.)
- `mods/dev/pci/drivers/rtl8139.cpp` (`NICDevice` fields) and
  `mods/dev/net/arp.cpp` (`arp_table`) — mainline TX vs. IRQ-driven
  RX/ARP-handling race, structurally identical to the AC97 one.
- `mods/dev/pit/pit.cpp` — `timer_ticks` is a `volatile uint64_t`
  incremented from IRQ0; on 32-bit x86 that's two 32-bit operations, so a
  tick landing mid-read/mid-increment can produce a torn 64-bit value.
- `mods/std/string.cpp` `itoa()` — single `static char buf[32]`, called
  from both normal logging code and ISR/panic-screen code. A fault
  interrupting a caller mid-consumption of its result, where the handler
  itself calls `itoa()`, silently clobbers the buffer.
- `mods/dev/idt/irq.cpp` — `currentInterrupt` global set from IRQ context
  with no guard; low risk today since no reader exists yet, but matches
  the same hazard shape if one gets added later.

Recommendation: treat as "Phase 0, part 2" — same fix
(`enter_critical()`/`exit_critical()`), fold into Priority 2 whenever
it's revisited.

#### GUI rendering performance

- `mods/dev/vbe/vbe.cpp` `fill()` — calls `draw_pixel()` per pixel for a
  full-screen fill instead of one linear fill over the contiguous
  framebuffer.
- `vbe.cpp` `draw_char()` — up to 1024 individual `draw_pixel()` calls
  per glyph at the default scale; this is the primitive behind all
  on-screen text.
- `mods/dev/console/console.cpp` `Terminal::scroll()` — scrolls via
  `get_pixel()`/`draw_pixel()` per pixel across the whole 1024x768
  framebuffer (~780K operations) instead of one `memmove` of the
  framebuffer region.
- `mods/core/wingman/manager.cpp` `composite()` — still reads the
  destination pixel unconditionally before calling `blend()`, even when
  `blend()`'s own fast paths (fully opaque/fully transparent source)
  would ignore it entirely. Minor next to the dirty-rect fix below, not
  yet done. (The "clears and fully re-blends the entire screen on every
  redraw" half of this item is DONE — see "Recently completed".)
- `mods/core/wingman/suite/explorer/explorer.cpp` `readDirectory()` —
  counts entries then fills them in two separate passes, and since
  `ramfs_readdir()` itself walks the linked list from the head per index,
  the whole listing is O(n²), doubled, and it re-runs on every
  navigation.
- `explorer.cpp` `draw_options()` — an arrow-key press (changing only the
  selection highlight) redraws every icon and every character of every
  filename, not just the old and new selected rows.

#### Drivers/networking performance

- `mods/dev/net/ip.cpp` `sendPacket()` — blocking ARP resolution: on a
  cache miss it retries up to 5x with `timer_wait(200)`, so a single
  outbound packet to an unresolved host can stall the entire single-core
  kernel for up to ~1 second.
- `mods/dev/serial/serial.cpp` — every log call is a per-byte busy-wait
  chain on the UART, and `serial_write_time()` (prepended by default)
  does a full CMOS/RTC read plus 6 `itoa()` calls per log line. Used
  pervasively, including inside IRQ handlers (AC97, RTL8139), so a hot
  log line inside an IRQ handler blocks that IRQ's own completion.
- `net/arp.cpp` — the table is append-only with no dedup; repeated
  traffic from the same host keeps appending duplicate entries instead of
  updating in place, wasting capacity and lengthening the linear-scan
  lookup over time.
- `rtl8139.cpp` / `ip.cpp` / `udp.cpp` — malloc/free churn per packet (a
  fresh heap alloc for every RX packet, a fresh scratch buffer for every
  UDP checksum, a fresh buffer for every IP send) instead of reusing
  preallocated buffers.
- `ac97.cpp`/`wav.cpp`/`chorus.cpp` — hand-rolled copy/zero loops
  reinventing `memcpy`/`memset`, plus per-sample-frame byte-at-a-time
  stores where one word store would do.

#### Boot/memory performance

- `src/boot/loader/floppy.cpp` / `libboot.cpp` — every single 512-byte
  sector read (FAT table, each cluster) does a full drive
  reset/recalibrate/motor-on cycle instead of batching contiguous
  sectors into one multi-sector read.
- `mods/dev/paging/paging.cpp` — eagerly identity-maps 512 page tables
  (2GB worth) into a 2MB static BSS array at boot, when the kernel only
  needs ~8MB mapped per its own comment; also does an unconditional full
  TLB flush (`mov cr3,cr3`) on every `map_physical_memory()` call
  regardless of page count, and never uses 4MB large pages for the bulk
  identity-map.
- `mods/std/string.cpp` — `strcmp()` does two full extra `strlen()` scans
  before comparing a single byte (and returns a raw pointer difference on
  mismatch, not a proper comparison result); `memcpy`/`memset` are
  byte-at-a-time with no word-sized fast path. Both are extremely hot
  (VFS lookups, ELF symbol search, every buffer op).

#### Lower priority / informational

- `tasking.cpp`'s O(n) `task_alloc()`/`pick_next()` — fine at
  `MAX_TASKS=32`, not worth touching now.
- `math.cpp`'s O(n²) Taylor-series `sin`/`cos` — currently unused
  anywhere in-tree.
- `pci.cpp`'s boot-time linear device-name scan and a fragile
  pointer-identity (not content) string comparison — boot-only, low
  impact.

---

## Recently completed

### Dirty-rect compositing — DONE (2026-07-16)

`WindowManager::composite()` cleared and re-blended the entire 1024x768
screen, and `wingman.cpp`'s `redraw_screen()` did two full-buffer
`memcpy()`s (through this project's byte-at-a-time `memcpy`), on every
single keystroke, click, or drag step — regardless of how much actually
changed. Added a `Rect` type (`mods/core/wingman/headers/types.h`) and
threaded it through the pipeline: `WindowManager::composite(Rect)` only
touches the dirty rect (intersected per-window), and a new
`redraw_screen_rect(Rect)` in `wingman.cpp` copies just that rect's rows
straight from the clean `wm->screen` buffer to hardware, no `outputBuffer`
involved. `mouseFunctionWindowManager()` now computes/accumulates the
right rect per event (focus change, drag — unioning the window's
last-drawn position with its current one to cover throttled-over steps,
widget interaction, keyboard). The original no-arg full-screen
`composite()`/`redraw_screen()` still exist, unchanged, for the one real
full-screen case (initial boot draw). See `docs/DOCS.md` ("mods/core/
wingman/headers/types.h — Rect / dirty-rect compositing") for the full
design writeup, including the per-event rect table.

### Rounded widget corners — DONE (2026-07-16)

`Button`, `TextInput`, and `Checkbox` (both styles) all drew hard 90°
rectangles. Added a shared `draw_rounded_rect_fill()` helper
(`mods/core/wingman/headers/shapes.h`) and switched all three widgets to
it — anti-aliased rounded corners, radius scaled to each widget's own
height (`height / 5`), with `Checkbox`'s toggle style upgraded to a real
capsule track + circular thumb (`height / 2`, the conventional switch
shape, not the general rule). See `docs/DOCS.md` ("mods/core/wingman/
headers/shapes.h") for the full design writeup. `MessageBox`'s buttons
picked up rounded corners for free, since they're the same shared
`Button` class — no changes needed there.

### True font rendering (real TrueType via stb_truetype) — DONE (2026-07-14)

All on-screen text used to go through the same fixed 8x8 bitmap `Font[]`
array, integer-scaled — blocky, no anti-aliasing. This had already been
attempted once before this session and reverted in full (no git trace),
after hitting a real bug (unsigned wraparound on negative TTF
`xoff`/`yoff`) and then appearing not to boot cleanly — but that "doesn't
boot" conclusion was never actually verified, because at the time the
sandbox's ability to boot-test via QEMU was wrongly believed to be
broken. This session proved that assumption false (see Phase 3 tasking
above), so this was a genuine retry with real verification, not a blind
repeat.

- **Font asset**: `Cousine-Regular.ttf` (SIL OFL 1.1, Google Fonts),
  shipped at `src/bin/` alongside this project's other binary assets,
  confirmed metrically monospace at bake time (every printable-ASCII
  glyph's `advanceWidth` checked equal) — required, since every layout
  call site (`Button` auto-size, `TextInput` scroll math, `FileManager`
  list layout) hard-codes a fixed `8*scale` pixel advance; switching to
  proportional spacing was explicitly out of scope.
- **Embedding**: NASM `incbin` (`mods/core/fontman/font_data.asm`), not a
  generated C byte array — confirmed empirically that `incbin` paths
  resolve relative to the assembler's CWD (matching how the Makefile
  already invokes `nasm`), and confirmed via direct read of `kernel.ld`
  that `.rodata` merges cleanly from every input object regardless of
  link order.
- **Package layout**: the font code (baking, blitting) lives at
  `mods/core/fontman/`, not `mods/std/` — a peer component of
  `mods/core/wingman/`, named to match (`wingman`/`fontman`), reflecting
  that it owns real boot-time state and is consumed by both the raw
  `Terminal` and every Wingman widget, not a stateless `mods/std/`-style
  utility. Moved there after initially landing in `mods/std/`; internal
  function/type names were left unrenamed since this was a location
  change, not an API rename.
- **`stb_truetype.h`** vendored directly (not a submodule, unlike
  `minimp3` — genuine single-file library) at
  `mods/ports/truetype/vendor/` (originally `mods/ports/stb_truetype/`,
  renamed since — see `docs/DOCS.md`), with a thin
  `truetype_impl.cpp` wrapper (was `stb_truetype_impl.cpp`, mirrors `minimp3.cpp`'s pattern)
  supplying real `STBTT_ifloor`/`STBTT_iceil`/`STBTT_fmod` (confirmed by
  grepping the vendored header that these are genuinely reachable from
  the rasterizer path used) and stubbing `STBTT_acos` (confirmed
  unreachable outside the SDF API, which is never called).
- **Bakes fixed-size glyph atlases once, at boot**, via
  `stbtt_PackFontRange` (3 tiers matching the 3 scale factors already in
  use — 16px/24px/32px) — never rasterizes live, since `-O0` + x87-only
  floats (`-mno-sse` etc.) makes live per-glyph rasterization real cost,
  worth paying once, not per redraw. Every offset in the blit path
  (`ttf_blit_glyph()`, `mods/core/fontman/fontman.h`) stays a
  signed `int` end to end and gets clipped before any caller's `unsigned`
  x/y is added in — the direct, explicit fix for the bug class that
  killed the prior attempt.
- **The panic screen was deliberately left untouched** — confirmed via
  `git diff` showing zero changes to `vbe.cpp`/`isr.cpp`/`font.h`, not
  just by inference. A fault handler is the wrong place to depend on a
  new subsystem (atlas lookups, boot-init having succeeded, possible
  heap corruption being what caused the fault in the first place).
- **A real, second bug found and fixed via boot-testing**: first full
  boot attempt page-faulted (`CR2=0xFFFFFFFC`, write, supervisor) deep
  inside `stbtt_MakeGlyphBitmapSubpixel`. Bisected via QEMU headless
  boot-testing (1 glyph → fine, 2 glyphs starting from space → crash, 1
  glyph = space alone → crash) down to `free(NULL)`:
  `mods/std/stdlib.cpp`'s `free()` never null-checked its argument (this
  one's still in `mods/std/`, unrelated to the fontman relocation), and
  unconditionally computed `ptr - sizeof(block_t)` — for `ptr == NULL`
  that's `0 - 8 = 0xFFFFFFF8`, and the very next line writes to
  `block->next` (offset +4 into the struct) = exactly `0xFFFFFFFC`,
  matching the fault address byte-for-byte. `free(NULL)` is standard-
  mandated to be a safe no-op in every real libc; this kernel's never
  was, and nothing had ever called `free(NULL)` before —
  `stbtt_MakeGlyphBitmapSubpixel` legitimately does exactly that for any
  glyph with zero contours (space has no outline). Fixed with a one-line
  null-check. Not a font-code bug at all — a real, pre-existing,
  previously-latent kernel bug, same "new correct code exercises an
  untested path" pattern as the `kernel_main()` task-creation race found
  earlier this session.
- **Verified visually, not just via boot success** — new technique this
  session: QEMU monitor's `screendump` command captures the guest
  framebuffer even under `-display none` (confirmed working), converted
  PPM→PNG via `sips`, and actually looked at the rendered output:
  genuine anti-aliased TrueType glyphs (smooth curves on 'a'/'f'/'o'/'m',
  proper letterforms), confirmed at both the scale-2 body-text tier and
  the title tier. Letter-spacing is visibly loose/generous — the direct,
  expected consequence of keeping the fixed monospace-grid stride
  instead of switching to proportional spacing, not a bug.
- See `docs/DOCS.md` ("Font Rendering System", "mods/std/stdlib.cpp —
  free(NULL)") for the full writeup.

### Keyboard ownership for concurrent stdin readers

Priority 3's "Keyboard focus routing" item, minimal-fix version (2026-07-14,
explicit go-ahead after scoping out and rejecting the bigger
window-focus-based version — see below). With tasking Phase 3 landed,
more than one task can legitimately be mid-`sys_read` on stdin at once,
but `syscall.cpp` only ever tracked one global `stdin_buf_ptr`/
`stdin_waiter` — a second concurrent reader would silently clobber the
first's buffer and waiter.

Fixed with a small fixed-size LIFO stack of `StdinFrame`
(`{waiter, buf, size, pos}`) in `syscall.cpp`, `MAX_STDIN_DEPTH = 8`.
Whichever task most recently started reading owns the keyboard
exclusively — `stdin_kb_callback` always writes into `stdin_stack[depth
- 1]` (the top frame) and only wakes the top frame's task on `'\n'` —
the same nesting a stack of modal dialogs would have. Older, still-
waiting readers are structurally guaranteed to still be on top when they
eventually *do* wake (nothing pushed after them can complete before they
do, since only the top frame ever receives input), so popping is always
safe. `stdin_kb_callback` itself stays a single shared function,
registered once (via the existing `kb_add_event()` dedup) when the stack
goes empty→non-empty and unregistered once it goes back to empty —
registering/unregistering per-call would have broken this, since an
inner frame completing and unregistering would cut off an outer frame
still waiting below it. `stdin_is_reading()` (still the only thing
external code, i.e. `wingman.cpp`, actually reads) now reports "stack
depth > 0" instead of a single boolean, same external meaning, correctly
generalized.

**Boot-tested for real (2026-07-14)**: temporarily spawned `MAIN.ELF`
twice back to back (two independent, concurrently-blocked readers),
typed "two" for the second (topmost) one — it alone received it,
completed and popped correctly — then typed "one" for the first, which
was correctly back on top and received it cleanly, with zero
cross-contamination between the two. No crash, no hang.

**Scoped down from a bigger version, deliberately**: the original framing
("tie into window-manager focus") doesn't fit today's reality — ELF
programs are fully headless (output only via serial, no window of their
own), so there's no window to click to focus a specific running program.
Building that would mean a real new UI feature (a console/terminal
`Window` subclass) — a separate, larger task from "fix the routing bug,"
not implemented here. This fix only makes ownership *correct and
identifiable* (a real stack instead of a global that lies about who's
listening); it doesn't add any new way for a user to *choose* which
program owns the keyboard beyond "whichever one asked most recently."

### Fix `TextInput` text overflowing its own bounds instead of scrolling

Reported behavior (2026-07-08, confirmed via screenshot in the widget
demo): typed text ran straight off `TextInput`'s right edge into
whatever was next to it once it got longer than the field, instead of
scrolling to keep up with typing like a real text field. Root cause:
`TextInput::draw()` (`mods/core/wingman/widgets/textinput.cpp`) drew
every character in `buffer` starting at a fixed `x` with no bound on how
far right that could go. Fixed by showing a trailing window of the
buffer instead of the whole thing — `maxVisibleChars` is however many
characters actually fit in the text area, and once the buffer exceeds
that, the visible window slides forward to keep the last
`maxVisibleChars` characters (and the caret, always at the logical end
per `TextInput`'s existing no-mid-editing simplification) in view. Also
fixed the caret's position to use the trailing window's length rather
than the buffer's full length, which would've placed it past the
widget's edge the moment scrolling kicked in.

### Stop the Wingman GUI from also reacting to keystrokes meant for a blocking stdin read

Reported behavior: pressing Enter to submit typed input to a running ELF
program (e.g. `main.asm` at the project root) also triggered the file
explorer to advance its selection and open/play whatever file it landed
on. Root cause: `kb_run_events()` (`mods/dev/kb/kb.cpp`) broadcasts every
keystroke to every registered callback unconditionally — `stdin_kb_callback`
(`mods/dev/syscall/syscall.cpp`, feeding the blocked `sys_read`) and
`keyboardFunctionWindowManager` (`mods/core/wingman/wingman.cpp`, routing
into the focused Wingman window, i.e. `FileManager::onKeyboard()`) both
receive the same keys at the same time, with no concept of exclusive
focus. `'s'`/`'w'` move the explorer's selection and `'\n'` activates
whatever's selected — so typing into a program could silently drift the
explorer's selection and then open something else entirely on Enter.

Fixed narrowly at the time (real per-task focus routing was a bigger,
open design question then — see "Recently completed" → "Keyboard
ownership for concurrent stdin readers" for where that landed later):
`stdin_read_line()` now sets a `stdin_reading` flag for the duration of
its blocking wait, exposed via `stdin_is_reading()`.
`keyboardFunctionWindowManager()` checks it first and returns immediately
if set, so the whole Wingman GUI stops processing keyboard input while
any program is mid-read — matching the existing `suppressCharacterOutput`
pattern already used in the boot terminal for the same class of problem.
See `docs/DOCS.md` for more.

**Not confirmed via boot-testing.**

### Forbid re-launching an already-running ELF file

`mods/core/wingman/suite/explorer/explorer.cpp` — Phase 2 of the tasking
proposal retired the kernel-side `elf_running` guard (running multiple
*different* programs concurrently is now intended), which left nothing
stopping the same `.elf` file from being launched twice (e.g. a fast
double-click) while a prior instance was still alive — two instances
would share global stdin state (`stdin_buf_ptr` etc. in `syscall.cpp`)
and collide. Fixed at the explorer level, not in the kernel:
`elf_spawn()` (`mods/dev/elf/elf.cpp`) now returns the `task_t*` handle
from `task_create()` instead of a bare success/fail `int`, and
`explorer.cpp` keeps a small fixed-size `{filename, task_t*}` table
(`runningElfTasks[]`) to check `task->state` before allowing a re-launch
of the same filename. Relies on task_t slots never being reused while
referenced here — currently true only because no task reaper exists yet
(see Phase 2's "known, deliberately deferred" note below); will need
revisiting once one does.

### Finish wiring up MessageBox

`mods/core/wingman/suite/message/message.{h,cpp}` — `MessageBox` is now a
real `KeyboardDelegate`/`MouseDelegate`: it takes a `WindowManager*` in
its constructor and self-registers/focuses, Enter or a click inside a
button's rect dismisses it (`dismiss()` calls `wm->remove(ref)`, nulls
`this->window` so `~MessageBox` doesn't double-free it, then
`delete this`). Buttons are now dynamic, not a single fixed "Okay" —
extracted into their own `Button` class/`ButtonCallback` typedef
(`mods/core/wingman/headers/widgets/button.h`,
`mods/core/wingman/widgets/button.cpp`), added via `addButton()`, capped
at `MESSAGEBOX_MAX_BUTTONS` (3), laid out by `layoutButtons()`. Also
picked up an icon override parameter (`int icon = -1`, indexing
`Icons[]`) and a hover-cursor swap over buttons.

This surfaced the two `WindowManager` gaps the original note called out,
both now fixed (`mods/core/wingman/headers/manager.h`, `manager.cpp`):
`focus(window_ref_t)` exists and also raises the window to the top of a
new z-order stack (`zOrder`/`zOrderCount`), and `remove()` clears
`focusedWindow` if the removed window was focused. The z-order work grew
into full click-to-focus/bring-to-front for the window manager in
general (`windowAt()`, wired up in
`mods/core/wingman/wingman.cpp`'s `mouseFunctionWindowManager`), not just
MessageBox's own focus needs.

Real call site is live in `initalizeWindowSystem()`
(`mods/core/wingman/wingman.cpp`) — the "chorus not initialized" message
box now has real "Initialize"/"Ignore" buttons wired to
`chorus_initalize()`.

### Distinguish recoverable vs. fatal exceptions in the fault handler

`mods/dev/idt/isr.cpp`, `_fault_handler()` — split into recoverable
(#DB/#BP, which just log and fall through so `iret` resumes normally) vs.
fatal (everything else, unchanged halt-forever behavior but now with real
diagnostics). Fatal path decodes `CR2` and the page-fault `err_code` bits
(protection-violation/write/user) when `int_no == 14`, walks the `EBP`
chain to print a bounded stack trace of return addresses
(`print_stack_trace()`), and draws an actual panic screen
(`draw_panic_screen()`) using only raw framebuffer primitives
(`fill()`/`draw_char()`) — deliberately no `malloc`/Surface/Window
dependency, since heap or GUI-state corruption could be what caused the
fault in the first place. The new helpers live in a `FaultHandler`
namespace; `ISRInstall()` and the ISR/IRQ glue stay loose functions since
they're called by exact symbol name from hand-written assembly. A
`test_fault_handler()` hook (`p-kernel.cpp`) deliberately triggers a #BP
then a #PF at `0xDEADB000` to exercise both paths — call manually while
testing, never boot-tested end-to-end in this environment (sandbox QEMU
limitation, unrelated to the kernel code).

### Validate ELF header offsets/counts before trusting them

`mods/dev/elf/elf.cpp` checked the ELF magic and machine/class fields but
never validated that `e_shoff`/`e_shnum`/`e_phoff`/`e_phnum` actually
pointed within the loaded file before looping over them. Fixed by adding a
`buffer_size` parameter to `elf_load_file()` (both call sites already had
the real size on hand) and a new `elf_validate_bounds()` check run before
anything else touches the header; `elf_section()` also now rejects any
index `>= e_shnum`, so a bad `sh_link`/`sh_info`/`e_shstrndx`/`st_shndx`
cross-reference elsewhere in the file can't walk off the end either, not
just a bad top-level offset. Still matters more if driver/module loading
off a swapped disk (see the multitasking plan file's Proposal 3 sketch)
ever becomes real, since that's untrusted file bytes controlling loop
bounds.

### Fix the hardcoded WAV buffer size in the file explorer

`mods/core/wingman/suite/explorer/explorer.cpp:359` had
`uint32_t buffer_len = 52640;` — a fixed size instead of the real file
size. The ELF/MP3 handlers already did this correctly (read
`vfs_find()->size`) — same fix, now applied to the WAV path too. This was
the original bug that kicked off the "why can't I play WAVs longer than 2
samples" thread.

---

## Full tasking proposal (copied from `agile-napping-stallman.md`)

**This is a proposal only — nothing gets implemented until you say go.**

### Proposal 1: real multitasking, with each ELF run as its own task

#### Context

Running `MAIN.ELF` today means calling its entry point as a plain, blocking
function call (`elf_run()`), synchronously, from whatever context asked for
it (a mouse click, or `procTestOne` at boot). We've been patching around the
consequences of that one at a time this session: the interrupt-gate/`sti`
issue so a blocking `sys_read` doesn't kill interrupts system-wide, the
EOI-ordering fix so a blocking click handler doesn't starve the mouse's own
IRQ line, and a re-entrancy guard so a second click can't corrupt the shared
`jmp_buf`. Each of those was a symptom of the same root cause: there is no
real task boundary around a running program, so it borrows whatever context
happened to invoke it (an interrupt handler) instead of having its own.

You want to fix this at the source: each ELF instance becomes a real task, so
running one doesn't block anything else, multiple can run concurrently, and
none of the fragile jmp_buf/hlt-spin machinery is needed anymore.

The good news: `mods/dev/tasking/tasking.cpp` already has a complete,
well-built preemptive round-robin scheduler — `task_create()`, a timer-driven
`scheduler_on_tick()`, a trampoline that calls `entry(arg)` and cleans up via
`task_exit()` on return. It's just never been turned on (commented out in
`p-kernel.cpp`, with an old comment claiming it page-faults). I checked that
theory: the stated cause ("`Logging::log` after `terminal_delete()`") no
longer holds — `terminal_write()` already null-checks the terminal pointer
today. The tasking code itself looks structurally sound on inspection. My
read is that comment is stale, not a live blocker — but Phase 1 below
verifies that for real before anything depends on it, using the EIP-dumping
fault handler we already added to `isr.cpp` this session if anything does
surface.

#### Proposed approach, in phases

##### Phase 0 — Preemption-safety hardening (new, must land before real tasks run)

A dedicated audit (2026-07-02) of every subsystem a task could touch found
that **the kernel is not currently safe to preempt** the moment more than
one task exists and does real work — this isn't a Phase-2/3 concern, it's a
prerequisite to Phase 1's own test-task spawn doing anything beyond printing
fibonacci numbers. Confirmed live hazards, all global mutable state mutated
with no critical section:

- **`malloc`/`free`** (`mods/std/stdlib.cpp`) — the free-list is walked and
  spliced with no guard. Two tasks (or a task and code it triggers) calling
  malloc "simultaneously" (one preempted mid-splice) corrupts the heap.
- **Physical page bitmap** (`mods/dev/memory/allocator.cpp`) — same shape of
  bug, bitmap bits flipped with no guard; `alloc_phys_pages()`'s multi-page
  loop isn't atomic either (partial allocation on preemption + failure).
- **VFS/ramfs pool bookkeeping** (`mods/dev/vfs/vfs.cpp`, `ramfs.cpp`, and
  the `file_table[MAX_OPEN_FILES]` fd pool in `stdio.cpp`) — `mounts[]`,
  ramfs's `dirs_used`/`files_used` pools, and fd-slot allocation in `fopen`
  are all unguarded global arrays. Two tasks calling `fopen` concurrently can
  hand out the same fd slot or corrupt ramfs's free-node tracking.
- **AC97/chorus playback state** (`ac97.cpp`, `chorus.cpp`) — this one's
  actually a *pre-existing* bug independent of tasking: the IRQ handler
  updates `sound_buffer_refilling_info` fields with no guard against
  mainline code doing the same thing mid-update. Tasking adds a second kind
  of contender (another task starting playback) on top of an already-real
  IRQ-vs-mainline race.
- **`sched_lock`/`sched_unlock`** (`tasking.cpp`) already exist but are a
  plain (non-atomic) counter increment/decrement — not itself safe under
  preemption. Worth fixing as part of the ground-up tasking rewrite you
  asked for, not patched in place.
- Serial output and the Wingman framebuffer composite/redraw can interleave
  under preemption too, but that's cosmetic garbling, not corruption — lower
  priority, doesn't need to block Phase 1.

Because this is a single-core kernel, the fix doesn't need real spinlocks —
a simple `cli`/`sti`-based critical section (save-and-restore `EFLAGS.IF` so
it nests correctly) around each of the hazards above is sufficient, since
the only source of preemption is the timer interrupt. Add one small
primitive (e.g. `enter_critical()`/`exit_critical()`) and apply it at the
handful of call sites above before Phase 1 spawns more than one task that
does real work (allocates, touches a file, or touches AC97 state).

##### Phase 1 — Turn on the existing scheduler (no ELF changes yet) — DONE (2026-07-08)

- [x] `tasking_init()` is called early in `kernel_main` (right after the
      interrupts-enabled check), so `g_current` becomes pid 0 from the
      start. Safe specifically because an empty runqueue makes
      `scheduler_on_tick()` a no-op every tick until real tasks exist —
      see `docs/DOCS.md` ("p-kernel.cpp — tasking_init() placement") for
      the full reasoning.
- [x] `KSTACK_SIZE` (`tasking.h`) raised from 4KB to 32KB.
- [x] `task0` (idle, `for(;;) hlt;`) is spawned first, so there's always
      something `TASK_READY` once the fibonacci tasks finish.
- [x] `task1`/`task2` (the fibonacci demo) uncommented and spawned via
      `task_create()`, all three at the very end of `kernel_main` (after
      every other boot step) — spawning any earlier would abandon the
      rest of boot the moment the first tick switches away, since the
      bootstrap task_t is never pushed onto the runqueue and has no path
      back into `pick_next()`'s rotation once execution leaves it.
- [x] `pit_init(1000)` already existed (Proposal 2) — no further work
      needed.
- Not yet found while doing this: `task_t` still has no `TASK_BLOCKED`
  state or `stack_base` field (Phase 2/3 still need to add both).
- **Boot-tested for real (2026-07-14)**, the "sandbox can't boot-test"
  limitation turned out to be wrong — QEMU runs headless in this
  environment (`-display none`, `-serial file:...`, plus a monitor
  socket to inject the boot-menu keypress via `sendkey`, since
  `stage0.asm`'s menu blocks on a real keystroke with no timeout).
  Confirmed genuinely working: booted with `task1`/`task2` enabled and
  captured real interleaved output (`[1] fibbanoci(27)` / `[2]
  fibbanoci(28)` / `[1] fibbanoci(28)` / ...) in the serial log — this is
  the first time this claim was checked against a real boot rather than
  code review. Phase 1 is solid.

##### Phase 2 — ELF runs become tasks, not blocking calls — DONE (2026-07-08)

- [x] `elf_run()` replaced by `elf_spawn(void* entry_point)`: mallocs a
      64KB task stack, calls `task_create(elf_task_trampoline,
      entry_point, stack)`, returns immediately. `explorer.cpp`'s `.elf`
      handler and `p-kernel.cpp`'s `test_elf_execution()` both updated to
      call it instead of blocking.
- [x] `elf_task_trampoline(void* entry_point)` just casts and calls the
      real entry point — it doesn't need to call `task_exit()` itself,
      since `task_create()`'s ASM trampoline already does that
      unconditionally whenever the entry function it invoked returns
      (see `docs/DOCS.md`). `setjmp`/`longjmp` machinery removed from
      `elf.cpp` entirely, `sys_exit` now calls `task_exit()` directly,
      and the `elf_running` re-entrancy guard is retired — running two
      ELF instances concurrently is now the intended behavior. The now
      fully-unused `mods/dev/context/setjmp.h`/`setjmp.asm` were deleted
      (confirmed unreferenced elsewhere first), which also meant removing
      the `%include` for them in `Kernel-Entry.asm`.
- [x] `elf_load_file()` (parsing + relocation) stays synchronous in the
      calling context, as planned.
- **Found and fixed along the way, not in the original plan**: making
  spawn non-blocking exposed a real use-after-free. `elf_load_file()`
  relocates sections **in place** inside the caller's file buffer, not
  into a copy — under the old blocking `elf_run()` that was safe (the
  caller's `free(buffer)` only ran after the program had already
  finished executing), but under non-blocking `elf_spawn()` the same
  `free(buffer)` would run before the task even starts, and the next
  timer tick would jump into freed memory. Fixed by only freeing the
  buffer on the failure paths (load/spawn failed); on a successful spawn
  it's intentionally leaked for now (see `docs/DOCS.md`) — same category
  as the task-stack/task_t-slot leak below, not yet solved by a reaper.
- **Known, deliberately deferred**: neither the malloc'd task stack nor
  the task_t slot (`g_tasks[MAX_TASKS]`, `MAX_TASKS = 32`) is ever
  reclaimed when a spawned ELF task exits — there's no reaper yet. In
  the worst case this means only 32 ELF programs can ever be run for the
  lifetime of one boot before the task table is exhausted, plus a
  64KB+file-size leak per run. The proposal's own "Files touched" section
  already flagged a `stack_base` field + reaper as a needed follow-up;
  this wasn't built now since it's genuinely separate from "make
  elf_run non-blocking," not a small addition.

**Priority note (2026-07-09, informed by an external peer review of
`docs/FULL.md`)**: when Phase 3 is picked up, do blocking/wakeups (below)
*before* reaping, not alongside it as equal-weight work. Reaping only
caps how many programs can ever be launched — a real but narrow limit.
Missing blocking affects every future subsystem that will ever want to
wait on something (stdin today, networking/timers/disk I/O later) — it's
the more foundational gap of the two.

##### Phase 3 — Blocking syscalls block the task, not the CPU — DONE (2026-07-14)

This is the part that makes concurrency actually useful rather than
cosmetic. `sys_read`'s stdin wait used to be `while (!done) hlt;` — even
after the EOI fix, that was "yield to any interrupt," not "let another
task run."

- [x] Added a `TASK_BLOCKED` state to `tasking.h` (`pick_next()` already
      skipped anything that isn't `TASK_READY`, so this needed no
      scheduler surgery) plus `task_block()`/`task_wake(task_t*)` in
      `tasking.cpp`, right next to `task_exit()` since it's the same
      immediate-reschedule-via-`int $0x20` trick.
- [x] `stdin_read_line()` (`syscall.cpp`) now calls `task_block()` instead
      of spinning on `hlt`; `stdin_kb_callback` calls
      `task_wake(stdin_waiter)` once it sees `'\n'`. The old
      `stdin_line_done` flag is gone — the wake itself is the "line
      ready" signal now, nothing left to poll.
- **Scoped down from the original plan, deliberately:** the stdin state
  (`stdin_buf_ptr`/`stdin_buf_size`/`stdin_buf_pos`) stayed a single
  global rather than moving to real per-task state. `stdin_waiter`
  (which task to wake) is also a single global. This means two different
  tasks both mid-`sys_read` on stdin at the same time would still
  clobber each other — but that's not a regression Phase 3 introduces,
  it's the exact same pre-existing gap the "Keyboard focus routing" item
  below already tracks (it existed the moment Phase 2 made concurrent
  `sys_read` calls possible at all, `hlt`-spin or not). Doing real
  per-task stdin state without also fixing keyboard routing would add
  bookkeeping for a scenario that's still broken for an unrelated reason
  — not worth doing until focus routing lands. See `docs/DOCS.md`
  ("`mods/dev/syscall/syscall.cpp` — `stdin_read_line()` real blocking")
  for the full reasoning.
- **Boot-tested for real (2026-07-14), found and fixed a genuine bug —
  not in Phase 3 itself.** The "sandbox can't boot-test" assumption this
  whole proposal was written under turned out to be wrong (see Phase 1's
  update above for how). Full sequence of what was found:
  - **Phase 1/2's underlying scheduler is confirmed solid**: booted with
    `task1`/`task2` (no ELF, no blocking) and got real interleaved
    fibonacci output. First actual proof this ever worked at runtime.
  - **First attempt at `MAIN.ELF` (prompts, then blocks on `sys_read`)
    failed**: `task_block()` returned almost instantly, before any
    keystroke arrived, `stdin_buf_pos` still `0`, the idle task's own
    debug print never fired. Fixed the known, already-flagged
    `syscall.asm` segment-register push/pop mismatch (see Miscellaneous
    → "Characters repeat..." below) on the theory that a corrupted nested
    `int 0x80` frame was preventing `task_block()`'s `int $0x20` from
    resuming correctly. **Re-tested: identical failure.** That fix was
    real and worth keeping (segment registers were genuinely getting
    cross-assigned), but it wasn't the cause of this bug.
  - **Actual root cause, found via a targeted runqueue dump**:
    `kernel_main()` spawns the ELF task (via `test_elf_execution()`)
    *before* it creates `task0` (idle), several lines later. With PIT
    already running at 1000Hz, a real timer tick can land in that gap —
    after the ELF task exists but before `task0` does. When it does,
    `scheduler_on_tick()`'s "first tick" logic permanently abandons the
    rest of `kernel_main()` (it's a one-time, non-requeueable handoff)
    and jumps into whatever's in the runqueue so far: just the ELF task,
    alone, self-looped. `task0` never gets created, "Tasking Enabled!"
    never prints, and `pick_next()` — correctly, given the actual
    runqueue state — can't find anything else `TASK_READY` to switch to,
    so `task_block()`'s reschedule is a genuine no-op, not a bug in the
    block/wake logic itself.
  - **Fixed**: wrapped `kernel_main()`'s entire task-creation span (from
    before the first `task_create()`/`elf_spawn()` call through the last)
    in `sched_lock()`/`sched_unlock()` — the same primitive that already
    makes `scheduler_on_tick()` a complete no-op while held, now exported
    from `tasking.h` so `p-kernel.cpp` can use it directly. No tick can
    hijack execution until every startup task actually exists.
  - **Re-tested end to end after the real fix**: booted, confirmed the
    ELF task's `sys_read` genuinely stays blocked (waited 6+ seconds with
    no keystrokes, nothing happened), typed "bob" + Enter, and the
    program correctly woke, captured "bob", and completed normally —
    real blocking, real wake, real captured input, no crash, no hang.
  - Test-only instrumentation (debug prints, runqueue dumps, temporarily
    re-enabling `test_elf_execution` in `p-kernel.cpp`) was fully
    reverted after testing. What's actually shipped: the `syscall.asm`
    fix, the `sched_lock()`/`sched_unlock()` fix in `p-kernel.cpp`, and
    Phase 3's original `task_block()`/`stdin_waiter` logic — no leftover
    debug code anywhere. See `docs/DOCS.md` ("p-kernel.cpp —
    kernel_main() task-creation race") for the full writeup.

##### Explicitly deferred (related, but not part of this proposal)

- **Keyboard focus routing**: with multiple tasks able to block on stdin,
  keystrokes need to go to one "focused" task, not all of them (`kb_run_events`
  currently broadcasts to every registered callback). This wants to tie into
  window-manager focus, which doesn't have a solid concept of "this window
  owns this task's console" yet — worth its own follow-up rather than
  bundling into this one, since it's really a window-manager design question.
- **Moving mouse/keyboard IRQ work out of interrupt context** (the cursor
  tearing conversation) — real task-blocking gives us the primitive to do
  this properly later (IRQ just wakes a compositor task instead of drawing
  inline), but it's a separate change against `cursor.cpp`/`wingman.cpp`,
  not required for ELF-as-task.
- **Per-task memory isolation** (ring 3, per-task page tables) — still out
  of scope, same boundary as the rest of this session's work. A crashing
  task can still take down the whole system; there's no MMU protection
  between tasks, same as there's none today.

#### Files touched

- `src/src/kernel/mods/dev/tasking/tasking.h` / `tasking.cpp` — enable init,
  raise `KSTACK_SIZE`, add `TASK_BLOCKED` + `task_block`/`task_wake`, add a
  `stack_base` field to `task_t` so a dead task's stack can be freed by the
  scheduler/a reaper rather than by the task itself (it can't free the stack
  it's currently running on).
- `src/src/kernel/p-kernel.cpp` — call `tasking_init()` early, add the idle
  task, replace the `tasks()` stub with the real test-task spawn (Phase 1),
  later replace `procTestOne`'s direct `elf_run` call with `elf_spawn`
  (Phase 2).
- `src/src/kernel/mods/dev/elf/elf.h` / `elf.cpp` — add `elf_spawn`/
  `elf_task_trampoline`, remove `setjmp`/`longjmp`-based `elf_run`/`elf_exit`
  once Phase 2 lands (the `context/setjmp.*` files can likely be deleted
  afterward if nothing else uses them).
- `src/src/kernel/mods/dev/syscall/syscall.cpp` — `sys_exit` calls
  `task_exit()` instead of `elf_exit()`; stdin state becomes per-task
  (Phase 3).
- `src/src/kernel/mods/dev/pit/pit.cpp` / `pit.h` — implement `pit_init(hz)`
  (optional, Phase 1).
- `src/src/kernel/mods/core/wingman/suite/explorer/explorer.cpp` — `.elf`
  handler calls `elf_spawn` instead of `elf_run` (Phase 2).

#### Verification

- Phase 1: boot, confirm via serial log that the fibonacci test tasks
  interleave (not run-to-completion one after another) — proves preemption
  is real. If it page-faults, the EIP-dump fault handler pinpoints exactly
  where, unlike the historical undiagnosed report.
- Phase 2: click the same `.elf` file twice in a row — both should start
  running as independent tasks instead of the second being rejected.
- Phase 3: start a program, let it block on `sys_read`, confirm (via serial
  timestamps or a second concurrently-running test task) that other tasks
  keep making progress while it waits — the actual "seamless" proof.

### Proposal 2: precise timer + accurate Unix millis — DONE

**STATUS: implemented and verified (clean build).** Kept below for
reference/history.

**Also a proposal only — separate from and independent of the multitasking
work above, though they share one piece (`pit_init`).**

#### Context

I checked what currently exists for timekeeping, and there isn't a working
"get the current Unix time" path at all:

- `clock()` (`mods/std/time.cpp`) reads `FetchCurrentCMOSTime().seconds` —
  that's just the **seconds field of the current time-of-day** (0–59), not
  an elapsed/Unix timestamp. It's a placeholder (the file's own header
  comment says "I have no idea what I'm doing").
- `time(time_t*)` doesn't exist yet — `time.h` has it commented out as
  "must add later."
- `mktime()` and the `gmtime`/`localtime` calendar math are actually
  correctly implemented (civil-from-days algorithm, BCD-safe) — they're just
  never fed a real value, because nothing converts a `CMOSTime` reading into
  a `struct tm`/`time_t` today.
- The PIT (`mods/dev/pit/pit.cpp`) runs at its uninitialized hardware
  default, ~18.2Hz — `pit_init(hz)` is declared-but-never-implemented
  (commented out in `pit.h`). So `timer_ticks` only has ~55ms resolution,
  which isn't "precise" by any reasonable bar.
- `CMOSDataFetch()` (`cmos.cpp`) reads all 128 CMOS registers in a loop with
  no "update in progress" check — the RTC can be mid-tick during a read,
  producing a torn value (e.g. seconds rolled over between reading the
  minutes and seconds registers). Real but rare in practice; worth fixing
  alongside "accurate."

#### Proposed approach

1. **Implement `pit_init(hz)`** — program the PIT's mode/frequency divisor
   registers (ports 0x40/0x43) for a chosen rate. Recommended **1000Hz**:
   it makes `timer_ticks` directly equal to elapsed milliseconds (no
   scaling math needed anywhere else), and it's the same change the
   multitasking proposal above wants for smoother preemption — one
   implementation, two consumers.
2. **Fix `CMOSDataFetch()`'s torn-read hazard** — check the RTC's
   Update-In-Progress flag (status register A, port 0x70/0x71 index 0x0A,
   bit 7) before reading, or read twice and retry if the two reads disagree.
3. **Add a real `time_t time(time_t*)`** — read `FetchCurrentCMOSTime()`,
   build a `struct tm` (combining `Time.century*100 + Time.year` for the
   full year — worth double-checking register 0x32/index 50 is really the
   century register on the QEMU CMOS being targeted, since it isn't
   universally standardized), and feed it through the **already-correct**
   `mktime()` to get real Unix seconds. This is the missing link — no new
   date math needed, just wiring existing pieces together.
4. **Add `uint64_t unix_millis(void)`** — capture a `(unix_seconds,
   timer_ticks)` reference pair once (e.g. at boot, via `time()` +
   current `timer_ticks`), then `unix_millis = reference_seconds*1000 +
   (timer_ticks - reference_ticks)` once PIT is at 1000Hz. Cheap, no CMOS
   read on the hot path (CMOS access is slow port I/O; you don't want every
   `unix_millis()` call hitting hardware). Worth resyncing the reference
   pair periodically (e.g. once a minute) since PIT tick-counting drifts
   slightly relative to the RTC over long uptimes.

#### Files touched

- `src/src/kernel/mods/dev/pit/pit.cpp` / `pit.h` — `pit_init(hz)`.
- `src/src/kernel/mods/dev/cmos/cmos.cpp` — Update-In-Progress guard.
- `src/src/kernel/mods/std/time.cpp` / `include/time.h` — real `time()`,
  new `unix_millis()`, fix `clock()` to mean something coherent (likely
  ticks-since-boot rather than time-of-day seconds).

#### Verification

- Print `time(NULL)` and `unix_millis()` to serial on a timer, confirm the
  seconds value matches real wall-clock time and millis increments smoothly
  and monotonically (no jumps backward, no stalls).
- Confirm `unix_millis() / 1000` stays consistent with `time(NULL)` across
  a multi-minute run (checks the resync logic doesn't drift the two apart).

### Proposal 3 (sketch only, not scoped): runtime disk-swap driver/module loading

**Not yet a real proposal — just a feasibility note. Nothing here is scoped
in detail or approved.**

#### Context

Idea: a Windows-9x-install-style flow where the kernel can prompt for a
disk swap after boot and load additional code (drivers, data) off it. A
targeted investigation found this is feasible but leans on pieces that
don't fully exist yet:

- **Floppy access after boot is not real-mode-only.** `src/boot/loader/floppy.cpp`
  is a real protected-mode FDC driver (direct port I/O to 0x3F0–0x3F7,
  handles seek/recalibrate/read), so re-reading the floppy after the switch
  to protected mode is architecturally fine — it isn't limited to BIOS INT
  13h. The one wrinkle: the kernel's current only call pattern
  (`test_load_flp`/`test_vfs_file_io` in `p-kernel.cpp`) disables paging
  around each call, which is likely a leftover from the bootloader code
  assuming physical==virtual addressing rather than a hard requirement —
  worth checking whether the bootloader-resident region is identity-mapped,
  which would remove the need to toggle paging at all.
- **FAT12 parsing only exists in the bootloader-resident stage2 blob**
  (`src/boot/loader/main.cpp`'s `read_file`/`fat12_next_cluster`), called
  from the kernel via a function pointer handed off at boot. This still
  works post-boot as long as that memory region stays reserved (not handed
  back to the physical allocator) and mapped — needs verifying, not yet
  confirmed either way.
- **No dynamic driver/module framework exists** — today's drivers
  (`ac97.cpp`, `rtl8139.cpp`) are statically linked in with hardcoded PCI
  class dispatch. But `mods/dev/elf/elf.cpp`'s existing `ET_REL`
  loader/relocator (used for `MAIN.ELF`) is the natural primitive to reuse
  for loading a driver blob off a swapped disk — same relocation problem,
  just a different entry-point symbol convention (`driver_init` instead of
  `_start`). `elf_lookup_symbol()` is currently stubbed to `return NULL`,
  which would need real implementation if a loaded driver needs to call
  back into kernel-exported functions rather than just using syscalls.
- **No other block-device driver exists** (no IDE/AHCI) — floppy would be
  the only swappable medium unless that's added too.

#### Sequencing question: can tasking (Proposal 1) come first?

**Yes — and it's the better order.** Two reasons:

1. A "wait for the user to swap the disk, then read it" flow is exactly the
   same shape of problem multitasking already exists to solve — a
   blocking, possibly slow operation that shouldn't freeze the whole
   system while it waits. Building the disk-swap flow before tasking exists
   means re-deriving the same blocking/EOI/re-entrancy patches this session
   already had to apply piecemeal to ELF execution; building it after means
   it can just be a task from day one.
2. Proposal 1's Phase 0 (added above) hardens malloc, the physical
   allocator, and VFS/ramfs with critical sections — all things a disk
   loader will need (buffers, writing loaded files into ramfs). Doing that
   hardening once, under Proposal 1, means Proposal 3 inherits safe
   primitives instead of needing its own pass later.

No known blocker requires Proposal 3 to land first or requires waiting on it
before starting Proposal 1.
