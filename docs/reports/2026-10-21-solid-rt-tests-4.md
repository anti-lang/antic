# Step rt-tests, fourth session

The session went on from the list of `2026-10-21-solid-rt-tests-3.md`,
which stays, as do the reports of the first two sessions. It wrote tests
for regions of `src/antic/`, `src/anti/` and `src/rt/` that a program,
an option or an input reaches. Logs are under `build/drive/logs/`,
named `rt4-*`. The list of the regions not reached is
`rt3-final-unreached.txt` at the start and `rt4-end-unreached.txt` at
the end. The probes are in `build/rt4/`: `ac.sh` compiles one source
with the coverage antic, `ap.sh` runs the coverage anti in a copy of a
project, `rtp2.sh` measures one program against the coverage runtime
and the glue of `anti.regex`, and `fmtchk.sh` compares a source with
what `anti fmt` writes.

## Coverage per directory

Regions reached. The start is `591b629b`, `rt3-final-summary.txt`.
`6c89deef` above it changed a report alone, so the coverage tree of the
third session measured the same sources. The end is `8f474ead`,
`rt4-end-summary.txt`. Every part of the run passed, `rt4-end-1.log` to
`-5.log`.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 93.77% of 9252 | 94.09% of 9252 | 576, 547 | 93.79% | 94.19% |
| `src/antic/` | 97.93% of 55587 | 97.96% of 55587 | 1152, 1136 | 97.97% | 98.00% |
| `src/anti/` | 97.82% of 11663 | 97.98% of 11663 | 254, 236 | 97.94% | 98.05% |

`trace.c` lost 2 regions and `platform_posix.c` 1 between two runs of
the same tree. A run of the suite reaches those few regions by the
timing of its threads alone.

## The tests

- antic, matrix: a conversion with two paths of a call, of an element
  and to a nullable pointer, `destroy(&*p)`, two classes of a pattern
  whose `[:` opens no POSIX class, a repeat before `\b` that the
  checker passes, and two arms that both name `size_of(c_long)`.
- anti: `forms.h` of `anti_bind_forms` binds a compound literal through
  a macro whose body is its bare parameter and leaves out two that do
  not expand, and keeps the pack after `#pragma pack(push)`.
  `anti_build_resolve` takes a diamond of repository packages beside a
  path dependency and writes again a lock file changed by hand. The
  bind case `api_without_a_suffix`.
- runtime: `clib_classes` calls `equals`, `hash` and `destruct` of the
  root from C, the plugin host builds classes by their bare names, the
  modules test `same_name` finds a class two modules declare by its
  module path, and the std tests `regex_same_pattern`,
  `toml_index_outside`, `deserialize_slice_broken` and
  `serialize_char_array` with a new case of `regex_patch_edges`.

## Findings

The step closed none. One is new, with its test.

- T165, minor: an `internal` function of an enum body gets its message
  and then `expected identifier` at `self`, since the parse of the
  module goes on inside the function it refused.
  `matrix_types_enum_internal_function_refused` holds it.

`tests/known-failures.txt` held 101 lines and 82 entries, 80 expected
to fail on the Mac, and holds 102 lines and 83 entries, 81 on the Mac.
No `[provisional]` entry, and no feature, option or command.

## Regions nothing reaches

- `src/antic/parser_decl.c` 507 to 511: the parser calls
  `parser_compatible_line` only when a number follows `compatible`. 655
  to 665: `parser_members_of` reads an enum body alone, with no list of
  nested types, so its arms for a class never run.
- `src/antic/lower.c` 482 to 491: `lower_calloc_function` has no
  caller.
- `src/antic/pattern.c` 401 to 404 and 432: PCRE2 refuses a pattern
  that ends in `\` or `\c` before the checker reads it. 514: a `[:`
  without its `:]` leaves its class open, which PCRE2 refuses.
- `src/antic/sema_expr.c` 1180 to 1186: the check of the declaration of
  `operator fn iter` refuses a result without `next` and `value`, a
  generic result as well.
- `src/antic/sema_const.c` 851 to 853: the checker refuses `mul_high`
  of a type the target sizes before the evaluator. 1041: it refuses a
  constant array whose length comes from `size_of`. 1128: a call is no
  constant, so the evaluation of its field stops before. 1133: the
  check of cycles refuses a class constant that names itself.
- `src/antic/lower_place.c` 278 to 283: a static field is atomic, and
  the checker refuses its address.
- `src/rt/patterns.c` 287: PCRE2 refuses `\K` in a lookaround, so no
  match ends before it starts. 844 to 849: an empty literal never
  reaches the match limit.
- `src/rt/object.c` 915: no value has the type `none`.

## Left for the next session

- `src/antic/`, 1136 places. `llvm_emit_function.c` 55,
  `sema_generic.c` 47, `driver_link.c` 43, `sema_const.c` 39,
  `antl_tree.c` 36 and `sema_copies.c` 35 have the most.
- `src/anti/`, 236 places: `bindclang.c` 34 and `bindwrite.c` 33,
  `deps.c` 20, `build.c` 20, `repo.c` 15 and `fmt.c` 15. The blank
  lines of a comment in `fmt.c` 1369 and 1470 did not come from five
  forms of comments the session tried.
- `src/rt/`, 547 places: `trace.c` 87 and `symbols.c` 55 are mostly ELF
  and PE, `plugin.c` 70, `object.c` 34 and `registry.c` 26. `stub` of
  `plugin.c` runs for a slot only `reflect.call` reaches, and the
  descriptor of the class of an older library lists no function for
  that slot, so a call by index needs a closer look.
- `src/antic/driver_link.c` 36 to 54 and 171 to 176 run on a Linux host
  alone, as the second report says.

## Not run on the target

- No change of this session runs on a Linux or a Windows machine alone.

## Gates

- No warning of our code: `rt4-host-b1.log`, `rt4-asan-build.log`,
  `rt4-ubsan-build.log` and `rt4-end-1.log`.
- Host, 5584 tests, `-I 1,2000` and `-I 2001,5584`: 2000 and 3610
  passed, the second with the fixtures ctest adds, in 264 s and 282 s.
  81 expected failures fail as listed, and the two
  `mimalloc_environment` tests skip: `rt4-host-1.log` and
  `rt4-host-2.log`.
- ASan, 5582 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5582`: 1150, 1114, 1576, 905 and 882 passed in 221 s,
  160 s, 179 s, 442 s and 338 s, `rt4-asan-1.log` to `-5.log`.
- UBSan, the same parts: all passed in 236 s, 151 s, 137 s, 169 s and
  293 s, `rt4-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, and no file of
  `tests/emit-identity/` or `tests/link-identity/` changed.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## State at the push

```text
$ git log --oneline -3
8f474ead Test a description of anti bind without a suffix
2fc20938 Test two arms of one target size, and record T165
22238bda Test regions of the runtime that no test reached
$ git status --short
$ git rev-parse HEAD origin/main
8f474ead5d77df74367fc1e50333e92ac9b01a20
8f474ead5d77df74367fc1e50333e92ac9b01a20
```

This report is written after that push, and its commit follows it.
