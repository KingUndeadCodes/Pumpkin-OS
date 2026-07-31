# Pumpkin-OS — TODO

Standing checklist of things to add/fix, ordered by recommended priority
(top = do next). Ring-0-only was the original design choice (like
TempleOS), but as of 2026-07-21 the project is deliberately moving toward
a three-tier ring-based privilege model (Level 0/kernel at ring 0, Level
1/trusted software at ring 1, Level 2/generic programs at ring 3, real
paging-enforced isolation for Level 2) — see "Recently completed" below
for Phase 0. Per-task memory isolation, listed elsewhere in this file as
permanently out of scope, refers specifically to *general-purpose*
per-process isolation of the kind rejected earlier in this project's
history, not to the narrower, now-active ring work.

**Next up:** "Priority 1 — Do next" is now fully done, and both
cursor-tearing items in "Priority 3" (the IRQ→task move, and the
hardware double-buffering fix for the framebuffer itself) are also done
— see "Recently completed" below. Phases 0 and 1 of the ring-based
architecture work
(ring-transition mechanism, then the syscall gate raised to DPL=3 with
`sys_read`/`sys_write`/`sys_open` hardened against a ring-3 caller) are
done and boot-verified; the next phases are: move `wingman`/`fontman`/
`mp3`/`chorus`/`Explorer` to ring 1, build real x86 call gates for Level 2
→ Level 1 (no gate at all for Level 2 → Level 0 hardware access), and only
then tackle Level 2 program internals (per-program memory region,
`elf_spawn()`'s ELF loader rework — `elf_exports[]` currently hands
ELF-loaded code raw kernel function pointers, bypassing even the existing
`int 0x80` path, and will need closing before this is safe). See
`docs/DOCS.md` ("mods/dev/gdt/gdt.cpp — ring-transition GDT/TSS (Phase
0)" and "mods/dev/syscall/syscall.cpp — ring-3 syscall gate (Phase 1)")
for the full design and what's already built.

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
(`p-kernel.cpp:258-267`, `mods/dev/pci/pci.cpp:236`'s `sprintf` format
string) included a trailing `\n`. `Terminal::write()` itself handles
`\n` correctly (confirmed by tracing it and by the "Welcome to
PumpkinOS" banner, which does include `\n` and rendered on its own
lines) -- the messages themselves were just missing it. Added `\n` to
each. Also fixed the same issue in `mods/dev/pci/drivers/rtl8139.cpp`'s
`Logging::log("[RTL8139] Waiting for transmit_ok ...")` call from item
7, found while checking for other call sites with the same bug.
`p-kernel.cpp:313`'s "Tasking Enabled!" message already had its own
trailing `\n` and uses `serial_write_string` directly rather than
`Logging::log`, so it was never affected by this bug -- it's live,
executed code (tasking has been on since 2026-07-14), not a
commented-out/disabled block as this note previously said.

### 11. Fix the `serialDevice` `LogDevice` function-pointer type mismatch — DONE

Found while chasing item 10's Terminal bug.
`p-kernel.cpp`'s `LogDevice serialDevice = { .log = &serial_write_string };`
assigned a 3-parameter function (`const char*, bool, enum Types`) to a
field typed `void (*log)(const char* message)` — default arguments don't
change a function's pointer type, so this was a genuine signature
mismatch. It compiled silently only because of `-fpermissive -w` in the
build flags. Every `Logging::propagate()` call ended up invoking
`serial_write_string` through a 1-argument-shaped call site, so the
function's own body read its 2nd/3rd parameters (`time_show`, `Type`)
as whatever garbage happened to be on the stack at those offsets — a real
ABI violation, not cosmetic: the timestamp and severity coloring on every
log line reaching serial through this path (the early-boot captured
buffer replayed via `Logging::flush()`, plus any `Logging::log()` call
after device registration) were driven by uninitialized stack data.
`terminalDevice`'s `&terminal_write` was already fine (its actual
signature is `void terminal_write(const char* str)`, matching exactly).

Fixed by adding `serial_log_adapter(const char* message)`
(`mods/dev/serial/serial.h`/`.cpp`, next to `serial_write_string` itself)
— a real `void(const char*)`-typed thin wrapper forwarding to
`serial_write_string(message)` with its normal defaults — and pointing
`serialDevice.log` at it instead, mirroring the pattern that already
worked for `terminal_write`. Boot-tested: every log line through the
`LogDevice`/`propagate()` path now shows consistent, correctly-formatted
`[timestamp] INFO - message` output (verified against the 3 real call
sites: the early-boot buffer replay, `"Jumped to kmsglog"`,
`"Hello, World!"`), no crash, normal boot completion.

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
`docs/FULL.md` — not previously tracked anywhere. `boot/loader/main.cpp`'s
`read_file_frontend` (a closure-like wrapper around the bootloader's own
FAT12 reader, bound to the already-open boot disk state) gets handed to
the kernel as its `read_file` function-pointer argument to
`kernel_main()` — this is how the kernel keeps reading arbitrary files
off the boot floppy post-boot without needing its own independent FAT12
implementation.

Traced end to end (2026-07-13) and found a real hazard, not just a
documentation gap: the pointer used to cross from
`boot/loader/main.cpp` into `boot/loader/entry.asm` via a
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

## Priority 2 — Phase 0 hardening (tasking Proposal 1)

Real multitasking is proposed in `agile-napping-stallman.md` (each ELF run
becomes its own preemptible task). Given an explicit go-ahead (2026-07-08),
**Phases 0, 1, 2, and 3 are all now done and boot-verified** — see "Full
tasking proposal" further down for the whole checklist. Tasking has been
on since 2026-07-14; this section's items below (Phase 0 hardening) were
the prerequisite work, kept here for the record. Task/stack reaping (the
one item that was still open after Phase 3) is now also done, as of
2026-07-20 — see "Full tasking proposal"'s Phase 2 notes.

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
- ~~Per-task memory isolation (ring 3, per-task page tables) — out of
  scope, same boundary as everything else. Ring 0 only, by design.~~ —
  **superseded 2026-07-21.** The project is now deliberately pursuing a
  ring-based privilege model; see "Recently completed" below for Phase 0
  (ring-transition mechanism proven) and this file's intro for the
  phase roadmap. Real per-task page-table isolation specifically for
  Level 2 programs is a later phase (after syscall DPL + Level 1 move +
  call gates), not yet started.

---

## Miscellaneous — parked, revisit later

### `README.md` still says "Everything runs in ring 0"

Flagged 2026-07-21 (user). `README.md:3` — "no ring 3. Everything runs in
ring 0, single address space, TempleOS-style, by design." — is now stale
given the active ring architecture work (see "Recently completed" /
[[project-ring-architecture]] equivalent). Not updated yet on purpose:
Phase 0's pilot ring-3 task is a throwaway proof-of-concept, not a real
subsystem running outside ring 0, so the claim is still substantively true
today. Update once a real phase (Level 1 move, or Level 2 programs
actually running) makes it genuinely false, not before.

### `KERNEL_LOCATION` (`0x400000`) is independently hardcoded in three places

Found (2026-07-13) while working item #14 — same class of hazard as that
item's `READ_FUNCTION_ADDRESS` bug, but worse blast radius. `0x400000` is
defined three separate times with no build-time link between the copies:

- `boot/loader/main.cpp:5` — `#define KERNEL_LOCATION 0x400000`
  (where the bootloader writes `KERNEL.BIN` and jumps to start it)
- `boot/loader/entry.asm:5` — `KERNEL_LOCATION equ 0x400000` (second
  independent copy of the same value)
- `kernel/kernel.ld:6` — `. = 0x400000;` (tells the linker what base
  address the *compiled kernel itself* assumes for every internal
  absolute symbol/global/jump)

If these three ever drift apart, the bootloader loads the kernel image
to one address while the kernel's own code was linked assuming a
different one — unlike the `read_file` pointer bug (one broken function
pointer), this would make essentially every symbol reference in the
entire kernel binary wrong at once. Realistically an instant
triple-fault or silent garbage execution, not something with a
debuggable symptom.

Also noticed in passing: `boot/stage1.asm:7` defines its own
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

### Cursor movement feels unnaturally fast after the `memcpy()` fix

User feedback (2026-07-24), right after the hardware double-buffering +
`rep movsl` `memcpy()` work landed: real 1:1 PS/2 tracking, no longer
bottlenecked by the byte-at-a-time copy, reads as *too* fast/twitchy on
real hardware compared to what feels natural. Not a bug — the driver is
now reporting motion accurately and promptly for the first time; this is
purely a feel/ergonomics question, the inverse problem of the slowness
just fixed.

Needs a deliberate, artificial slowdown somewhere in the mouse pipeline
(most likely scaling down the raw PS/2 delta in `mods/dev/mouse/mouse.cpp`
before it reaches `update_mouse_position()`/`redraw_cursor()`, rather than
touching the now-fast present path that was just fixed) — a fixed
divisor/multiplier on `dx`/`dy` would be the simplest first cut.

- **Optional subtask**: make the slowdown factor variable rather than a
  single hardcoded constant — a user-facing "mouse sensitivity" setting
  (e.g. a multiplier the user can adjust, persisted somewhere and read at
  cursor-update time) instead of one fixed value baked in. Lower priority
  than just picking a single reasonable constant first; only worth doing
  once there's an actual settings/config surface for a user to adjust it
  through, which doesn't exist yet anywhere in this codebase today.

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

- ~~`mods/dev/vbe/vbe.cpp` `fill()` — calls `draw_pixel()` per pixel for a
  full-screen fill instead of one linear fill over the contiguous
  framebuffer.~~ — DONE. `fill()` now writes directly through a linear
  `uint32_t*` framebuffer pointer, no `draw_pixel()` call. See
  `docs/DOCS.md` ("`mods/dev/vbe/vbe.cpp` — bulk framebuffer operations").
- ~~`vbe.cpp` `draw_char()` — up to 1024 individual `draw_pixel()` calls
  per glyph at the default scale.~~ — DONE, same fix, same DOCS.md
  section.
- ~~`mods/dev/console/console.cpp` `Terminal::scroll()` — scrolls via
  `get_pixel()`/`draw_pixel()` per pixel across the whole 1024x768
  framebuffer (~780K operations).~~ — DONE. Now calls a new
  `scroll_framebuffer_up()` (`vbe.cpp`), which uses `memmove()`.
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

- `boot/loader/floppy.cpp` / `libboot.cpp` — every single 512-byte
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
  mismatch, not a proper comparison result); `memset` is still
  byte-at-a-time with no word-sized fast path. Both are extremely hot
  (VFS lookups, ELF symbol search, every buffer op). `memcpy` itself is
  fixed — see "Recently completed" below.

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

### Positioned constructor overloads for Wingman widgets — DONE (2026-07-30)

`Button`, `TextInput`, `Checkbox`, `Slider` each gained a second constructor
overload accepting trailing `int x, int y`, so callers can position at
construction time (`new TextInput(64, "Type here...", 170, 105)`) instead of
setting `->x`/`->y` on two follow-up lines. Implemented as C++11 delegating
constructors, not duplicated bodies -- each positioned overload just calls
the original and overwrites `x`/`y`. Migrated all 5 real candidates in
`WidgetDemo::WidgetDemo()`; deliberately left `MessageBox::addButton()`'s
`Button` untouched, since its position is computed later by `layoutButtons()`
rather than known at construction. See `docs/DOCS.md`
("mods/core/wingman/headers/widgets/*.h -- positioned constructor
overloads"). Boot-tested: WidgetDemo renders pixel-identical to before, and
all five widgets (button click, checkbox, toggle, text input, slider drag)
confirmed still functional at their new positions via serial log
(`[WidgetDemo] Button clicked`, `Checkbox checked`, `Toggle on`, `Slider
value: 13`), no serial errors.

Follow-up (same day): `TextInput` alone gained a third overload appending
`int width, int height`, chaining onto the positioned overload above it.
Not added to `Button`/`Checkbox`/`Slider` -- those compute their own
width/height internally and nothing overrides it, so adding an override
param would be capability no call site exercises. `TextInput` is the only
widget with no self-computed size, so this directly replaces the
`->width`/`->height` lines that were still following its constructor call
in `widgetdemo.cpp`. Re-verified: clean build, pixel-identical boot
screenshot, no serial errors.

### Wingman's duplicated drawing helpers consolidated — DONE (2026-07-30)

Wingman had 13 definitions of four drawing helpers spread across six files,
most byte-identical copy-paste. Consolidated to 4 free functions in one new
TU (`mods/core/wingman/draw.cpp` + `headers/draw.h`), taking `Surface*` as
the first parameter — the shape `titlebar.cpp`/`button.cpp`/`textinput.cpp`
had already independently converged on. Named `surface_draw_*` to avoid
silently overloading `vbe.h`'s same-named raw-framebuffer globals.

Motivation was concrete, not stylistic: the `0x10` transparency bug in
`utility_draw_icon` existed in every copy (propagated by the copy-paste),
and the duplication blocked adding bounds-checking to the `putPixelUnsafe`
path — the fix for the class of bug that has now caused three separate
incidents. Both are single-site changes now.

Deliberately kept as a pure move — no behaviour changes bundled in. Bodies
were copied verbatim, **including the `0x10` bug**, so the diff is reviewable
as a mechanical migration. Measured `kernel.bin` 648,580 → 621,540 bytes
(27,040 saved) since `draw.cpp` is now the only wingman TU pulling the large
`Icons[]`/`Font[]`/palette tables. Boot-tested: all four windows render
identically to the pre-refactor screenshot, Explorer navigation still updates
its path title, MessageBox still dismisses via its close button, no serial
errors. See `docs/DOCS.md` ("mods/core/wingman/draw.cpp / headers/draw.h --
shared drawing helpers").

**Follow-ups this unblocks** (deliberately not done here):
- Fix the `0x10` transparency branch (missing `continue`/`else`) — now one site.
- Add bounds-checking/clipping to `surface_draw_*` — now one site. This is the
  real prize; see the Windows/macOS discussion about making the title band
  genuinely unwritable rather than incidentally-overwritten.
- Four hand-rolled per-character loops (`explorer.cpp` ×2, `message.cpp`,
  `titlebar.cpp`) remain unmerged with `surface_draw_string` on purpose —
  their clamping logic genuinely differs (column width, page-indicator
  length, word wrap, `maxChars`), so folding them would be a behaviour change.

### Title-bar drawing decoupled from each app's own `redraw()` — DONE (2026-07-29)

`TitleBar::draw()` was removed as a class method in favor of a free
function, `draw_title_bar(Surface*, windowWidth, const TitleBar&, const
char* title)` in `titlebar.cpp` (a `friend` of `TitleBar`, so it can
still reach the private fields `configure()` sets). `WindowManager::composite()`
now calls it for every window on every composite pass, using
`window->title` -- not from any app's own `redraw()` -- so the title
band stays correct even if an app never redraws it itself. `Window`
gained `setTitle(const char*)` so apps with dynamic titles (a file path,
say) can update it without needing a title-bar-specific redraw of their
own. This broke the build (`message.cpp` still called the now-gone
`titleBar.draw(...)`), which also exposed a real design problem in how
`MessageBox` drew its dialog icon+text: since composite() now redraws
the band on *every* pass regardless of which app triggered it (a focus
change, a drag), `MessageBox`'s old hand-drawn icon+text overlay -- only
repainted when `MessageBox` itself called `redraw()` -- would get erased
by an unrelated composite pass and not reappear until `MessageBox`
redrew again. Fixed by migrating `MessageBox` onto the same
`hasIcon`/`iconId`/`window->title` mechanism `FileManager` already used
for its folder icon, instead of hand-drawing over the band; `MessageBox`
no longer owns any title-band drawing code at all now (`draw_title()`
and its private `utility_draw_icon()` helper were deleted). See
`docs/DOCS.md` ("mods/core/wingman/headers/titlebar.h /
mods/core/wingman/window.h -- TitleBar owned by Window" and
"mods/core/wingman/suite/message/message.h / message.cpp -- MessageBox
close button and title") for the full writeup. Boot-tested via QEMU:
all three windows render their title bars (including `MessageBox`'s
icon+"Information"/"Error"/"Warning" text) correctly on first paint,
dragging `MessageBox` by its title bar keeps the icon+text intact and
in sync with the window across every drag-throttled composite pass
(the exact case that used to be at risk), and the close button still
dismisses the dialog cleanly with no serial errors.

Follow-up (same day, caught by `/code-review`): the migration above had
three more real problems, all fixed and re-boot-tested. (1) `FileManager`
lost a previously-working feature -- its title bar used to show the
current directory path (`this->path`, or `"/"` at root), via its own
now-deleted `draw_title()`; nothing wired that into the new
`Window::setTitle()` mechanism, so it was stuck on the static "File
Manager" string. Fixed with a small `updateTitle()` helper called from
the constructor and from both branches of `fileClick()`'s directory
navigation. (2) `WindowManager::composite()` called `draw_title_bar()`
for every window *before* checking whether that window's rect
intersected the dirty region, so every composite pass redrew every
window's full title band regardless of what actually changed -- exactly
the cost the dirty-rect plumbing (`docs/DOCS.md`, "Rect / dirty-rect
compositing") was built to avoid. Fixed by moving the call after the
`rect_empty(region)` check. (3) `draw_title_bar()`'s available-width
calculation for clamping title length could go negative for a
sufficiently narrow window/wide icon+button zone, and casting that
straight to `size_t` for the maxChars division would wrap to a huge
value instead of clamping to 0, letting a title draw off the right edge
of the window's surface via bounds-unchecked `Surface::putPixelUnsafe`.
Fixed by clamping `availableWidth` to `0` before the division. No
shipped window configuration currently triggers (3), but nothing
prevented a future one from doing so. See `docs/DOCS.md`
("mods/core/wingman/suite/explorer/explorer.cpp -- title bar shows the
current path") for the full path-title writeup. Boot-tested via QEMU:
Explorer's title reads `/` at the root and updates live to `/hello` on
entering that directory and back on navigating up via `".."`, with no
regressions in any of the other three windows.

### Minimize (yellow) / maximize (green) buttons added, purely cosmetic — DONE (2026-07-29)

Extended `TitleBar::draw()` to render the full macOS traffic-light trio
next to each other whenever a window has a close button, not just the
red one -- yellow and green dots, same size/border style, in their own
slots immediately to the right with no gap (matching macOS's tight
spacing). Deliberately *not* functional yet: `closeButtonContains()`
still only tests the red slot, so clicking yellow/green does nothing
(falls through to the delegate, hits no widget). `closeButtonZoneWidth()`
now covers all three slots so the window-dragging carve-out in
`wingman.cpp` correctly treats the whole trio as non-draggable, not just
the working button. `MessageBox`'s hand-drawn icon offset (it doesn't use
`TitleBar`'s icon/text layout -- see below) now derives from
`titleBar.closeButtonZoneWidth() + 8` instead of a hardcoded number, so
it can't silently drift out of sync with `TitleBar`'s own geometry the
way earlier close-button code did. See `docs/DOCS.md`
("mods/core/wingman/headers/titlebar.h / mods/core/wingman/window.h --
TitleBar owned by Window") for the full writeup. Boot-tested across all
three windows: all three dots render with correct spacing and no overlap
into icon/title text, the close button still closes correctly (confirmed
via ground-truth `serial_write_string` logging after several visually-
correct clicks didn't register -- the same synthetic-input flakiness
from earlier close-button testing, not a hitbox bug), and dragging still
works from just past the wider 3-dot zone.

### `MessageBox` gets a close button too — DONE (2026-07-29)

Follow-up to the `TitleBar` work below, which deliberately left
`MessageBox` untouched since it already had a working dismiss path.
Gave it a real close-X to match every other window: `MessageBox` now
calls `titleBar.configure(64, thickness, true, 0)` and
`titleBar.draw(surface, width, nullptr)` with `hasIcon=false` and
`title=nullptr`, so `TitleBar` contributes only the band fill, divider,
and close button -- `MessageBox` keeps drawing its own dialog icon
(32x32 at a fractional 1.5x scale) and title text itself, since those
use a different Y for icon vs. text and a bigger/fractionally-scaled
icon than `TitleBar`'s model (built for Explorer's small integer-scale
folder icon) supports. Both got shifted right (`iconX = thickness + 38`)
to clear the new button, preserving the original icon-to-text gap.
Registration is the same trampoline pattern as WidgetDemo/Explorer, just
calling the existing `dismiss()`. See `docs/DOCS.md`
("mods/core/wingman/suite/message/message.h /
mods/core/wingman/suite/message/message.cpp -- MessageBox close button")
for the full writeup. Boot-tested via QEMU with ground-truth
`serial_write_string` logging (the same synthetic-click flakiness from
earlier close-button testing showed up again -- several visually-correct
clicks didn't register before one did, confirmed via logged
coordinates that the hitbox math itself was correct throughout): the
close button renders and closes the dialog correctly, and the existing
Initialize/Ignore buttons are unaffected.

Follow-up: the dialog icon was later resized from 1.5x scale (48x48) to
1x (32x32, `utility_draw_icon(iconX, 18, iconType, 1.0f)`) to match
Explorer's folder icon size exactly, with `textBase = iconX + 42`
adjusted to preserve the original ~10px icon-to-text gap at the smaller
width. Boot-tested: icon renders at the smaller size with no overlap
against the close button or title text.

### Window closing support, standardized via a `TitleBar` class on `Window` — DONE (2026-07-29)

WidgetDemo and Explorer had no way to close at all (they're spawned once
at boot and lived forever); `MessageBox` already could via its own
`dismiss()`. Added a macOS-style circular close button (top-left, red
dot) to both, mirroring `MessageBox::dismiss()`'s `wm->remove(ref);
this->window = NULL; delete this;` pattern. Explorer needed a small
structural change first (adding the `WindowManager*`/`ref` pair
WidgetDemo already had).

The close button's geometry ended up hand-duplicated across both apps
*and* independently copied into `wingman.cpp`'s window-dragging logic (a
click in a window's drag-handle band has to be carved out from "start
dragging," or the button is visible but unclickable). That duplication
caused two real bugs during boot-testing: an off-by-`thickness` gap in
the carve-out width, and a top-right-vs-top-left mismatch after the
button's position moved. Both were only diagnosable via temporary
`serial_write_string` ground-truth logging, since QEMU's synthetic
`mouse_move` isn't reliably 1:1 across boots and a screenshot alone
can't tell "hovering but click didn't register" from "not actually
hovering."

Fixed by extracting a `TitleBar` class that `Window` now owns as a
plain member, handling the title band's drawing, hit-test, and (new)
the close click itself: `Window::handleMouse()` intercepts clicks on the
button directly and fires a registered `onCloseRequested(void*)`
callback (mirroring `Button`'s own callback pattern), so WidgetDemo/
Explorer no longer contain any close-button code at all -- just a small
per-class `static` trampoline registered once in the constructor.
`wingman.cpp`'s drag carve-out now queries the real `TitleBar` instance
instead of a hand-copied constant, closing off that whole class of
drift. See `docs/DOCS.md` ("mods/core/wingman/wingman.cpp -- closing
windows" and "mods/core/wingman/headers/titlebar.h / mods/core/wingman/
window.h -- TitleBar owned by Window") for the full writeup, including a
latent const-correctness gap this surfaced and fixed in `wingman.cpp`
(previously masked by `-fpermissive -w`). Boot-tested: both close buttons
render and close correctly, hover shows the pointer cursor, dragging from
outside the close-button zone still works, and a full boot with all
remaining windows (including untouched `MessageBox`) renders correctly.

This work was originally built and tested against three apps -- the
above two plus Calculator -- but Calculator was removed shortly after
(see below), so this entry and the `TitleBar` docs describe the
as-shipped, Calculator-free state.

### RTL8139 receive driver dropped nearly all packets in a burst — DONE (2026-07-28)

`RTL8139_HANDLER()` (`mods/dev/pci/drivers/rtl8139.cpp`) only ever called
`RTL8139_RECEIVE_PACKET()` once per interrupt. Multiple packets arriving
close together get coalesced into a single ROK interrupt, so anything
past the first packet sat undrained in the ring forever (measured: 30
packets sent back-to-back, 1 received). An earlier attempt to fix this
by looping `RTL8139_RECEIVE_PACKET()` while the Command register's BUFE
bit was clear made things much worse (~399,000 "Unknown packet type
detected" log lines) and was reverted rather than shipped.

The real root cause wasn't the missing loop by itself: `RX_BUFFER_SIZE`
was set to `16 * 1024`, but `RCR_DEFAULT` never sets the RBLEN ring-size
bits, so the hardware is actually configured for the smallest ring (8K+16,
RBLEN=00) — the driver's own wraparound math was tracking a 16KB ring the
NIC was never using. The buffer was also allocated with zero slack past
its nominal size, even though `RCR_WRAP` is set (telling the NIC it's
allowed to DMA a packet's tail past the ring's nominal end rather than
splitting it). Lightly-loaded single-packet traffic rarely reached the
wraparound boundary, which is why this stayed latent until the earlier
drain-loop attempt pushed more data through per interrupt and hit it hard.

Fixed by correcting `RX_BUFFER_SIZE` to `8192` (matching the actually
configured 8K+16 ring) and allocating `RX_BUFFER_SIZE + 1500 + 16` bytes
of real DMA memory instead of exactly `RX_BUFFER_SIZE`, then reintroducing
the per-interrupt drain loop (Command register BUFE bit, bounded to 64
iterations as defense in depth). See `docs/DOCS.md`
("mods/dev/pci/drivers/rtl8139.cpp — receive ring size/allocation
mismatch") for the full writeup. Boot-tested via QEMU's UDP tunnel
netdev: a 30-packet zero-delay burst now delivers 30/30 with zero garbage
reads; a 300-packet burst still loses some packets, but confirmed (via
the QEMU network-filter pcap dump) that the loss happens before the
frames even reach the emulated NIC -- an inherent limitation of QEMU's
`-netdev socket` backend (plain, unreliable UDP over loopback), not the
guest driver. No "Unknown packet type detected" spam in any test.

### Calculator app added to Wingman, then removed — DONE (2026-07-27, removed 2026-07-29)

New `Calculator` class, the first real (non-demo) app built on the
`Button`/`Slider`/etc. widget set -- a basic 4-function calculator
(add/subtract/multiply/divide, decimal point, clear, left-to-right
evaluation, no operator precedence) in a 4x5 button grid, spawned at
boot alongside `FileManager`/`WidgetDemo`. Boot-tested via QEMU mouse
injection: multi-digit entry, an operator, and `=` all verified correct
(`23 + 7` -> `30`), plus the divide-by-zero `Error` path and its recovery
via `C`. Later picked up a macOS-style close button and a `TitleBar`
migration alongside WidgetDemo/Explorer (see above).

Removed on 2026-07-29 as out of place among the other suite apps --
deleted `mods/core/wingman/suite/calculator/` entirely, along with its
`wingman.cpp` spawn call and Makefile build/link rules. `FileManager`,
`MessageBox`, and `WidgetDemo` are unaffected.

### Slider widget added to Wingman — DONE (2026-07-26)

New `Slider` class (`mods/core/wingman/widgets/slider.cpp`) alongside
`Button`/`TextInput`/`Checkbox`, built to the same `Widget` base-class
shape. Wired into `WidgetDemo` as a fourth demo row. See `docs/DOCS.md`
("mods/core/wingman/widgets/slider.cpp") for the full design writeup,
including why it needs a `buttons`/`pressedEdge`-aware `onMouse()` called
on every event (not just clicks) unlike the other three widgets.
Boot-tested via QEMU monitor mouse injection: renders correctly, drags
correctly (thumb + fill track the cursor while held), a plain hover with
no button held leaves the value untouched, click-elsewhere jumps to that
position, and both ends of the min/max range clamp correctly (confirmed
via the serial-logged `onChange` callback values).

### Ring-3 syscall gate (Phase 1 of ring-based privilege model) — DONE (2026-07-21)

Follow-up to Phase 0: raised the `int 0x80` syscall gate from DPL=0 to
DPL=3 so a ring-3 task can actually reach the agreed safe-list syscalls
(file ops, task control). The DPL flip itself was a one-line, mechanically
self-contained change (`mods/dev/idt/irq.cpp:53`) — but research found it
would have been a real vulnerability on its own: `sys_read`/`sys_write`
did zero validation of their `buf`/`size` arguments, and `sys_open`
trusted `path` as an unbounded C string. Since the syscall handler always
runs at ring 0 once inside the trap, and CPL0 is exempt from the CPU's
own U/S page check, simply raising the DPL would have handed any ring-3
caller a genuine arbitrary-kernel-memory-read (`sys_write`) and -write
(`sys_read`) primitive.

Fixed with a new `is_user_accessible()` (`paging.cpp`) that replicates
the hardware U/S check in software (checking both PDE and PTE, the same
asymmetry Phase 0's `mark_region_user()` already had to handle), gated on
`g_current->ring == 3` so ring-0/ring-1 callers see zero behavior change.
Verified concretely, not just reviewed: temporarily extended the Phase 0
pilot task with two real `int $0x80` attempts — a valid buffer succeeded
(`[pilot] ok` on the serial log), an invalid buffer pointed at
`page_directory` itself was rejected with no leaked bytes and no crash,
confirmed via screenshot that the rest of the system stayed fully alive
afterward. Both attempts were temporary verification wiring, since
removed — `pilot_ring3_fn` is back to its bare Phase 0 counter loop; only
the DPL flip, `is_user_accessible()`, and the ring-gated hardening stay.
Full writeup, including two things confirmed explicitly out of scope
(`elf_exports[]`'s bypass, the syscall fd table's missing per-task
ownership): `docs/DOCS.md` ("mods/dev/syscall/syscall.cpp — ring-3
syscall gate (Phase 1)").

### Ring-transition mechanism (Phase 0 of ring-based privilege model) — DONE (2026-07-21)

First step of a deliberate, agreed pivot away from ring-0-only: a
three-tier model (Level 0/kernel at ring 0, Level 1/trusted software at
ring 1, Level 2/generic programs at ring 3 with real paging-enforced
isolation). Phase 0's scope was narrow and foundational: prove the actual
ring-transition mechanism works end-to-end, with a single throwaway pilot
ring-3 task, and zero behavior change to anything that existed before it.

Built from scratch since none of it existed: a new GDT (`mods/dev/gdt/`,
8 descriptors — the bootloader's original 3-entry GDT can't host a TSS
and is left untouched, this one supersedes it via a fresh `lgdt` at the
top of `kernel_main()`), a TSS (`ltr`'d once, `ESP0` kept current on every
context switch), ring-aware `task_create()` (a 5-value `iret` frame for
ring-3 targets vs. the existing 3-value one, needing zero changes to
`irq_common_stub` — the CPU is what's ring-aware there, not the stub), and
`mark_region_user()` (the `PAGE_USER` flag existed unused in `paging.h`
since day one; the real work was a PDE-vs-PTE gotcha where an
already-present page directory entry silently blocks ring-3 access even
with the leaf PTE correctly marked).

Two real bugs surfaced and fixed while building the pilot task itself —
see `docs/DOCS.md` ("mods/dev/gdt/gdt.cpp — ring-transition GDT/TSS
(Phase 0)") for both in full, plus the exact GDT/TSS byte layout and
verification detail. Boot-tested in 5 incremental steps; final state
confirmed via a temporary counter that rose steadily across repeated
serial prints while the rest of the system (compositor, mouse/keyboard,
hover cursor) stayed fully responsive — real evidence of preemption and
resumption working correctly, not just "didn't crash."

### Idle task (`task0`) taken out of round-robin rotation — DONE (2026-07-21)

Direct follow-up to the IRQ→task change below: moving mouse/keyboard
compositing off the IRQ path made the whole desktop noticeably slower,
not faster. Root cause: `task0` (`for(;;) hlt;`) was a real round-robin
participant, on equal footing with the new input-worker task — no
priority scheduling exists, `pick_next()` just gives one task per 1ms
tick. So every other tick now went to an idle task doing nothing, instead
of draining queued input/compositing work — roughly halving GUI
throughput compared to the old exclusive-IRQ-context handling. Fixed by
taking the idle task out of rotation entirely: `task_create()` gained an
`enqueue` parameter (default `true`) so `tasking_spawn_idle()` can build
it without ever pushing it onto `g_runqueue`, and `pick_next()` now falls
back to it directly only when nothing else is `TASK_READY`. Also fixed a
latent bug this surfaced: `pick_next()`'s old fallback (`return cur`) was
only ever safe because `task0` guaranteed there was always a real
alternative — with that gone, a task that just called `task_block()`
(the reaper, the input worker) could have been resumed as if it had never
blocked at all, since nothing checked whether `cur` was still actually
runnable before falling back to it. `pick_next()` now checks `cur`'s
state explicitly. See `docs/DOCS.md` ("mods/dev/tasking/tasking.cpp —
idle task") for the full writeup.

### Moving mouse/keyboard IRQ work out of interrupt context (cursor tearing) — DONE (2026-07-21)

Mouse/keyboard IRQ handlers (interrupt gates, `IF` cleared for the whole
handler) used to call `mouseFunctionWindowManager()`/
`keyboardFunctionWindowManager()` directly — real `WindowManager::composite()`
blending and a raw `memcpy` into the live framebuffer, synchronously, with
interrupts fully masked for the duration. One of two causes of cursor
tearing (the other, no double-buffering/vsync at the hardware level, was
a separate problem at the time — closed 2026-07-24, see "Hardware double
buffering" below). Fixed by reusing the `task_block()`/
`task_wake()` primitive the task/stack reaper already established: a
bounded ring-buffer queue of input events in `mods/core/wingman/wingman.cpp`,
two thin push functions (`queueMouseEventForWingman()`/
`queueKeyEventForWingman()`) now registered with `mouse_add_event()`/
`kb_add_event()` in place of the heavy functions, and a dedicated worker
task (`wingman_input_worker_fn()`, spawned via
`wingman_spawn_input_worker()`) that drains the queue and calls the
original, unchanged heavy functions from task context instead. Also
removed a now-stale `sti`/comment in `explorer.cpp`'s MP3 playback path,
which existed only to compensate for the interrupt-gate's cleared `IF` and
no longer applies once that code runs from the worker task. See
`docs/DOCS.md` ("mods/core/wingman/wingman.cpp — input queue / worker
task") for the full design writeup.

Boot-tested (2026-07-21): normal boot completion with no fault-handler
output, mouse click/focus/drag and keyboard selection both verified
working end-to-end through the new queue+worker path via screenshot
comparison, and a 40-event rapid-fire mixed mouse/keyboard burst produced
no crash, hang, or stuck state. MP3 playback itself couldn't be
conclusively verified in the headless QEMU test environment (no audio
backend was configured, and `play_mp3()` failed early with "AC97: cannot
start; missing DMA buffer or stream info" — most likely a test-environment
gap, not a regression, since this project's `wingman.cpp`/`ac97.cpp`
changes are unrelated to DMA/stream setup) — but the system stayed fully
responsive throughout the attempt, and the `sti` removal is sound by
construction regardless, since the interrupt-gate constraint it existed to
work around no longer applies once the call runs from task context.

### `memcpy()` byte-at-a-time loop (`mods/std/string.cpp`) — DONE (2026-07-24)

Found immediately after landing hardware double buffering (below): the
double-buffering design requires every present, including a plain cursor
move, to copy a full ~3MB frame via `memcpy()` — and this project's
`memcpy()` was a plain `for` loop doing one `unsigned char` store per
iteration (already flagged as a known hot path in "Boot/memory
performance" above, but not yet fixed). At PS/2's packet rate that's tens
of megabytes a second through the slowest possible copy shape, and the
user confirmed on real hardware it made cursor movement "really freaking
slow" — bad enough to be worth reverting double buffering entirely, before
this fix instead addressed the actual bottleneck.

Fixed with `rep movsl` inline assembly: copies `size / 4` words via the
x86 string-copy instruction (no compiled loop-branch overhead per
iteration, unlike the byte loop), then finishes any 0-3 trailing bytes
with the original byte-at-a-time copy. `cld` first to guarantee the
forward direction regardless of `DF`'s state on entry, since nothing in
this freestanding kernel can be assumed to uphold the normal ABI
guarantee that `DF` is always clear. No SIMD available (`-mno-sse
-mno-sse2 -mno-mmx`), so this is the fastest option without hand-rolling
a wider software copy. `memset`/`strcmp` in the same file are still
byte-at-a-time — separate, not fixed here (see "Boot/memory performance"
above).

Boot-tested: full desktop render pixel-identical (fonts, RAMFS copy,
every other `memcpy()` call site in the boot path all still correct),
and cursor movement through the double-buffered present path re-verified
artifact-free. Real-hardware throughput improvement itself isn't
provable headlessly (same limitation as double buffering's tear-proof,
above) — architecturally this cuts the ~3MB copy from ~3.1M single-byte
stores (each with loop overhead) to ~780K hardware string-op iterations.

### Hardware double buffering (cursor tearing/flicker, the hardware half) — DONE (2026-07-24)

The other cause of cursor tearing flagged as still-open in the IRQ→task
item above: no double-buffering/vsync at the framebuffer level, so every
present wrote directly into the live, currently-displayed VRAM. Fixed via
the BGA/VBE `dispi` interface's page-flipping registers
(`VBE_DISPI_INDEX_VIRT_HEIGHT`/`_X_OFFSET`/`_Y_OFFSET`, defined in `vbe.h`
but never used before this): VRAM doubled to two regions, `vbe_flip()`
atomically switches which region is displayed via a single `Y_OFFSET`
write, and every present (`wingman.cpp`'s `redraw_screen()`) now composites
a full frame into the back region before flipping — real page-flipping
means a partial patch can't be safely flipped into view. This supersedes
the hardware-blit half of "Dirty-rect compositing" below (the
`Rect`-driven `composite()` optimization itself is unaffected — only
`redraw_screen_rect()`, which is now a thin full-frame wrapper). Also
fixed the panic screen (`isr.cpp`'s `draw_panic_screen()`) to force
`Y_OFFSET` back to 0 as its first line, since a panic can now fire while
the non-zero region is displayed. See `docs/DOCS.md` ("mods/dev/vbe/
vbe.cpp — hardware double buffering") for the full design writeup,
including the QEMU register behavior this was verified against.

Boot-tested incrementally (VRAM doubling alone, then the flip mechanism
in isolation via a temporary register-readback self-test, then wired
through `wingman.cpp`/`cursor.cpp`) and end-to-end: full desktop render,
click/focus, window drag, keyboard input, and MessageBox interaction all
confirmed working through the new flip-based present path; the panic
screen confirmed visible (not blank) when triggered after at least one
flip. Genuine tear-elimination is a real-time scanout property no static
screenshot or serial log can prove headlessly — see the DOCS.md writeup's
verification section for what is and isn't provable this way.

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
  shipped at `src/bin/fonts/`, alongside this project's other binary assets,
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
`test_fault_handler()` hook (`p-kernel.cpp`) once existed to deliberately
trigger a #BP then a #PF, exercising both paths manually — it was removed
during tasking Phase 2 cleanup and no longer exists. The fault-handler
paths it was meant to exercise have since been boot-tested end-to-end
anyway (a real forced page fault against a deliberately unmapped address,
confirming `draw_panic_screen()`'s rendered output is pixel-correct) —
see `docs/DOCS.md` ("`mods/dev/vbe/vbe.cpp` — bulk framebuffer
operations", verification note).

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

`mods/core/wingman/suite/explorer/explorer.cpp` (WAV handler, now at
line 398) had `uint32_t buffer_len = 52640;` — a fixed size instead of
the real file
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
  state or `stack_base` field (Phase 2/3 still need to add both). Both
  now exist -- `TASK_BLOCKED` landed with Phase 3, `stack_base` with the
  task/stack reaper (2026-07-20).
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
  it's intentionally leaked for now (see `docs/DOCS.md`) — this ELF
  *file buffer* leak is still open (separate from the task-stack/
  task_t-slot leak below, which is now fixed by the reaper).
- ~~**Known, deliberately deferred**: neither the malloc'd task stack nor
  the task_t slot... is ever reclaimed when a spawned ELF task exits —
  there's no reaper yet.~~ — DONE (2026-07-20). Added a `stack_base`
  field to `task_t` (NULL for statically-allocated stacks like
  `task0`/`task1`/`task2`, set by `elf_spawn()` for dynamically-allocated
  ones) and a dedicated reaper task (`tasking_spawn_reaper()`,
  `mods/dev/tasking/tasking.cpp`), woken via `task_wake()` from
  `task_exit()`, that unlinks dead nodes from the runqueue, frees their
  stack, and clears their `g_tasks[]` slot. Found and fixed a real
  prerequisite gap along the way: `elf_spawn()`'s own `task_create()`
  call wasn't wrapped in `sched_lock()`/`sched_unlock()`, the same race
  class already fixed for `kernel_main()`'s direct calls — closed that
  too. Also updated `explorer.cpp`'s `runningElfTasks[]` guard to check
  `pid`, not just `state`, since a `task_t*` can now outlive the specific
  program run it originally tracked once slots get recycled. Boot-tested
  by spawning 50 short-lived tasks in one session against `MAX_TASKS =
  32` — all 50 succeeded, proving slots actually get reclaimed and
  reused, not just "doesn't crash." See `docs/DOCS.md`
  ("`mods/dev/tasking/tasking.cpp` — task/stack reaper") for the full
  design writeup.

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

- ~~**Keyboard focus routing**~~ — DONE (2026-07-14), the minimal
  per-task stdin-ownership version (a LIFO stack of stdin readers, see
  "Recently completed"). The window-manager-focus version (routing based
  on which *window* is focused) is still genuinely deferred, since ELF
  programs don't have windows at all today — a real, separate, bigger
  feature.
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

- **Floppy access after boot is not real-mode-only.** `boot/loader/floppy.cpp`
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
  (`boot/loader/main.cpp`'s `read_file`/`fat12_next_cluster`), called
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
  `_start`). `elf_lookup_symbol()` is no longer a stub — it was
  implemented for real (item 13 above, `elf_exports[]` table) — so a
  loaded driver blob could already call back into kernel-exported
  functions by name, not just use syscalls, without further work here.
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
