# Fix step 20, the code both sides write and the runtime prefix

The part of fix step 20 that crosses areas: the rule 26 row "Code the
runtime and the host both write" and rule 25 as Eddie settled it. The
minor findings carry no IDs, so each is named by its rule and the place
the summary gives it. Logs are under `build/drive/logs/`, named `xrt-*`.

## Findings

- Rule 25, `src/rt/utf.h:11` (cross-cutting) and `src/rt/signal.h:8`
  (rt). Fixed. The 23 exported names of `anti_cpu_`, `anti_elf_`,
  `anti_macho_`, `anti_coff_`, `anti_utf8_`, `anti_utf16_` and
  `anti_split_command_line` are `anti_rt_`, with every caller in antic,
  anti and the tests. The inline f16 conversions follow, and the signal
  macros are `ANTI_SIGINT` and `ANTI_SIGBREAK`. The new test `rt_names`
  reads all 18 runtime libraries of the archive and failed on the 23
  names before the rename (`xrt-red-names2.log`).
- Rule 26, `src/antic/sha256.c:1`, SHA-256. Fixed. The hash stands in
  `src/rt/digest.c` alone, and `antic_core` compiles it.
  `anti_rt_sha256_stream` digests a stream its caller opened.
- Rule 26, UTF-8. Fixed. The lexer and `json.c` call
  `anti_rt_utf8_encode` and `anti_rt_utf8_decode`, and `antic_core`
  compiles `src/rt/utf.c`.
- Rule 26, the `__TEXT` walk. Fixed. `anti_rt_macho_text` of
  `symbols.c` serves `trace.c` and `syms.c`. The copy of `anti` checked
  neither the end of a command nor its size. `macho_text` of
  `test_symbols.c` holds the malformed headers and failed to build before
  the function existed (`xrt-red-macho.log`).
- Rule 26, the names of the levels. Fixed. `anti_rt_cpu_level_name` is
  an inline function of `cpu_level.h`, and `test_cpu.c` compares all six.
- Rule 26, `file_bytes` and `same_bytes` of `rt`. Fixed.
  `anti_rt_fs_read` of `fs.c` replaces `file_bytes`, `index_bytes` and
  the loop of `load` in `trace.c`. `test_fs` failed to build before it
  (`xrt-red-fs.log`). `anti_rt_same_bytes` of `text.c` replaces both
  copies.
- `anti_licenses`. Left, and not a defect of rule 25. The program
  defines it, and the specifications name it.

Left outside this step, though the front-end and back-end reports of
step 20 sent them here:

- Rule 25 for the host, `lexer.h` and `optimize.h`. The task of this
  session names the runtime prefix alone.
- The out-of-memory exits, `allocate` of `layout.c` and `write_file` of
  `driver.c`. They are the rule 26 row "The out-of-memory block and
  `allocate` copies", code of the host alone and not of this row.

## Gates

- Zero warnings in the host, ASan and UBSan builds. The Mac linker prints
  `ignoring -lto_library` for the pinned clang, as before.
- Host: 841 of 841 passed, `xrt-ctest-host.log`.
- ASan: 840 of 840 passed, `xrt-ctest-asan.log`.
- UBSan: 840 of 840 passed, `xrt-ctest-ubsan.log`.
- Docs style: nothing on every touched C file, header, `.cmake` file and
  document. It reads `tests/CMakeLists.txt` as Markdown and reports each
  CMake comment as a heading, 223 errors at the base commit and 224 now
  with the one comment of `rt_names`.
- Each runtime change moved `tests/link-identity/return42.macos-arm64.sha256`,
  which holds the digest of the last one.

## Decisions

Four `[provisional]` entries under "Names and shared code of the
runtime" in `docs/decisions.md`: the types and macros that keep `anti_`
and `ANTI_`, `anti_licenses`, the files of `src/rt/` that the host
compiles, and `anti_rt_fs_read` with its `ENOMEM`.

## Questions

- Should the host prefixes of rule 25 and the host out-of-memory block
  be a session of their own? Both are listed as left above.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
42a92f8 Record the names and the shared code of the runtime
a809d71 Read a whole file and compare bytes once in the runtime (rule 26)
df93f23 Spell the names of the processor levels once (rule 26)
$ git status --short
$ git rev-parse HEAD origin/main
42a92f85210d5354f2e4a96569dee85548594e64
42a92f85210d5354f2e4a96569dee85548594e64
```
