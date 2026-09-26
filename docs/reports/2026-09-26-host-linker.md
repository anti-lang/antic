# The Mac's host linker, and the answers on warnings

The step builds Eddie's three answers to the questions of
`docs/reports/2026-09-26-warnings.md`.

## What was done

- `ba2d68d`: the Mac links antic, anti and every test program with the
  pinned ld64.lld. `tools/pinned-compiler.cmake` names the option once as
  `ANTIC_HOST_LINK_OPTIONS` and puts it into the linker flags of the cache.
  The build takes the SDK of `tools/macos-sdk-pin` as `CMAKE_OSX_SYSROOT`,
  because ld64.lld 23.1.1 reads the `libSystem.tbd` of the Xcode SDK on this
  Mac as malformed. That SDK choice was not in the answer. The packer makes the
  same pairing, and the linker cannot run without it. The test scripts that
  link a C program on this host take the option on their link lines as
  `HOST_LINK`. clang warns about it on a compile-only call.
- `clib_bundle` failed under ld64.lld. The linker names a duplicate Mach-O
  symbol without the leading `_` that llvm-objdump prints, and the test now
  reads both.
- `259af73`: `tests/bind/layout.h` is included with `-I`. A pragma push and
  pop for clang and for gcc silences the warning of C11 at the one wide
  enumerator. A comment gives the reason.
- The four entries under "Warnings" in `docs/decisions.md` carry no tag, and
  the entries on `layout.h` and on the host linker stand beside them.
  `7ea9328`, a commit of the session `antic-01` that added the whole of
  `docs/decisions.md`, took them in before this step committed its code.
  Eddie chose to leave that commit as it is.

## Gates

- The three trees built from scratch at `ba2d68d` with no linker warning in
  any configure or build log. Our code printed no compiler warning. raylib
  prints its 67 on the two Windows targets under its own flags.
- The host suite passed 1146 of 1146, and the ASan and UBSan suites 1145 of
  1145 each.
- The test scripts show the output of a link only on failure. The 30 tests
  that take `HOST_LINK` passed again with `-Wl,-fatal_warnings` added to it,
  a flag that fails a link of ld64.lld on any warning. None was skipped.
- A clone under `build/scratch/` ran the host suite at `259af73`:
  1146 of 1146, with the pinned clang first on `PATH`.

## Questions for Eddie

None.
