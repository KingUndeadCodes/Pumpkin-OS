# Pumpkin-OS — TODO

Standing checklist of things to add/fix, ordered by recommended priority
(top = do next). Ring-0-only is an intentional design choice (like
TempleOS), not something any of these notes are trying to walk back.

---

## Priority 1 — Do next

Small, self-contained, no dependency on tasking. Ordered by recommended
sequence.

### 1. Fix the AC97/chorus playback race

`mods/dev/pci/drivers/ac97.cpp`, `mods/dev/chorus/chorus.cpp` — the AC97
IRQ refill handler updates `sound_buffer_refilling_info` fields with no
guard against mainline code doing the same thing mid-update. This is a
real, live correctness bug *today* (garbled/corrupted playback state),
independent of whether tasking ever lands — it just happens to also be one
of the Phase 0 hazards tasking would need fixed anyway (see Priority 2).
Smallest, most self-contained fix on this list: one `cli`/`sti`-style
guard around the shared struct's read-modify-write in both the IRQ path
and mainline callers.

### 2. Distinguish recoverable vs. fatal exceptions in the fault handler — DONE

### 3. Finish wiring up MessageBox — DONE

### 4. Validate ELF header offsets/counts before trusting them — DONE

### 5. Fix the hardcoded WAV buffer size in the file explorer — DONE

---

## Priority 2 — Before tasking (Proposal 1) can be turned on

Real multitasking is proposed in `agile-napping-stallman.md` (each ELF run
becomes its own preemptible task) but is deliberately **not being
implemented yet** — the plan is approved in concept, but the goal right
now is to keep expanding the feature/fix backlog first. Don't start on any
of this without an explicit go-ahead.

First two items below are done; the rest haven't been started yet.

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
- [ ] Make `sched_lock`/`sched_unlock` actually atomic
      (`mods/dev/tasking/tasking.cpp`) — currently a plain
      `g_sched_lock++`/`--`, not itself preemption-safe.
- [x] AC97/chorus playback race — promoted to Priority 1 item 1, since
      it's a real bug independent of tasking, not just a prerequisite.

---

## Priority 3 — Explicitly deferred past tasking (not blockers)

- Keyboard focus routing for multiple tasks blocked on stdin — ties into
  window-manager focus, which doesn't have a solid "this window owns this
  task's console" concept yet (see "Recently completed" → "Finish wiring
  up MessageBox" for the single-focus/z-order work already done — related,
  but this is the multi-task version of that problem).
- Moving mouse/keyboard IRQ work out of interrupt context (the cursor
  tearing problem) — real task-blocking gives us the primitive to do this
  properly later, but it's a separate change.
- Per-task memory isolation (ring 3, per-task page tables) — out of scope,
  same boundary as everything else. Ring 0 only, by design.

---

## Miscellaneous — parked, revisit later

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

---

## Recently completed

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

##### Phase 1 — Turn on the existing scheduler (no ELF changes yet)

- Call `tasking_init()` early in `kernel_main`, so the boot context becomes
  `g_current` (pid 0) from the start.
- Raise `KSTACK_SIZE` (`tasking.h`) from 4KB to something more comfortable —
  32KB or so. We've confirmed there's 500KB+ of free, identity-mapped space
  below the boot stack and nothing else contends for it; 4KB is needlessly
  tight for anything that calls into `syscall_dispatch`/VFS/keyboard-event
  chains.
- Give the kernel a real idle task (`for(;;) hlt;`) instead of letting
  `kernel_main` fall off the end after `tasks()` — this is the same "the
  kernel needs an actual idle/main loop" gap that came up in the cursor
  discussion; multitasking is what finally provides it.
- Spawn two or three throwaway test tasks (the existing commented-out
  `task1`/`task2` fibonacci demo is perfect for this) purely to confirm
  round-robin preemption actually works via interleaved serial output,
  before wiring anything real into it.
- ~~Optional: implement `pit_init(hz)`~~ — **done**, via Proposal 2.
  `pit_init(1000)` is already called in `kernel_main` (`p-kernel.cpp:274`,
  right before `sti`), so preemption already ticks at 1000Hz (1ms
  resolution) once tasking is turned on — no further work needed here.
- Confirmed still accurate as of 2026-07-02: `tasking_init()` is still
  fully commented out (`p-kernel.cpp:253`, inside the dead `tasks()` block
  with the stale page-fault comment at line 254); `KSTACK_SIZE` is still
  4096 (`tasking.h:6`); `task_t` still has no `TASK_BLOCKED` state or
  `stack_base` field (Phase 2/3 still need to add both). One new fact:
  `scheduler_on_tick()` is *already* wired to fire on every IRQ0
  (`irq.cpp:144`) even today — it's a harmless no-op only because
  `tasking_init()` never runs, so `g_current` never changes. Turning on
  real tasking is strictly "call `tasking_init()` + spawn tasks," not
  "wire up the timer," which is already done.

##### Phase 2 — ELF runs become tasks, not blocking calls

- Replace `elf_run()`'s blocking model with `elf_spawn(void* entry_point)`:
  `malloc()` a dedicated stack (generous size, e.g. 64KB — real programs may
  need more headroom than a kernel helper), call
  `task_create(elf_task_trampoline, entry_point, stack)`, return immediately.
  The file-explorer click handler (and `procTestOne`) just spawn and move on
  — no more blocking the caller at all, which is what makes this "seamless"
  instead of "eventually gets un-frozen."
- `elf_task_trampoline(void* entry_point)` calls the entry point directly;
  on return, calls `task_exit()`. **This removes the `setjmp`/`longjmp`
  machinery in `elf.cpp` entirely** — since each run now has its own real
  stack, `sys_exit` can just call `task_exit()` directly instead of
  longjmping across a shared buffer. That also retires the re-entrancy guard
  I added last turn (`elf_running`) — running two instances concurrently
  becomes the *intended* behavior, not a hazard to block.
- `elf_load_file()` (parsing + relocation) stays synchronous in the calling
  context — it's fast and does no I/O waits, no reason to defer it.

##### Phase 3 — Blocking syscalls block the task, not the CPU

This is the part that makes concurrency actually useful rather than
cosmetic. Right now `sys_read`'s stdin wait is `while (!done) hlt;` — even
after the EOI fix, that's "yield to any interrupt," not "let another task
run." Under real tasking it should be "let another task run":

- Add a `TASK_BLOCKED` state (`pick_next()` already skips anything that
  isn't `TASK_READY`, so this needs no scheduler surgery) plus
  `task_block()`/`task_wake(task_t*)`.
- `stdin_read_line()` marks its own task `TASK_BLOCKED` and immediately
  yields (`int $0x20`, the same trick `task_exit()` already uses) instead of
  spinning on `hlt`. The keyboard callback calls `task_wake()` once a line
  is ready.
- The stdin state (`stdin_buf_ptr`, `stdin_line_done`, etc.) in `syscall.cpp`
  is currently one global — it has to move to per-task state once more than
  one task can legitimately be blocked on stdin at the same time.

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
