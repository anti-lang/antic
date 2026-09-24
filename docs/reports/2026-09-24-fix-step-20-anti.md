# Fix step 20, the minor findings of the anti tool

Step 20 of "Fix steps" in `docs/audit/summary.md`, the session for the
area `src/anti/`. Branch `fix/mtool`. Logs are under `build/drive/logs/`,
named `s20a-*`.

## Findings

Each row of the minor table whose place lies in `src/anti/`, with the
detail from `docs/audit/anti-tool.md` and `docs/audit/cross-cutting.md`.

| Rule and place | State | Change |
|---|---|---|
| 3, `bindexpr.c:438` | fixed | `bind_signed` computes the value of 64 bits above `INT64_MAX`. The narrowing sign-extends through it, and a negative value shifts right as the complement of its complement. |
| 5, `test.c:157` | fixed | `files_array`, `files_resize` and `files_grow` check the product before `calloc` or `realloc`. The test list holds `item_count + 1` elements. |
| 9, `bindclang.c:571` | fixed | Names of anonymous records are built in a `struct text`, so two long names no longer collide. The other buffers of `anti bind` went the same way, and a cut JSON message ends in `...`. |
| 11, `zip.h:48` | fixed | The comments of `zip_read`, `json_read`, `manifest_read`, `deps_resolve`, `unit_read`, `repo_index` and `repo_module` name the free function and the state after a failure. |
| 14, `bindexpr.c:115` | fixed | Literals, array lengths, `COLOR` fields and AST constants out of range are refused. The age stamp of an index is checked too. |
| 15, `zip.c:212` | fixed by steps 14 to 16 | `test_zip.c`, `run_anti_symbols_input.cmake`, the malformed index and lock tests and the `anti_bind` malformed cases exist. |
| 16, `zip.c:121` | fixed | `zip_write` refuses a value past its field. |
| 18, `thresholds.txt` | left | The summary gives the splits of rule 18 to step 21. |
| 20, `bindtype.c:89` | fixed | `bind_known_name`, `json_string` and `repo_cache_dir` are `static`. |
| 21, `build.h:5` | fixed | `build.h` and `repo.h` no longer include `<stddef.h>`. |
| 22, `bind.c:97` | fixed | The branch went with the copy of `base_name`. |
| 24, `doc.h:18` | fixed in part | `doc_run` and the defines of `anti bind` are `const`. `bind_header` and the `inject` of `test_run` are left: they fill `options.roots` and `options.inject` of `src/antic/driver.h`, which are `const char **`, outside this step. |
| 25, `files.h:10` | fixed | The prefix `files_`. `jsontree` is not a defect: its names share `json_`. |
| 26, `test.c:29` | fixed | `anti test` reads its modules with `unit_read`, which now collects tests and fixtures, and `unit_flat_path` serves `check.c` and `test.c`. |
| 26, `syms.c:109` | fixed | One `files_base_name` and `module_path_last` for the copies. One `files_out_of_memory` for ten. The four growable lists grow through `files_grow`. |
| 26, `syms.c:135` | fixed | `syms.c` calls `path_is_absolute`. |
| 26, `build.c:636` | fixed | The dead text and three unused parameters are gone. |
| 27, `test.c:371` | fixed | The comment stands above `run_unit`. |

## Tests

- `anti_bind_clang_malformed` binds a record with two anonymous records
  whose joined names agree in their first 256 bytes. It failed before
  the fix: `s20a-red-synth.log`.
- `expressions` and `deep_input` of `test_bind.c` refuse literals and a
  length out of range: `s20a-red-numbers.log`. `anti_bind_api_malformed`
  skips a `COLOR` define past a `long long`: `s20a-red-color.log`. The
  shifts and casts of rule 3 already gave the right values on this host,
  so their checks pass before and after.
- `write_limits` of `test_zip.c` refuses a name of 70000 bytes and 65536
  entries: `s20a-red-zip.log`.
- `grow_array` and `base_names` of `test_files.c` and `json_messages` of
  `test_bind.c` cover the new helpers. `jsontree.c` joins the unit tests.

## Gates

- Zero warnings in the host, ASan and UBSan builds. The Mac linker prints
  `ignoring -lto_library` for the pinned clang, as before.
- Host suite 837 of 837: `s20a-host-suite.log`.
- ASan 836 of 836: `s20a-asan-suite.log`.
- UBSan 836 of 836: `s20a-ubsan-suite.log`.
- The docs-style checker reports nothing on every touched C file, header
  and document. It reads `tests/CMakeLists.txt` as Markdown and reports
  the same 222 errors and 27 warnings there as at the base commit.

## Decisions

Eight `[provisional]` entries under "Step 20, the anti tool" in
`docs/decisions-mtool.md`. They cover the prefix, the allocation
helpers, the separators and the host rules of absolute paths. They also
cover the refused literals and lengths, the cut message and the zip
limits.
