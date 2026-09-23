# Fix step 4: the record checks of the library reader

Step 4 of "Fix steps" in `docs/audit/summary.md`. Every change is in `src/antic/antl.c`. The tests are in `tests/unit/test_modules.c`: `damaged_records` writes a library from source, reads it back, and refuses it after each of 12 single-record changes. `damaged_files` refuses a body without a block. All 13 were watched failing before the fix, in `build/drive/logs/s4-unit-red.log`, and passing after, in `build/drive/logs/s4-unit-green.log`.

## Findings

| ID | Result |
|---|---|
| S12 | Fixed. A member of a class body is refused unless its type is an unbound function whose parameter count is the names the declaration wrote, plus `self`, plus the out pointer of `may fail`. |
| S15 | Fixed. The target of `jump`, and both targets of `branch` and `branchov`, are refused unless they are blocks. |
| S16 | Fixed. A body with no block is refused. |
| S1, `read_tables` | Fixed. A field of a simd aggregate of the IR that is a bitfield or the unit break `_` is refused. The `check_simd_struct` half belongs to step 2. |
| M3 | Fixed, in both places. A bitfield of the type table needs a fixed-width integer, at most its bits, and a name other than `_`. A bitfield of the IR needs `i8` to `i64`, at most its bits. |
| M4 | Fixed. The alignment of an IR aggregate is a power of two or 0. |
| M5 | Fixed. A text constant has the type `str` or `[]u8`. |
| M6 | Fixed. The base of an enum is an integer type. |
| M31, library reader | The malformed-input tests above cover the record checks. The depth and oversized-chain inputs belong to step 5. |

S12 as the audit worded its fix would have refused a correct file. The `get` of a singleton is a carried member that takes no `self`, and the reader set `has_self` on every member. The reader now derives the mark from the type. It is the first `[provisional]` entry of `docs/decisions-library.md`.

## Provisional decisions

Three, all in `docs/decisions-library.md`: the `self` mark of a library member comes from its type, a text constant is `str` or `[]u8`, and every field of a simd aggregate of the IR is a lane.

## Gates

- Build: zero compiler warnings on host, asan and ubsan, in `build/drive/logs/s4-final-*-build.log`. Each link prints `ld: warning: ignoring -lto_library .../build/deps/clang/lib/libLTO.dylib, file does not exist`. The base build prints the same line, in `build/drive/logs/s4-build0.log`.
- Suites: 789 of 793 on host, and 788 of 792 each on asan and ubsan, in `build/drive/logs/s4-final-*-ctest.log`. Every other test passes.
- The four failures are `anti_bind_clang`, `anti_bind_raymath`, `anti_bind_raylib` and `anti_bind_refusals`. In each, `anti bind` refuses clang 21 from `PATH`, in `build/drive/logs/s4-*-bind.log`. `src/anti/bindclang.c` looks for the pinned clang in `deps/clang` beside the build tree of `anti`. This worktree takes the pinned downloads from `ANTI_DEPS_DIR`, as the driver said, so `build/deps` does not exist here and the lookup falls back to `PATH`. This step does not touch that code, and the fix is outside its files.
- The docs-style checker reports nothing on the changed files.

## Question

Should `anti bind` take the pinned clang from `ANTI_DEPS_DIR` when it is set, as the configure step does? That change is in `src/anti/bindclang.c`, outside this step.
