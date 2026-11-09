# The window of anti.graphics: T217, its size and its sizes in points

The port of the Pythagoras tree from `anti.raylib` to `anti.graphics` found
two gaps. A window with `highdpi: false` drew at twice its size on a Retina
Mac, and a program could not give the size of its window. Eddie answered
both on 2026-11-09 and asked for the sizes of the monitor and of the window
in points as well. "Answers of 2026-11-09" in `docs/decisions.md` holds the
answers. Eddie's direction of 2026-11-09 is the Mac alone, so the VMs were
not touched.

## Commits

- `4bff126e` records the answers, a line in the entry of the feature freeze
  and the design in `docs/notes/graphics.md`.
- `73c1f455` fixes T217.
- `b7d6c8a0` builds `Window { width, height }` and the four queries.
- This report, with the commit in the row of T217 and the times in
  `CLAUDE.md`.

## T217, a window without HiDPI on a Retina Mac

`RaylibDisplay.open` opens the window of raylib hidden at 640 by 360 and
then sets its size. GLFW 3.4 of raylib 6.0 reports the framebuffer of a new
window in the pixels of the Retina screen until the window has drawn. It
does so where `FLAG_WINDOW_HIGHDPI` is off and the framebuffer holds points
as well. A probe
read GLFW and raylib around `SetWindowSize(1706, 960)`:

| When | GLFW window | GLFW framebuffer | raylib screen and render |
|---|---|---|---|
| after `InitWindow` | 640 by 360 | 1280 by 720 | 640 by 360 |
| after `SetWindowSize` | 1706 by 960 | 3412 by 1920 | 3412 by 1920 |
| 30 frames later | 1706 by 960 | 1706 by 960 | 3412 by 1920 |

raylib keeps the doubled size, and every frame drew twice as large from the
bottom left corner. The same sequence in raw raylib shows it, so the
defect is in GLFW, and `anti.graphics` met it by its order of calls. A size
set after the window has drawn reaches raylib as it should.

A window without HiDPI now draws frames while it is hidden, and changes its
size away and back, until raylib holds no more than the points it asked
for, at most 30 frames. `settle` holds the loop under a `DESIGN:` comment.
`std_graphics_hidpi_off` opens a hidden window on the Mac and compares the
screen and the render size of raylib with two thirds of the monitor. It
failed before the fix and passed six runs in a row after it. The window
of the fix was looked at on screen, with a circle in the centre of the
square and a frame around `f.bounds`.

## The size of a window and the sizes in points

- `Window { width, height }` gives the size in screen points. A field of 0
  or less keeps two thirds of the monitor on its axis, and a size above the
  monitor takes the monitor's.
- `w.monitor_width()` and `w.monitor_height()` are `GetMonitorWidth` and
  `GetMonitorHeight` of the current monitor. `w.screen_width()` and
  `w.screen_height()` are `GetScreenWidth` and `GetScreenHeight`.
- raylib reads the monitors after `InitWindow` alone. The first query of the
  monitor before `start` therefore opens the window of raylib hidden, and
  `start` gives it its title, its size and its place. This keeps the query
  before `open` on raylib, without a call of GLFW.
- `Display` gains `prepare`, `ready` and the four queries. `Window.start`
  now refuses a second window, since a field of the singleton
  `RaylibDisplay` that a worker reaches is refused.
- The Recorder stands for a monitor of 1920 by 1080 points. A window over it
  opens at the size it asks for within it, or at 1280 by 720, and its
  `aspect` follows the window.

`std_graphics_window` takes every case over the Recorder, in release mode,
in dev mode and under `--memory-checks`, and holds the example that the
note adds. `std_graphics_window_points` opens a hidden window of raylib on
the Mac and checks the query before `start` and the size after it.

## Suites

- The host suite ran before each code commit, and every test passed.
  Before the commit of T217 it held 6999 tests, and its three parts took
  382 s, 270 s and 337 s. Before the commit of the size it held 7003, and
  the parts took 402 s, 275 s and 324 s.
- Both sanitizer suites ran in the six parts of `asan` after the commit of
  the size, 7001 tests each, and every test passed. `asan` took 254 s,
  189 s, 265 s, 196 s, 441 s and 456 s. `ubsan` took 260 s, 189 s, 261 s,
  206 s, 464 s and 476 s.

## Questions for Eddie

1. `std_graphics_hidpi_off` and `std_graphics_window_points` open a hidden
   window of raylib during the host suite of the Mac. Nothing shows on the
   screen. Should they stay, or should the suite open no window at all?
2. A program that queries the monitor before `start` gets a window of
   raylib with the flags its fields hold at that moment. A flag changed
   after the query does not reach raylib. Should a later change of a flag
   be refused, or is the provisional entry enough?
