# Pumpkin-OS — Design Notes

Longer "why" rationale that would otherwise bloat the source as multi-line
comments. Each entry here has a single-line comment back in the source
pointing to its section below.

---

## `mods/core/wingman/headers/widgets/widget.h` — common `Widget` base class

Extracted once there were three real widgets (`Button`, `TextInput`,
`Checkbox`) to check the shape against, not before -- see the earlier
`Button`-only discussion in this file's history for why one
implementation wasn't enough to design this from.

Comparing all three, `x`/`y`/`width`/`height` and the bounds-check in
`contains()` were genuinely identical (the same 4-line check, copy-pasted
three times), so those moved into `Widget` as concrete, non-virtual
members every subclass inherits -- not reimplements. `draw(Surface*, int)`
generalizes as a *signature* (all three take the same parameters) but not
as an *implementation* (each widget's rendering is completely different),
so it's a pure virtual hook subclasses must provide.

Deliberately left out of `Widget`: anything about *how* a widget reacts
to input. `Button` expects the caller to hit-test and invoke `onClick`
itself; `Checkbox::click()` does its own hit-test-and-toggle in one call;
`TextInput::onKeyboard()` reacts to keystrokes, not clicks, and checks
its own `focused` flag internally. Three real widgets still don't agree
on one interaction shape -- forcing one into `Widget` would repeat the
same mistake as designing from a single example, just with more data
backing the guess.

`WidgetType` exists because this kernel builds with `-fno-rtti`, so
`dynamic_cast` isn't available -- a container holding a generic
`Widget**` array can redraw or hit-test everything uniformly via the
base class, but still needs `widget->type` (not a cast) to know which
concrete type it has, if it needs to call a widget-specific method.

---

## `mods/core/wingman/suite/widgetdemo/widgetdemo.cpp` — `WidgetDemo` window

A dedicated window rather than folding this into `MessageBox`, since
`MessageBox` is semantically an alert (notify, dismiss) and this is a
persistent showcase -- mixing those changes what `MessageBox` means
rather than just adding a feature to it. Structured identically to
`MessageBox`/`FileManager` (owns a `Window`, implements
`KeyboardDelegate`/`MouseDelegate`, self-registers and focuses with the
`WindowManager` in its own constructor) so it fits the existing pattern
rather than inventing a new one.

Holds one of each widget -- `Button`, `TextInput`, and both `Checkbox`
styles side by side (box and toggle) specifically to show the enum
distinction, since that's the actual new capability being demonstrated,
not just "a checkbox exists."

Keyboard/mouse routing is intentionally minimal, matching each widget's
own current scope:

- `onMouseEvent()` only acts on a press-edge (`pressedEdge & 1`); plain
  hover has no effect (no hover-cursor feedback, unlike `MessageBox`'s
  buttons) -- not essential to demonstrating the widgets themselves.
- Clicking inside `textInput` focuses it; clicking anywhere else in the
  window (including on the other widgets) unfocuses it. With only one
  keyboard-consuming widget in this window, this is sufficient -- it
  doesn't attempt to solve multi-widget focus routing, which is the same
  open gap already tracked in `docs/TODO.md` (Priority 3).
- Every click and every consumed keystroke redraws all four widgets
  (`draw_widgets()`) rather than tracking which one actually changed --
  same "just redraw the whole affected region" precedent as
  `MessageBox::draw_buttons()`, and cheap enough at four widgets that
  finer-grained tracking isn't worth it yet.

Wired into `initalizeWindowSystem()` (`wingman.cpp`) as a second
always-open window alongside the file explorer and the AC97 `MessageBox`,
positioned at `(500, 120)` so it doesn't sit on top of either at boot.

---

## `mods/core/wingman/widgets/checkbox.cpp` — `Checkbox` widget (box + toggle styles)

One class, `CheckboxStyle` enum (`CheckboxStyleBox`/`CheckboxStyleToggle`)
picks the rendering, since a checkbox and a SwiftUI-style toggle switch
are the same underlying behavior (a persisted boolean, click to flip it)
with two different looks -- not two different widgets.

`draw()` branches on `style` inline rather than splitting into private
per-style helper methods; each branch is short enough that the extra
indirection wouldn't earn its keep.

The toggle style approximates SwiftUI's pill-track-plus-sliding-thumb
look with sharp corners, not rounded ones -- there's no
circle/rounded-rect primitive anywhere in this codebase to draw a true
pill with, and sharp corners keep it visually consistent with
`Button`/`TextInput`/`MessageBox`, which are all sharp-cornered too. The
`thickness` parameter (shared with `Button`/`TextInput`'s border
convention) does double duty here as the thumb's inset from the track
edges in toggle style, instead of a border width -- toggle style has no
border, only `CheckboxStyleBox` does.

`click(px, py)` differs from `Button`'s split of `contains()` (hit-test)
+ a separately-called `onClick`: since a checkbox's whole interaction is
"was I hit -> flip my own state -> tell whoever's listening," it's all
one self-contained call here, while `contains()` stays exposed separately
too for a caller that wants its own hover/hit-test logic without
triggering a toggle.

Now wired into `mods/core/wingman/suite/widgetdemo/widgetdemo.cpp`, and
retrofitted onto the `Widget` base class (see that section) once it gave
a third data point to design the shared interface from.

### `CHECKBOX_COLOR_OFF_TRACK` contrast fix

Originally `0xFF3a3a3c`, which is nearly the same brightness as
`WidgetDemo`'s own window background (`0xFF403a39`) -- the track was
effectively invisible against it, leaving only the white thumb visible.
That made the toggle look shorter/less substantial than `CheckboxStyleBox`
(which has a stark white border clearly outlining its full extent
instead of relying on fill contrast) and didn't read as a real toggle at
all, unlike SwiftUI's clearly-visible off-state track. Fixed by using a
noticeably lighter gray (`0xFF6e6862`) that stays clearly distinct from
the surrounding window background regardless of what it's embedded in.

---

## `mods/core/wingman/widgets/textinput.cpp` — `TextInput` widget

Built to the same shape as `Button` (`contains()` for hit-test, `draw()`
for rendering, position/size default to 0 and get set by whoever lays it
out). At the time this was written only two concrete widgets existed, so
it predated the formal `Widget` base class (see that section) -- it was
retrofitted onto `Widget` once `Checkbox` gave a third data point to
design the shared interface from.

Deliberately scoped down for a first version:

- **Single-line, append/backspace-at-the-end only.** No mid-string
  cursor movement -- `onKeyboard()` explicitly ignores the negative
  sentinel values `KeyboardHandler` (`mods/dev/kb/kb.cpp`) reports for
  arrow keys, along with `\n`/`\t`. Real cursor positioning (arrow-key
  movement, click-to-position) would need a `caretIndex` distinct from
  `length` and insert-in-the-middle buffer logic -- not built until
  something actually needs it.
- **No focus management of its own.** `focused` is a plain public bool
  the *owner* sets -- `TextInput` doesn't decide when it's focused, and
  `onKeyboard()` is a no-op (`return false`) whenever it isn't. This
  mirrors the still-unresolved "who owns keyboard focus" gap already
  tracked in `docs/TODO.md` (Priority 3) -- a real multi-widget focus
  model belongs there, not invented ad hoc inside one widget.

### `draw()` — horizontal scroll

Originally drew the whole buffer starting at a fixed `x`, with no bound
on how far right that could go -- typing past roughly `(width -
4*thickness) / charWidth` characters ran text straight off the widget's
own right edge into whatever was next to it, confirmed visually
(`docs/TODO.md`'s Miscellaneous section had this tracked before it was
fixed). Fixed by showing a trailing window of the buffer instead of the
whole thing: `maxVisibleChars` is however many characters actually fit
in the text area, and `startIndex` is `0` until the buffer overflows that
width, at which point it slides forward exactly enough to keep the last
`maxVisibleChars` characters (and therefore the caret, since the caret is
always at the logical end per the no-mid-editing simplification above)
in view. The caret's `x` position uses `visibleLen` (the trailing
window's length), not `this->length` (the buffer's full length) --
using the latter would place the caret past the widget's edge the moment
scrolling kicks in.

---

## `mods/core/wingman/widgets/button.cpp` — `Button` owns its own rendering/hit-test

Previously `Button` was a pure data bag (label, color, callback, rect) --
all of its actual behavior (hit-testing, hover, drawing) lived in
`MessageBox::draw_buttons()`/`onMouseEvent()`, reaching into `Button`'s
fields directly. That's backwards from how a control normally works (it
should own its own hit-test and rendering; the container's job is layout
and forwarding events, not deciding whether a click landed on a child or
how that child looks) and meant nothing else could reuse `Button` without
duplicating that logic. `contains()` and `draw()` move that logic onto
`Button` itself; `MessageBox` now only computes layout
(`layoutButtons()`) and calls into each button rather than rendering it
inline. `shade()` (the bevel highlight/shadow helper) and the
`drawChar()` helper moved into `button.cpp` alongside it, since they're
purely part of a button's own rendering now, not `MessageBox`'s.

`draw()` takes `thickness` as a parameter rather than owning a fixed
value itself -- `MessageBox` still controls that "house style" (its
window border uses the same thickness), it's just no longer the one
doing the actual pixel-level rendering.

### Content-based default size

Reported (2026-07-08, via screenshot): a button's label ("Click Me")
rendered past its own right edge. Root cause: `draw()` centers the label
using the button's actual `width`, but `width` defaulted to `0` and
nothing about `Button` itself prevented a caller from setting it smaller
than the label needs -- `WidgetDemo` had hardcoded `width = 90` for an
8-character label that needs 128px at `scale=2`, so the centered text
overflowed symmetrically on both sides.

Fixed by giving the constructor a sensible content-based default
(`labelLen * charWidth + 32` wide, matching `Checkbox`'s existing
precedent of computing a default size instead of leaving one at `0`) --
safe for `MessageBox`, which unconditionally overwrites `width`/`height`
in `layoutButtons()` regardless of what the constructor set, so this
only changes behavior for callers (like `WidgetDemo`) that don't go
through a layout pass. `WidgetDemo`'s own hardcoded override was removed
so it actually gets the new default instead of immediately clobbering it
back to the too-narrow value.

---

## `mods/core/wingman/wingman.cpp` — window dragging

Drag detection lives in `mouseFunctionWindowManager()`, not in any
individual widget, since it needs to work the same way for every window
type without each one (`MessageBox`, `FileManager`, future widgets)
re-implementing it. `WINGMAN_DRAG_HANDLE_HEIGHT` (30px) defines a
drag-handle band across the top of any window -- chosen because neither
`MessageBox` nor `FileManager` currently draws anything interactive that
high up (`MessageBox`'s buttons sit near the bottom, `FileManager`'s
clickable file list starts well below its title row), so claiming that
band for dragging doesn't intercept real content clicks.

A press-edge landing in that band starts a drag: `draggingWindow` records
which window, and `dragOffsetX`/`dragOffsetY` record the offset from the
window's top-left corner to the mouse position at drag-start, so the
window doesn't jump to snap its corner to the cursor on the first move.
While a drag is active, ordinary content dispatch (`handleMouse()`) is
skipped entirely -- a drag should be exclusive, not also register as a
click on whatever's under the cursor. The drag ends the moment the button
is no longer held (checked every packet via `buttons & 1`, not just on a
release edge, so it can't get stuck active if a release packet is ever
missed).

### Throttling the redraw during a drag

`composite()` is a full-screen clear + full re-blend of every window
(see its own section below), which is fine for occasional click-driven
redraws but was never designed to run on every single mouse packet --
which is exactly what continuous dragging does, easily hundreds of times
a second. `dragged->offsetX`/`offsetY` are still updated unconditionally
every packet (so the logical position never lags behind the mouse), but
the expensive part -- setting `needsRedraw = true`, which triggers
`composite()` + `redraw_screen()` -- is capped to once per
`WINGMAN_DRAG_REDRAW_INTERVAL_MS` (33ms, ~30fps) via `timer_ticks`
(millisecond-resolution since Proposal 2's `pit_init(1000)`). When a drag
ends, a redraw is forced unconditionally regardless of the throttle
window, so the window doesn't visibly end up a throttle-interval behind
its true final position. This is a targeted fix for the drag path
specifically, not a change to `composite()` itself -- the underlying
full-screen-recomposite cost this works around is still there and
already tracked separately (`docs/TODO.md`'s Miscellaneous audit
findings, "GUI rendering performance").

### Clamping to the screen

`newX`/`newY` are clamped to `[0, screenWidth - width]` /
`[0, screenHeight - height]` before being applied -- i.e. the clamp keeps
the *whole* window rectangle on screen, not just the point under the
cursor. Clamping only the cursor-tracked corner would still let the rest
of the window (and, at the extreme, its entire drag handle) end up
off-screen and undraggable back. `maxX`/`maxY` are floored at 0 so a
window that's already larger than the screen doesn't compute a negative
upper bound and get clamped to some position off in the wrong direction.

---

## `mods/dev/syscall/syscall.cpp` — `stdin_is_reading()` / stdin exclusivity

`kb_run_events()` (`mods/dev/kb/kb.cpp`) is a flat, global broadcast --
every registered callback fires for every keystroke, unconditionally,
with no concept of exclusive focus. `stdin_kb_callback` (this file) and
`keyboardFunctionWindowManager` (`mods/core/wingman/wingman.cpp`, which
routes into whatever Wingman window is currently focused, e.g.
`FileManager::onKeyboard()`) are both registered through that same
system. So while an ELF program is blocked in `stdin_read_line()` reading
a line, every key the user types is *also* being delivered to the file
explorer at the same time -- `'s'`/`'w'` move its selection, `'\n'`
activates whatever's currently selected. Reported symptom: pressing Enter
to submit typed input to a running program could also trigger the
explorer to open/play whatever file the selection had drifted to.

Fixed narrowly, not by adding real focus routing (that's the Priority 3
"Keyboard focus routing for multiple tasks blocked on stdin" item in
`docs/TODO.md`, a bigger design question tied to multi-task console
ownership): `stdin_read_line()` now sets a `stdin_reading` flag for the
duration of its blocking wait, exposed via `stdin_is_reading()`.
`keyboardFunctionWindowManager()` checks it first and returns immediately
if set, so the Wingman GUI simply stops processing keystrokes at all
while a program is mid-read -- matching the existing
`suppressCharacterOutput` pattern already used in
`mods/std/graphics.cpp`'s boot-terminal keystroke handler for the same
kind of "someone else owns input right now" situation.

---

## `mods/core/wingman/suite/explorer/explorer.cpp` — running-ELF tracking

Forbids re-launching a `.elf` file that's already running, at the
explorer/UI level rather than in the kernel -- `elf.cpp` deliberately
doesn't track this itself anymore (Phase 2 of the tasking proposal
retired the old kernel-side `elf_running` guard, since running multiple
*different* programs concurrently is now the intended behavior; only
re-launching the exact same file while its previous instance is still
alive is what this blocks).

`elf_spawn()` now returns the `task_t*` handle from `task_create()`
instead of a plain success/fail `int`, so a caller can check
`task->state` later to know whether a specific launched instance is
still alive. `runningElfTasks[]` is a small fixed-size
{filename, task_t*} table; `isElfAlreadyRunning()` checks it (and opportunistically
reclaims a slot if the tracked task has since gone `TASK_DEAD`),
`trackElfTask()` records a new launch, reusing the first empty-or-dead
slot.

This relies on task_t slots never being reused by a different task while
still referenced here, which holds today only because there's no
reaper yet (see the Phase 2 "known, deliberately deferred" note in
`docs/TODO.md`) -- once one exists, a freed slot could theoretically be
handed to an unrelated task before this table's stale entry gets
reclaimed, so this tracking will need revisiting whenever that lands.

---

## `mods/dev/kb/kb.cpp` — `kb_add_event()` dedup

Unlike `irq_install_handler()` elsewhere in this codebase, `kb_add_event()`
had no "already registered" check -- every call just appended a new entry
to `global_callbacks[]`, even for a callback function pointer that was
already registered. `mods/dev/syscall/syscall.cpp`'s `stdin_read_line()`
registers `stdin_kb_callback` once per blocking stdin read and removes it
via `kb_remove_event()` once the line is done; if that registration ever
ends up duplicated (e.g. two overlapping calls into the same registration
path before the first one's removal runs -- now a real possibility with
preemptive tasking live, where a blocking syscall can be interleaved with
other scheduler activity in ways it never was under the old fully
synchronous `elf_run()` model), every keystroke gets appended to the
shared `stdin_buf_ptr` buffer twice, since `kb_run_events()` invokes both
entries. This was the leading suspect for a reported bug (characters
doubling while typing into a running ELF program, e.g. "hello" arriving
as "hheellllooo") -- not confirmed via boot-testing, but a direct,
low-risk fix regardless: return the existing entry's id instead of
appending a duplicate, matching `irq_install_handler()`'s own established
pattern for the exact same class of problem. `mods/dev/mouse/mouse.cpp`'s
`mouse_add_event()` has the identical gap, spotted but not fixed here
since it wasn't implicated in the reported symptom.

---

## `mods/dev/elf/elf.cpp` — `elf_spawn()`

### Why `elf_task_trampoline()` doesn't call `task_exit()` itself

`task_create()`'s ASM trampoline (`tasking.asm`) already calls
`task_exit()` unconditionally whenever the entry function it invoked
returns -- this is the exact same mechanism `task0`/`task1`/`task2` rely
on (none of them call `task_exit()` themselves either). So
`elf_task_trampoline()` just needs to correctly cast and call the real
ELF entry point; when that returns (or when `sys_exit()`'s now-direct
`task_exit()` call fires mid-execution, in which case this function never
returns at all), the ASM trampoline handles cleanup generically, no
ELF-specific exit path needed.

### Why the caller's file buffer isn't freed on a successful spawn

`elf_load_file()`/`elf_load_rel()` relocate sections **in place** inside
the buffer they're given -- `elf_section_data()` returns `hdr +
sh_offset` for anything with real file data (`.text`, `.data`, etc.), a
pointer directly into the caller's buffer, not a copy. Under the old
blocking `elf_run()`, this was safe: the caller's `free(buffer)` only ran
after the whole program had already finished executing inside that
memory. Under `elf_spawn()`, which returns immediately, freeing the
buffer right after spawning would free memory the task hasn't even
started running from yet -- the next timer tick would jump into freed
memory. So callers (`explorer.cpp`'s `.elf` handler, `p-kernel.cpp`'s
`test_elf_execution()`) now only free the buffer on the failure paths
(load failed, spawn failed) where nothing will ever execute from it. On
success, it's intentionally leaked -- same category of known gap as task
stacks/task_t slots never being reclaimed (see `docs/TODO.md`'s tasking
proposal, "Files touched" -- `stack_base` + a reaper are still Phase 2/3
follow-ups, not yet implemented). A real fix would have the loader copy
relocated sections into their own independently-owned memory instead of
pointing into the transient file buffer, decoupling the two lifetimes
entirely -- bigger change, not done here.

### `mods/dev/context/setjmp.h`/`setjmp.asm` deletion

Confirmed nothing else in the tree referenced `setjmp`/`longjmp`/`jmp_buf`
after removing them from `elf.cpp`, so both files were deleted along with
the now-empty `mods/dev/context/` directory, per the tasking proposal's
own note ("the `context/setjmp.*` files can likely be deleted afterward
if nothing else uses them"). This also surfaced how they were actually
linked in the first place: `Kernel-Entry.asm` `%include`s
`mods/dev/context/setjmp.asm` directly (the same pattern used for
`tasking.asm` and `syscall.asm`) rather than the Makefile building it as
a separate object -- deleting the file without removing that `%include`
line broke the nasm build (`unable to open include file`), so the
`%include` was removed too. Worth noting: this means `setjmp`/`longjmp`
were only ever linked because of that one `%include` line existing
already -- if it hadn't, the old `elf_run()` would have had the exact
same silent-undefined-symbol problem `task_start_trampoline` briefly hit
during Phase 1 (`x86_64-elf-ld`'s `--oformat binary` output doesn't error
on undefined symbols the way a normal ELF-format link does).

---

## `p-kernel.cpp` — `tasking_init()` placement

Phase 1 of the tasking proposal (`docs/TODO.md`) calls for `tasking_init()`
to run early, so the boot context becomes `g_current` (pid 0) from the
start. That's safe specifically because `scheduler_on_tick()`'s behavior
on an empty runqueue is a no-op: the very first tick after
`tasking_init()` just captures the current ESP into `g_current->saved_esp`
(since it starts `NULL`) and returns immediately (`runqueue_head()` is
`NULL` -- nothing to switch to). Every tick after that takes the normal
path, but `pick_next(g_current)` on an empty `g_runqueue` returns `cur`
unchanged too. So the entire rest of `kernel_main()`'s boot sequence
executes exactly as it did before, just now "wrapped" as pid 0's task,
right up until real tasks are actually pushed onto the runqueue.

That happens at the very end of `kernel_main()`, after everything else
(VFS, Wingman, PCI, AC97, networking) has already been set up -- not
earlier. The moment `task_create()` pushes a `TASK_READY` task, the next
timer tick switches away from `g_current` and never comes back to it: the
bootstrap task_t was built directly via `task_alloc()`, not
`runqueue_push()`, so it's never actually in the circular runqueue and has
no path back into `pick_next()`'s rotation once execution leaves it. That's
fine here specifically because kernel_main has nothing left to do at that
point anyway (it would otherwise just fall through to `Kernel-Entry.asm`'s
closing `jmp $` spin) -- but it's why task creation can't happen any
earlier than the true end of boot without abandoning whatever boot steps
were still left unexecuted.

`task0` (idle, `for(;;) hlt;`) is spawned before `task1`/`task2` so there's
always something `TASK_READY` in the rotation once the fibonacci tasks
finish and `task_exit()` marks them `TASK_DEAD` -- without it, `pick_next()`
would find nothing runnable and just keep re-selecting whatever task
happened to finish last, rather than a clean idle state.

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

## `mods/dev/elf/elf.cpp` — `elf_lookup_symbol()`

Previously a stub that always returned `NULL`, meaning any ET_REL ELF
program with an undefined external symbol (i.e. any program that wants to
call a kernel function by name rather than being fully self-contained)
would fail relocation outright.

Given the single-address-space, ring-0-only design, there's no reason for
"a program calls into the kernel" to mean "traps through a syscall number."
The int 0x80 syscall table (`sys_open`/`sys_read`/`sys_write`/`sys_close`/
`sys_exit`) still exists for programs that want an explicit trap boundary,
but it isn't the only door in: `elf_lookup_symbol()` now does a linear
`strcmp` scan over a static `elf_exports[]` table of `{name, address}`
pairs and hands back the real function pointer, exactly like a minimal
static/dynamic linker's symbol resolution. A program can link against
`malloc`, `strcmp`, `fopen`, etc. directly, by name, and the relocator
patches the call site to jump straight at the kernel's own implementation
-- no new syscall number needed for each new thing a program wants to do.

The export table intentionally mirrors only functions that are genuinely
implemented in `mods/std/`, not everything declared in its headers.
`ftell()` is declared in `mods/std/include/stdio.h` but has no definition
anywhere in the kernel; exporting it would hand a program a pointer to
nothing, which `--oformat binary` linking (see build-system section
above) would not catch at kernel-build time either. Any future export
added to this table should be checked the same way before being listed.

## Bootloader → kernel `read_file` handoff

Traced end to end (2026-07-13) while working `docs/TODO.md` item #14.
The kernel receives one function pointer from the bootloader —
`load_floppy` (typed `read_file` in `p-kernel.cpp`) — that lets it keep
reading arbitrary files off the boot floppy after boot, without its own
FAT12 implementation. How that pointer actually crosses from the
bootloader into the kernel turned out to be more fragile than it looked
at a glance, and worth writing down precisely.

**The chain, stage by stage:**

1. `src/boot/loader/main.cpp` has its own `kernel_main()` (confusingly
   named the same as the real kernel's `p-kernel.cpp:kernel_main()` —
   they are different functions in different binaries that happen to
   share a name). This one loads `KERNEL.BIN` to `0x400000`, then sets
   `g_read_file_ptr = &read_file_frontend;` — a plain, normally-linked
   global — before returning.
2. `src/boot/loader/entry.asm`'s `_start` is stage2's real entry point
   (confirmed via `src/Makefile`'s `$(STAGE2)` rule: `entry.o` is listed
   first in the link line, and stage2 has no other candidate start
   symbol). It resumes right after that call returns, does
   `push dword [g_read_file_ptr]`, then `jmp 0x400000` into the
   just-loaded kernel image.
3. The kernel's very first instruction, `Kernel-Entry.asm`'s
   `start_kernel`, does `call kernel_main` (this time the real one, in
   `p-kernel.cpp`). Because of the push in step 2, cdecl's `[esp+4]`
   convention hands that pointer to `kernel_main(read_file load_floppy)`
   as its one argument — there's no actual C-level call from `entry.asm`
   into `p-kernel.cpp`; it's a `jmp`, with the call argument
   hand-assembled onto the stack beforehand.

**What used to be here, and why it changed:** steps 1–2 used to pass this
pointer through a hand-computed absolute address
(`READ_FUNCTION_ADDRESS`, a `#define` chain off `KERNEL_LOCATION` in
`main.cpp`) that `entry.asm` referenced as a bare hex literal (`0x3FFBF8`)
with no connection to the `#define` at all — two files, two languages,
one number, no shared build step keeping them in sync. Changing
`KERNEL_LOCATION` or any buffer size ahead of it in `main.cpp` without
also updating the literal in `entry.asm` would silently hand the kernel
a garbage pointer, with no compiler or linker error — it would only
surface the first time something called `load_floppy(...)`. Replaced
with a real linked symbol (`g_read_file_ptr`, `extern "C"` on the C++
side, `extern g_read_file_ptr` on the NASM side) specifically to close
that hazard: the linker resolves it for real, so the two sides can't
drift apart unnoticed.

**A verification note, in case anyone re-derives this from scratch:**
`g_read_file_ptr` lands in `.bss`, at whatever address the linker
happens to place it — checked via `-Map` against the *actual*
`--oformat binary` link (a separately-formatted ELF link of the same
inputs gave a different, misleading address, since ld's default section
layout differs by output format; don't trust an ELF-format re-link as a
stand-in for the real flat-binary one). That address (`0xa01c` as of this
writing) falls *past* `stage2.bin`'s `truncate -s 8192` boundary
(`0xa000`) — `--oformat binary` doesn't write `.bss` content at all, so
nothing at that address is ever actually loaded from disk. This is safe
only because `main.cpp`'s `kernel_main()` always writes
`g_read_file_ptr` before `entry.asm` ever reads it back (the read happens
strictly after `call kernel_main` returns) — whatever happened to be in
low physical memory at that address before boot is irrelevant. This is
not a new risk introduced by the symbol-reference change: `bootSector`
and `disk_id`, the pre-existing `.bss` globals this same file already
relied on, sit in the same past-the-boundary region (`.bss` spans
`[0xa012, 0xa028)` end to end) and depend on the identical
write-before-read guarantee. Don't add a zero-fill/truncation-boundary
"fix" here — there's nothing broken to fix, and it would just be
speculative code for a scenario that doesn't occur.

**Memory ownership — corrected from an earlier, wrong guess:** both
boot-owned regions (stage2's own code/data footprint at `[0x8000,
0xA000)`, and this handoff's own storage just past it) are already
protected from the physical page allocator today — `init_phys_allocator()`
(`mods/dev/memory/allocator.cpp`) marks every page from address `0`
through `endkernel` (the linker-defined end of the kernel image, based at
`0x400000`) as used, and that blanket sweep covers both ranges. That
protection is *incidental*, though — a side effect of the reservation
loop starting at `0` rather than at the kernel's own load address, not
something written with stage2 in mind, and not documented anywhere else
as a dependency until now. If that loop's start point ever changes (e.g.
"optimized" to start from `kernel_start` since addresses below that seem
irrelevant to the kernel), this breaks silently — a task or future
`load_floppy` call could get handed memory that's still holding live
stage2 code/data. Deliberately not adding new allocator code to guard
against that here, since nothing is broken today and this project avoids
speculative defensive code for scenarios that can't currently happen —
this paragraph is the mitigation: the invariant is now written down
somewhere a future change to `init_phys_allocator()` would have to
actively contradict, rather than silently violate.

**What `load_floppy` guarantees, for any future caller:** synchronous
(blocks until the read completes or fails), not reentrant (relies on
`main.cpp`'s static `bootSector`/`disk_id`, not per-call state — a second
concurrent call would race), and every existing call site
(`p-kernel.cpp`'s `test_vfs_file_io()`, `copy_floppy_file_to_ramfs()`,
`test_elf_execution()`) wraps it in `disablePaging()`/`enablePaging()`.
Whether that's load-bearing (a real dependency on physical==virtual
addressing somewhere in the FDC driver's DMA setup) or leftover caution
from an earlier real-mode-era version of this code is still an open
question — see `docs/TODO.md`'s Proposal 3 sketch, not resolved here.

## `mods/dev/tasking/tasking.cpp` — `task_block()`/`task_wake()`

Tasking proposal Phase 3. Adds one new state (`TASK_BLOCKED`) and two
functions, `task_block()`/`task_wake(task_t*)`, to `tasking.h`/`tasking.cpp`.

`task_block()` is the same trick `task_exit()` already used: mark the
current task's state (here, `TASK_BLOCKED` instead of `TASK_DEAD`), then
force an immediate reschedule with a software `int $0x20` — the same
vector the hardware timer IRQ uses, so `scheduler_on_tick()` runs exactly
as it would on a real tick and hands control to another task. No
scheduler surgery was needed for this: `pick_next()` already only
selects `TASK_READY` tasks, so `TASK_BLOCKED` gets skipped automatically,
the same way `TASK_DEAD` already was.

`task_wake(t)` just flips a specific task back to `TASK_READY` if (and
only if) it's currently `TASK_BLOCKED` — guarded so a stray or duplicate
wake call on a task that's already running/ready/dead is a harmless
no-op, not a state-machine bug.

Why `int $0x20` doesn't need `EFLAGS.IF` set first, unlike the `hlt`-spin
it replaces: `hlt` only resumes when *some* interrupt fires, so the old
code needed IF=1 or it would never wake up at all. `int N` is a software
interrupt — the CPU executes it unconditionally when the instruction runs,
regardless of IF, since IF only gates automatic delivery of *hardware*
interrupts. So `task_block()` works correctly no matter what IF happens
to be at the call site. Once control switches to whatever task
`pick_next()` picks, IF gets re-enabled naturally from *that* task's own
saved frame — every task built by `task_create()` (including the Phase 1
idle task, which always exists and is always `TASK_READY`) has `EFLAGS`
pre-set to `0x202` (IF=1) in its initial stack frame, so switching to any
other task, even just the idle task's `hlt`-loop, is what lets the
keyboard IRQ that will eventually call `task_wake()` actually fire.

## `mods/dev/syscall/syscall.cpp` — `stdin_read_line()` real blocking

`stdin_read_line()` used to be `while (!stdin_line_done) { hlt; }` —
"yield to any interrupt," burning a full task slot's scheduling turn on
every tick just to check one flag, and (before Phase 3) unable to let a
*different* task make real progress while waiting, since there was no
such thing as a task giving up its turn on purpose. Now it calls
`task_block()` instead, and `stdin_kb_callback` (the keyboard-IRQ-driven
callback that already appended typed characters to the buffer) calls
`task_wake(stdin_waiter)` once it sees `'\n'`. The `stdin_line_done` flag
is gone entirely — the wake itself *is* the "a full line is ready"
signal, so there's nothing left to poll.

**Deliberately out of scope, matching a pre-existing limit, not a new
one:** `stdin_waiter` (which task to wake) is a single global, same as
`stdin_buf_ptr`/`stdin_buf_size`/`stdin_buf_pos` always were. If two
different tasks both call `sys_read` on stdin at the same time, the
second call's `stdin_read_line()` overwrites the first's buffer pointers
and waiter out from under it — a real bug, but not one Phase 3
introduces: it already existed with the old `hlt`-spin design too, the
moment Phase 2 made it possible for more than one task to be independently
mid-`sys_read` at once. `kb_run_events()` still broadcasts every keystroke
to every registered callback unconditionally regardless of which task
"should" receive it. Fixing this for real needs actual per-task keyboard
focus routing (`docs/TODO.md`'s Priority 3 item), which was always
sequenced to come *after* Phase 3 specifically because it needs
`task_block()`/`task_wake()` to exist first — this doesn't make that gap
worse, it's the same gap Phase 3 was always going to leave for that next
item to close.

**Update (2026-07-14): actually boot-tested, found the real cause, fixed
it.** The "sandbox can't boot QEMU" assumption behind the paragraph above
was wrong — QEMU runs headless here (`-display none`, `-serial
file:...`, plus a monitor socket to `sendkey` past `stage0.asm`'s boot
menu, which blocks on a real keystroke with no timeout).

Retested Phase 1/2's scheduler in isolation first (`task1`/`task2`, no
ELF, no blocking) and got real interleaved `[1] fibbanoci(...)` / `[2]
fibbanoci(...)` serial output — first actual confirmation this ever
worked at runtime, not just on code review. The scheduler itself was
never the problem.

Spawning `MAIN.ELF` (prompts, then blocks on `sys_read`) failed though:
`task_block()` returned almost instantly, before any keystroke arrived,
`stdin_buf_pos` still `0`. First guess was the already-known, unfixed
`syscall.asm` segment push/pop mismatch (see `mods/dev/syscall/syscall.asm`
below) corrupting the nested `int 0x80` frame `task_block()`'s `int
$0x20` needs to resume into correctly — fixed that bug (it was real), 
retested, **identical failure**. Not the cause.

Actual cause, found by dumping the live runqueue at the exact failing
tick: `kernel_main()` calls `test_elf_execution()` (which spawns the ELF
task) several lines *before* it creates `task0` (idle). PIT is already
running at 1000Hz by then. A real timer tick landing in that gap —
after the ELF task exists, before `task0` does — hits
`scheduler_on_tick()`'s "first ever tick" branch, which is a **one-time,
non-requeueable handoff**: it permanently abandons whatever's left of
`kernel_main()` and jumps into the runqueue as it exists *at that exact
moment*. If that moment falls in the gap, the runqueue contains exactly
one task (the ELF task), self-looped. `task0` never gets created,
`kernel_main()`'s "Tasking Enabled!" line never prints, and `pick_next()`
— correctly, given that actual runqueue — can't find anything else
`TASK_READY`, so `task_block()`'s reschedule is a genuine no-op. Confirmed
directly: this run's log was missing "Tasking Enabled!" entirely, and a
runqueue dump at the failing tick showed a single node whose own `next`
pointer pointed at itself.

This is a Phase 1 gap, not a Phase 3 bug: `kernel_main()`'s startup tail
was never written to be safe against preemption partway through. It only
surfaced now because this specific ordering (spawn a task doing real
(slow) disk I/O, *then* spawn the fast idle task) had never actually been
boot-tested before.

**Fixed** by wrapping `kernel_main()`'s entire task-creation span — from
before the first `task_create()`/`elf_spawn()` call through the last —
in `sched_lock()`/`sched_unlock()`. That's the exact primitive that
already makes `scheduler_on_tick()` a complete no-op while held (used
internally by `task_exit()`/`task_block()` already); it just wasn't
exported for `kernel_main()` itself to use, so `sched_lock`/`sched_unlock`
were added to `tasking.h`. `test_udp_echo()`/`procMan()` — which sit
between the first and last `task_create()` call in the existing boot
order and don't themselves need the lock — ended up inside the span too,
rather than reordering unrelated boot steps just to shrink it.

**Re-tested end to end after the real fix**: booted, waited 6+ seconds
with zero keystrokes sent and confirmed the ELF task's `sys_read`
genuinely stayed blocked (no premature completion), then typed "bob" +
Enter via `sendkey` and got a correct wake, correct capture, clean
program completion, no crash, no hang.

All debug instrumentation used across this investigation (prints, a
targeted runqueue-walk dump gated behind a one-shot trace flag) was
reverted afterward. What's actually shipped: the `syscall.asm` fix (real,
worth keeping, just not the cause of this bug), the `sched_lock()`/
`sched_unlock()` fix here and in `p-kernel.cpp`, and Phase 3's original
`task_block()`/`stdin_waiter` logic in `syscall.cpp` — no leftover debug
code anywhere.

## `p-kernel.cpp` — `kernel_main()` task-creation race

See the update above for the full investigation. Summary: any function
that creates more than one task (directly via `task_create()`, or
indirectly via `elf_spawn()`) must hold `sched_lock()` across the entire
span from its first task-creating call to its last. Once *any* task
exists in the runqueue, a real timer tick can trigger
`scheduler_on_tick()`'s one-time "first tick" handoff and permanently
abandon whatever code was still running — there is no path back into
that abandoned context, since it was never a member of the scheduler's
own circular runqueue to begin with. `kernel_main()` is the only place
this currently applies (it's the only function that creates multiple
startup tasks in sequence with other code, like disk I/O, running in
between) — anything spawning exactly one task via `elf_spawn()` (e.g.
`explorer.cpp`'s `.elf` handler) doesn't have this race, since there's no
"in between" for a tick to land in.

## `mods/dev/syscall/syscall.cpp` — stdin keyboard ownership

With tasking Phase 3's real blocking, more than one task can legitimately
be mid-`sys_read` on stdin at the same time — `syscall.cpp` used to
track exactly one reader (`stdin_buf_ptr`/`stdin_buf_size`/`stdin_buf_pos`/
`stdin_waiter`, all flat globals), so a second concurrent reader would
silently overwrite the first's state.

Replaced with a small fixed-size LIFO stack, `stdin_stack[MAX_STDIN_DEPTH]`
(`MAX_STDIN_DEPTH = 8`), of `StdinFrame { waiter, buf, size, pos }`.
`stdin_kb_callback` always operates on `stdin_stack[stdin_stack_depth -
1]` — the top frame — appending characters there and waking only the top
frame's task on `'\n'`. Whichever task most recently called
`stdin_read_line()` owns the keyboard exclusively, the same nesting a
stack of modal dialogs would have: older readers are blocked further,
not receiving any input at all, until everything pushed after them
completes.

This is provably safe without extra bookkeeping about "which index am
I": a frame can only ever be woken while it's on top (only the top frame
ever sees a `'\n'`), and nothing can be pushed *after* a frame without
first requiring that frame to still be blocked below it — so by
construction, whenever `stdin_read_line()` resumes from `task_block()`,
its own frame is guaranteed to still be exactly at `stdin_stack_depth -
1`. Popping is always safe.

`stdin_kb_callback` itself stays a single shared function, registered
once via the existing `kb_add_event()` dedup — but critically, the
register/unregister calls moved from *per `stdin_read_line()` call* to
*per stack transition* (empty→non-empty registers, non-empty→empty
unregisters). Registering/unregistering per-call would have been a real
bug under nesting: an inner (more-recently-pushed) frame finishing and
calling `kb_remove_event()` would silently cut off an outer frame still
waiting below it, which still needs the callback registered until *it*
finishes too.

`stdin_is_reading()` (the only piece of this any other file reads —
`wingman.cpp`'s `keyboardFunctionWindowManager()`, to keep suppressing
GUI keyboard dispatch while stdin has an active reader) now reports
`stdin_stack_depth > 0` instead of a single boolean. Same external
contract, correctly generalized to "is anyone at all reading," not "is
the one reader we know about reading."

**Boot-tested for real (2026-07-14)**: temporarily spawned `MAIN.ELF`
twice in a row from `kernel_main()` (two independent, concurrently-
blocked readers, both waiting at their own `sys_read`), then via QEMU
monitor `sendkey`: typed "two" — only the second (topmost, most
recently blocked) reader received it, completed, and popped correctly;
then typed "one" — the first reader, now correctly back on top, received
it cleanly with zero characters leaked from the other read. No crash, no
hang. Test instrumentation (the double-spawn) was reverted after
verification.

**Deliberately not the bigger version**: this doesn't tie ownership to
window-manager focus, because ELF programs don't have a window at all
today — they're fully headless, stdout only ever goes to the serial log.
Building "click a program's console window to give it keyboard focus"
would mean a real new UI feature (a console/terminal `Window` subclass
with its own click-to-focus, wired into `WindowManager` the way
`MessageBox`/`FileManager` already are) — a genuinely separate, larger
task, not attempted here. What this fix does is narrower but real: make
"who owns the keyboard" an actual, correct, identifiable fact (a stack)
instead of a global that silently lied about it the moment a second
reader existed. There still isn't a way for a user to *choose* which of
several running programs owns the keyboard beyond "whichever one most
recently asked" — that choice is exactly what the window-focus version
would add, whenever it's picked up.
