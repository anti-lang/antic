# Appends of text.Builder in place

## What changed

`tests/bench/builder.anti` ran at 5.4 times its C twin after the work order
of `docs/work-order-llvm-optimization.md`. Its optimized IR showed three
causes, all in `src/std/anti/text.anti`:

- `pad` read the whole buffer back, sliced it and counted its characters
  after every `append_int`, at the default width of 0 as well.
- `put_digits` appended each digit with its own call of the runtime.
- `append` and `append_byte` called `anti_rt_builder_append`, which LLVM
  cannot inline. The C twin inlines its `append` to a bounds check and a
  store.

The three changes:

- `pad` returns at once when the width is 0 or less.
- `put_digits` appends the run of digits with one call.
- The new private `put` appends in place while the room holds the bytes and
  their NUL, and calls the runtime to grow the room. `append`, `put_digits`
  and `append_byte` use the same path.

Eddie decided the third on 2026-10-06. The DESIGN comment above
`anti_rt_builder_append` in `src/rt/text.c` now says that the class appends
in place under the rule of that function. `docs/decisions.md` holds the
entry beside the one on `take_in`.

## Measurement

`tests/bench/builder.anti` on macos-arm64, 15 alternating runs, medians.
Every build printed `117223321`, the line of the C twin.

| Build | Median | To C | To baseline |
|---|---:|---:|---:|
| baseline | 251.5 ms | 5.40 | 1.00 |
| with the three changes | 59.9 ms | 1.29 | 0.24 |
| C twin | 46.6 ms | 1.00 | 0.19 |

Each change alone, measured from scratch builds of `text.antl` in
`build/bench-scratch/bldr/`:

| Variant | To baseline |
|---|---:|
| `pad` returns at a width of 0 | 0.87 |
| `put_digits` with one call | 0.59 |
| both | 0.47 |
| both and `put` for `append` and `append_byte` | 0.27 |
| all three, `put_digits` through `put` | 0.24 |
| both, with `--lto full` | 0.53 |

The LTO of the runtime made `builder` slower, at 1.08 of the baseline
without the changes.

## Tests

- `std/builder_room.anti` appends bytes, texts of 1 to 12 bytes, an empty
  text and numbers across the first room and its growths. It checks the
  length and the NUL after every append.
- `std_builder_room_memory_checks` runs the same program under
  `--memory-checks`, on every host but windows-arm64.

Four broken forms of the fast path checked the tests. A missing NUL fails
under `--memory-checks` alone, since a fresh block of a plain run holds a
zero by chance. A room check of `<=` in `put` or in `append_byte` fails
both tests. An `append_byte` that keeps its length fails both.

A test that pins the speed itself would fail by chance, as the entry on
`tests/bench` in `docs/decisions.md` says. A test of the optimized text
cannot show the change either: the release pipeline keeps `append_int` a
call, so its width is no constant inside it.

`emit_identity` is re-pinned. 72 of its digests changed, those of the
twelve programs that reach `text.Builder` on the six targets.

## Gates

- The three builds: zero warnings of our own code. The `asan` and `ubsan`
  builds print 67 warnings each, all from the sources of raylib under
  `build/deps/raylib`, which build with raylib's own flags.
- Host suite: 1621 of 1621, 250 s.
- ASan suite: 1620 of 1620, 757 s.
- UBSan suite: 1620 of 1620, 358 s.
- The docs-style checker reports nothing on `docs/decisions.md` and this
  report. `anti fmt --check` passes on both changed sources.

The ASan suite ran past the 9 minutes of CLAUDE.md in the worktree, while
the step `pgo` ran its own builds and suites on the same Mac.

After `pgo` finished, the branch was rebased onto `1ecaec5e` and `main`
moved to it. The three suites ran again on that tree, on an idle Mac:

- Host suite: 1622 of 1622, 226 s.
- ASan suite: 1621 of 1621, 500 s.
- UBSan suite: 1621 of 1621, 441 s.

The ASan run of 500 s stays inside the 9 minutes, so the time of the first
run came from the shared machine.

## State

The work stood on branch `feat/builder-fast-path` in the worktree
`build/wt-builder` while the step `pgo` held the main checkout. Eddie
allowed the worktree for this task. The branch is on `main` now, and the
worktree and the branch are removed.
