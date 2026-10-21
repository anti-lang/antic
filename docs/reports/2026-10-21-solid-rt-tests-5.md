# Step rt-tests, fifth session

The session went on from the list of `2026-10-21-solid-rt-tests-4.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt5-*`. The list of the regions not reached is
`rt4-end-unreached.txt` at the start and `rt5-end-unreached.txt` at the
end. The probes are in `build/rt5/`: `rtf.sh` measures one program
against the fault runtime of the coverage tree, `sweep.py` runs a fault
program under every kind and count of fault, and `ct.sh` measures tests
of the coverage tree by name.

## Coverage per directory

Regions reached. The start is `8f474ead`, `rt4-end-summary.txt`.
`bd036090` above it changed a report alone. The end is `3608f9a1`,
`rt5-end-summary.txt`. Every part of the run passed, `rt5-end-1.log` to
`-5.log`.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 94.09% of 9252 | 94.51% of 9252 | 547, 508 | 94.19% | 94.63% |
| `src/antic/` | 97.96% of 55587 | 97.97% of 55587 | 1136, 1127 | 98.00% | 98.02% |
| `src/anti/` | 97.98% of 11663 | 98.02% of 11663 | 236, 231 | 98.05% | 98.10% |

`trace.c` gained 2 regions and `hooks.c` 1 by the timing of threads
alone, as the fourth report describes.

## The measurement

The glue of `anti.regex` in the fault runtime compiled without the flags
of the coverage tree, so the counts of `faults_regex` never reached the
report. `04bf759a` compiles it as the glue of `runtime/` is compiled in
that tree. That alone counts `keep` of `regex.c` and three regions of
`patterns.c` that existing fault cases reached.

## The tests

- runtime, faults: `faults_regex` compiles a byte pattern and a pattern
  that does not compile, with three new cases. `faults_fs` refuses a
  later variable of the environment, `faults_conf` the bytes of the
  configuration file and `faults_toml` a pair of an inline table.
- runtime, programs: `std/deserialize_refused` reads one text with an
  allocator that refuses each block in turn, `std/serialize_regex_field`
  compares an object whose `Regex` field holds no pattern, and the plugin
  host `locals_host` names a class that its library declares as well.
- antic: a library for C exports a function of a class alone, and an
  export of `clib/failing` names the error through a parameter alone. An empty exported enum stands in `clib/views`, and the matrix
  gives out a pointer into the fields through two private functions.
- anti: `anti bind` of a last field that is an array without a length.
  `-D` gives the shim a name alone, and a description holds two lines.

## Findings

The step closed none. Three are new, each with its test.

- T166, severe: a search with a `Regex` field that `Object.deserialize`
  left without a pattern crashes with SIGSEGV.
  `std_deserialize_regex_search` holds it.
- T167, major: `anti bind --clang` of a `static` function that returns
  a pointer to a function writes a shim that clang refuses.
  `anti_bind_message_clang_static_returns_a_function` holds it.
- T168, major: a write in a concurrent class to a field of its concurrent
  base is not checked. `matrix_threads_concurrent_class_writes_a_field_of_its_base_refused`
  holds it.

Four new cases stand under T139: `std_faults_regex_fault_bytes_memory`,
`bytes_map_memory`, `message_memory` and `std_faults_toml_fault_memory_15`.

`tests/known-failures.txt` held 102 lines and 83 entries, 81 expected
to fail on the Mac, and holds 109 lines and 90 entries, 88 on the Mac.
No feature, option or command.

One `[provisional]` entry, under "Regular expressions" in
`docs/decisions.md`: a `Regex` that holds no pattern stops the program
with a message at a search. No document decided it, and T166 needs a
rule to be written against.

## Regions nothing reaches

- `src/rt/registry.c` 696 to 711: `anti_rt_read_float` fails for no span
  that `anti_rt_json_valid_number` accepted, since `read_decimal` reads
  every JSON number.
- `src/rt/sync.c` 73, 190 and 206: no program holds a channel without
  one. A `?chan` is tested before a use, and `own` takes no channel.
- `src/antic/lexer.c` 1570 to 1584: `parser_expect` never asks for a
  literal, a comment or the end of the file, and the two other callers
  name operators.
- `src/antic/header.c` 192 to 195: C takes no slice in an exported
  signature, so no name of a tuple holds one. 922: a union does not nest
  in a class.
- `src/anti/bindwrite.c` 125 to 130: clang gives a parameter declared as
  a function as the pointer it decays to. 218 to 220 run once T167 is
  fixed.

## Left for the next session

- `src/antic/`, 1127 places: `llvm_emit_function.c` 55, `sema_generic.c`
  47, `driver_link.c` 43, `sema_const.c` 39 and `antl_tree.c` 36 have the
  most. `sema_generic.c` 1885 to 1914 may yield to a generic call whose
  argument of the wrong shape follows a tuple of two equal tuples.
- `src/anti/`, 231 places: `bindclang.c` 34, `bindwrite.c` 28, `deps.c`
  20 and `build.c` 20. Most of `bindclang.c` reads shapes of the AST that
  only the fake clang of `ast_malformed` writes.
- `src/rt/`, 508 places: `trace.c` 111 and `symbols.c` 55, mostly ELF and
  PE, `plugin.c` 70 and `object.c` 32.

## Not run on the target

- No change of this session runs on a Linux or a Windows machine alone.

## Gates

- No warning of our code: `rt5-host-b1.log`, `rt5-asan-build.log`,
  `rt5-ubsan-build.log` and `rt5-end-1.log`.
- Host, 5600 tests, `-I 1,2000` and `-I 2001,5600`: 2000 and 3626
  passed, the second with the fixtures ctest adds, in 348 s and 293 s. 88
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt5-host-1.log` and `rt5-host-2.log`.
- ASan, 5598 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5598`: 1151, 1114, 1576, 905 and 898 passed in 210 s, 176 s,
  184 s, 450 s and 335 s, `rt5-asan-1.log` to `-5.log`.
- UBSan, the same parts: all passed in 238 s, 157 s, 137 s, 168 s and
  293 s, `rt5-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no file of
  `tests/emit-identity/` or `tests/link-identity/` changed.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## State at the push

```text
$ git log --oneline -3
3608f9a1 Record T168: a write to a field of a concurrent base is not checked
52ee30f9 Test a pointer into the fields through two private functions
716ae097 Test a description of two lines under anti bind
$ git status --short
$ git rev-parse HEAD origin/main
3608f9a1930a821235eea624f4cb3a61b46d80c8
3608f9a1930a821235eea624f4cb3a61b46d80c8
```

This report is written after that push, and its commit follows it.
