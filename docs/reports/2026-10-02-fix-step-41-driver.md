# Fix step 41, the split of driver.c

The part of fix step 41 of `docs/audit/summary.md` for `src/antic/driver.c`:
the file split along its parts, so each file has one purpose. Behaviour is
unchanged. Logs are under `build/drive/logs/`, named `s41d-*`.

## Findings

- Rule 19, `src/antic/driver.c`, 3017 lines at the audit and 3079 at the
  base. Fixed. The tool-pass report names the planning of the link, from
  `xcrun` to `link_program`, as the part to move first. The structure
  report adds the search of the library files, the bundle and the plugin
  index as purposes of their own. Five files and a header:

  | File | Lines | Part |
  |---|---|---|
  | `driver.c` | 1635 | the files of a compilation, the passes, the back end, `driver_run` |
  | `driver_link.c` | 499 | the facts, inputs and command of the link of a program |
  | `driver_search.c` | 351 | the library files of the imports and their load order |
  | `driver_library.c` | 388 | a library for C or a plugin: join, bundle, archive |
  | `driver_index.c` | 227 | `anti-plugins.toml` |
  | `driver_parts.h` | 133 | four structs, two suffixes, 18 shared functions |

- Rule 18, the long functions of the driver: `build_c_library` (188
  lines), `compile` (173), `back_end` (152, 7 parameters),
  `driver_interface` (133), `driver_run` (127), `load_libraries` (108,
  nest 5), and the parameter lists of `dump_ir` (12), `lower_checked`
  (10) and `whole_checked` (9). Left, moved unchanged. The tool-pass
  report marks each as kept from the first audit and keeps its
  judgement. This session was given the file split.

## How the split was checked

`build/s41d/chunks.py` and `build/s41d/plan.py` cut the file at its
top-level definitions and list every name one part calls from another.
`build/s41d/assemble.py` and `build/s41d/files.py` write the files. A
shared function takes the prefix `driver_` under rule 25, and the rest
stay `static`. `driver_` is as long as the `static ` it replaces, so a
definition keeps its alignment. The calls the prefix pushed past 80
columns are wrapped. `build/s41d/verify.py` compares the tokens of all 85
functions with the base, ignoring the prefix, and finds no difference.
No comment of the base is lost (`s41d-verify.log`). The one DESIGN
comment that named `link_inputs_of` and `link_facts_free` now gives the
prefixed names.

The test `header_sections` now checks `driver_parts.h`. It fails with a
declaration moved under the wrong file (`s41d-header-neg.log`). The entry
of `docs/decisions.md` on the objects of `anti test` names
`driver_search.c` for `driver_libraries`. No note named a moved function.
`git status` lists no file after all three suites.

## Gates

- Zero warnings in the host, ASan and UBSan builds (`s41d-host-build.log`,
  `s41d-asan-build.log`, `s41d-ubsan-build.log`).
- Host: 1459 of 1459 passed, `s41d-ctest-host.log`.
- ASan: 1458 of 1458 passed, `s41d-ctest-asan.log`.
- UBSan: 1458 of 1458 passed, `s41d-ctest-ubsan.log`.
- Docs style: nothing on `docs/decisions.md` and this report,
  `s41d-docs1.log`, `s41d-docs2.log` and `s41d-docs3.log`.

## Decisions

One `[provisional]` entry under "Files of the driver" in
`docs/decisions.md`. It names the five files with the part each holds,
the shared header and the prefix `driver_` of the shared functions.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
5c0350c4 Record the files of the driver
3a2c3467 Split driver.c along its parts (rule 19)
ca100731 Add the report of fix step 41 for antl.c
$ git status --short
$ git rev-parse HEAD origin/main
5c0350c44771cf340bdb996b6ec9302e4c5c0490
5c0350c44771cf340bdb996b6ec9302e4c5c0490
```
