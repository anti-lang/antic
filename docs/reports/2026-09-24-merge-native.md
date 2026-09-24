# Merge of lane/native

The round-four driver could not bring `lane/native` onto main. The seven
commits after `ab03345`, the base that `build/drive/add4-status/lane-a-base`
names, are now cherry-picked onto main in order. The branch and its worktree
in `build/worktrees/native` stay, to be removed by hand.

## Done

- PCRE2, raylib, miniaudio, Mbed TLS and SQLite build in `src/native/` for
  all six targets. Each of the six directories of
  `build/host/runtime/lib/` holds all five archives.
- Their pins are in `tools/`, and the X11 and OpenGL packages of the Linux
  sysroot are the `MEDIA_*` lines of `tools/sysroot-pins`.
- `docs/decisions-native.md` stays a file of its own. The next step folds it
  into `docs/decisions.md`.

## Conflicts

Only the raylib and miniaudio commit conflicted, in `tools/sysroot-pins`.
Main had added the `GLIBC_<arch>_X11`, `_X11_DEV`, `_GL` and `_GL_DEV` lines
for `link linux "X11";`, and the lane had added the `MEDIA_*` block at the
same place. Both are kept. `tools/get-sysroot.cmake` unpacks the first set,
and `src/native/get-media-sysroot.cmake` unpacks the second over the same
glibc sysroot. A rerun of `get-sysroot.cmake` deletes the sysroot and with it
the `.media-packages` stamp, so the media packages then go in again.

The other changes on main needed no edit. The lane names no runtime symbol
and no file of the runtime's platform code. It reads the downloads through
`ANTIC_DEPS_DIR`, which `tools/deps-dir.cmake` still sets whether or not
`ANTI_DEPS_DIR` is given. The top-level `CMakeLists.txt` takes one
`add_subdirectory(src/native)` after the tests, and the lane registers its
tests there, so `tests/CMakeLists.txt` did not change.

## Gates

- The host build, with every native library for all six targets: 942 of 942
  tests pass, none skipped.
- ASan and UBSan: 941 of 941 tests pass in each, without `no_paths`,
  which needs a build that no sanitizer wrote paths into.
- The docs-style checker: no findings on the lane's documents or this one.

## Questions

None.
