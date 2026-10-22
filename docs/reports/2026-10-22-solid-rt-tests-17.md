# Step rt-tests, seventeenth session

The session went on from the list of `2026-10-22-solid-rt-tests-16.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/anti/`, `src/rt/` and `src/antic/` that a program, a
command or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt17-*`. The list of the regions not reached is
`rt16-end-unreached.txt` at the start and `rt17-end-unreached.txt` at the
end. The probes and scripts are in `build/rt17/`.

## Coverage per directory

Regions reached. The start is `rt16-end-summary.txt`, measured at
`4f1b3be8`. The end is `rt17-end-summary.txt`, measured at `2deaeca7`
with `./c coverage part 1/5` to `5/5`, `rt17-end-1.log` to `-5.log`.
`b2e1fd70` came after it and reaches `bindclang.c` 556 as well.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.88% of 9252 | 95.94% of 9252 | 381, 376 | 96.13% | 96.18% |
| `src/antic/` | 98.19% of 55587 | 98.20% of 55587 | 1004, 1003 | 98.26% | 98.26% |
| `src/anti/` | 98.63% of 11663 | 98.92% of 11663 | 160, 126 | 98.69% | 98.93% |

## The tests

- runtime, `unit_plugin_table`: a search directory whose index does not
  fit a path is passed over, `discover_in` of `plugin.c`, `c4cf1b81`.
- runtime, `matrix_classes_type_name_of_an_unfilled_element`: `type_name`
  of an element of `alloc(T, n)` with a zero table gives the empty text,
  `anti_lang_Object_type_name` of `object.c`, `f9b1a3fd`.
- runtime, `matrix_classes_serialize_a_flags_field`: a `Flags` field is a
  struct without a descriptor, written as `null` and passed over by
  `deserialize`, `read_value` of `registry.c`, `723e5435`.
- anti, 17 cases of `tests/bind/messages.cmake`, `f2ce49ad` and
  `b2e1fd70`. Eight headers of the pinned clang reach a tag a parameter
  declares, a field of an anonymous enum, a function through a typedef, a
  wide anonymous enum in a macro, a packed enum, a chain of 300 typedefs
  and the probe of an enum of zero. A stand-in clang that serves
  `ast.json` and `pre.i` reaches the guards of `bindclang.c` for parts
  clang leaves out, and a version line after another line.
- anti, `run_without_a_version` and `fault_imported_object_directory_not_made`
  of `tests/anti-build/messages.cmake`, `options_key` and `module_object`
  of `build.c`, `ae8ac78a`. The second fails as T153 says and is listed
  under it.
- anti, `memory_checks_last_line_without_a_newline`: the last line of the
  standard error passes the symbolizer of `anti run` without a newline,
  `platform_run_lines` of `src/antic/platform.c`, `9f676996`.
- anti, `repository_of_a_drive_letter`: `file:///C:/nowhere` names
  `C:/nowhere`, `file_url_path` of `repo.c`, `f5af216a`.
- anti, `configuration_in_the_root_not_there` of
  `tests/anti-symbols/messages.cmake`: the directory of `/nowhere.toml` is
  `/`, `directory_of` of `syms.c`, `2deaeca7`.

## Findings

The step closed none. It recorded one, with its test in
`tests/known-failures.txt`:

- T179, `3f418676`: a project takes a path dependency, which takes a
  package without a version by path. The build ends with `no repository
  of the manifest has a version of com.example.p2`. The package builds as a direct path
  dependency, and the chain builds when it names a version.

`fault_imported_object_directory_not_made` is a second test of T153.
The case `fault_object_directory_not_made` of T153 fails its `mkdir:13`
at the library directory, not at the object directory it names. It will
fail on the fix of T153 for that reason. Its count is left for the
step of T153.

`tests/known-failures.txt` held 130 lines and 111 entries, 109 expected
to fail on the Mac. It holds 132 lines and 113 entries, 111 expected to
fail on the Mac. No feature, option or command, and no `[provisional]`
entry.

## Regions nothing reaches

- `src/rt/registry.c` 207: every caller passes the size of a descriptor
  or a product that 507 and 539 check. 790: a variant descriptor always
  holds the record of its tag. 927: `from` is a `*Allocator`, which holds
  no `none`.
- `src/rt/plugin.c` 1248: `interfaces.` and a count of 19 digits fit 32
  bytes. 1259: each item of the list is a pair of the document. The loop
  ends at the first missing item before it reaches the count.
- `src/antic/header.c` 40: `export` refuses a `char`, and every other
  type that reaches `scalar_name` has its case.
- `src/anti/build.c` 198: `text_cstr` gives no NULL. 687: a build holds
  one module at least, or it stops before.

## Not run on the target

- `unit_plugin_table`: `long_directory` builds the search path with the
  separator of the system and a directory of 4100 bytes.
- `fault_imported_object_directory_not_made` counts the directories of
  the Mac.
- `repository_of_a_drive_letter` names an absolute path on Windows.
- `configuration_in_the_root_not_there` on Windows.
- The real clang cases of `tests/bind/messages.cmake` run on Linux.

## Gates

- No warning of our code: `rt17-hb.log`, `rt17-asan-build.log`,
  `rt17-ubsan-build.log` and the builds `rt17-b1.log` to `-b9.log`.
- Host, 5773 tests, `-I 1,2000` and `-I 2001,5773`: 2000 and 3800 passed,
  in 374 s and 329 s. Part 2 runs 27 fixtures of part 1 again. 111
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt17-host-1.log` and `rt17-host-2.log`.
- ASan, 5771 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5771`: 1151, 1106, 1577, 906 and 1073 passed in 240 s, 186 s,
  185 s, 447 s and 354 s, `rt17-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1106, 1577, 906 and 1073 passed in 247 s,
  165 s, 138 s, 171 s and 295 s, `rt17-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report and on
  `docs/audit/tests-findings.md`.

## State at the push

```text
$ git log --oneline -3
b2e1fd70 Put an anonymous union before the field of an anonymous enum
2deaeca7 Test anti symbols check on a configuration in the root directory
f5af216a Test a file URL of a repository that holds a drive letter
$ git status --short
$ git rev-parse HEAD origin/main
b2e1fd709401ff441cadb0bbb250261a7aa3b140
b2e1fd709401ff441cadb0bbb250261a7aa3b140
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/anti/`, 126 places: `bindwrite.c` 23, among them `spell` 125 to
  142, which no parameter of the AST of clang reaches, and `held_none`.
  `deps.c` 373 and `repo.c` 349 need a library file whose header names a
  dependency without a constraint or with a name that is no package name,
  which `antic --dependency` writes and no table of messages can.
  `doc.c` 400, 446 and 480.
- `src/antic/`, 1003 places. `build/rt17/untriaged.py` lists those no
  report has given a reason for. `sema_generic.c`, `sema_copies.c`,
  `whole_tables.c`, `coff.c`, `antl_tree.c` and `antl_read_ir.c` hold the
  most. `driver_link.c` 171 to 175 need a Linux host, which takes the
  platform linker for a Linux target.
- `src/rt/`, 376 places: `trace.c` of ELF and PE, `object.c` 243 and
  341, and `plugin.c` 642, 719 and 864, which need a damaged library.

CONTINUE
