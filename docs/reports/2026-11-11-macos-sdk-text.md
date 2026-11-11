# The text of the macOS SDK

Eddie answered the question of
`docs/reports/2026-11-11-license-plain-sources.md` on 2026-11-11: the text
`licenses/macos-sdk.txt` goes. It said that the stubs of the macOS
sysroot come from `MacOSX26.5.sdk`, and they come from Zig. This step
records the answer, takes the name out of `RUNTIME_UNSOURCED` and removes
the stale copies of the trees. The VMs were not touched.

## Commits

- `e34e93f5` records the answer under "Answers of 2026-11-11". It changes
  the entry on the record, the entry on the identifiers and
  `docs/distribution.md`, and the entries keep their tags.
- `2aab2c36` takes `macos-sdk` out of `RUNTIME_UNSOURCED` of
  `src/antic/shared.h`, names the two that remain in the entry on the
  plain form and drops the note on the SDK from the comment of
  `src/antic/notice.c`.
- This report, with the times and the counts in `CLAUDE.md`.

## The stale text

With the list changed, `runtime_sources` failed with `sources.txt has no
line for macos-sdk, and RUNTIME_UNSOURCED of src/antic/shared.h does not
list it`. That is the reason the step wanted.

The configure step copies every file of `build/deps/sysroot/licenses/`
into `licenses/` of the runtime archive with `file(COPY)`. The packer
copies the same directory into a package. The copy in `build/deps` came
from a run of 2026-09-18, and no script writes the file now. The copy in
`build/deps/sysroot/licenses/` was removed by its path, and so were the
copies in `licenses/` of the trees `host`, `asan`, `ubsan`, `coverage`
and `release`. Nothing else under `build/deps` changed. After a fresh
configure and a build of each tree the file stayed absent, and
`runtime_sources` passed.

Older copies remain in directories that no preset builds and no step of
the suite reads. Among them are `build/llvm/runtime`, `build/runtime`,
the exports under `build/dist/export` and `build/dist/dry-run/export`,
`build/dist/verify` and directories of earlier sessions under
`build/review`, `build/scratch` and `build/step18`. They were left
alone. The next release writes its export and package from the trees,
which no longer hold the file.

## The other texts

Every other text of `licenses/` in the five trees has a writer that a
fresh checkout runs. The five trees hold the same 37 files.

- `anti_rt.txt` and `sources.txt`: `configure_file` and
  `antic_write_sources` of `CMakeLists.txt`.
- `pcre2`, `sqlite`, `mbedtls`, `miniaudio`, `raylib` and `mimalloc`:
  `antic_native_license` of `src/native/`, from the pinned sources.
- `glibc`, `linux-headers`, the 22 texts of the X11 and OpenGL packages,
  `musl`, `mingw-w64`, `compiler-rt` and `zig`:
  `tools/get-sysroot.cmake`, from the pins of `tools/sysroot-pins`,
  `tools/zig-stubs-pin` and the pinned clang.
- `apsl.txt`: `tools/get-sysroot.cmake` downloads it from `APSL_URL` of
  `tools/zig-stubs-pin`, release 3.29.0 of the SPDX licence list, and
  checks `APSL_DIGEST`. The copies of `build/deps/sysroot/licenses`, of
  `.download/` and of the tree all have that digest.

No other text is on this Mac alone.

## Suites

- The host suite ran before the code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,7178`: 378 s, 263 s and 302 s. Every test
  passed apart from the two skips of `mimalloc_environment`.
- Both sanitizer suites ran after it, in the six parts of `asan` up to
  test 7176, and every test passed. `asan` took 247 s, 191 s, 261 s,
  187 s, 452 s and 453 s. `ubsan` took 254 s, 191 s, 262 s, 204 s,
  470 s and 469 s.

## Provisional lines

No new provisional line.
