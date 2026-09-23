# raylib and miniaudio in src/native: blocked

The step asked for raylib and miniaudio, built in `src/native/` for all six targets and
linked for every target through a small C test. It stopped before any code, on the two
Linux targets. Nothing under `src/native/`, `tools/` or `tests/` changed.

## What blocks it

- `docs/decisions.md`, "Runtime archive", says that a program which imports `anti.raylib`
  or `anti.miniaudio` links dynamically against glibc 2.35 and not statically against
  musl. The reason given there is that X11, Wayland, OpenGL, ALSA and PulseAudio are
  loaded at run time and a static musl executable cannot use them. The same section
  says the glibc sysroots `linux-x86_64-glibc` and `linux-arm64-glibc` are left out
  "until the link mode against glibc exists".
- That link mode does not exist. `src/antic/linker.c` links every Linux program with
  ld.lld statically against musl, with `--no-dynamic-linker`. Only the platform linker of
  a Linux host links against glibc, and it uses the host's own glibc. A link test "as a
  program of that target links", which is the form the PCRE2 step used, cannot be
  written for either Linux target. Adding the mode means changing `src/antic/`, which is
  outside this step's fence.
- Nothing here holds the glibc sysroots. `build/deps/sysroot/` has only the six musl,
  macOS and Windows sysroots.
- raylib's GLFW back end on Linux includes `X11/Xlib.h`, `Xcursor`, `Xrandr`,
  `Xinerama`, `XInput2`, `XKBlib` and `shape.h` (`src/external/glfw/src/x11_platform.h`
  of the pinned 6.0). No pin names a package that supplies those headers. The three
  glibc packages per processor in `tools/sysroot-pins` are libc6, libc6-dev and
  linux-libc-dev. Choosing which X11 packages to pin, and where to pin them, is a
  design decision the documents do not make.

Building the Linux libraries against musl would go against the decision above. Building
only the macOS and Windows libraries would leave out part of the task. Neither was done.

## Questions for Eddie

1. Should the glibc link mode of antic (ld.lld, `--dynamic-linker`, the glibc sysroot,
   `-lc -lm -lpthread -ldl`) be a step of its own that comes before this one? Or does
   this step's fence widen to include `src/antic/linker.c` and `src/antic/driver.c`?
2. Where do the X11 headers for raylib on Linux come from? One option is more Ubuntu
   jammy packages in `tools/sysroot-pins`, installed into the glibc sysroots by
   `tools/get-sysroot.cmake`: libx11-dev, x11proto-dev, libxrandr-dev, libxinerama-dev,
   libxcursor-dev, libxi-dev, libxext-dev, libxrender-dev and libxfixes-dev. Another is
   a separate pin for raylib alone. Should Wayland be built as well as X11?
3. Until the first two are settled, should the step build only macOS and Windows, with
   Linux recorded as open?

## Found on the way, for the next attempt

- The macOS sysroots here have no `sdk/`. raylib's Cocoa sources need Apple's SDK to
  compile, and the Command Line Tools on this Mac hold 15.2, 26.5 and 27.0. antic's rule
  of the newest SDK up to 26 would pick 26.5.
- The Windows sysroots hold `OpenGL32.Lib` and `WinMM.Lib` for both processors.
- miniaudio has no pin yet. `tools/miniaudio-pin` would be inside the fence.

## Gates

No code changed, so the build, the suites and the specification check were not run.
The docs-style checker runs on this report.

## Provisional entries added

None.
