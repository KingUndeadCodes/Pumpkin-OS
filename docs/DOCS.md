# Pumpkin-OS — Design Notes

Longer "why" rationale that would otherwise bloat the source as multi-line
comments. Each entry here has a single-line comment back in the source
pointing to its section below.

---

## `mods/dev/pci/drivers/ac97.cpp` / `mods/dev/chorus/chorus.cpp` — playback race guard

`sound_buffer_refilling_info` is touched from two contexts that can
genuinely interleave: `AC97_IRQ_HANDLER` (via `ac97_refill_fragment()`/
`ac97_complete_descriptor()`) fires asynchronously via hardware IRQ, and
`play_sound_with_refilling_buffer()` (mainline, e.g. triggered by
clicking a WAV/MP3 in the file explorer) writes the same struct's fields
with interrupts enabled. Starting a new playback while a previous stream
is still active (`ac97_stream_active == true`, since `AC97StopPlayback()`
only actually runs deep inside `AC97PlayData()`, called at the very end
of `play_sound_with_refilling_buffer()` -- after every field on the
struct has already been rewritten) leaves a real window for an AC97 IRQ
to land mid-update and read a torn mix of old/new field values, or for
the IRQ-driven `ac97_refill_fragment()` and the mainline preload loop to
write into the same `pcm_data` ring concurrently.

Guarded with `enter_critical()`/`exit_critical()` on both sides:
`ac97_refill_fragment()` and `ac97_complete_descriptor()` wrap their
whole bodies (matching the project's existing "guard the whole logical
operation" precedent), and `play_sound_with_refilling_buffer()` wraps
its struct-field-init block. The fragment-preload loop's
`last_filled_buffer` write is guarded on its own, separately from the
`fill_buffer()` callback call itself -- that callback does real decode
work and can take a while, so it deliberately stays outside any critical
section rather than holding interrupts off for its whole duration.

Kept intentionally minimal, matching the original TODO note's own
scoping ("smallest, most self-contained fix... one cli/sti-style guard
around the shared struct's read-modify-write") -- reordering
`AC97StopPlayback()` to run before the mainline mutations instead of
after would shrink the race window further, but that's a bigger,
separate change than what was asked for here.

---

## `mods/std/logging.cpp` — NUL-termination fix in `flush()`/`log()`

Both functions read into a `malloc`'d buffer via `fread()` and then treat
it as a NUL-terminated C string (`log()` via `strlen(buffer)`, `flush()`
by handing it straight to `Logging::log()`, which does the same). Two
problems: `fread()` doesn't append a NUL, and the buffer was never
zeroed, so on anything but a full exactly-2048-byte read, the tail of the
buffer is uninitialized heap garbage -- `strlen()` reads until it happens
to find a zero byte, which could be past the buffer entirely (no
guarantee one exists within the allocation, since `malloc()` here doesn't
zero it). Fixed by allocating one extra byte, capturing `fread()`'s
actual return value, and explicitly NUL-terminating at exactly that
offset (`buffer[bytes_read] = '\0'`) instead of relying on `strlen()` to
discover where the real data ends. `log()`'s "does the file already end
in a newline" check was rewritten the same way (`buffer[bytes_read - 1]`
instead of `strlen(buffer) - 1`), which also fixes a latent
empty-file case: the old `strlen(buffer) - 1` would underflow to
`SIZE_MAX` on an empty read since `strlen("") == 0`; the new
`bytes_read > 0` guard skips the newline-prepend correctly for that case
instead of indexing `buffer[SIZE_MAX]`.

---

## `mods/dev/pci/drivers/rtl8139.cpp` — `RTL8139_SEND_PACKET()` TX-wait fix

`while (transmit_ok & (1 << 15) == 0)` had an operator-precedence bug:
`==` binds tighter than `&` in C++, so this was actually
`transmit_ok & ((1 << 15) == 0)` -- `(1 << 15) == 0` is always false
(`0`), so the whole expression was always `transmit_ok & 0`, i.e. always
`0`/false. The loop body never ran, not even once -- "wait for TX
complete" was a no-op that fell through immediately regardless of the
hardware's actual status.

Fixing the parens alone would have been a landmine: this loop had never
executed before, and its body called `Logging::log(...)` on every
iteration. `Logging::capturing` is set `true` at boot
(`p-kernel.cpp`'s `Logging::capture()` call), so `Logging::log()` isn't
a no-op -- it's a real `fopen`/`fread`/`malloc`/`fwrite`/`fclose` on
`/kmsglog`. Since the loop was dormant, this was never actually hit; once
the condition is fixed, a genuinely slow NIC (or one that never sets the
TOK bit) would spin doing full VFS file I/O as fast as the CPU could
issue it. Moved the log call to fire once, only if a wait is actually
needed, instead of once per poll -- keeps the diagnostic value without
turning the send path into a filesystem-I/O spin loop.

---

## `mods/dev/memory/memory.cpp` — `queryMemoryMap()` collects every usable region

The old code called `init_phys_allocator()` from inside the SMAP-entry
loop, gated on `baseCount++ == 1` -- so it only ever fired for the
*second* `Type==1` (usable) entry, passing that one region alone. The
first usable region (almost always the largest/lowest) was silently
skipped, and any usable regions past the second were ignored entirely.
`init_phys_allocator()` itself already supported an array of regions
(`mem_region_t* regions, size_t region_count`) -- it just was never
actually given more than one. Fixed by collecting every usable region
into a small fixed-size array (`MAX_USABLE_REGIONS`, generous for a
BIOS/QEMU-generated SMAP) while iterating, then calling
`init_phys_allocator()` once after the loop with the real count. A stack
array is used, not `malloc()`, since this function is what determines
what the heap/physical allocator even has to work with -- it runs before
either exists.

---

## `mods/dev/tasking/tasking.cpp` — `sched_lock()`/`sched_unlock()`

`g_sched_lock` gates whether `scheduler_on_tick()` (called directly from
the IRQ0/timer handler) is allowed to switch tasks -- callers like
`task_exit()` bump it around a state mutation they don't want interrupted
by a reschedule mid-update. But `g_sched_lock++`/`g_sched_lock--` are each
a plain read-modify-write, not a single instruction: if IRQ0 fires between
the read and the write (entirely possible, since ordinary code runs with
interrupts on), `scheduler_on_tick()` can observe a stale value, or the
increment/decrement itself can lose an update, which defeats the whole
point of the lock -- it stops actually excluding the reschedule it was
meant to block. Guarding the read-modify-write with `enter_critical()`/
`exit_critical()` makes each bump atomic; `scheduler_on_tick()`'s own read
of `g_sched_lock` needs no separate guard, since it only ever runs from
inside the IRQ0 handler itself, where interrupts are already off.

---

## `mods/dev/vfs/vfs.cpp` — `vfs_mount()` re-checks under the lock

The limit check (`mount_count >= VFS_MAX_MOUNTS`) and the double-mount
check (`dir->mountpoint`) both already ran earlier in the function, but
non-atomically -- a second `vfs_mount()` call could interleave between
that check and the array write below. Redoing both checks inside the
critical section, immediately before the write, makes the whole
check-then-write sequence atomic instead of just the write. `vfs_find_mount()`'s
scan over the same `mounts[]`/`mount_count` is guarded too, so it can't
observe a write to `mounts[mount_count]` that hasn't been followed by the
`mount_count++` yet (or vice versa mid-torn-update).

---

## `mods/std/stdio.cpp` — `alloc_fd()` claims inside the scan

Previously `alloc_fd()` found a free slot and returned its index, then
`fopen()` set `file_table[fd].in_use = 1` afterward -- two separate steps
with a gap between them where a second concurrent `fopen()` could scan
before the first one's claim landed and hand out the same fd twice. Moving
the claim (`in_use = 1`) inside the same critical section as the scan that
found the slot makes "find a free slot" and "claim it" one atomic step.
`fclose()`'s corresponding `in_use = 0` clear is guarded too, matching
`free_phys_page()`'s treatment of the physical page bitmap.

---

## `mods/dev/ramfs/ramfs.cpp`

### `ramfs_create_node()` — duplicate check + splice as one unit

Two concurrent creates in the same directory could otherwise both pass
the "does this name already exist" check before either one links its new
`ramfs_dirent_t` onto `pd->dir.children`, then race the linked-list head
splice itself (`entry->next = pd->dir.children; pd->dir.children = entry;`)
-- a classic lost-update on a linked list head. Guarding the whole
sequence (duplicate check, alloc, splice) makes it atomic. `ramfs_alloc_node()`'s
own `malloc()` calls nest safely under the outer critical section, same
pattern as `alloc_phys_pages()` calling `alloc_phys_page()`.

### `ramfs_delete_node()` — unlink as one unit

Same shape of hazard on the way out: finding the link to unlink and
splicing it out of `pd->dir.children` needs to be atomic relative to a
concurrent create/delete on the same directory. The actual `free()` calls
happen after the critical section ends, since by that point the node is
already unlinked and unreachable through the directory it used to belong
to.

### `ramfs_readdir()` / `ramfs_finddir()` — read-side guards

Guarded for the same reason `get_kmalloc_free_bytes()` guards its
free-list walk: a plain traversal of `pd->dir.children` could otherwise
read a list that's mid-splice from a concurrent create/delete.

### `ramfs_read()` / `ramfs_write()` — whole-body guards

These traverse (and, for writes, grow via `ramfs_grow_file()`) a file's
per-node block list (`head`/`tail`/`block_count`). Guarding the whole
function, not just the list-walk portion, matches the project's existing
precedent (`alloc_phys_pages()`, `malloc()`/`free()`) of guarding the full
logical operation rather than trying to carve out the "unsafe part" more
narrowly -- simpler to reason about, and these are small test-scale files
in current usage, so holding interrupts off for the copy isn't a real
latency concern yet.

---

## `mods/dev/memory/allocator.cpp`

### `alloc_phys_pages()` — one critical section for the whole operation

`enter_critical()`/`exit_critical()` nest safely (save/restore `EFLAGS.IF`,
only the outermost pair actually toggles anything), so wrapping the entire
function -- not just each individual page via `alloc_phys_page()`'s own
guard -- means the outermost call is the one holding off preemption,
covering the scan/rollback sequence as a single atomic unit. Without this,
another allocation or free could interleave between two pages of the same
multi-page request and break the contiguity check, or race the rollback
loop after a failure partway through.

### `alloc_phys_pages()` — rollback-address fix

The non-contiguous-page branch used to free `base + i * PAGE_SIZE` for
`j <= i`, but the page that broke contiguity is at `addr`, not
`base + i * PAGE_SIZE` -- that mismatch is the whole reason this branch
runs. The old code was freeing an address that was never actually
allocated (potentially clearing some other page's bitmap bit out from
under it) while leaking the real page at `addr`. Fixed to free `addr`
directly, plus the previously-collected pages `0..i-1` via `base + j *
PAGE_SIZE`.

---

## `mods/dev/idt/isr.cpp` — `FaultHandler` namespace

### `is_recoverable()`

Exceptions where it's genuinely safe to just resume execution afterward --
the assembly stub's `iret` will pick back up exactly where it left off.
This isn't "recovering from an error": #DB/#BP aren't errors at all
(they're literally how single-stepping and `int3` breakpoints work), so
continuing past them is always correct, in any context. Everything else
stays fatal -- for #DE/#UD/#GP/etc there's no generally-safe way to resume
without possibly running further off the rails with corrupted state, and
for genuinely catastrophic ones (#DF, #MC) continuing at all would be
actively wrong. #PF is deliberately still fatal too: recovering from it for
real (demand paging, copy-on-write) needs a memory-management story this
kernel doesn't have yet -- see TODO.md.

### `print_stack_trace()`

Walks the EBP chain to print return addresses, so a crash can usually be
pinpointed (cross-referenced against a symbol map/objdump) without needing
to reproduce it under a debugger. Bounded, and stops at the first
implausible frame rather than trusting a possibly-corrupt stack
indefinitely -- we're already crashing, so a second fault here would just
double-fault instead of finishing the report.

### `draw_panic_screen()`

Direct framebuffer writes only (`fill()`/`draw_char()`, which bottom out in
`draw_pixel()`) -- no malloc, no Surface/Window/Terminal objects. If the
fault was caused by heap or GUI-state corruption, the panic screen still
needs to render, so it can't depend on any of the things that might be
what's actually broken.

---

## `mods/dev/port.cpp` — `enter_critical()`

Saves EFLAGS.IF and clears it. Nests correctly: a nested call sees IF
already 0 and stores that, so its `exit_critical()` won't re-enable
interrupts while an outer critical section is still active -- only the
outermost enter/exit pair actually toggles anything.

---

## `mods/core/wingman/cursor.cpp` / `headers/cursor.h`

### `draw_cursor_into_buffer()`

Stamps the cursor into a buffer that's about to be blitted anyway (regular
memory, not MMIO) -- for folding the cursor into a recomposite that's
already happening, not for plain mouse movement (`redraw_cursor()` is that
path -- copying/touching the whole screen just to move the cursor would be
far more expensive than the old poke-the-cursor-directly approach it would
replace).

### `redraw_cursor()`

Cheap path for plain mouse movement, when nothing else on screen changed:
touches only the small, fixed-size cursor footprint (old position + new
position) directly in the real framebuffer, using the window manager's
clean (cursor-free) composited buffer to know what to restore at the old
position -- not a full-screen copy or recomposite.

PS/2 can deliver several packets per physical movement (or none) -- it
skips redoing the erase/draw entirely when the clamped position is
actually unchanged.

It writes to the same framebuffer address `wingman.cpp`'s `redraw_screen()`
already blits to in bulk -- whole rows via `memcpy` instead of one
`draw_pixel()` call (a function call + a single volatile MMIO write) per
pixel. The new-cursor pass merges background + glyph into a small scratch
row (regular memory) first, then blits that whole row in one shot.

### `update_mouse_position()`

Records the latest mouse position (unclamped -- `draw_cursor_into_buffer()`
clamps against whatever buffer/dimensions it actually draws to).

---

## `mods/core/wingman/headers/manager.h` / `manager.cpp`

### `WindowManager::zOrder`

Draw/stacking order, bottom to top, compacted (no gaps). Separate from
`windows`' slot indices so a window's ref stays stable while `focus()` can
still reshuffle where it renders relative to others.

### `WindowManager::focus()`

Focuses the window and raises it to the top of the stack, same as clicking
a window brings it to front on any other desktop OS.

### `WindowManager::windowAt()`

Topmost window whose rect contains (x, y), or `WINGMAN_INVALID_WINDOW`.
"Topmost" = last in the z-order, matching `composite()`'s draw order.

### `zOrderRemove()`

Removes `ref` from the z-order list if present, shifting later entries
down to close the gap. No-op if `ref` isn't in the list.

---

## `mods/core/wingman/wingman.cpp`

### `outputBuffer`

Scratch copy of `wm->screen`'s composited buffer -- the cursor gets
stamped onto this (regular memory, not MMIO) before the single blit to the
real VESA framebuffer, instead of poking the cursor there as its own
separate draw. `wm->screen` itself stays cursor-free so a later real
recomposite doesn't need to know or care where the cursor was.

### `lastButtons`

PS/2 delivers a packet at a fairly high rate, not just on state changes,
so a single physical click can arrive as several packets with buttons=1.
Tracking the previous packet's button state here lets us compute a
press-edge (0->1 this packet) once, globally, before any window/delegate
ever sees it -- instead of every MouseDelegate treating "buttons == 1" as
its own fresh click and re-firing on every packet held down.

### `mouseFunctionWindowManager()`

A fresh press anywhere refocuses and raises whatever window is actually
under the cursor -- clicks go to whatever's on top at that point, not
whatever silently still held focus from before. Raising a window is
itself a visual change (the stacking order moved) even if the click
doesn't otherwise land on anything interactive, so it needs its own
redraw trigger independent of `handleMouse()`'s return.

Cursor ID is reset to the default arrow before dispatch every time -- a
delegate can override this (e.g. MessageBox showing a hand cursor over a
button), but if the mouse isn't over anything that cares (or leaves a
window that did), there's no separate "mouse left" event to undo a stuck
custom shape otherwise.

When something changed, content is already getting a full recomposite +
blit, so the cursor is folded into that same blit instead of a separate
poke right after it. Otherwise (nothing changed), only the cursor moves,
touching just its small fixed-size footprint instead of the whole screen.

### `initalizeWindowSystem()`

MessageBox registers and focuses its own window with `wm` internally, so
it doesn't need (and shouldn't get) an external `wm->add()` here. It also
starts with no buttons -- the caller adds whichever ones fit.

---

## `mods/core/wingman/headers/widgets/button.h`

`ButtonCallback` fires when a button is clicked or activated via Enter
(default button only); `userdata` is whatever was passed in when the
button was created. `Button`'s layout rect (`x`/`y`/`width`/`height`) is
assigned by whoever owns the button (e.g. `MessageBox::layoutButtons()`)
and isn't meaningful before that.

---

## `mods/core/wingman/suite/message/message.h` / `message.cpp`

### `MESSAGEBOX_MAX_BUTTONS`

Past this, the row gets too cramped at the box's fixed width to stay
readable -- three covers every realistic case (Yes/No/Cancel and the
like) without needing per-button width to shrink further.

### `MessageBox` (class)

All three dialog types are acknowledge-and-continue alerts, not yes/no
prompts -- clicking any button (or pressing Enter for the first one)
dismisses the box after running that button's callback, if any.

### `icon` (field + constructor param)

-1 means "use dialogBoxType's default icon"; otherwise an index into
`Icons[]` (see `graphics/icons.h`), overriding that default.

### `buttonSectionDividerY`

Fixed independently of `buttonRowY`, so the button row can be recentered
in the section below it without dragging the divider along. In the
constructor: the divider sits a fixed distance above the border/button
block, and the row is then centered in the section below it, independent
of where that centering puts `buttonRowY`.

### `addButton()`

Appends a button, relayouts the row, and redraws it. Returns the new
button's index, or -1 on allocation failure.

### `shade()`

Shifts each channel of `color` by `delta` (clamped to [0,255]), keeping
full alpha. Used for the button bevel highlight/shadow.

### `layoutButtons()`

Evenly distributes all current buttons across the row, with `padding`
gaps between them, so `draw_buttons()` and the mouse hit-test in
`onMouseEvent()` always agree on where each button actually is.

### `draw_title()` — icon bounds check

`Icons[]` (`graphics/icons.h`) currently holds 12 icons -- an
out-of-range override falls back to `dialogBoxType`'s default rather than
reading past the array.

### `draw_background()` — title bar band

Title bar band + divider, so the icon/title read as a distinct header
instead of blending into the body.

### `draw_buttons()`

Repaints the whole row first -- `addButton()` can reshuffle every button's
position/width (e.g. two buttons splitting a row that used to hold just
one), and without this, whatever a previous layout drew in a spot no
longer covered by any button stays on screen as a leftover sliver.

### `dismiss()`

Closes the box: hands the window back to the WindowManager (which owns
and deletes it from here on), then self-destructs. `this->window` is
nulled first so `~MessageBox` doesn't also delete a window
`wm->remove()` already freed.

### `onMouseEvent()` — cursor shape

Hand cursor over any button, default arrow elsewhere -- reset to the
default happens in `wingman.cpp` before dispatch, since this only runs
while the mouse is inside the box at all.

---

## `p-kernel.cpp` — `test_fault_handler()`

Deliberately exercises both paths of the reworked `_fault_handler()`: #BP
(recoverable -- should log and fall straight through to the next line) and
then #PF against a deliberately unmapped address (fatal -- should print
CR2/err_code diagnostics, a stack trace, and the panic screen, then halt).
Only call this manually while testing; it never returns once the #PF
fires.
