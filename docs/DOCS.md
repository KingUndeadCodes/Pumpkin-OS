# Pumpkin-OS — Design Notes

Longer "why" rationale that would otherwise bloat the source as multi-line
comments. Each entry here has a single-line comment back in the source
pointing to its section below.

---

## `mods/core/wingman/headers/shapes.h` — rounded widget corners

Every widget (`Button`, `TextInput`, `Checkbox`) drew hard 90° rectangles
via hand-rolled per-widget pixel loops: a `thickness`-inset fill pass, a
separate border-outline pass. Rounding corners for real (not just
cropping them square) needed the same non-trivial math in every one of
those loops, so it's a single shared helper,
`draw_rounded_rect_fill(surface, x, y, w, h, radius, color)`, rather
than duplicated per widget.

**Why a plain `Surface*` parameter, not a templated `PlotFn` like
`ttf_blit_glyph()`:** `ttf_blit_glyph()` needs to serve both a free-function
framebuffer caller (`console.cpp`) and `Surface`-based Wingman callers, so
it's templated on the plot callback to do that with no virtual dispatch.
Rounded corners are scoped to Wingman widgets only -- every current and
realistically foreseeable caller already has a concrete `Surface*` -- so
a direct, non-templated function is simpler and avoids generalizing for
a caller that doesn't exist.

**Cost model:** interior and straight-edge pixels are a single plain
`putPixelUnsafe()` call, identical cost to the old hard-cornered loops.
Only pixels inside a `radius`-sized corner box get the extra work: a
`sqrt()` distance-from-corner-center check, and -- only for the thin ~1px
ring actually straddling the arc boundary -- a `getPixel()` read plus an
alpha blend via `ttf_blend_over()` (reused from `fontman.h` rather than
duplicated, since every caller already depends on `fontman.h` for text).
Bounded by `radius² × 4` pixels checked per shape at most (e.g. radius=8
→ at most 256 corner-box pixels, of which only a slim boundary ring
actually blends), paid once per redraw, the same cost class as TTF glyph
blending -- not a new performance concern.

**Radius convention:** each widget's own `radius = height / 5`, so
rounding scales with widget size instead of being a fixed pixel constant
that would look disproportionate on a very small or very large instance.
`Checkbox`'s toggle style is the deliberate exception: `radius = height / 2`
for the track (a full capsule, the conventional switch shape) and
`radius = thumbSize / 2` for the thumb (a full circle) -- not the general
`/5` rule, since a toggle switch is expected to read as a pill, not a
slightly-rounded rectangle.

**Two-call pattern:** each widget calls `draw_rounded_rect_fill()` twice
-- once for the full rect in the border color, once for a
`thickness`-inset rect (with a correspondingly smaller radius, so the
inner curve stays concentric with the outer one) in the fill color --
reproducing the exact "filled rect with a border ring" look the old
fill+border loop pairs had, just with rounded corners on both edges.
`Button`'s inner bevel highlight/shadow lines are a deliberate exception:
they stayed as straight 1px lines (rounding a 1px line isn't meaningful),
just clipped to start/end at `radius` instead of `thickness` so they
don't overhang past the new curve.

---

## `mods/core/wingman/draw.cpp` / `headers/draw.h` — shared drawing helpers

Wingman had **13 separate definitions** of four drawing helpers across six
files: `utility_draw_char`/`drawChar` ×6, `utility_draw_pixel` ×4,
`utility_draw_icon` ×2, `utility_draw_string` ×1. Most were byte-identical
copy-paste — `button.cpp`'s and `textinput.cpp`'s `drawChar` were literally
identical, and `explorer.cpp`'s and `message.cpp`'s `utility_draw_char`
differed only in the class name.

The cost was not stylistic. The `color == 0x10` transparency branch in
`utility_draw_icon` is missing a `continue`/`else` — the black write is
immediately overwritten, so the transparent marker never works — and that
bug was present in *every* copy, having been propagated by the copy-paste
itself. It survived a third copy being deleted (`MessageBox`'s) purely by
accident. More importantly, the duplication blocked the real fix for
`putPixelUnsafe`: adding bounds-checking/clipping meant touching 13 sites,
which is why the same out-of-bounds class of bug has produced three
separate incidents (Explorer's multi-column heap corruption, `titlebar.cpp`'s
negative-`availableWidth` wrap, `draw_circle_fill`'s unclipped rows).

Consolidated into free functions taking `Surface*` as the first parameter —
the shape `titlebar.cpp`, `button.cpp`, and `textinput.cpp` had already
independently converged on. The three suite-app class-method versions were
the outliers; they only ever reached `this->window->surface`, so `this`
bought them nothing but an implicit parameter.

**Not a virtual base class**, despite "shared drawing interface" suggesting
one. `Widget::draw()` receives a `Surface*` rather than owning one, so
widgets could never fit a hierarchy keyed on "the surface I own" — a base
class would have consolidated only the three suite copies and left the six
widget/titlebar ones. And virtual dispatch in a per-pixel loop (~786k calls
for a full-screen fill) would contradict the existing decision to keep
`ttf_blit_glyph()` template-based specifically to avoid it, documented in
the `shapes.h` section above.

Naming and signature notes:
- **`surface_`-prefixed deliberately.** `mods/dev/vbe/vbe.h` declares global
  `draw_pixel`/`draw_char`/`draw_icon` writing the raw linear framebuffer,
  transitively visible in the suite files via `headers/cursor.h`. Unprefixed
  names would have silently overloaded them.
- **`int` coordinates**, matching `Surface::putPixelUnsafe(int, int, color_t)`.
  Behaviour-neutral while no bounds check exists (`int` and `unsigned` index
  out of bounds identically), and a prerequisite for adding one.
- **No default arguments.** All 33 external call sites already passed `scale`
  explicitly, so the previously-declared defaults were dead — and had already
  drifted (`explorer.h` said `scale = 4`, `widgetdemo.h` said `scale = 2`).
  `explorer.cpp`/`message.cpp` also illegally repeated defaults on the
  out-of-class definition, compiling only because of `-fpermissive`.

Deliberately a real `.cpp` rather than `shapes.h`'s header-only `static
inline` pattern: these helpers depend on the large `static const` tables
(`Icons[12][32][32]`, `Font[128][8]`, `vgaPaletteConvertorRGB32[]`), which a
header-only version would re-instantiate in every including TU. `draw.cpp`
is now the only wingman TU pulling them, and `explorer.h`/`titlebar.cpp`/
`message.h`/`widgetdemo.h` dropped those includes entirely. Measured:
`kernel.bin` went from 648,580 to 621,540 bytes, **27,040 bytes saved**.

The `0x10` bug and the bounds-checking work were deliberately left out of
this change so it stayed a reviewable pure move — both are now single-site
fixes rather than 13-site ones.

---

## `mods/core/wingman/headers/widgets/*.h` — positioned constructor overloads

Every widget used to be constructed, then have `->x`/`->y` set on two
follow-up lines. Each widget (`Button`, `TextInput`, `Checkbox`, `Slider`)
gained a second constructor overload with `int x, int y` appended, so
`new TextInput(64, "Type here...", 170, 105)` works alongside the original
position-less form. A genuine second overload, not default arguments on the
one constructor — the positioned overload delegates to the original via a
C++11 delegating constructor (`: Button(message, color, onClick, userdata) {
this->x = x; this->y = y; }`) so there's exactly one real implementation per
class; the second "overload" is a one-line pass-through.

Where the original constructor has trailing defaults (`TextInput`'s
`placeholder`, `Checkbox`'s `initialChecked`/`onChange`/`userdata`,
`Slider`'s `onChange`/`userdata`), the positioned overload requires them
explicitly -- C++ doesn't allow a default parameter before a mandatory one,
and `x`/`y` have no sensible default of their own (that's what the original
constructor is for). Costs nothing at the five real call sites in
`widgetdemo.cpp`, which already passed every parameter explicitly.

**`MessageBox::addButton()`'s `Button` deliberately was not migrated.** Its
`x`/`y`/`width`/`height` are computed later by `layoutButtons()` (evenly
splitting the button row across however many buttons exist at the time),
not known at construction — passing `0, 0` there would misrepresent that
position as meaningful.

**`TextInput` alone also gained a third overload** appending `int width,
int height` (`: TextInput(maxLength, placeholder, x, y) { this->width =
width; this->height = height; }`, chaining onto the positioned overload
above it, which chains onto the base one -- each level adds exactly what it
introduces). Deliberately not added to `Button`/`Checkbox`/`Slider`: those
three already compute their own width/height internally (message length,
toggle-vs-box style, a fixed track size) and no call site anywhere
overrides it -- `Slider::onMouse()`'s thumb-centering math and `Checkbox`'s
toggle-track geometry both assume the derived value. `TextInput` is the
only widget with no self-computed size (defaults to `0,0` from `Widget`'s
base constructor); every real call site already set `->width`/`->height`
manually, so this directly replaces existing lines rather than adding an
override capability nothing exercises.

---

## `mods/core/wingman/wingman.cpp` — keyboard-path NULL deref, and the Initialize-button lambda

Found while digging into an unreproducible visual glitch a user hit in a
real (non-headless) QEMU session -- neither confirmed as that glitch's
cause (leading theory is a QEMU host-side display artifact, since headless
automated testing couldn't reproduce it across focus-change, drag-away,
and drag-the-window-itself scenarios), but both real bugs found along the
way.

**`keyboardFunctionWindowManager()` used to re-read `wm->focusedWindow`
*after* calling `wm->keyboard_handler()`.** Pressing Enter while
`MessageBox` is focused runs `onKeyboard()` -> `dismiss()` ->
`wm->remove(ref)`, which deletes the `Window` and correctly nulls
`wm->focusedWindow` when it matches (`WindowManager::remove()`).
`keyboard_handler()` still returns `true` (the keystroke was consumed).
The re-read then dereferenced a now-null pointer for `dirty`'s rect --
a crash. The mouse path already handles this correctly
(`mouseFunctionWindowManager()`'s non-drag branch caches
`rectX`/`rectY`/`width`/`height` into locals *before* calling
`handleMouse()`), the keyboard path just never got the same treatment.
Fixed by caching the focused window's rect before calling
`keyboard_handler()`, mirroring the mouse path exactly.

**The boot-time "Initialize" button's callback had the wrong signature.**
`ButtonCallback` is `void(*)(void* userdata)`, but the lambda passed to
`addButton()` was `[](void) { ... }` -- zero parameters. A non-capturing
lambda only converts to a function pointer matching its own parameter
list, so this was a genuine type mismatch; it only compiled because
`CFLAGS` carries both `-fpermissive` and `-w`. Calling a zero-arg-compiled
function with one argument passed happens not to crash under cdecl (the
caller pushes the extra arg, the callee's prologue never reads it, the
caller cleans the stack after), which is presumably why nothing was
visibly broken by it -- but it was undefined behavior, not a guarantee.
Fixed to match every other button/checkbox callback in the codebase:
`[](void* userdata) { (void)userdata; ... }`.

Boot-tested: with `MessageBox` explicitly focused (clicking its body,
since `WidgetDemo` -- created last in `initalizeWindowSystem()` -- holds
focus by default at boot), pressing Enter now dismisses it cleanly with
no crash/panic screen, and the serial log shows both
`Initializing AC97 Audio Codec...` and `chorus: initialized DMA PCM
buffer.` -- confirming the Initialize callback (exercising the second fix)
ran correctly, followed by `dismiss()` deleting the window and the
keyboard-path recomposite completing without dereferencing the
now-dangling reference (exercising the first fix), in a single test.

---

## `mods/core/wingman/headers/app.h` — common `WingmanApp` base class

`FileManager`, `MessageBox`, `WidgetDemo` each independently hand-wrote
the same boilerplate for "an app that owns a `Window`": identical
`WindowManager* wm` / `window_ref_t ref` / `Window* window` members, an
identical close-then-`delete this` method paired with an identical
static trampoline, identical tail-of-constructor wiring
(`setOnCloseRequested`/`setKeyboardDelegate`/`setMouseDelegate`/
`wm->add()`/`wm->focus()`), and identical `malloc`/`free`-backed
`operator new`/`operator delete` overloads (needed since this is a
freestanding kernel with no global `operator new` -- confirmed no such
override exists anywhere in the tree). Same bar already held elsewhere
in this codebase for "not premature": three real, working call sites
needed exactly this, unlike e.g. the dead `WidgetType` enum. `Widget`
(`headers/widgets/widget.h`) is the direct precedent already in this
codebase for the same move, applied to UI controls instead of apps.

`WingmanApp` is header-only (like `Widget`), no matching `.cpp`, so no
Makefile changes. It inherits `KeyboardDelegate`/`MouseDelegate` itself
-- collapsing what used to be `class X : public KeyboardDelegate, public
MouseDelegate` repeated three times into `class X : public WingmanApp`
once. Bonus fix: `explorer.h`'s old inheritance was `class FileManager :
public KeyboardDelegate, MouseDelegate` -- the second base had no access
specifier, defaulting to **private** for a `class` (a real inconsistency
vs. `MessageBox`/`WidgetDemo`'s `public` inheritance of the same
interface). The single shared base fixes this for free.

`registerWindow()` (protected, called once as the last line of each
derived constructor, after `window` is fully built/configured) folds in
`setOnCloseRequested`/`setKeyboardDelegate`/`setMouseDelegate` *and* the
`wm->add()`/`wm->focus()` tail -- all identical single-line calls across
all three apps. Safe regardless of each app's slightly different internal
ordering before this change (`FileManager` called these slightly later
than `MessageBox`/`WidgetDemo` did) because nothing renders until the one
shared `wm->composite()` call at the end of `initalizeWindowSystem()` --
ordering among these calls has no observable effect before then.

`close()`/`closeTrampoline()`/`registerWindow()` are `protected` --
nothing outside the class hierarchy called `closeWindow`/`dismiss`/
`closeTrampoline` by name before this change. `~WingmanApp()` is
`virtual` and `public` (matching `Widget`'s own `virtual ~Widget() {}`),
deleting `this->window` if non-null -- required for `close()`'s
`delete this` (invoked through a base pointer) to correctly run each
derived class's real destructor first. `MessageBox`'s two internal
self-close call sites (`onKeyboard()` on Enter, `onMouseEvent()` on a
button click) were renamed from `this->dismiss()` to `this->close()`,
the only behavioral rename anywhere -- `dismiss()` stops existing as a
distinct name.

**Deliberately out of scope**: `width`/`height`/`offsetX`/`offsetY`/
`padding`/`thickness` are also duplicated ints across all three, but
they're read directly (`this->width`, not `this->window->width`)
throughout each app's own drawing code -- unifying that would mean
touching every `draw_border()`/`draw_background()`/etc., a much bigger
and riskier change than killing the wm/ref/window/close/registration
boilerplate. Not done here.

Boot-tested: visual output pixel-identical to the pre-refactor baseline;
closed all three windows in turn via their title-bar close buttons,
each disappearing cleanly with the other two unaffected (exercising
`close()`/`closeTrampoline()`/the base destructor's window deletion for
all three derived classes); re-verified `MessageBox`'s Enter-to-dismiss
path specifically, since it's the exact scenario the keyboard NULL-deref
fix above targets -- confirmed clean dismissal, no panic screen, and the
serial log showing both `Initializing AC97 Audio Codec...` and
`chorus: initialized DMA PCM buffer.`, same as before the `dismiss()` ->
`close()` rename. No serial errors anywhere in the run.

---

## `mods/core/wingman/wingman.cpp` / `manager.{h,cpp}` — interaction state moved into `WindowManager`

`wingman.cpp` used to own drag state (`draggingWindow`, `dragOffsetX`/
`dragOffsetY`, `lastDragRedrawMs`, `dragLastRect`) and click-processing
state (`lastButtons`) as file-static globals, while focus and z-order --
the same category of "how does the window manager respond to
interaction" state -- already lived as real `WindowManager` members. No
architectural reason for the split; drag support was evidently added
later into whichever file was already open. `mouseFunctionWindowManager()`
and `keyboardFunctionWindowManager()`, the two full dispatch functions
containing the actual interaction logic (focus-click routing, drag
lifecycle, delegate dispatch, dirty-rect accumulation), moved onto
`WindowManager` too, as `handleMouseEvent()`/`handleKeyboardEvent()`.
`wingman.cpp` is now exactly two things: the input-queue/worker-task
plumbing that decouples IRQ context from processing, and the boot-time
wiring (`initalizeWindowSystem()`) that launches the starting apps.

**What deliberately did not move, and why:**
- `redraw_screen()`/`redraw_screen_rect()` stay in `wingman.cpp`. They're
  hardware-presentation glue (`vbe_get_back_buffer()`, `vbe_flip()`,
  `draw_cursor_into_buffer()`) -- a different concern from compositing
  into `WindowManager`'s own offscreen `Surface`. Confirmed via grep that
  `redraw_screen()` is also called from `cursor.cpp` (inside
  `redraw_cursor()`) and is part of the public `headers/wingman.h` API,
  so it needs to keep external linkage from this TU regardless.
- `redraw_cursor(WindowManager* wm, int x, int y)` and `set_cursor_id()`
  stay as free-function calls from `wingman.cpp`'s thin wrapper, matching
  the *existing* precedent already in this codebase: `redraw_cursor()`
  already took `WindowManager*` as a parameter rather than being a
  method, and already internally called into the hardware-presentation
  layer. Not disturbing that boundary.
- `stdin_is_reading()`'s check stays in `wingman.cpp` -- it's a policy
  question of whether an event should reach Wingman at all (a task owns
  stdin right now), not a `WindowManager` concern. Keeping it out avoids
  giving `WindowManager` a new dependency on `mods/dev/syscall/syscall.h`
  for something unrelated to window management.
- The static `WindowManager* wm` pointer, `bufferSize`, and the whole
  input-queue/worker-task block are untouched -- exactly the boot-wiring
  and queue-plumbing the refactor is meant to leave behind.
- `fileManager`'s own dangling-pointer-on-close problem is a separate,
  not-yet-addressed item (a registry would replace the bespoke global) --
  out of scope here.

`handleMouseEvent()`/`handleKeyboardEvent()` return `bool` (did anything
change) and write the affected region into an out-parameter `Rect*
dirty` rather than presenting to hardware themselves -- `WindowManager`
stays hardware-agnostic (it only ever touches its own `Surface`), and
`wingman.cpp`'s thin wrapper functions own deciding what to do with a
dirty region (call `redraw_screen_rect()`) versus no change (call
`redraw_cursor()` to keep just the sprite in sync). `handleKeyboardEvent()`
wraps the existing `keyboard_handler()` method unchanged, adding the
dirty-rect-cached-before-the-call fix from a recent session (caching
`focusedWindow`'s rect before calling `keyboard_handler()`, since
handling the key can delete the focused window) -- this fix now lives on
`WindowManager` instead of the free function it used to guard.

Boot-tested: pixel-identical to the pre-refactor baseline; dragged
`WidgetDemo` to a new position and confirmed smooth movement with no
stale pixels left behind (the relocated throttled-redraw/union-rect
logic); clicked between overlapping windows and confirmed correct
raise-to-front/z-order behavior (relocated `windowAt()`/`focus()`
logic); closed a window via its title-bar button with no crash and the
others unaffected; re-ran the `MessageBox` Enter-to-dismiss regression
specifically (focus it, press Enter) and confirmed clean dismissal with
the same `Initializing AC97 Audio Codec...` / `chorus: initialized DMA
PCM buffer.` serial output as before, now exercised through
`WindowManager::handleKeyboardEvent()` instead of the free function it
replaced. No serial errors anywhere in the run.

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

## `mods/core/wingman/apps/widgetdemo/widgetdemo.cpp` — `WidgetDemo` window

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

- `onMouseEvent()` computes hover state (`this->button->contains(x, y) ||
  ...`) and calls `set_cursor_id()` on *every* event, before the
  press-edge check -- matching `MessageBox`'s buttons, which do the same.
  This used to be entirely absent: `pressedEdge & 1` was checked first
  and everything else (including hover) returned early on a plain mouse
  move, so hovering the button never changed the cursor. Scoped to
  `button`/`checkbox`/`toggle` (things a click actually does something
  to) -- `textInput` doesn't get a distinct hover cursor since there's no
  text-beam sprite in `cursorArray` (only 3 sprites exist: default arrow,
  one unused, and the hand used here).
- Clicking inside `textInput` focuses it; clicking anywhere else in the
  window (including on the other widgets) unfocuses it. With only one
  keyboard-consuming widget in this window, this is sufficient -- it
  doesn't attempt to solve multi-widget focus routing, which is the same
  open gap already tracked in `docs/TODO.md` (Priority 3).
- Every consumed keystroke redraws just `textInput` (self-clearing --
  `TextInput::draw()` fills its own background before drawing text, so
  repeated redraws don't accumulate anything).
- Every click used to call `draw_widgets()` alone -- see "click redraw"
  below for why that was a real bug, not the safe "redraw the whole
  affected region" precedent it looked like at the time. It now calls
  `redraw()` (the full border+background+title+widgets repaint), cheap
  enough at this window's size that finer-grained tracking isn't worth
  pursuing instead.

Wired into `initalizeWindowSystem()` (`wingman.cpp`) as a second
always-open window alongside the file explorer and the AC97 `MessageBox`,
positioned at `(500, 120)` so it doesn't sit on top of either at boot.

### `mods/core/wingman/apps/widgetdemo/widgetdemo.cpp` — click redraw

`onMouseEvent()` used to call `draw_widgets()` alone on every click
anywhere in the window (not just on the button). `draw_widgets()` draws
the field labels ("Button:", "Text:", "Checkbox:", "Toggle:") directly
via `utility_draw_string()`, with no background clear first -- unlike
`Button::draw()` (fills its own background every call) or `TextInput::draw()`
(same), those four labels have nothing protecting them from redraw.

With the old solid 1bpp `Font[]` glyphs this was harmless (a full-opacity
blit just overwrites the previous one outright, same as the
`explorer.cpp` selection-redraw bug's history). With alpha-blended TTF
glyphs it isn't: every anti-aliased edge pixel got re-blended on top of
its own previous value on every click, and since both draws use the
*same* foreground color here (unlike `explorer.cpp`'s selection, which
swaps between two different colors), the visible symptom isn't smearing
between colors -- it's the edge alpha compounding toward full opacity
with each pass, i.e. the labels visibly got bolder with every single
click anywhere in the window, not just on the button. Same root cause as
`mods/core/wingman/apps/explorer/explorer.cpp`'s "selection-highlight
redraw" (below), different symptom because the color didn't change
between redraws there.

Fix: call `redraw()` (border + background + title + widgets) instead of
`draw_widgets()` alone, so the label area is cleared before being
redrawn. `MessageBox` was checked for the same pattern and is clean --
`draw_buttons()` clears its own background before redrawing, and
clicking one of its buttons dismisses the whole window rather than
looping back through a partial redraw, so it was never exposed to this.

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

Now wired into `mods/core/wingman/apps/widgetdemo/widgetdemo.cpp`, and
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

## `mods/core/wingman/widgets/slider.cpp` — `Slider` widget

Built to the same `Widget` shape as `Button`/`TextInput`/`Checkbox`
(`contains()` inherited, `draw(surface, thickness)` overridden), plus one
new interaction method the other three don't need: `onMouse(px, py,
buttons, pressedEdge)`. A checkbox/button only care about the exact
packet where the button transitions down (`pressedEdge`), but a slider
also has to keep updating while the button stays *held* and the cursor
moves through later packets that carry no edge bit at all -- so it needs
`buttons` (the live hold state) too, and it needs to be called on every
mouse event delivered to the window, not gated behind `pressedEdge` like
`WidgetDemo`'s existing button/checkbox click dispatch. `Slider` tracks
its own `dragging` bool across calls to know whether an in-progress drag
should keep tracking the cursor even after it strays outside the widget's
own bounds (the same UX real sliders/scrollbars everywhere have) --
dragging starts on a press-edge hit inside `contains()`, continues for as
long as `buttons & 1` stays set regardless of `px`/`py`, and ends the
instant the button releases.

`min`/`max`/`value` are `float`, not `int` -- this project already builds
with `-mfpmath=387` (x87, no SSE) and uses `float` elsewhere
(`mods/core/fontman/fontman.cpp`'s TTF baking, `MessageBox`'s icon-scale
parameter), so there was real precedent and no new toolchain risk. A
purely-integer slider would have forced either a fixed granularity or its
own separate fixed-point scheme for no real benefit.

`onMouse()` returns whether `value` actually changed, mirroring
`Checkbox::click()`'s boolean-return shape -- lets the caller
(`WidgetDemo::onMouseEvent()`) redraw only when something moved instead
of unconditionally, since this method is now called on every mouse event
rather than just on a click.

Visually: a rounded track (reusing `draw_rounded_rect_fill()` from
`shapes.h`, same helper every other widget's rounded corners go through)
with a second, shorter rounded rect drawn on top from the track's left
edge to the thumb's center to show the filled/value portion, and a
circular thumb -- same "square + radius == half the side" trick
`Checkbox`'s toggle-style thumb already uses to get a true circle out of
`draw_rounded_rect_fill()` without a dedicated circle primitive.

Wired into `mods/core/wingman/apps/widgetdemo/widgetdemo.cpp` (a 0-100
demo slider whose `onChange` logs the new value to serial) as the fourth
widget row; `WidgetDemo`'s window grew from 250px to 290px tall to fit
it. Boot-tested via QEMU monitor `mouse_move`/`mouse_button` plus
`screendump`: track/fill/thumb render correctly at the initial 50%
value, drag-tracking moves the thumb and updates the fill in real time
(confirmed both via screenshot and the serial-logged callback values),
moving the cursor across the widget *without* the button held leaves the
value untouched, and a fresh click elsewhere on the track jumps straight
to that position -- clamped correctly at both ends (`0`/`100` observed
directly in the serial log).

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

### Cursor duplication during drag

Found (2026-07-23, user report + screenshot) after the mouse/keyboard
IRQ→task change made mouse events queued rather than handled
synchronously. `mouseFunctionWindowManager()`'s tail:

```cpp
if (needsRedraw) {
    wm->composite(dirtyRect);
    update_mouse_position(mouse_get_x(), mouse_get_y());
    redraw_screen_rect(dirtyRect);
} else {
    redraw_cursor(wm, x, y);
}
```

Every throttled-out drag packet (`needsRedraw == false`, most of them,
per the throttling above) took the `else` branch, which called
`redraw_cursor(wm, x, y)` using the raw event parameters -- the mouse
position *at the time this event was queued*, not necessarily its
position *now*. Once events are drained by a worker task instead of
handled inline in the IRQ, those can differ: a fast drag floods the
queue faster than the worker task can drain it, so by the time a given
queued event actually runs, `x`/`y` can be stale relative to
`mouse_get_x()`/`mouse_get_y()` (the driver's live position) -- exactly
the same staleness the `needsRedraw == true` branch already guards
against (see its own comment, one function above this one).

`redraw_cursor()` (`cursor.cpp`) does erase-old/draw-new correctly *for
the position it's told* -- but `update_mouse_position()` (used by the
other branch) has no erase step at all, it just overwrites the tracked
`mouse_x`/`mouse_y`. So the sequence that produced a permanent ghost
cursor: a throttled packet draws a cursor sprite at a stale position via
`redraw_cursor(wm, x, y)`; the next non-throttled packet recomposites
and calls `update_mouse_position(mouse_get_x(), mouse_get_y())`, jumping
the tracked position straight to the driver's current one with no erase
-- orphaning the sprite drawn at the stale position permanently in the
live framebuffer, since nothing will touch that screen region again
unless some unrelated redraw happens to overlap it.

Fix: `redraw_cursor(wm, mouse_get_x(), mouse_get_y())` -- same live-read
discipline as everywhere else in this function, closing the one place
that still trusted the possibly-stale event parameters. Verified via two
separate rapid simulated drags (30 and 25 rapid move steps) with no ghost
cursor in either case, and the real cursor rendering correctly at its
true final position both times.

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
`mods/dev/console/console.cpp`'s boot-terminal keystroke handler for the same
kind of "someone else owns input right now" situation.

---

## `mods/core/wingman/apps/explorer/explorer.cpp` — hover cursor

Added (2026-07-21) so hovering a file switches the cursor to the same
hand sprite (`cursor_id = 2`) `WidgetDemo`'s buttons and `MessageBox`'s
buttons already use for "this is clickable" -- Explorer just never had
this despite being the most-clicked window in the system. Same pattern as
`widgetdemo.cpp`: compute hover state and call `set_cursor_id()` on
*every* `onMouseEvent()` call, before the `pressedEdge` check, so a plain
mouse move (not just a click) updates it.

The hit-test itself (which file index, if any, is under a given point)
used to live inline in the `pressedEdge & 1` click-handling branch only.
Factored out into `fileIndexAt(x, y)` (returns `-1` for "no file here")
since the hover check needs the exact same column/row/page math the click
handler already had -- not a new abstraction speculatively built ahead of
need, just the same real computation now genuinely called from two sites.
`onMouseEvent()` itself is otherwise unchanged: `set_cursor_id(fileIndexAt(x, y)
!= -1 ? 2 : 0)` runs unconditionally first, then the existing
`pressedEdge & 1` branch calls `fileIndexAt()` again for the click itself
(cheap enough per-event that caching across the two calls isn't worth the
complexity).

## `mods/core/wingman/apps/explorer/explorer.cpp` — running-ELF tracking

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

## `mods/core/wingman/apps/explorer/explorer.cpp` — selection-highlight redraw

`FileManager::redraw()` takes a bitmask (`0b00111000` by default: background
+ title + options) so callers can skip repainting parts of the window that
didn't change, instead of a full repaint on every interaction. Two call
sites -- `onMouseEvent()`'s "clicked a different row" branch and
`onKeyboard()`'s `'s'`/`'w'` selection-move handlers -- used to pass
`0b00001000` (options only), skipping `draw_background()`.

That was fine back when `draw_options()` drew the old 1bpp `Font[]`
bitmap glyphs (a solid, fully-opaque blit -- redrawing a character in a
new color just overwrote every pixel of the old one outright). It became
a real, visible bug once `draw_options()` switched to alpha-blended TTF
glyphs (`ttf_blit_glyph()`/`ttf_blend_over()`, see "Font Rendering
System" above): a freshly-selected row's blue text was blitted directly
on top of the *previous* frame's white text at the same pixel positions,
with no background clear in between. Fully-opaque glyph pixels (alpha
255) were unaffected -- `ttf_blend_over()` returns pure `fg` when
alpha is 255, ignoring `under` entirely -- but every antialiased edge
pixel (partial alpha) blended the new color against the *old glyph's
ink* instead of the plain background, reading as soft/fuzzy/washed-out
text rather than crisp anti-aliasing. Purely a stale-pixel bug, not a
font, color-blend, or letter-spacing issue.

Fix: both call sites now pass `0b00111000` (the same "full content"
bitmask the default parameter and `fileClick()`'s directory-navigation
success path already used), so `draw_background()` clears the row back
to the plain background color before `draw_options()` redraws it in its
new color.

### `mods/core/wingman/apps/explorer/explorer.cpp` — selection color

`COLOR_B` (the selected-row text color) was `0xFF0000FF`, pure blue.
Against this window's `0xFF403a39` background, that read as faint/
washed-out -- not an anti-aliasing artifact, but a genuine low-contrast
color choice: perceived luminance weights blue at only ~7% (vs ~72% for
green, ~21% for red), so pure blue's perceived brightness (~18/255) was
actually *lower* than the background's (~59/255) despite being fully
saturated in raw RGB terms.

Changed to `0xFFFF5C04`, the pumpkin-orange accent already used in
`mods/dev/console/console.cpp`'s `"Welcome to \(FF5C04)PumpkinOS...` boot
banner -- meaningfully higher perceived luminance (~120/255) against the
same background, and reuses an existing palette color instead of
introducing a new one. Also deliberately not `COLOR_R` (0xFFFF0000,
already used elsewhere for error/danger, e.g. the red "Ignore" button in
`MessageBox`), to avoid a selection state visually implying an error
state.

### `mods/core/wingman/apps/explorer/explorer.cpp` — multi-column layout

`draw_options()` used to loop over every entry in `files[]` unconditionally,
mapping list index directly to a y-offset (`106 + 35*i`) with no bound tied
to window height and no column concept at all. Beyond the ~9 rows that fit
on screen, this wasn't just a cosmetic overflow -- `Surface::putPixelUnsafe`
does zero bounds checking, so a large enough directory wrote straight past
the end of the window's pixel buffer (heap corruption, not just off-screen
drawing). Replaced with a column-major grid, paginated when there isn't
room for every column at once.

**Column-major index math.** For file index `i`, `column = i / rowsPerColumn`
and `row = i % rowsPerColumn`, filling one column top-to-bottom before
starting the next -- reading order matches visual order. `page = column /
columnsPerPage` falls out of the same arithmetic, so a page is just "every
column whose `column / columnsPerPage` matches," not separately-tracked
state. This is why `onKeyboard()`'s `'a'`/`'d'` handlers are just
`currentSelection -= rowsPerColumn` / `+= rowsPerColumn` (clamped to
`[0, fileCount-1]`) -- jumping a full column naturally crosses into the
next/previous page once `column` crosses a multiple of `columnsPerPage`,
with no separate "flip page" branch needed. `'s'`/`'w'` (`currentSelection
+/- 1`, unchanged from before this work) also already did the right thing
for free: incrementing/decrementing the flat index moves down/up within a
column and rolls into the next/previous column at the row boundary.

**`computeGridLayout()`** (`explorer.h`'s `FileGridLayout` struct) is the
single source of truth for `listStartX`/`listStartY`/`columnWidth`
(`200`px, matching the original single-column mouse hit-box)/`columnGutter`
(`padding`, `10`px)/`rowHeight` (`35`px, unchanged) and the two derived
values, `rowsPerColumn` and `columnsPerPage`, computed from the window's
actual content geometry (with a `reservedBottom = 25`px strip for the page
indicator). `draw_options()` and `onMouseEvent()` both call it and must
agree on these values -- worth keeping as one shared method rather than
duplicating the arithmetic, since a drift between the two would mean clicks
resolve to the wrong file.

**`onMouseEvent()`** extends the original 1D "which row" bucket
(`(y-listStartY)/rowHeight`) with a matching column bucket
(`(x-listStartX)/(columnWidth+columnGutter)`), rejects clicks landing in
the gutter between columns, then reconstructs a flat index from
`page*capacityPerPage + localColumn*rowsPerColumn + row` -- the inverse of
the column-major math above.

**Page indicator**: `"Page N/M"`, bottom-right of the content area,
computed from `currentSelection`/`capacityPerPage` and `fileCount`. Not a
separate `redraw()` bitmask bit -- it's drawn as part of `draw_options()`
since it needs to be current whenever the file grid is.

**Defensive per-column text clamp**: a filename longer than fits in
`columnWidth` is now truncated at draw time instead of running unbounded
into the next column's space, computed from the same `charAdvance` value
used to lay out each character. Not purely cosmetic either -- `columnWidth`
is chosen so a column's footprint stays inside the window, so this also
keeps `utility_draw_char`'s `x` argument bounded, closing off the same
class of unbounded-write risk `draw_options()`'s row-count bound closes.

### `mods/core/wingman/apps/explorer/explorer.cpp` — window chrome

Explorer's `draw_background()` filled its *entire* inner content rect with
the flat body color (`0xFF403a39`) and never painted a distinct header
band -- every other Wingman window (`MessageBox`, `WidgetDemo`) paints a
darker `COLOR_TITLEBAR` (`0xFF2d2928`) band under the title with a
`COLOR_DIVIDER` (`0xFF55504f`) line beneath it, so the title reads as a
separate header rather than floating on the body background. Explorer now
does the same, `titleBarHeight = 64` (matching `MessageBox`'s height, not
`WidgetDemo`'s `40`).

The title icon was originally left at `draw_title()`'s default scale (`2`,
i.e. 64x64 rendered) -- taller than the 64px band itself, so it visibly
overflowed past the divider into the body. Fixed by explicitly drawing it
at scale `1` (32x32) instead, vertically centered in the band (`iconY =
18`), with the path text positioned relative to the icon's actual width
(`textStartX = iconX + 48`) rather than the old hardcoded offset sized for
the bigger icon.

### `mods/core/wingman/apps/explorer/explorer.cpp` — current path display

`this->path` already existed and was already correctly maintained by
`fileClick()` (`nullptr` at root, a real path string once you descend into
a subdirectory) -- it just wasn't drawn anywhere. `draw_title()` now shows
it in place of the previously-hardcoded `"Select File"` string (root
displays as `"/"`), with the same defensive length clamp used for
filenames, so a sufficiently deep path can't run past the window's right
edge.

### Verification

Boot-tested (2026-07-20) via two temporary, fully-reverted test setups:
(1) a page-capacity override (forced `rowsPerColumn=2, columnsPerPage=2`)
to confirm real 2-column, 2-page rendering and correct `"Page 1/2"` math
without needing dozens of real files, and (2) the `MessageBox`/`WidgetDemo`
windows temporarily disabled to get an unobstructed screenshot of
Explorer's title bar/path/grid/page-indicator all at once. Both confirmed
correct. Separately: an attempt to generate ~35 real dummy files via a
`sprintf(name, "/dummy%02d.txt", i)` loop surfaced that this kernel's
`sprintf()` doesn't substitute the `%02d` width/zero-pad flag (every
"different" filename collapsed to the literal `"dummy02d.txt"`) -- a real,
separate, minor `sprintf()` limitation, not a ramfs bug, not chased further
here since it wasn't blocking (the page-capacity-override test above
verified pagination without needing many real files).

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
returns -- this is the exact same mechanism the idle task/`task1`/`task2`
rely on (none of them call `task_exit()` themselves either). So
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

The idle task (`tasking_spawn_idle()`) is spawned before anything else in
this span, so it exists before any real task could possibly call
`task_block()`. As of 2026-07-21 it's no longer a round-robin participant
at all (see "mods/dev/tasking/tasking.cpp — idle task" for why) --
`pick_next()` falls back to it directly whenever nothing in `g_runqueue`
is `TASK_READY`, rather than it occupying an actual rotation slot the way
`task0` used to.

Currently `task1`/`task2`'s `task_create()` calls are commented out in
`kernel_main()` (temporarily disabled while debugging an unrelated
ELF-stdin character-doubling bug) -- only the reaper and Wingman's input
worker actually run at boot right now, alongside the (no-longer-rotating)
idle task. The spawn-ordering rationale above still applies whenever
`task1`/`task2` are re-enabled.

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

## `mods/dev/logging/logging.cpp` — NUL-termination fix in `flush()`/`log()`

Relocated from `mods/std/logging.cpp` for the same reason `console` and
`fontman` moved out of `mods/std/` earlier: `Logging` owns real state (a
static linked list of registered `LogDevice` sinks, `Logging::head`, plus
a `capturing` flag), wired up at boot in `p-kernel.cpp`
(`Logging::addLogDevice(&terminalDevice)`/`&serialDevice`) -- not a
`mods/std/`-style stateless utility. `mods/core/` didn't fit either
(nothing GUI-facing about it -- consumers include `mods/dev/pci/pci.h`
and `rtl8139.h`), so it landed in `mods/dev/` alongside the device-level
sinks it dispatches to. Location/package change only, same as those two
moves -- `Logging`, `LogDevice`, `addLogDevice()`, `log()`, `capture()`,
`flush()` all kept their existing names; only the files, directory, and
every include path reaching them changed. The 3 consumers
(`p-kernel.cpp`, `mods/dev/pci/pci.h`, `mods/dev/pci/drivers/rtl8139.h`)
all reached this via angle-bracket `<logging.h>` resolved through `-I
mods/std/include` -- the same pattern `graphics.h` used before the
console move -- converted to relative includes here too.

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

## `mods/dev/pci/drivers/rtl8139.cpp` — receive ring size/allocation mismatch

`RTL8139_HANDLER()` called `RTL8139_RECEIVE_PACKET()` once per interrupt,
no drain loop. A burst of packets arriving close together gets coalesced
by the hardware into a single ROK interrupt, so anything past the first
packet sat undrained in the ring indefinitely (measured: 30 packets sent
back-to-back over the QEMU UDP tunnel netdev, 1 received).

An earlier attempt just wrapped the call in `while ((inb(CMD) & CMD_BUFE)
== 0) { ... }` with no other changes. That made things dramatically
worse: raw packet drainage went up (1864 vs. 1 "Received IPv4 packet"
log lines), but so did `ethernet_handle_packet()`'s "Unknown packet type
detected" fallback -- about 399,000 times. That's not "a few more real
packets got through"; it's the loop reading garbage header bytes and
mis-parsing them as more (bogus) packets. That attempt was reverted
rather than shipped.

The actual bug was a receive-ring configuration mismatch that predates
the drain-loop attempt entirely: `RX_BUFFER_SIZE` was `16 * 1024`, but
`RCR_DEFAULT` (`0xF | (1 << 7)`) never sets the RBLEN ring-size bits
(bits 11:12 of the RCR), which default to `00` -- the smallest ring
option, 8K+16, per the datasheet. So the driver was tracking
`current_packet_ptr` wraparound against a 16KB boundary that the NIC
itself was never using; the actual hardware DMA pointer wraps at 8K+16
regardless of what the software believes the ring size is. Compounding
this, `RCR_WRAP` is set, which permits the NIC to DMA a packet's tail
past the ring's *nominal* end instead of splitting it across the
wraparound boundary -- but the DMA allocation in `RTL8139_INIT()` was
exactly `16 * 1024` bytes with zero slack for that overflow, so such a
write would corrupt whatever memory happened to follow the buffer.

Both bugs were latent under light, single-packet traffic (the wraparound
boundary is rarely reached), which is exactly why the earlier drain-loop
attempt made things so much worse: draining more per interrupt pushes
`current_packet_ptr` through the wraparound boundary far more often,
landing squarely on the pre-existing corruption bug.

Fixed by:
- Changing `RX_BUFFER_SIZE` to `8192`, matching the ring size the
  hardware is actually configured for (`RCR_DEFAULT`'s RBLEN bits).
- Allocating `RX_BUFFER_SIZE + 1500 + 16` bytes of real DMA memory in
  `RTL8139_INIT()` (was exactly `RX_BUFFER_SIZE`), giving the NIC real,
  valid memory for the `RCR_WRAP` overflow case instead of running past
  the allocation.
- Reintroducing the drain loop in `RTL8139_HANDLER()` (Command register
  `CMD_BUFE` bit, offset `0x37`), now bounded to 64 iterations per
  interrupt as defense in depth rather than trusting the flag
  unconditionally.

Boot-tested via the same QEMU UDP tunnel netdev used for the earlier
diagnosis, on an isolated instance (separate tunnel ports) so as not to
disturb an already-running QEMU session. A 30-packet zero-delay burst
now delivers 30/30 with zero "Unknown packet type detected" lines,
repeated across several runs. A heavier 300-packet zero-delay burst
still loses some packets, but the QEMU network-filter pcap dump
(`-object filter-dump`) shows only ~114 of the 330 total frames sent
across both tests ever reached the emulated NIC's wire in the first
place -- confirming the remaining loss is QEMU's `-netdev socket`
backend itself (plain, unreliable UDP over loopback, no guaranteed
delivery), not the guest driver. Every frame that did reach the NIC was
parsed correctly (zero garbage-read lines across all tests). This
matches why the real streaming use case paces sends to the real
playback rate rather than bursting.

---

## `mods/core/wingman/wingman.cpp` — closing windows

`MessageBox` already had a working self-dismiss path (`dismiss()`,
called from any button click or Enter), but the persistent suite apps --
WidgetDemo and Explorer (`FileManager`) -- had no way to close at all;
they're spawned once at boot and lived forever. Added a close control to
each, following the same pattern `MessageBox::dismiss()` already
established: `wm->remove(ref)` to pull the `Window` out of the
`WindowManager`, then `this->window = NULL; delete this;` to free the
app object itself. Explorer needed a small structural change first --
unlike WidgetDemo, `FileManager` didn't store a `WindowManager*`/
`window_ref_t ref` pair or take `wm` in its constructor, so that was
added to match.

The close control's hit-test needed to cooperate with the existing
window-dragging feature (see the section below): a mousedown inside the
drag-handle band (the top `WINGMAN_DRAG_HANDLE_HEIGHT` pixels of any
window) is treated as "start dragging" *before* the click ever reaches
the window's own `onMouseEvent` -- so a close button living in that same
band needs an explicit carve-out in `wingman.cpp`'s drag logic, or it's
visible and hoverable but physically unclickable. This exact failure
mode was hit twice while building this feature: once from an
off-by-`thickness` gap between the per-app hitbox math and the
carve-out's width (a few pixels at the button's near edge fell through
to the drag handler instead), and once from the carve-out being written
for the button's original top-right position while a later revision
moved the button to top-left. Both were only caught by boot-testing with
temporary `serial_write_string`-logged ground-truth coordinates at each
click -- visual screenshot inspection alone couldn't distinguish "hovering
the button, click didn't register" from "not actually hovering the
button" (QEMU's monitor `mouse_move` deltas aren't reliably 1:1 across
boots, so the cursor's apparent position doesn't always match its real
logical one).

This duplication (the same geometry and hit-test math independently
written in wingman.cpp plus each app) is what motivated later folding
it all into a single `TitleBar` class -- see the section below.

---

## `mods/core/wingman/headers/titlebar.h` / `mods/core/wingman/window.h` — `TitleBar` owned by `Window`

Once WidgetDemo/Explorer each had their own close button, the
title-band drawing (background fill, divider line, the button itself)
and its hit-test were byte-for-byte identical across both files,
differing only by macro name prefix. That duplication is what caused
both bugs described in the section above -- the fix in one file's
constants didn't automatically propagate to the other, or to
`wingman.cpp`'s independent copy of the same geometry.

Fixed by extracting a `TitleBar` class (`headers/titlebar.h` /
`titlebar.cpp`) that `Window` now owns as a plain value member
(`window->titleBar`), configured once via `titleBar.configure(height,
thickness, hasCloseButton, contentY, ...)` right after `new Window(...)`
in each app's constructor -- the same post-construction wiring style
already used for `setMouseDelegate()`/`setKeyboardDelegate()`. `contentY`
(the single Y pixel used for both the optional icon and the title text)
is a plain parameter rather than something derived from `height`,
since the two existing bands (40px tall with content at y=12, and 64px
tall with content at y=18) were tuned by hand, not by a formula. A
default-constructed `TitleBar` is inert (`height() == 0`, no close
button), so `MessageBox`'s `Window` needs no changes at all; it simply
never calls `configure()`.

`closeButtonContains(x, y)` takes window-local coordinates (the same
space `Window::handleMouse` receives) and is always top-left (macOS
style); it returns `false` unconditionally when no close button is
configured.

`TitleBar` itself holds no drawing code -- it is pure config/geometry.
Rendering (background fill, divider row, close/minimize/maximize
buttons, icon, then title text, in that fixed order) lives in a free
function, `draw_title_bar(Surface*, windowWidth, const TitleBar&, const
char* title)` in `titlebar.cpp`, declared a `friend` of `TitleBar` so it
can read the private fields `configure()` set. `WindowManager::composite()`
calls it for every window on every composite pass, passing
`window->title` -- *not* from each app's own `redraw()`. This means the
title band is always correct regardless of whether any given app
remembers to redraw it, and an app with dynamic title text (a file
path, say) only needs to call the new `Window::setTitle(const char*)`
(frees the old title, copies the new one, same allocation pattern as the
constructor) when that text changes; the next composite pass picks it up
automatically. It also means a title band is redrawn in full on *every*
composite pass touching that window -- e.g. a focus change or a drag,
not just when the app itself decides to redraw -- which is why no app
should try to hand-draw anything inside the title band itself anymore
(see the `MessageBox` section below, which used to do exactly that and
had to be migrated off it for this reason). `draw_title_bar()` is a
no-op when `height() == 0`, matching the old `draw()`'s behavior.

The title-text width clamp (`availableWidth = (windowWidth - t) -
contentX`, then `maxChars = availableWidth / charAdvance`) clamps
`availableWidth` to `0` before that division. A window narrow enough
(or a close-button/icon zone wide enough) that `contentX` exceeds
`windowWidth - t` makes `availableWidth` negative; casting a negative
`int` straight to `size_t` for the division wraps it to a huge unsigned
value, so `len > maxChars` would never trim the title and
`utility_draw_char()` would draw off the right edge of the window's
surface via `Surface::putPixelUnsafe` (no bounds checking, by design,
elsewhere in this codebase). No shipped window configuration currently
triggers this, but nothing prevented a future one from doing so.

It actually renders a full macOS-style traffic-light trio when
`hasCloseButton` is true, not just the close (red) button -- yellow
"minimize" and green "maximize" dots are drawn immediately to its right,
each in their own `CLOSE_BUTTON_SIZE`-wide slot with no gap between
slots (matching macOS's tight traffic-light spacing), *not yet wired to
anything*: `closeButtonContains()` still only tests the first (red)
slot, so clicking the yellow/green dots is inert -- they fall through
`Window::handleMouse` to the delegate, hit no widget, and do nothing.
`closeButtonZoneWidth()` (the drag-handle carve-out `wingman.cpp` uses)
covers all three slots (`thickness + margin + 3 * CLOSE_BUTTON_SIZE`),
so a mousedown anywhere across the whole trio -- not just the close
button -- correctly doesn't start a drag, matching real window-manager
behavior even though only the first dot does anything yet. Content
(icon/text) start position shifts out past all three slots accordingly.
`MessageBox`'s own hand-drawn icon position (see the section below)
derives its offset from `closeButtonZoneWidth()` rather than a
hardcoded number specifically so it can't drift out of sync with this
again.

The bigger change is where the close button's *click handling* now
lives: `Window::handleMouse()` itself intercepts any event inside
`titleBar.closeButtonContains(x, y)` -- hover sets the pointer cursor and
returns without forwarding to the app's delegate at all; a click fires a
registered `onCloseRequested(void* userdata)` callback (same shape as
the existing `Button::onClick`/`ButtonCallback` pattern) before
forwarding is even considered. Each app registers this once via
`window->setOnCloseRequested(&AppType::closeTrampoline, this)`, where
`closeTrampoline` is a small private `static` method that casts
`userdata` back and calls the app's own (unchanged) `closeWindow()`.
WidgetDemo/Explorer no longer contain *any* close-button
code themselves -- no `closeButtonContains()`, no `draw_close_button()`,
no hit-test branch in `onMouseEvent()`.

Since a click on the close button can lead to `wm->remove(ref)` deleting
the very `Window` whose `handleMouse()` is still executing (a few call
frames down, through the callback), `Window::handleMouse()` returns
immediately after invoking the callback without touching `this` again --
the same "delete this; return;" idiom `MessageBox::dismiss()` already
relied on, just one frame deeper. `wingman.cpp`'s drag-handle carve-out
now queries `hitWindow->titleBar.closeButtonZoneWidth()` directly instead
of a hand-copied constant -- it returns `0` for a titlebar with no close
button (e.g. `MessageBox`'s, which never calls `configure()`), so the
carve-out is a natural no-op there without any extra check. This also
fixes a latent, unrelated const-correctness gap: `wm->focusedWindow` was
being read into a `const Window*` and then used to call the
(non-`const`) `handleMouse()` -- legal only because the Makefile's
`-fpermissive -w` silently downgraded it. Changed to a plain `Window*`
since it's no longer accurate to call
this pointer's target immutable.

One side effect worth noting: the drag carve-out used to apply uniformly
to every window regardless of whether it drew a close button there, so a
corner of `MessageBox`'s title band was previously non-draggable for no
real reason (`MessageBox` was left out of this refactor initially -- see
below). With the carve-out now driven by each window's actual `TitleBar`
state, that corner became draggable like the rest of its title strip
until `MessageBox` later adopted a real close button of its own, which
made that corner non-draggable again -- this time correctly, since it
now has a real button living there.

---

## `mods/core/wingman/apps/message/message.h` / `mods/core/wingman/apps/message/message.cpp` — `MessageBox` close button and title

`MessageBox` was deliberately left out of the `TitleBar` migration above
at first -- it already had a working self-dismiss path (`dismiss()`,
fired from any button click or Enter), and giving it a close-X was a
separate, later decision. Registration mirrors WidgetDemo/Explorer: a
private `static closeTrampoline(void* userdata)` casts back and calls
the existing `dismiss()` unchanged. `onKeyboard()`/`onMouseEvent()`
needed no changes at all -- `Window::handleMouse()` already intercepts
close-button clicks before they'd ever reach `MessageBox`'s own button
hit-testing.

Its icon+title text did, initially, stay hand-drawn: the dialog icon
was drawn at a different Y than the title text (`y=18` vs `y=24`),
which didn't fit `TitleBar`'s single-`contentY` model built for
Explorer's small leading folder icon, so `MessageBox::draw_title()` drew
its own icon and text on top of what `titleBar.draw()` contributed
(band, divider, close button). That stopped being viable once
title-band drawing moved to the free `draw_title_bar()` function called
from `WindowManager::composite()` on every pass (see the section above)
-- `MessageBox`'s hand-drawn overlay only ran when `MessageBox` itself
called `redraw()`, so any composite pass triggered by something else
(a focus change, a drag) would repaint the band underneath it and erase
the icon+text until `MessageBox` happened to redraw again. Rather than
keep a second, less-reliable drawing path alive, `MessageBox` was
migrated onto the same mechanism Explorer already used: `titleBar.configure()`
is called with `hasIcon=true`, `iconId` set from `dialogBoxType`/the
optional `icon` override, and `contentY=18` (reusing Explorer's value,
since both are 64px-tall bands with a 32x32-at-1x icon), and the title
string ("Information"/"Error"/"Warning") is passed via
`window->setTitle(...)` instead of being drawn directly. `draw_title_bar()`
now renders the icon and text itself, in step with the band, on every
composite pass -- `MessageBox` no longer owns any title-band drawing at
all, and `draw_title()` / its private `utility_draw_icon()` helper were
deleted along with the `redraw()` bit that used to call them.

---

## `mods/core/wingman/apps/explorer/explorer.cpp` — title bar shows the current path

`FileManager` used to have its own `draw_title()` that drew `this->path`
(or `"/"` at the root, since `this->path` is `nullptr` there) directly as
the title text. That method -- and the current-path-in-title behavior
with it -- was lost when title-band drawing moved to the free
`draw_title_bar()` function called from `WindowManager::composite()`
(see "mods/core/wingman/headers/titlebar.h / mods/core/wingman/window.h
-- TitleBar owned by Window"): nothing was left calling `draw_title()`,
and nothing replaced it with the new `Window::setTitle()` mechanism, so
the title bar was stuck on the static string passed to `new Window(...)`
at construction ("File Manager") regardless of which directory was
actually open.

Fixed with a small private `updateTitle()` that calls
`window->setTitle(this->path != nullptr ? this->path : "/")`, called
once in the constructor (right after `this->path` is initialized to
`nullptr`, so the title starts as `"/"` rather than the placeholder
passed to `Window`'s constructor) and once more wherever `fileClick()`
finishes updating `this->path` after a directory change (covers both
navigating into a subdirectory and navigating up via `".."`). No other
`this->path` call site needed touching -- these are the only two places
that ever change it. Boot-tested: title reads `/` at the root, updates
to `/hello` on entering that directory, and back to `/` on navigating up
via `".."`, with no stale text left over from the previous directory.

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

Direct framebuffer writes only (`fill()`/`draw_char()`, see
"mods/dev/vbe/vbe.cpp -- bulk framebuffer operations") -- no malloc, no
Surface/Window/Terminal objects. If the fault was caused by heap or
GUI-state corruption, the panic screen still needs to render, so it
can't depend on any of the things that might be what's actually broken.

Its first line is `BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, 0)` (added
2026-07-24, see "mods/dev/vbe/vbe.cpp -- hardware double buffering") --
since Wingman's real page-flipping means region 1 can be the one
currently displayed when a panic fires, and the system halts right after
drawing, this permanently forces region 0 (the one `fill()`/`draw_char()`
write to) back on screen so the panic is actually visible.

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

Updates the tracked cursor position and asks for a present
(`wingman.cpp`'s `redraw_screen()`) -- see "Superseded" note under
"Cursor flicker during movement" below for why this no longer patches the
framebuffer directly itself.

PS/2 can deliver several packets per physical movement (or none) -- it
skips the redraw entirely (no position update, no present) when the
clamped position is actually unchanged.

#### Cursor flicker during movement

Found (2026-07-24, user report), separate from the cursor-duplication
bug fixed in `mods/core/wingman/wingman.cpp` ("window dragging" section)
just before this: even with duplication fixed, moving the cursor
flickered ever so slightly on every single movement, a different root
cause. This function used to write to the live framebuffer in **two
separate passes**: erase the old cursor (restore clean background from
the off-screen buffer), *then*, as a second, later write, draw the new one.
There's no vsync/double-buffering on this framebuffer (a known, separate,
already-tracked limitation -- see `docs/TODO.md`'s cursor-tearing item),
so between those two passes there was a real window where the live
framebuffer held neither cursor image, just bare background. If the
display's actual scanout happened to sample during that gap, the cursor
visibly blinked out for one frame -- on every movement, since the gap
existed on every call.

Fixed (at the time) by computing the union of the old and new cursor
rects and writing each pixel in that union exactly once -- background by
default, cursor glyph overlaid only within the new rect -- so there's a
single write pass per movement instead of two, closing the gap entirely
rather than just shrinking it.

**Superseded (2026-07-24) by real hardware double buffering** -- see
"mods/dev/vbe/vbe.cpp -- hardware double buffering" below. Once presents
go through `vbe_flip()`, a partial in-place patch can't be flipped into
view safely (the untouched regions would be two frames stale, not one),
so `redraw_cursor()` no longer touches the framebuffer at all: it just
updates `mouse_x`/`mouse_y` and delegates to `wingman.cpp`'s
`redraw_screen()`, which composites into the back buffer and flips.

### `update_mouse_position()`

Records the latest mouse position (unclamped -- `draw_cursor_into_buffer()`
clamps against whatever buffer/dimensions it actually draws to).

---

## `mods/core/wingman/headers/types.h` — `Rect` / dirty-rect compositing

**Partially superseded (2026-07-24)**: the software-compositing half of
this (`WindowManager::composite(Rect)` only clearing/re-blending the
changed region) is still exactly as described below and still worth
doing -- it's unrelated to how the result reaches hardware. But the
hardware-blit half (`redraw_screen_rect()` only copying the changed rows
straight to `0xE0000000`) is gone: real page-flipping (see
"mods/dev/vbe/vbe.cpp -- hardware double buffering") means every present
has to copy the *complete* current frame into the back buffer before
flipping, so `redraw_screen_rect(Rect)` is now a thin wrapper that
ignores its argument and calls the full-frame `redraw_screen()`. The
`Rect` plumbing described below (computing the right dirty rect per
event) is retained for `composite()`'s benefit even though the final
blit no longer uses it.

`WindowManager::composite()` used to unconditionally `clearScreen()` the
entire 1024x768 screen and re-blend every window's entire surface against
it, on every single keystroke, click, or drag step -- 786,432 pixels'
worth of clear+blend work regardless of whether a text field gained one
character or a whole window moved. `wingman.cpp`'s `redraw_screen()` then
made it worse on top: two full-buffer `memcpy()`s (screen -> `outputBuffer`
-> the real `0xE0000000` framebuffer) every time, and this project's
`memcpy` is a byte-at-a-time loop with no word-sized fast path (see
`docs/TODO.md`'s "Boot/memory performance" audit), so that's ~3MB copied
twice, byte by byte, per interaction.

Fixed by threading a `Rect` (the minimal bounding box that actually
changed) through the whole pipeline: `WindowManager::composite(Rect)`
only clears and re-blends that rect (intersected with each window's own
bounds), and `wingman.cpp`'s new `redraw_screen_rect(Rect)` only copies
that rect's rows from the clean `wm->screen` buffer straight to hardware
-- no `outputBuffer`, no full-screen copy at all for the interactive path.
The no-arg `composite()`/original `redraw_screen()` still exist,
unchanged, as thin wrappers over the full-screen rect -- kept for the one
real full-screen case, the initial draw in `initalizeWindowSystem()`,
which is a one-time boot cost with no perf pressure on it.

**Deliberately a single bounding rect, not a rect list**: a proper dirty
region tracker (a list of disjoint rects, with merging/splitting logic to
maintain that invariant) is real complexity for a marginal extra win over
"one rect covering everything that changed this event" -- which is
already a large improvement over the full-screen path it replaced. Two
far-apart dirty areas in the same event (rare -- e.g. a window dragged a
long distance in one throttled step) redraw the strip between them
unnecessarily, but that's still bounded and cheap next to a full-screen
redraw, and the code stays simple: `rect_union()`/`rect_intersect()` are
the only primitives needed anywhere in the pipeline.

**`rect_empty()`** (`w <= 0 || h <= 0`) is the required check after any
`rect_intersect()` before touching its result's `x`/`y`/`w`/`h` --
intersecting two non-overlapping rects produces exactly that (negative or
zero width/height at some arbitrary position), not a crash and not a
sentinel value, so skipping the check silently processes a bogus rect
instead of failing loudly. `rect_union()` has no such caveat (a union of
any two real rects is always itself a real rect); it exists specifically
for combining a moved window's before/after position into one dirty rect
covering both the vacated spot and the new one.

**Computing the right rect per event** (`mouseFunctionWindowManager()`
in `wingman.cpp`):

| Event | Dirty rect |
|---|---|
| Focus change (click raises a background window) | that window's own rect -- nothing outside it can change from a pure z-order reorder |
| Window drag, each throttled step | union of the window's rect *at the last actual redraw* and its current rect -- not just its current rect, since throttling means several position updates can happen between redraws, and the true dirty span covers all of them |
| Drag released | same union, forced through regardless of the throttle, using the window reference captured *before* `draggingWindow` is cleared (needed to still reach it) |
| Widget interaction (`handleMouse()` returns true, not dragging) | the focused window's own rect -- individual widgets don't report sub-rects, so the window is the finest granularity available |
| Keyboard (`keyboard_handler()` returns true) | the focused window's own rect -- `keyboard_handler()` can only return true via `focusedWindow->handleKeyboard()`, so `focusedWindow` is guaranteed non-null here |

A single event can trigger more than one of these (e.g. a click that both
refocuses a background window *and* lands on one of its buttons), so
`mouseFunctionWindowManager()` accumulates via a local `markDirty()`
lambda that unions into a running rect rather than assuming only one
branch fires per event.

**Cursor handling in `redraw_screen_rect()`**: the cursor sprite is only
redrawn if its own rect overlaps the rect that was just patched (it could
have been overwritten by the content underneath it). If not, the
framebuffer's cursor is already correct -- `redraw_cursor()` (unchanged)
keeps it in sync incrementally on pure mouse-move events that don't touch
window content at all.

**`draw_title_bar()` must be called after the per-window dirty-rect
check, not before it.** When title-band drawing moved into
`WindowManager::composite()` (see "mods/core/wingman/headers/titlebar.h
/ mods/core/wingman/window.h -- TitleBar owned by Window"), the call was
initially placed before `rect_intersect(windowRect, dirty)` /
`rect_empty(region)`, so every composite pass -- even one triggered by a
single unrelated window's own redraw -- re-rendered every other window's
full title band (background fill, up to three anti-aliased circles, and
title text) regardless of whether that window's rect intersected the
dirty region at all. That defeats the whole point of the dirty-rect
plumbing above: cost scaled with total window count on every redraw
instead of just the window(s) actually touched. Moved to after the
`rect_empty(region)` check/`continue`, so a window's title band is only
redrawn on a composite pass that already includes it -- which is always
true on the pass where that window's own content or title actually
changed, since the app requesting that redraw marks its own rect dirty.

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

### `redraw_screen()` (real hardware double buffering, 2026-07-24)

Replaced the old `outputBuffer` scratch-copy scheme entirely -- see
"mods/dev/vbe/vbe.cpp -- hardware double buffering" for why. Every present
is now: `memcpy` `wm->screen`'s composited buffer straight into
`vbe_get_back_buffer()`'s VRAM region, stamp the cursor into that same
back buffer via `draw_cursor_into_buffer()`, then `vbe_flip()`. Always a
full frame, never a partial rect (`redraw_screen_rect()` is now a thin
wrapper that ignores its `Rect` and calls this) -- a real back buffer
can't be safely flipped into view with a stale partial patch in it.
`initalizeWindowSystem()`'s final call to this is the one-time full
desktop render at boot; it needed no changes.

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

When something changed, `redraw_screen_rect(dirtyRect)` triggers a full
present (see "mods/dev/vbe/vbe.cpp -- hardware double buffering" -- every
present is full-frame now, the `dirtyRect` argument is vestigial), which
always stamps the cursor in via `draw_cursor_into_buffer()`. Otherwise
(nothing else changed), `redraw_cursor()` handles just the cursor moving,
still triggering its own full present but skipping it entirely when the
clamped position is unchanged (PS/2 repeat packets).

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

## `mods/core/wingman/apps/message/message.h` / `message.cpp`

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
`Icons[]` (see `mods/core/wingman/data/icons.h`), overriding that default.

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

`Icons[]` (`mods/core/wingman/data/icons.h`) currently holds 12 icons -- an
out-of-range override falls back to `dialogBoxType`'s default rather than
reading past the array.

### `draw_background()` — title bar band

Title bar band + divider, so the icon/title read as a distinct header
instead of blending into the body.

### `draw_body()` — word wrap

Used to wrap at a hardcoded `i % 30` (every 30 characters, fixed 16px row
height) -- tuned for the old fixed 8px-bitmap-grid character stride
(`8 * scale` = 16px at scale 2), where 30 chars × 16px ≈ 480px happened
to roughly fill the 500px-wide box. Once `ttf_font_char_advance()`
started returning the font's real (narrower) advance width instead of
that grid assumption, the same "30" wrapped at only ~240px -- roughly
half the box's actual text width -- and silently stayed wrong, since
nothing tied the wrap count to the box's real dimensions or the font's
real metrics.

Rewritten to compute `charsPerLine` from the box's actual text-area
width (`this->width - 2 * (2 * padding)`) divided by the real
`charAdvance`, so it can't drift out of sync with either again. Also now
breaks at the last space within budget (scanning backward from the width
limit) instead of a raw character cut, so words don't get split
mid-word -- falls back to a hard break only if a single "word" is wider
than the whole line (no space found in budget). A literal `\n` in the
message forces a break wherever it falls, even short of the width
budget, in case a caller ever wants an explicit line break.

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

## `mods/dev/elf/elf.cpp` — `elf_lookup_symbol()`

Previously a stub that always returned `NULL`, meaning any ET_REL ELF
program with an undefined external symbol (i.e. any program that wants to
call a kernel function by name rather than being fully self-contained)
would fail relocation outright.

At the time this was written (2026-07-12), the project was still
single-address-space and ring-0-only, so there was no reason for "a
program calls into the kernel" to mean "traps through a syscall number."
The int 0x80 syscall table (`sys_open`/`sys_read`/`sys_write`/`sys_close`/
`sys_exit`) still exists for programs that want an explicit trap boundary,
but it isn't the only door in: `elf_lookup_symbol()` does a linear
`strcmp` scan over a static `elf_exports[]` table of `{name, address}`
pairs and hands back the real function pointer, exactly like a minimal
static/dynamic linker's symbol resolution. A program can link against
`malloc`, `strcmp`, `fopen`, etc. directly, by name, and the relocator
patches the call site to jump straight at the kernel's own implementation
-- no new syscall number needed for each new thing a program wants to do.

**No longer the current design philosophy** as of the ring-based privilege
pivot (2026-07-21, see "mods/dev/gdt/gdt.cpp -- ring-transition GDT/TSS
(Phase 0)" below) -- this table is exactly the kind of Level-2-bypasses-
Level-0 hole the new model is meant to close. `elf_spawn()` still creates
every ELF task at `ring = 0` today, so `elf_exports[]` has zero
interaction with the syscall-gate hardening done in Phase 1 -- but once
ELF programs actually run at ring 3, handing them 35 raw kernel function
pointers with no validation defeats the whole point of that isolation.
Explicitly flagged as needing to be closed before "Level 2 program
internals" is safe (`docs/TODO.md`'s intro and Phase 1's "Explicitly out
of scope" section below).

The export table intentionally mirrors only functions that are genuinely
implemented in `mods/std/`, not everything declared in its headers.
`ftell()` is declared in `mods/std/include/stdio.h` but has no definition
anywhere in the kernel; exporting it would hand a program a pointer to
nothing, which `--oformat binary` linking (see "`mods/dev/context/
setjmp.h`/`setjmp.asm` deletion" above -- `x86_64-elf-ld`'s `--oformat
binary` output doesn't error on undefined symbols the way a normal
ELF-format link does) would not catch at kernel-build time either. Any
future export added to this table should be checked the same way before
being listed.

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

## `mods/dev/tasking/tasking.cpp` — idle task

`task0` used to be a real round-robin participant: `for(;;) hlt;`, spawned
via plain `task_create()` like everything else, sharing rotation equally
with every other task. That was fine when it was the *only* other task
most of the time, but once the input-worker task (above) put real,
latency-sensitive GUI work on the same round-robin footing, it became a
real problem: `pick_next()` gives one task exactly one tick before
rotating to the next, with no priority concept, so every other 1ms tick
went to `task0` doing nothing useful instead of draining queued
mouse/keyboard work. Compositing that used to run atomically inside the
IRQ handler (interrupts masked, exclusive CPU access, however long it
took) now had its effective throughput roughly halved by an idle task
that produces nothing — the direct cause of a user-reported "everything
got slower" regression right after the input-worker change landed
(2026-07-21).

Fixed (2026-07-21) by taking the idle task out of round-robin rotation
entirely: `task_create()` gained an `enqueue` parameter (default `true`,
unchanged for every other caller) that skips the `runqueue_push()` call.
`tasking_spawn_idle()` uses `enqueue=false` to build the idle task's
`task_t`/stack frame without ever putting it in `g_runqueue`, and stores
it in `g_idle_task`. `pick_next()` now falls back to `g_idle_task`
directly whenever nothing in `g_runqueue` is `TASK_READY`, instead of
treating idle as just another rotation candidate — so it only ever runs
when there's genuinely nothing else to do, not once every other tick
regardless of load.

### A latent bug this surfaced (and fixed as a prerequisite)

`pick_next()`'s old fallback, when nothing else was `TASK_READY`, was
`return cur` unconditionally — correct only because `task0` was *always*
present as a real alternative, so the fallback path was never actually
exercised in practice. Taking `task0` out of rotation exposed the real
bug underneath: `task_block()`/`task_exit()` both force an immediate
reschedule (`int $0x20`) right after setting `g_current`'s state to
`TASK_BLOCKED`/`TASK_DEAD` — if `pick_next()` still found nothing else
ready and returned `cur` (itself, now non-runnable) instead of a genuine
idle fallback, `scheduler_on_tick()` would resume execution right where
`task_block()`/`task_exit()` left off, as if the call had just returned
normally. For `task_block()` specifically, that would silently defeat the
whole point of blocking — the reaper or input-worker task's "wait until
woken" loop would just spin as if nothing happened, whenever they were
the only other real task and momentarily had nothing to do.

This is the same failure mode already documented in "`p-kernel.cpp` —
`kernel_main()` task-creation race" (further down), where a real timer
tick landing before `task0` existed at all hit an equivalent no-alternative
case — that fix (deferring all ticks via `sched_lock()`/`sched_unlock()`
until every startup task exists) only guaranteed idle exists *before
scheduling starts*, not that `pick_next()` handles "nothing ready" safely
once scheduling is already live. `pick_next()` now checks `cur`'s actual
state (only `TASK_RUNNING` is safe to just keep running) before ever
falling back to it, and falls back to `g_idle_task` otherwise — a real
fix to the underlying gap, not just a workaround for the case that
happened to surface it.

## `mods/dev/tasking/tasking.cpp` — task/stack reaper

Phase 2 left a known gap: neither a spawned ELF task's `malloc`'d stack
nor its `g_tasks[MAX_TASKS]` slot was ever reclaimed on exit, capping
the kernel at 32 ELF programs ever launched per boot. Closed
(2026-07-20) with a dedicated reaper task, not scheduler- or
join-triggered reaping — see "why a dedicated task" below.

### `stack_base`

`task_t` gained a `stack_base` field: the original `malloc`'d stack
pointer, set only by `elf_spawn()` (`elf.cpp`) right after a successful
`task_create()`. Statically-allocated stacks (the idle task's, the
reaper's own, `task1`/`task2`) leave it `NULL` -- `task_alloc()` already
zeroes every new `task_t` byte-for-byte, so this is the default with no
extra code.
The reaper unconditionally calls `free(t->stack_base)` on every reaped
task; `free(NULL)` is a guaranteed safe no-op (see "Font Rendering
System" → "Two real bugs found while building this" → Bug 1, the
session that actually added that guarantee), so one code path handles
both stack ownership cases correctly without a second flag.

### Why a dedicated task, not scheduler- or join-triggered reaping

`g_runqueue` is a singly-linked **circular** list with no `prev` pointer
and no existing remove function -- `runqueue_push()` is the only thing
that ever mutates it. Dead tasks were never unlinked; `pick_next()` just
skipped over them forever via the `state != TASK_READY` check. Reaping
therefore isn't just "free some memory," it's real list surgery (walk to
find the predecessor, splice the dead node out), and that doesn't belong
in `scheduler_on_tick()`/`pick_next()` -- doing it in the timer IRQ path
would extend the single most latency-sensitive code path in the kernel
for work that has no urgency of its own, on principle the same reasoning
already written into the "moving IRQ work out of interrupt context"
deferred item elsewhere in `docs/TODO.md`. Join-based reaping (the way
Linux ties `task_struct` freeing to a parent's `wait()`) was considered
and rejected too, not on principle but on cost: there's no parent/child
relationship or `wait()`-equivalent blocking primitive in this codebase
yet, and building one just to reap tasks would be substantially more
new infrastructure than the reaper itself.

A dedicated reaper task reuses everything that already exists instead:
`task_block()`/`task_wake()` for being woken, `sched_lock()`/
`sched_unlock()` for safely mutating the runqueue outside the IRQ path
(the same primitive `kernel_main()`'s task-creation race fix already
established for exactly this kind of "safely touch scheduler state
without a tick interrupting mid-mutation" need). No new synchronization
primitive was needed anywhere.

### `task_reap_dead()` and `reaper_task_fn()`

`task_exit()` now also sets a `g_reap_pending` flag and calls
`task_wake(g_reaper_task)`, both inside/right after the same
`sched_lock()` span it already used to mark `TASK_DEAD`. The reaper
task loops: `sched_lock()`, check-and-clear `g_reap_pending`, and if it
was set, call `task_reap_dead()` (which walks the runqueue, unlinks
every `TASK_DEAD` node, frees `stack_base`, and clears the
corresponding `g_task_used[]` slot via pointer arithmetic,
`idx = cur - g_tasks`), then `sched_unlock()`; if nothing was pending
it calls `task_block()` instead of looping tight.

The walk in `task_reap_dead()` is bounded by a plain `MAX_TASKS`
iteration count, deliberately not by walking back to a captured "start"
node: once nodes get unlinked mid-walk, the list can shrink to a
self-looped single remaining node that never again equals whatever
pointer the walk started at, which would make a start-based termination
condition loop forever. A fixed iteration bound visits every real node
at least once regardless of how much the list shrinks during the walk,
and harmlessly revisits an already-checked live node at most a few
times near the end of the budget if the list is now shorter than
`MAX_TASKS`.

`task_wake()` is a no-op if the reaper hasn't actually reached its own
`task_block()` call yet (e.g. very early after boot) -- this is not a
lost-wakeup bug, since `g_reap_pending` stays set regardless of whether
the wake landed, and the reaper reaps on its very next run either way,
just possibly slightly delayed rather than instant.

### A prerequisite bug found while building this: `elf_spawn()`'s missing `sched_lock()`

`kernel_main()`'s own direct `task_create()` calls (`task0`, etc.) were
already wrapped in `sched_lock()`/`sched_unlock()` (see "kernel_main()
task-creation race" above) -- but `elf_spawn()`'s call to the same
`task_create()`, which also calls `runqueue_push()`, was not. Since the
reaper depends on the runqueue's link structure staying coherent, this
was a real, exploitable gap: a timer tick landing mid-`runqueue_push()`
from an ELF spawn could hand `scheduler_on_tick()` a partially-updated
list. Closed by wrapping `elf_spawn()`'s `task_create()` call the same
way.

### `explorer.cpp`'s `runningElfTasks[]` guard needed a matching fix

Before the reaper existed, a dead task's `g_tasks[]` slot was never
reused, so a stored `task_t*`'s `.state` field was a reliable, permanent
signal that "this specific program run is done." Once slots can be
reaped and recycled for an unrelated task, that's no longer true --
`.state` on a recycled slot reflects the *new* occupant, not the
original tracked run. Fixed by also storing the `pid` captured at
tracking time in `RunningElfEntry` and checking `task->pid ==
storedPid` before trusting `.state` at all; a pid mismatch is treated
the same as `TASK_DEAD` (the slot's stale, reclaim it).

### Verification

Boot-tested (2026-07-20) by spawning 50 short-lived tasks in a single
session via a temporary driver task (not inline in `kernel_main()` --
see the next paragraph for why that distinction mattered) against
`MAX_TASKS = 32`: 50 succeeded, 0 failed, no panic, no fault, no hang --
proof slots are actually reclaimed and reused, not just "doesn't crash
at 32."

One real bug surfaced by the first attempt at this test, worth keeping
in mind for any future test code in this area: test logic placed inline
in `kernel_main()` *after* real tasks exist silently stops executing
partway through and never completes, with no crash or error. `g_current`
(pid 0, `kernel_main()`'s own bootstrap context) is built via
`task_alloc()` directly, never `runqueue_push()`'d (see "tasking_init()
placement" above) -- so once any real task is scheduled, the very next
timer tick's "first tick" branch in `scheduler_on_tick()` permanently
abandons whatever's left of `kernel_main()`, test code included. Test
logic that needs to actually run after tasking is live has to be its
own spawned task, not more of `kernel_main()`'s own flow.

## `mods/core/wingman/wingman.cpp` — input queue / worker task

Mouse/keyboard IRQ handlers (`irq.cpp`'s IDT gates for IRQ1/IRQ12 are
interrupt gates, `IDTSetGate(..., 0x8E)`, so `IF` is cleared for the whole
handler) used to call `mouseFunctionWindowManager()`/
`keyboardFunctionWindowManager()` directly. Those functions do real work —
`WindowManager::composite()` (per-pixel blending) and the final `memcpy`
into the live framebuffer at `0xE0000000` — synchronously, with interrupts
fully masked for the duration. This was one of two causes of cursor
tearing (the other, no double-buffering/vsync at the hardware level, was
a separate problem at the time this fix landed -- closed 2026-07-24, see
"mods/dev/vbe/vbe.cpp -- hardware double buffering" below). Closed (2026-07-20) by
moving that work off the IRQ stack entirely, reusing the
`task_block()`/`task_wake()` primitive the task/stack reaper (above)
already established as the pattern for "safely deferred work triggered
from a hot path."

### The queue

A small bounded ring buffer (`WINGMAN_INPUT_QUEUE_SIZE = 32`) of tagged
`WingmanInputEvent` structs (mouse or keyboard fields), pushed by two new
thin functions — `queueMouseEventForWingman()`/`queueKeyEventForWingman()`
— now registered with `mouse_add_event()`/`kb_add_event()` in place of the
heavy functions. Push/pop critical sections use `enter_critical()`/
`exit_critical()` (`mods/dev/port.cpp`), not `sched_lock()` — this is a
plain data race guard around a few field writes, not scheduler-state
surgery, so the lighter primitive is the right one (unlike the reaper's
runqueue unlinking, which does need `sched_lock()`). On overflow (queue
full), a push is silently dropped rather than blocking the IRQ handler —
acceptable for input events, which are naturally re-sent on the next
mouse/keyboard tick.

### The worker task

`wingman_input_worker_fn()` loops: block via `task_block()` while the
queue is empty, otherwise pop one event and call the original, unchanged
`mouseFunctionWindowManager()`/`keyboardFunctionWindowManager()` from task
context (interrupts enabled). Spawned by `wingman_spawn_input_worker()`,
which wraps its `task_create()` call in `sched_lock()`/`sched_unlock()` —
the same pattern `elf_spawn()` and `tasking_spawn_reaper()` use — called
from `p-kernel.cpp` alongside the reaper, inside the existing
`kernel_main()` task-creation span (see "`kernel_main()` task-creation
race" below).

### `explorer.cpp` — MP3 playback buffer lifetime

`play_mp3()`'s launch site in Explorer used to `asm volatile("sti")`
before its `while (AC97IsPlaying()) hlt;` wait loop, because it ran from
inside the mouse IRQ handler chain (an interrupt gate, `IF` cleared on
entry) — without it, neither the AC97 refill IRQ nor anything else could
fire while waiting, and `hlt` would've halted the whole system instead of
just pausing the handler. Now that `mouseFunctionWindowManager()` runs
from the worker task instead, this code path runs in ordinary task
context with `IF` already set, so the `sti` no longer compensates for
anything and was removed. The remaining wait loop still exists for an
unrelated reason: the AC97 IRQ refill callback reads directly from the
decoded buffer for the whole playback duration, so it can't be freed
until playback finishes (same constraint the boot-time MP3 test handles).

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
`syscall.asm` segment push/pop mismatch (see "Font Rendering System" →
"Two real bugs found while building this" → Bug 2, further down in this
document, for the fix itself) corrupting the nested `int 0x80` frame
`task_block()`'s `int $0x20` needs to resume into correctly — fixed that
bug (it was real), 
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

## Font Rendering System — full reference (TrueType via stb_truetype)

This is the complete, top-to-bottom reference for how text gets from a
`.ttf` file on disk to anti-aliased pixels on screen, covering every
component involved: the vendored font asset, the `incbin` embedding
mechanism, the vendored `stb_truetype` library, the boot-time atlas
bake, the shared glyph-blit core, all 6 integration points, the one
deliberate exclusion (the panic screen), the two real bugs found while
building it, and how it was verified.

Replaces the blocky, integer-scaled 8x8 bitmap `Font[]`
(`mods/dev/vbe/font.h`) with real anti-aliased TrueType
rendering, for every draw-char call site *except* the kernel panic
screen. This had been attempted once before this session and reverted
in full with no git trace — it hit a real bug (unsigned wraparound on
negative TTF offsets, now understood and explicitly avoided below) and
then appeared not to boot, but that conclusion was never actually
verified: at the time, the sandbox's ability to boot-test via QEMU was
wrongly believed broken. This session proved that assumption false (see
the tasking Phase 3 sections above), making this a genuine,
properly-verified retry — including a real visual screenshot of the
final result, not just a clean boot log.

### Data flow, end to end

```
Cousine-Regular.ttf (disk, build time)
  -> incbin (font_data.asm)                    embeds raw bytes into .rodata
  -> _binary_font_ttf_start/_end symbols        C++ reads the embedded bytes
  -> ttf_font_init() at boot                     (graphics_initalize_stage1)
       -> stbtt_PackBegin/PackFontRange/PackEnd  once per size tier (2/3/4)
       -> FontAtlas { bitmap, chardata[], ... }  3 of these, baked once
  -> ttf_font_get_atlas(scale)                   looked up by every draw-char call
  -> ttf_blit_glyph()                            alpha-blend blit from atlas
  -> draw_pixel() / Surface::putPixelUnsafe()    same primitives as before
```

Nothing in this pipeline touches the rasterizer after boot — every draw
call from then on is a cheap lookup-and-blit against pre-baked coverage
bitmaps.

### Package layout: `mods/core/fontman/`

Originally landed under `mods/std/` (font rendering felt adjacent to
`mods/std/graphics.cpp`, as it was called at the time), then deliberately
relocated to `mods/core/fontman/` — a new component, a peer of
`mods/core/wingman/` rather than nested inside or beneath it, following
the same naming convention (`wingman` for the window-manager suite,
`fontman` for the font-manager subsystem). This is a better fit for what
the system actually is: it owns real boot-time state (3 baked atlases),
has its own init entry point (`ttf_font_init()`), and is consumed by
*both* `mods/dev/console/console.cpp` (the raw-framebuffer `Terminal`)
and every `mods/core/wingman/` widget — it was never really a
`mods/std/`-style stateless utility to begin with. (At the time fontman
moved, the console module was still `mods/std/graphics.cpp`; that same
"owns real boot-time state, not a stateless utility" reasoning is what
later motivated relocating it too, first to `mods/core/console/` and
then to its current home — see "Package layout: `mods/dev/console/`"
below.)

Current layout:
```
mods/core/fontman/
  fontman.cpp     -- atlas baking (was mods/std/ttf_font.cpp)
  fontman.h       -- FontAtlas, ttf_blit_glyph, ttf_blend_over (was mods/std/include/graphics/ttf_font.h)
  font_data.asm   -- incbin embedding (was mods/std/font_data.asm)
```
Internal symbol names (`ttf_font_init`, `ttf_font_get_atlas`,
`FontAtlas`, `ttf_blit_glyph`, `ttf_blend_over`, `TTF_FIRST_CHAR`,
`TTF_NUM_CHARS`) were deliberately left unrenamed in this move — this
was a location/package change, not an API rename, and renaming the
public surface would have meant touching every call site's function
calls in addition to every include path, for no functional benefit.

### Package layout: `mods/dev/console/`

`mods/std/graphics.cpp`/`mods/std/include/graphics.h` first relocated to
`mods/core/console/console.cpp`/`console.h`, for the same reason
`fontman` moved: this module owns real boot-time state (the `Terminal`
singleton, its keyboard-event registration, cursor/scroll position) and
has an explicit two-stage init lifecycle
(`graphics_initalize_stage1()`/`graphics_initalize_stage2()`) — not a
`mods/std/`-style stateless utility.

It didn't stay there. `mods/core/` had, by that point, been described as
"the stateful, GUI-facing components" (`fontman`, `wingman`), but
`console`/`Terminal` isn't GUI-facing at all -- it's the opposite: a
*pre-GUI* boot console. It doesn't integrate with Wingman (no `Window`,
never composited by `manager.cpp`), and its lifecycle is guaranteed to
end before Wingman's ever begins: `procMan()` in `p-kernel.cpp` calls
`terminal_delete()` immediately before `initalizeWindowSystem()`, so the
two literally never coexist on screen. While it's alive (early boot,
before Wingman starts) it does two real jobs: it's the `LogDevice` that
mirrors every kernel `INFO`/`FAIL` log line onto the screen, and it
renders the boot banner and PCI enumeration output -- the only on-screen
feedback before Wingman is up. That's boot-phase device infrastructure,
the same category as `vbe`/`pit`/`kb`, not a GUI subsystem -- so it moved
again, to `mods/dev/console/`, a sibling of those rather than of
`fontman`/`wingman`.

As with the fontman move, both of these were location/package changes
only -- `VBEScreen`, `Terminal`, `terminal_write()`, `terminal_delete()`,
`graphics_initalize_stage1()`, and `graphics_initalize_stage2()` all
kept their existing names throughout; only the files, their directory,
and every include path reaching them changed. `mods/std/include/graphics/`
(the `font.h`/`icons.h`/`cursor.h` bitmap-data subfolder used by
`console.h` for the pre-TTF fallback path) deliberately stayed in
`mods/std/` through both of *these* moves, since splitting it up too
would have widened this reorg's blast radius for no benefit at the time.
It didn't stay there permanently, though -- see "Package layout:
`mods/dev/vbe/font.h` / `mods/core/wingman/data/icons.h` /
`cursor.h`" below for why each of those three ended up split across
three different, more specific homes instead of all three staying
together as one `graphics/` bundle.

### Package layout: `mods/dev/vbe/font.h` / `mods/core/wingman/data/icons.h` / `cursor.h`

`mods/std/include/graphics/` held three unrelated bitmap-data files —
`font.h` (`Font[]`, the 8x8 fallback glyph grid), `icons.h` (`Icons[]`,
file/dialog icon bitmaps), and `cursor.h` (`cursorArray`, the 3 mouse
cursor sprites) — bundled together only because they were all "graphics
data," not because anything actually used them as a set. Splitting them
to each file's real primary consumer, rather than keeping the bundle
intact indefinitely, was the natural next step once `mods/std/` had
already been mostly emptied of stateful/GUI-adjacent content by the
`console`/`fontman`/`logging` moves:

- **`font.h` -> `mods/dev/vbe/font.h`.** `Font[]`'s one safety-critical
  consumer is the free-function `draw_char()` in `vbe.cpp` — the kernel
  panic screen's *only* text-rendering path, which must stay free of any
  dependency on Wingman/fontman infrastructure (a fault handler is the
  wrong place to add a new failure surface; see "Deliberate, permanent
  exclusion: the kernel panic screen" below). Living beside `vbe.cpp`
  (its own directory, not a shared `headers/`) reflects that primary
  ownership, even though several Wingman widgets also fall back to it
  when a TTF atlas isn't baked.
- **`icons.h` -> `mods/core/wingman/data/icons.h`.** `Icons[]`'s
  primary consumers are squarely Wingman-suite GUI code (`explorer.cpp`'s
  file-type icons, `message.cpp`'s dialog icons) — `headers/` already
  holds shared, Wingman-wide (not single-widget-owned) data like
  `types.h`, `shapes.h`, and now `cursor.h`'s array, so this fits the
  same shelf. `vbe.cpp`'s free `draw_icon()` and `console.h` also reach
  into it (the same "a lower layer reaching up into a higher one for a
  shared data asset" shape already accepted for `fontman`), which is
  fine — it's still fundamentally a GUI-presentation asset, not
  boot-critical the way `font.h`'s panic-screen usage is.
- **`cursor.h`'s array merged directly into `mods/core/wingman/headers/cursor.h`**,
  not relocated to its own file anywhere. It had exactly one consumer in
  the entire codebase (that same header), so a standalone shared file was
  pure indirection with no actual sharing behind it — merging it in
  removes a file and a needless include.

Location-only changes again throughout: no symbol was renamed (`Font[]`,
`Icons[]`, `cursorArray` all kept their exact names), only include paths.

### Component: the font asset

`src/bin/Cousine-Regular.ttf` (~297KB) plus `OFL.txt` (SIL Open Font
License 1.1 text, for attribution), from `googlefonts/cousine`. Lives
alongside this project's other shipped binary assets
(`test.mp3`/`main.elf`/`test.wav`/etc.) rather than under `mods/`, since
it's data, not kernel source — even though, unlike those other
`src/bin/` files, it's never `mcopy`'d onto the floppy image as its own
file; it's embedded directly into the kernel binary at build time via
`incbin` (below) instead.

Chosen specifically for being a *metrically* monospace face, not just
visually one — this matters because every layout call site in this
codebase (`Button`'s content-based auto-size, `TextInput`'s
scroll-window math, `FileManager`'s list positions, `WidgetDemo`'s
label layout) hard-codes a fixed `8*scale` pixel advance per character
with zero awareness of real font metrics, inherited from the old 8-wide
bitmap grid. Switching to genuinely proportional spacing would mean
re-deriving all of that pixel math across every widget — explicitly out
of scope here. This project is "replace how a glyph is drawn," not
"replace how strings are laid out." `fontman.cpp`'s `bake_tier()`
verifies the monospace assumption explicitly at boot (below), rather
than trusting it silently.

The visible consequence of this scope choice: letter-spacing reads as
generous/loose in practice (confirmed via screenshot), since Cousine's
natural glyph advance at these pixel sizes is somewhat narrower than the
fixed cell stride inherited from the old grid. Not a bug — a direct,
accepted tradeoff.

### Component: `incbin` embedding

`mods/core/fontman/font_data.asm`:
```nasm
[bits 32]
section .rodata
global _binary_font_ttf_start
global _binary_font_ttf_end
_binary_font_ttf_start:
    incbin "../../bin/Cousine-Regular.ttf"
_binary_font_ttf_end:
```

Chosen over generating a giant C byte-array source file (the pattern
`mods/core/wingman/data/icons.h` already uses, at 78KB for a much
smaller asset than a ~300KB font). Two things were confirmed
empirically *before* writing any real code, not assumed:
- NASM resolves `incbin` paths relative to the assembler's CWD, not the
  including `.asm` file's own directory — matching exactly how the
  Makefile already invokes `nasm` from `src/src/kernel/` for every other
  `.asm` file, so the path above resolves correctly relative to
  `src/src/kernel/` (two levels up to `src/`, then into `bin/`)
  regardless of where `font_data.asm` itself physically lives. Reverified
  after `font_data.asm` moved from `mods/std/` to `mods/core/fontman/`
  (below) and the font asset itself moved from a since-removed
  `mods/assets/fonts/` to `src/bin/` — the path is CWD-relative, so
  neither move required touching this directive's resolution logic,
  only the literal string.
- `kernel.ld`'s `.rodata : { *(.rodata) }` merges the `.rodata`
  contribution from every input object file regardless of link order —
  so this "just works" the same way every other `.rodata` content in
  this flat-binary (`--oformat binary`) kernel already does.

C++ side (`mods/core/fontman/fontman.cpp`):
```cpp
extern "C" const uint8_t _binary_font_ttf_start[];
extern "C" const uint8_t _binary_font_ttf_end[];
```
Size is read via pointer subtraction (`_binary_font_ttf_end -
_binary_font_ttf_start`), deliberately *not* via an extra
`incbin`-adjacent size label (a common tutorial pattern) — a size
label's *address* holds the value as raw bytes, and a caller that reads
the symbol directly instead of taking its address silently gets
garbage. `ttf_font_init()` checks this computed size is non-zero before
baking anything, specifically to catch a broken `incbin` (e.g. an empty
file) with a clear log line instead of a confusing downstream parse
failure.

### Component: vendored `stb_truetype`

`mods/ports/truetype/vendor/stb_truetype.h` — vendored directly (not
a submodule, unlike `minimp3` — this is a genuine single-file library;
pulling in the whole `nothings/stb` monorepo for one header would be
needless). Originally `mods/ports/stb_truetype/`; the directory (and the
wrapper `.cpp` below) were later renamed to drop the `stb_` prefix, since
it's redundant once the directory already lives under `mods/ports/` and
is the only "truetype" thing there -- the vendored header itself keeps
its real upstream filename (`stb_truetype.h`), unrenamed, since it's a
verbatim copy of what `nothings/stb` actually ships.

`mods/ports/truetype/truetype_impl.cpp` (was `stb_truetype_impl.cpp`) --
the one translation unit that defines `STB_TRUETYPE_IMPLEMENTATION`,
mirroring `mods/ports/minimp3/minimp3.cpp`'s existing wrapper pattern
exactly. Its `STBTT_*` macro overrides were decided by actually grepping
the vendored header before writing them, not by assumption:

| Macro | Routed to | Why |
|---|---|---|
| `STBTT_ifloor`/`STBTT_iceil` | real implementations in the wrapper | `mods/std/math.cpp`'s `complexFloor()` returns 0 for every `x <= 1` including all negatives — not usable. Confirmed genuinely reachable from `stbtt_PackFontRange`'s rasterizer path (bezier flattening). |
| `STBTT_fmod` | real implementation in the wrapper | `fmod` doesn't exist anywhere else in this codebase. Confirmed reachable from `stbtt__compute_crossings_x` (scan-conversion), part of the main rasterizer. |
| `STBTT_sqrt`/`STBTT_fabs`/`STBTT_cos`/`STBTT_pow` | existing `sqrt`/`fabs`/`cos`/`pow` in `math.cpp` | Already correct and present; `sqrt` confirmed reachable from bezier curve-length calculations in the main path. |
| `STBTT_acos` | stubbed, `return 0.0` | Traced every call site in the vendored header: only reachable from `stbtt_GetGlyphSDF`/`stbtt__solve_cubic` (the signed-distance-field API), which this kernel never calls (only `stbtt_PackFontRange` is used). Without an override, the header's own default (`acos(x)`) would reference a symbol that doesn't exist — `acos` is commented out, unimplemented, in `mods/std/include/math.h` — and fail to link. |
| `STBTT_malloc`/`STBTT_free` | the kernel's own `malloc`/`free` | Ignoring the `alloc_context` parameter entirely (never dereferenced). |
| `STBTT_assert` | no-op | No `assert.h` in this freestanding environment. |

`mods/core/fontman/fontman.h` includes the vendor header for
*declarations only* (no `STB_TRUETYPE_IMPLEMENTATION`) — exactly
mirroring how `mods/dev/chorus/mp3.cpp` includes `minimp3.h` without
redefining `MINIMP3_IMPLEMENTATION`.

### Component: boot-time atlas baking (`mods/core/fontman/fontman.cpp`)

Bakes fixed-size glyph atlases once, at boot, and never rasterizes
live. `ttf_font_init()` calls `stbtt_PackBegin`/`stbtt_PackFontRange`/
`stbtt_PackEnd` for 3 size tiers, matching the 3 scale factors already
used across every draw-char call site in the codebase:

| Tier | Scale | Cell size (`8*scale`) | Atlas dims |
|---|---|---|---|
| 0 | 2 | 16px | 256×128 |
| 1 | 3 | 24px | 256×128 |
| 2 | 4 | 32px | 256×256 |

Called from the top of `graphics_initalize_stage1()`
(`mods/dev/console/console.cpp`) — after `initialize_memory_pool()` has already
run in `kernel_main()` (so `malloc` is available) and before `Terminal`
or any Wingman widget exists (so every real caller always sees a baked,
or explicitly-invalid-with-fallback, atlas).

Baking once at boot instead of rasterizing per-draw is deliberate, not
incidental: this build has no optimization flag at all (`-O0` by
default) and floating point goes through the x87 FPU exclusively
(`-mno-sse -mno-sse2 -mno-mmx -mfpmath=387` — SSE is disabled entirely),
which makes `stb_truetype`'s rasterizer (bezier flattening,
scan-conversion, coverage accumulation) real, non-trivial CPU cost —
worth paying exactly once per tier at boot, never per glyph per redraw.

Each `bake_tier(int tier, const uint8_t* fontData)` call:
1. `malloc`s a `atlasW * atlasH` single-channel (8bpp coverage) bitmap.
2. `stbtt_PackBegin` initializes stb's internal rectangle packer against
   that bitmap.
3. `stbtt_PackFontRange` rasterizes all 95 printable-ASCII codepoints
   (`TTF_FIRST_CHAR=32` through `126`) at this tier's pixel size in one
   call, filling `atlas.chardata[]` (an array of `stbtt_packedchar` —
   each entry holds `x0,y0,x1,y1` bounding box in the atlas bitmap, plus
   signed `xoff`/`yoff`/`xadvance` offsets).
4. `stbtt_PackEnd` releases stb's internal packing state (the atlas
   bitmap itself is kept).
5. A fixed `baselineRow` is computed once from the font's own vertical
   metrics (`stbtt_GetFontVMetrics`'s `ascent`, scaled via
   `stbtt_ScaleForPixelHeight`) — so every glyph in the tier sits on the
   same baseline row within its cell; ink position varies per glyph via
   `xoff`/`yoff`, the cell origin itself never does (the whole point of
   a monospace grid).
6. The metrically-monospace check (see "the font asset" above): calls
   `stbtt_GetCodepointHMetrics` for all 95 glyphs and confirms every
   `advanceWidth` is identical, logging (not failing) a warning if not.
7. On any failure at steps 1-3, the tier is left with `valid = false`
   and the function returns `false` — `ttf_font_init()` logs a
   `tier %d bake failed, falling back to bitmap Font[]` line but keeps
   booting; a partial or total bake failure degrades gracefully to the
   old renderer rather than blocking boot or leaving garbled text.

`ttf_font_get_atlas(unsigned scale)` maps `scale` (2/3/4) to a tier
index and returns that tier's `FontAtlas*`, or `nullptr` if that tier
never baked successfully — every call site checks this and falls back
to `Font[]` when null.

### Component: the shared glyph-blit core (`ttf_font.h`)

`ttf_blend_over(under, fg, alpha)` — a plain alpha-over compositing
formula (`result = fg*alpha + under*(1-alpha)`, per channel, integer
math) with no dependency on Wingman's `types.h` — `mods/std` stays below
`mods/core/wingman` in this codebase's layering, same as everything else
in this file's neighborhood.

`ttf_blit_glyph(atlas, c, cellOriginX, cellOriginY, fg, plot)` — the
shared "alpha-blend one glyph from a baked atlas" core. Templated on the
plot callback (`plot(px, py, alpha, fg)`, called once per covered
destination pixel) so it works uniformly for both a free-function
framebuffer write (`vbe.cpp`'s `draw_pixel`) and a `Surface` member call
(every Wingman widget), with no virtual dispatch — this project builds
`-fno-rtti`, so there's no `dynamic_cast`/polymorphic-interface option
here even if one were wanted.

**The signed-int discipline, and why it exists**: `stbtt_packedchar`'s
`xoff`/`yoff` fields are signed floats, frequently negative (left
overhang, accents, etc). Every intermediate value derived from them in
`ttf_blit_glyph` — `originX`, `originY`, `dstRow`, `dstCol` — is declared
`int`, never `unsigned`, all the way through, and gets clipped to
`[0, cellSize)` *before* the caller's (usually `unsigned`)
`cellOriginX`/`cellOriginY` is ever added in. This is the direct,
explicit fix for the exact bug class that killed the prior attempt at
this feature: an unsigned wraparound on a negative offset causing a page
fault. By construction, the only place an `unsigned` value participates
is the final `cellOriginX + dstCol` / `cellOriginY + dstRow`, where
`dstCol`/`dstRow` are already proven non-negative and bounded — so that
addition can't itself introduce a new wraparound.

### Integration: the 6 non-panic call sites

Each of these kept its original `Font[]`-loop body as a fallback (gated
on `ttf_font_get_atlas()` returning non-null) and gained a new branch
that looks up the glyph in the right tier's atlas and blits it through
the *same* pixel-plot primitive the file already used. Signatures and
every call site elsewhere in the codebase are completely unchanged —
`draw_string`/`utility_draw_string` and all width-dependent layout math
keep calling the same functions the same way, unaware anything about the
rendering changed underneath them.

**`VBEScreen::draw_char` is the one exception, and no longer fits this
table's own premise.** It originally gained the same TTF branch as the
other 5, but was later deliberately reverted to *always* use the
`Font[]` bitmap path unconditionally, with the TTF branch removed
entirely — a direct response to a request to make the boot `Terminal`
keep the plain, blocky bitmap look, matching the kernel panic screen's
own permanently-bitmap-only rendering (see "Deliberate, permanent
exclusion: the kernel panic screen" above). It's the only one of the 6
that fills an opaque `bg` rect before drawing, unrelated to the TTF
question — it composites onto a raw framebuffer region (the boot
`Terminal`) rather than a Wingman `Surface` that already tracks a known
background, so it can't just blend against whatever pixel happens to be
there already the way the other 5 do.

| Call site | File | Plot primitive | Background handling |
|---|---|---|---|
| `VBEScreen::draw_char` | `mods/dev/console/console.cpp` | `draw_pixel` | Fills an opaque `bg` rect first, then draws the `Font[]` bitmap glyph directly (no TTF atlas lookup at all — see above). |
| `MessageBox::utility_draw_char` | `mods/core/wingman/apps/message/message.cpp` | `utility_draw_pixel` → `Surface::putPixelUnsafe` | Transparent — reads the existing pixel via `Surface::getPixel` first, blends, writes back. |
| `FileManager::utility_draw_char` | `mods/core/wingman/apps/explorer/explorer.cpp` | same | same |
| `WidgetDemo::utility_draw_char` | `mods/core/wingman/apps/widgetdemo/widgetdemo.cpp` | same | same |
| `Button`'s file-local `drawChar` | `mods/core/wingman/widgets/button.cpp` | `Surface::putPixelUnsafe` directly | same |
| `TextInput`'s file-local `drawChar` | `mods/core/wingman/widgets/textinput.cpp` | `Surface::putPixelUnsafe` directly | same |

Each of the 5 transparent-compositing sites follows the identical shape
(shown once, `Button`'s version):
```cpp
static void drawChar(Surface* surface, unsigned x, unsigned y, char c, unsigned color, unsigned scale) {
    const FontAtlas* atlas = ttf_font_get_atlas(scale);
    if (atlas == nullptr) {
        /* ...original Font[]-loop fallback, unchanged... */
        return;
    }
    ttf_blit_glyph(atlas, c, (int)x, (int)y, color,
        [&](int px, int py, uint8_t alpha, uint32_t fg) {
            color_t under = surface->getPixel(px, py);
            surface->putPixelUnsafe(px, py, ttf_blend_over(under, fg, alpha));
        });
}
```

Each corresponding header (`message.h`, `explorer.h`, `widgetdemo.h`, and
the two widget `.cpp` files directly) gained one new
`#include ".../graphics/ttf_font.h"` line alongside its existing
`font.h` include — the fallback path still needs `Font[]`, so that
include was never removed.

### Deliberate, permanent exclusion: the kernel panic screen

`Font[]` (`mods/dev/vbe/font.h`) and the free `draw_char()`
(`mods/dev/vbe/vbe.cpp`) got **zero code changes** — confirmed via
`git diff` showing no modifications to either file, or to
`mods/dev/idt/isr.cpp` (`FaultHandler::panic_draw_string()`/
`draw_panic_screen()`), not merely assumed safe by having avoided them
on purpose.

This isn't a temporary gap to close later. A fault handler is
deliberately the wrong place to depend on a new subsystem: atlas lookups
require boot-init to have already succeeded, and `malloc`/`Surface`
availability is exactly the kind of thing a fault handler must never
assume, given heap or GUI-state corruption could be what caused the
fault in the first place. After this change, `draw_char()` in `vbe.cpp`
has exactly one caller left in the whole codebase — the panic path —
and that's correct, not something to "clean up" by finding it a second
caller.

### Two real bugs found while building this

**Bug 1 — the actual crash, `free(NULL)` (`mods/std/stdlib.cpp`)**:
`free(void *ptr)` never checked for `ptr == NULL`. It unconditionally
computed `block_t *block = (block_t*)((uint8_t*)ptr - sizeof(block_t));`
and wrote through it (`block->next = free_list;`). `block_t` is
`{ size_t size; block_t* next; }` — 8 bytes on this 32-bit build, no
padding. For `ptr == NULL`: `block = (block_t*)(0 - 8) = 0xFFFFFFF8`.
`next` is the struct's *second* field, offset `+4` — so the write lands
at exactly `0xFFFFFFFC`. First boot attempt page-faulted at precisely
that address (`CR2=0xFFFFFFFC`, write, supervisor), deep inside
`stbtt_MakeGlyphBitmapSubpixel`.

Every real libc defines `free(NULL)` as a safe no-op; this kernel's
never did, and nothing had ever called it that way before `stb_truetype`
came along. `stbtt_MakeGlyphBitmapSubpixel` legitimately calls
`STBTT_free(vertices, ...)` unconditionally at the end, for *every*
glyph — and for a glyph with zero contours (the space character has no
outline), `stbtt_GetGlyphShape` never allocates `vertices` in the first
place, leaving it `NULL`. Completely standard, correct behavior on
stb_truetype's part; this codebase's `free()` just had never been asked
to handle it.

Bisected via the same headless-QEMU boot-testing technique used for the
tasking bugs earlier this session, by varying how many glyphs
`stbtt_PackFontRange` was asked to bake in a scratch build: 1 glyph
(`'A'`) baked fine; 2 glyphs starting from space crashed; 1 glyph = space
*alone* crashed. That narrowed it straight to the space glyph's cleanup
path before a single line of `stdlib.cpp` had been read. Fixed with a
one-line guard, `if (ptr == NULL) return;`, at the top of `free()`. Not
a bug in the font integration at all — a real, previously-latent bug in
the kernel's own allocator, found by genuinely correct third-party code
finally exercising a path this codebase had never tested.

**Bug 2 — a real but not-the-actual-cause bug, `syscall.asm`'s segment
push/pop mismatch**: found and fixed *before* bug 1, on the (reasonable
but ultimately wrong) theory that it was corrupting a nested interrupt
frame relevant to font baking. It wasn't related to this feature at
all — see "`mods/dev/syscall/syscall.cpp` — `stdin_read_line()` real
blocking" further up in this document for the actual story (it was the
real cause of a *different* bug, the tasking Phase 3 boot-test failure).
Left here as a cross-reference since
it was genuinely investigated as a font-baking suspect first, and ruling
it out (re-testing after fixing it, seeing the identical crash) is part
of how bug 1 actually got found.

### Verification

Boot-tested headless via QEMU (`-display none`, `-serial file:...`, a
monitor socket to `sendkey` past the boot menu) — the same technique
proven earlier this session for the tasking bugs. Confirmed via serial
log: all 3 tiers report `tier %d baked ok`, no monospace-mismatch
warning (Cousine's advance widths are genuinely uniform), boot completes
through `Tasking Enabled! Tasks spawned.` with no fault-handler output.

**New technique this session, specifically for this visual feature**:
QEMU's monitor `screendump <path>.ppm` command captures the guest's
actual framebuffer even under `-display none` (the display backend still
tracks VRAM state regardless of whether a window is shown) — confirmed
working on the first try. Converted PPM→PNG via macOS
`sips -s format png`, then actually looked at the result: real
anti-aliased TrueType glyphs, smooth curves on letterforms like
`a`/`f`/`o`/`m`, confirmed at both the scale-2 body-text tier and the
larger title tier. Subsequently confirmed again by the user's own boot,
on their own screen — matching. This is a materially better
verification method than anything available earlier in this project's
history for *visual* changes, and worth reusing for any future UI work
in this sandbox instead of relying only on the user's own screenshots
after the fact.

### Known limitations / not attempted here

- **ASCII only** (`TTF_FIRST_CHAR=32` through `126`, 95 codepoints) — no
  Unicode, no extended Latin, matching `Font[]`'s own prior 0x00-0x7F
  range. Extending this means widening `TTF_NUM_CHARS` and probably
  moving off a flat `stbtt_packedchar[95]` array to something sparser if
  the range grows much beyond ASCII.
- **Fixed monospace-grid layout, not real proportional text** — see "the
  font asset" above. `ttf_font_char_advance()` does return the font's
  real, narrower-than-`8*scale` advance width (verified uniform across
  all 95 glyphs at bake time), and every draw-string call site now
  advances by that instead of the bitmap-grid cell size -- but it's
  still one uniform value per tier, not a true per-glyph proportional
  advance read from each character's own `xadvance` individually.
- **No kerning, no ligatures, no hinting beyond whatever
  `stbtt_PackFontRange`'s default rasterization applies** — this is the
  "bake a plain atlas" API, not `stbtt_PackFontRangesGatherRects`-style
  advanced layout.
- **Regular weight only** — no bold/italic variants embedded or wired
  up; every call site's `color` parameter is the only per-call styling
  knob that exists today.
- **Only 3 fixed pixel sizes** (16/24/32px) — adding a new scale factor
  anywhere in the codebase means adding a 4th tier to `ttf_font.cpp`'s
  `kCellSizes[]`/`tier_for_scale()`, not something that falls out
  automatically.

---

## `mods/dev/vbe/vbe.cpp` — bulk framebuffer operations

`fill()`, `draw_char()`, and the new `scroll_framebuffer_up()` write
directly through a plain (non-`volatile`) `uint32_t*` onto the linear
framebuffer instead of going through `draw_pixel()` per pixel.
`draw_pixel()` itself stays `volatile` (it's the single-pixel API other
callers still use), but that qualifier was forcing the compiler to
re-issue one uncombined store per call even when the caller was writing
whole rows/blocks/screens -- `fill()` alone is 786,432 calls per full
clear, `draw_char()` up to `scale*scale` per set bit. The BGA linear
framebuffer at `VBE_DISPI_LFB_PHYSICAL_ADDRESS` is plain mapped memory
(`map_physical_memory(..., PAGE_PRESENT | PAGE_WRITABLE)`), not
side-effecting MMIO, so batching these writes is safe.

`scroll_framebuffer_up()` reuses `mods/std/string.cpp`'s existing
`memmove()` to shift the rows above the scrolled-off region up in one
call, rather than the row-by-row `get_pixel()`/`draw_pixel()` loop
`Terminal::scroll()` used before. `Terminal::scroll()` in
`mods/dev/console/console.cpp` now calls this instead of walking pixels
itself.

Verified via the real panic-screen path (`FaultHandler::draw_panic_screen()`
in `mods/dev/idt/isr.cpp`, the only call site exercising `fill()`+`draw_char()`
outside of normal boot/terminal rendering): forcing a page fault against a
deliberately unmapped address and confirming the rendered panic screen's
text and background are pixel-correct.

---

## `mods/std/string.cpp` — `memcpy()` (2026-07-24)

Was a plain byte-at-a-time `for` loop (already flagged as a known hot
path -- see `docs/TODO.md`'s "Boot/memory performance" audit -- but not
fixed until this forced the issue). Landing hardware double buffering
(below) meant every present, including a plain cursor move, now copies a
full ~3MB frame through this function; at PS/2's packet rate that's tens
of megabytes a second through the slowest possible copy shape, and the
user confirmed on real hardware it made cursor movement "really freaking
slow."

Fixed with `rep movsl` inline assembly: `size / 4` words copied via the
x86 string-copy instruction (no compiled loop-branch per iteration, unlike
the byte loop it replaces), then any 0-3 trailing bytes finished with the
original byte-at-a-time copy. `cld` first, unconditionally -- normal ABI
code can assume `DF` is always clear on entry, but nothing in this
freestanding kernel enforces that guarantee, so it's not safe to assume
here. No SIMD available (`-mno-sse -mno-sse2 -mno-mmx`, see the kernel's
`Makefile`), so a wider software copy would need hand-rolled 64-bit
tricks for a further, much smaller win -- not worth the complexity over
this.

`memset`/`strcmp` in the same file are still byte-at-a-time -- separate,
not fixed here.

---

## `mods/dev/vbe/vbe.cpp` — hardware double buffering (2026-07-24)

Cursor duplication (fixed 2026-07-23) and cursor flicker (fixed 2026-07-24,
see "mods/core/wingman/cursor.cpp" above) were both software-level symptoms
of the same underlying hardware gap: every screen update used to write
directly into the live, currently-displayed framebuffer at `0xE0000000`,
with no double-buffering or vsync at the hardware level. Fixing this for
real needed the BGA/VBE `dispi` interface's page-flipping registers
(`VBE_DISPI_INDEX_VIRT_HEIGHT`/`_X_OFFSET`/`_Y_OFFSET`), already defined in
`vbe.h` but never written anywhere before this.

**VRAM layout**: `init()` now maps `VBE_FRAMEBUFFER_COUNT` (2) frames'
worth of VRAM instead of 1 (`SCREEN_X * SCREEN_Y * (SCREEN_BPP/8) * 2` =
6MiB, comfortably inside QEMU's default 16MiB PCI VGA framebuffer).
`currentBackRegion` tracks which half (0 or 1) is currently *not*
displayed; `vbe_get_back_buffer()` returns a pointer into that half, and
`vbe_flip()` writes `Y_OFFSET` to switch which half the display hardware
scans out, then flips `currentBackRegion` to the other half.

**QEMU-specific register behavior**, confirmed against upstream
`hw/display/vga.c`, not guessed:
- `VIRT_HEIGHT` is effectively inert on QEMU -- `vbe_fixup_regs()`
  recomputes it from `VIRT_WIDTH` and real VRAM size on every register
  write. Still written for spec correctness/portability.
- `BgaSetVideoMode()`'s own `ENABLE` write resets
  `VIRT_WIDTH`/`X_OFFSET`/`Y_OFFSET` to 0, so any offset writes meant to
  persist must happen *after* it returns -- this is why `init()`'s offset
  writes come after `BgaSetVideoMode()`, not before or interleaved.
- `X_OFFSET`/`Y_OFFSET` are in pixel units, not bytes.
- An out-of-bounds `Y_OFFSET` write is **silently clamped back to 0** by
  the device rather than faulted -- there's no other way to know the
  VRAM-size assumption actually holds, so `init()` uses exactly this as a
  cheap, permanent, serial-loggable boot-time self-test (write `SCREEN_Y`,
  read it back, restore 0): `"VBE: double-buffer VRAM check OK"` vs.
  `"...FAILED -- Y_OFFSET clamped, vbe_flip() will be a no-op"`.

**Confirmed design tradeoff**: real page-flipping means every present --
including a plain cursor move -- now copies the *complete current frame*
into the back buffer before flipping, not a small dirty-rect patch. A
buffer that's a stale partial patch from an earlier frame can't be
flipped into view safely (it would be two frames stale in the untouched
regions, not one). This is a deliberate reversal of the dirty-rect
optimization work described in "Rect / dirty-rect compositing" below --
`redraw_screen_rect()` is now a thin wrapper that ignores its `Rect`
argument and always calls the full-frame `redraw_screen()`. A ~3MB copy
is still cheap in absolute terms next to eliminating tearing/flicker at
the hardware level rather than just narrowing the software-level window.

`fill()`/`draw_char()`/`scroll_framebuffer_up()`/`draw_pixel()`/
`get_pixel()` needed no changes -- they keep targeting `linearFramebuffer`
directly (region 0), which is safe because the boot-time text console (the
only thing that uses them pre-Wingman) provably stops writing before
Wingman starts, and `Y_OFFSET` stays 0 until Wingman's first flip.

**Panic screen**: `draw_panic_screen()` (`mods/dev/idt/isr.cpp`) can fire
while region 1 is displayed. Since the system halts right after drawing,
its first line is now `BgaWriteRegister(VBE_DISPI_INDEX_Y_OFFSET, 0)` --
force region 0 back on screen, permanently, so the panic is actually
visible instead of drawn into the VRAM half nobody's looking at.
Boot-tested by triggering a division-by-zero fault (temporary
`int $0x00` in `p-kernel.cpp`, removed after) immediately after Wingman's
first flip -- confirmed the panic screen renders fully, not blank.

**Verification, in decreasing rigor**: (1) the `Y_OFFSET` readback in the
boot-time self-test is a direct, first-party confirmation the physical
device accepted the offset change -- not just that software thinks it
did; (2) a temporary alternation self-test (two `vbe_flip()` calls,
`BgaReadRegister()` after each) confirmed `Y_OFFSET` genuinely alternates
`0 -> 768 -> 0` across consecutive flips; (3) what headless verification
*cannot* prove: genuine tear-elimination is a real-time property of the
actual display's scanout relative to VRAM writes, which neither a static
`screendump` nor a serial log can observe, in QEMU or otherwise -- (1)
and (2) prove the double-buffer/flip machinery is wired correctly and the
hardware is really alternating, the necessary precondition, but the
user's own eyes on real display output is what finally confirms
tearing/flicker is gone.

---

## `mods/dev/gdt/gdt.cpp` — ring-transition GDT/TSS (Phase 0)

First step of a much larger, explicitly agreed direction: a three-tier
privilege model (Level 0 = kernel+drivers at ring 0, Level 1 = "important
software" like `wingman`/`fontman`/`mp3`/`chorus`/`Explorer` at ring 1 —
can't touch hardware directly but fully trusts Level 0's memory, an
"honor system" boundary — Level 2 = generic programs at ring 3, real
paging-enforced isolation, reaching Level 0 only through a small curated
syscall gate and reaching Level 1 only through real x86 call gates for
anything hardware-adjacent). Phase 0's own scope is narrower: prove the
ring-transition mechanism itself works, end-to-end, with one throwaway
pilot ring-3 task, and zero behavior change to anything that existed
before it. No syscall DPL changes, no moving any real subsystem to ring
1, no call gates, no ELF loader changes — all later phases.

The codebase had zero privilege infrastructure before this: a flat
3-descriptor GDT (null/ring0-code/ring0-data, all DPL 0) built once in
`src/boot/include/gdt.asm` and loaded via `lgdt` in `src/boot/stage1.asm`
— 16-bit real-mode bootloader code, a separate build unit from the C++
kernel, never touched again after boot. No TSS anywhere. Every IDT gate
(32 exceptions, 16 IRQs, the existing `int 0x80` syscall gate) DPL=0.
Every task's `iret` frame in `tasking.cpp`'s `task_create()` hardcoded
`CS=0x08`. Every page table entry Supervisor-only (`PT_USER`/`PD_USER`
were defined in `paging.h` but never actually used anywhere).

### A new GDT, built in C++, superseding the bootloader's

The bootloader's GDT can't host a TSS descriptor — a TSS descriptor's
`base` field needs `&g_tss`, a C++ address that doesn't exist yet at
16-bit boot-sector assembly time. Rather than trying to runtime-patch the
bootloader's GDT bytes at a fragile hardcoded offset, `mods/dev/gdt/`
builds a **second GDT inside the kernel**, loaded once via a fresh `lgdt`
at the very top of `kernel_main()` — before `PagingInstall()`/
`IDTInstall()`/`sti`. `gdt.asm`/`stage1.asm` are untouched; the new table
reproduces the ring-0 code/data descriptors byte-for-byte identically
(same access/granularity bytes as `gdt.asm`'s `0x08`/`0x10`), so reloading
CS/DS/ES/FS/GS/SS to numerically identical selector values in
`GDTLoad()`'s far-jump+segment-reload sequence is a true no-op.

8 descriptors (`GDTEntry`/`GDTPointer`, packed structs mirroring
`idt.cpp`'s `IDTEntry`/`IDTPointer` style): null (`0x00`), ring0 code
(`0x08`)/data (`0x10`, both DPL=0, byte-identical to the bootloader's),
ring1 code (`0x18`)/data (`0x20`, DPL=1 — added now even though nothing
uses ring 1 yet, to avoid re-touching the GDT in a later phase), ring3
code (`0x28`)/data (`0x30`, DPL=3), and a TSS descriptor (`0x38`). Ring-3
selectors with RPL bits set for actual use: CS=`0x2B`, data=`0x33`.

### TSS

`struct TSS` (packed, `sizeof == 104`) lives in the same `gdt.cpp` module
— tightly coupled to the descriptor's `base` field, not worth a separate
directory for ~15 lines. Only `esp0`/`ss0` are meaningfully used: this
project only ever does software task-switching via `iret` (never hardware
far-jmp switching), so `eip`/GP-register/segment fields in the TSS stay
zeroed. `iomap_base = sizeof(TSS)` means no I/O permission bitmap exists
at all, so any `IN`/`OUT` above IOPL (0 today) simply `#GP`s — exactly the
restriction Level 1/Level 2 need in later phases.

`TSSInstall()` zeroes the struct, sets `ss0 = 0x10`, patches the GDT's TSS
descriptor with the real `&g_tss`/`sizeof(TSS)-1`, and issues
`ltr 0x38`. `GDTSetKernelStack(uint32_t esp0)` is the only other public
entry point — a narrow setter (not exposing `g_tss` itself) that
`tasking.cpp`'s `scheduler_on_tick()` calls on every context switch:
`g_tss.esp0 = g_current->kernel_stack_top;`, unconditionally, right after
picking the next task. This is a functional no-op for ring-0 tasks (ESP0
is only ever consulted by the CPU on a CPL-crossing trap) but has to be
set *before* any interrupt can catch a ring>0 task running — placing the
write during the very tick that's switching *to* that task satisfies that
ordering for free.

### Ring-aware `task_create()`

`task_t` gained two fields: `ring` (target CPL, 0 for every task except
the pilot) and `kernel_stack_top` (written into `TSS.ESP0` whenever this
task is current). `task_create()` gained two trailing default arguments
(`ring = 0`, `user_stack_top = NULL`) — the same pattern as the existing
`enqueue = true`, so every pre-existing call site compiles and behaves
identically with zero edits.

The only behavioral branch is in the `iret` frame construction: a ring-3
target pushes the 5-value form (`SS=0x33`, `ESP=user_stack_top`,
`EFLAGS=0x202`, `CS=0x2B`, `EIP`) instead of the existing 3-value form
(`EFLAGS`, `CS=0x08`, `EIP`), and uses `0x33` instead of `0x10` for the
DS/ES/FS/GS slots further down the frame. **No change to
`irq_common_stub`** (`idt.asm`) was needed at all — the plain `iret`
instruction already inspects the popped CS's RPL against the CPU's
current CPL and automatically pops the extra ESP/SS pair only when a
privilege change is detected; the CPU is what's ring-aware here, not the
stub. Loading `0x33` into DS/ES/FS/GS from CPL0 code (which happens just
before `iret` executes) is legal too — the rule for data-segment loads is
CPL≤DPL, not CPL==DPL — so by the time `iret` retires, segment state
already matches the ring about to be entered.

### `mark_region_user()` and the PDE gotcha

`map_physical_memory()`/`get_or_create_page_table()` only OR the caller's
flags into a page directory entry when that PDE doesn't exist yet.
Virtually every physical address in normal use already has its PDE
created at boot by `PagingInstall()` with `PD_PRESENT | PD_READWRITE` —
no `PD_USER`. x86 requires the U/S bit set at *both* PDE and PTE for a
CPL3 access to succeed, so marking just the leaf PTE via
`map_physical_memory()` alone silently does nothing for ring-3 access —
the already-present PDE still blocks it. `mark_region_user(addr, size,
writable)` (`paging.cpp`) does both: calls `map_physical_memory()` for the
PTEs, then walks the same range OR'ing `PAGE_USER` directly into each
covered `page_directory[]` entry as a fix-up. Deliberately local — not a
general repair of `get_or_create_page_table()`'s existing contract.

### Two real bugs found while building the pilot task

1. **`pilot_counter` needs `PAGE_USER` too.** The original plan assumed a
   ring-0-only counter, incremented by ring-3 code, could stay
   Supervisor-only — but that's a contradiction: if ring-3 code writes to
   it directly (no syscall gate exists yet to do it indirectly), its page
   *has* to be User-accessible, full stop. Boot-testing this produced a
   real page fault (`err_code=7`: present, write, user) — a good
   hardware-verified proof the CPL3/paging protection was working
   correctly, and a genuine correction to the original design, not
   something to route around.
2. **`pilot_user_stack` wasn't page-aligned.** Declared with
   `__attribute__((aligned(16)))` (fine for a plain kernel stack, which
   never needs page-level U/S granularity), but `mark_region_user()` was
   called with its raw, non-page-aligned address as the base — since
   `map_physical_memory()`'s page-stepping loop assumes a page-aligned
   starting address, this silently marked the *wrong* physical pages User,
   not the ones actually backing the buffer. Symptom: an identical page
   fault at an address exactly one page past `pilot_counter`'s real
   location — the pilot's very first stack push, 4 bytes below the top of
   `pilot_user_stack`. Fixed by rounding the base down (`& ~0xFFF`) and
   padding the size by one extra page, the same defensive pattern already
   used for the pilot's code region.

### Verification

Boot-tested in 5 incremental steps (new GDT alone → `+ltr` → ring-aware
`task_create()`/`scheduler_on_tick()` with `ring` defaulting to 0
everywhere → `mark_region_user()` as dead code → the full pilot wired in),
each isolating exactly one new failure mode. Final state confirmed via a
temporary periodic serial print (since removed): `pilot_counter` rose
steadily across repeated prints while the rest of the system — desktop
compositor, mouse click/focus routing, hover cursor — stayed fully
responsive, proving the ring-3 task is genuinely executing and surviving
real timer-driven preemption/resumption without disturbing anything else.

### The pilot task itself was later removed (2026-07-23)

`pilot_ring3_fn` (`for(;;) pilot_counter++;`) was spawned with
`enqueue=true`, making it a full round-robin participant -- and since it
never blocks, it's permanently `TASK_READY`, the exact same anti-pattern
already fixed once this session for the idle task (`docs/TODO.md`, "Idle
task (`task0`) taken out of round-robin rotation"). With the input-worker
task also runnable during any mouse activity, round-robin gave the pilot
and the input-worker equal footing -- every other tick that could have
gone to draining/processing mouse events went to the pilot's pointless
counter increment instead, measurably slowing down cursor tracking
(user-reported, 2026-07-23). Its actual job -- proving the ring-transition
mechanism -- was already done and fully captured above; it didn't need to
keep running forever to stay true. Removed entirely (the function,
`pilot_counter`, `pilot_user_stack`, the `mark_region_user()` calls, the
`task_create()` spawn) rather than just disabled, matching how every
other phase's temporary verification wiring in this project gets fully
reverted once its proof is captured in documentation. The underlying
mechanism -- GDT/TSS, ring-aware `task_create()`, `mark_region_user()`,
`is_user_accessible()` -- is untouched and still fully available for
Phase 2+.

---

## `mods/dev/syscall/syscall.cpp` — ring-3 syscall gate (Phase 1)

Phase 1 of the ring-based privilege work: raise the existing `int 0x80`
syscall gate from DPL=0 to DPL=3, so a ring-3 (Level 2) task can actually
reach the small "safe list" of direct Level 0 syscalls (file ops, task
control) agreed in the project's design discussion. The mechanical flip
itself — `mods/dev/idt/irq.cpp:53`, flags `0x8E` → `0xEE` (same present
bit and 32-bit interrupt gate type, DPL raised 00→11), every other
`IDTSetGate()` call staying `0x8E` so hardware IRQs/CPU exceptions remain
unreachable by a direct `int` from ring 3 — needed no companion change to
`syscall.asm`'s `syscall_handler` stub. Its `iretd` already auto-handles
the extra ESP/SS pop whenever the popped CS's RPL differs from the
current CPL, the same CPU behavior Phase 0 already verified for
`irq_common_stub`.

**That flip alone would have been a real vulnerability, not a feature.**
`sys_read`/`sys_write` did zero validation of their `buf`/`size`
arguments, and `sys_open` trusted `path` as an unbounded C string —
harmless while every caller was trusted ring-0 kernel code, but the
syscall handler itself always runs at ring 0 once inside the trap, and
CPL0 code is exempt from the CPU's own U/S page-permission check
entirely. Raising the DPL with no other change would have handed any
ring-3 caller a real arbitrary-kernel-memory-read primitive (`sys_write`,
pointed at any address, echoes it out over serial or a file) and an
arbitrary-kernel-memory-write primitive (`sys_read`, pointed at any
address, writes attacker-controlled data there) — defeating the entire
point of Phase 0's isolation work. Hardening these three syscalls was
therefore real Phase 1 work, not a follow-on.

### `is_user_accessible()` — replicating the U/S check in software

Since CPL0 code doesn't get the hardware U/S check for free, a syscall
handler validating a ring-3 caller's pointer has to do it itself. New
function in `mods/dev/paging/paging.cpp`/`paging.h`, reusing the exact
PDE/PTE indexing `mark_region_user()` (Phase 0) already established:

```cpp
bool is_user_accessible(uintptr_t addr, size_t size) {
    if (size == 0) return true;
    uintptr_t end = addr + size;
    if (end < addr) return false; // overflow wraparound
    uintptr_t page = addr & ~0xFFF;
    while (page < end) {
        size_t pd_index = (page >> 22) & 0x3FF;
        if (!(page_directory[pd_index] & PAGE_PRESENT)) return false;
        if (!(page_directory[pd_index] & PAGE_USER)) return false;
        uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
        size_t pt_index = (page >> 12) & 0x3FF;
        if (!(page_table[pt_index] & PAGE_PRESENT)) return false;
        if (!(page_table[pt_index] & PAGE_USER)) return false;
        page += PAGE_SIZE;
    }
    return true;
}
```

Checks both PDE and PTE for `PAGE_USER` — the same PDE-vs-PTE asymmetry
`mark_region_user()` already had to fix up (a present PDE without
`PAGE_USER` silently blocks ring-3 access even with a correctly-marked
leaf PTE), checked deliberately here since there's no hardware fault to
lean on at CPL0.

### Hardening, gated on caller ring

`syscall.cpp` already `#include`s `tasking.h` and already dereferences
`g_current` (in `stdin_read_line()`) — `g_current->ring` (added in Phase
0) was available with zero new plumbing. All new validation gates on
`g_current->ring == 3` specifically, so ring-0/ring-1 callers (everything
that exists today) see zero behavior change — the same guiding principle
every prior phase has held to:

```cpp
static bool syscall_buf_ok(uint32_t buf, uint32_t size) {
    if (g_current->ring != 3) return true;
    return is_user_accessible((uintptr_t)buf, (size_t)size);
}
```

`sys_read`/`sys_write` call this as their first line, before `buf` is
touched at all. `sys_open` has no `size` argument for its C-string `path`,
so it's capped instead: `SYS_OPEN_MAX_PATH = 256`, validated via
`is_user_accessible()` for a ring-3 caller, then confirmed to actually
contain a NUL terminator within that capped range before `fopen()` is
called — rejecting (returning `(uint32_t)-1`) if either check fails.

### Verification: extended the Phase 0 pilot, not just code review

The pilot ring-3 task (Phase 0) temporarily gained two `int $0x80`
attempts, built as raw inline assembly directly inside `pilot_ring3_fn`
itself rather than via `port.cpp`'s `syscall()` wrapper — at `-O0` an
"inline" function isn't guaranteed to actually be inlined, and a real
`call` out to an un-inlined copy would fault for the wrong reason (its
code isn't in a User-executable page). The message buffer itself was
built with plain byte assignment, not `sprintf`/`memcpy`, for the same
reason: nothing but the pilot's own code is marked User-executable.

1. `sys_write` with a buffer inside the pilot's own `mark_region_user()`-
   marked stack region — succeeded; `[pilot] ok` appeared on the serial
   log exactly once.
2. `sys_write` with a buffer pointing at `page_directory` itself
   (deliberately never marked User) — rejected: no second message, no
   leaked kernel bytes, no crash. Screenshot-verified afterward that the
   desktop compositor/mouse routing were still fully alive, not just
   "didn't immediately fault."

Both syscall attempts were temporary diagnostic wiring for this
verification and were fully removed afterward, matching the established
pattern from every prior phase — `pilot_ring3_fn` is back to its Phase 0
form (a bare counter loop). Only the real, permanent Phase 1 work stays:
the DPL flip, `is_user_accessible()`, and the `syscall_buf_ok()` gate.

### Explicitly out of scope, confirmed via direct research (not assumption)

- **`elf_exports[]`** (`mods/dev/elf/elf.cpp`) — a completely separate,
  parallel bypass that hands ELF-loaded code 35 raw kernel function
  pointers (`malloc`, `fopen`, all of `string.h`, plus the 5 `sys_*`
  functions), resolved directly into `call` instructions at relocation
  time — no trap involved at all. Confirmed orthogonal to this phase:
  `elf_spawn()` still creates every ELF task at `ring = 0` (the
  `task_create()` default), so raising the syscall gate's DPL has zero
  interaction with it. Will need closing in the later "Level 2 program
  internals" phase.
- **The syscall fd table** (`syscall_fd_table[]`) — a single flat global
  array, no per-task ownership. Any task can already `sys_close()`/read/
  write any other task's fd by guessing its number. Real gap, but
  pre-existing (not introduced by ring 3 — even today's ring-0-only tasks
  share it), so not fixed here.
