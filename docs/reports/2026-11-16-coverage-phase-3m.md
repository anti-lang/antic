# Batch 3m of the third phase of coverage

This step sorts the regions that `./c coverage` at the end of batch 3l
showed unreached in `src/rt/conf.c`, `src/antic/text.c` and
`src/antic/sema_declare.c`, and `ir_lane_bytes`, which 3l left open. It
goes on with `src/rt/json.c`, `src/antic/llvm_debug.c`,
`src/antic/sema_class.c`, `src/antic/sema_pattern.c`,
`src/antic/sema_export.c` and `src/antic/lower_error.c`, which no batch
had sorted. It follows the method of
`docs/reports/2026-11-12-coverage-phase-3d.md`: each region first ran
through the antic or the unit tests of the coverage tree alone, with a
profile of its own. Eddie's direction is the Mac alone, so the VMs were
not touched. `src/antic/layout.c` waits for the answer on its guard in
`compute`, and the framework region of `driver_search.c` stays as it
is. The probes found two defects, T265 and T266. T267 is the next free
id.

## Commits

- `c0dff869` lists T265, which stays open. See the first question.
- `d1b4bbf3` lists T266, `72697e32` fixes it.
- `a1d1eb08` tests `conf.c`, `text.c`, `json.c`, `llvm_debug.c` and
  `ir_lane_bytes`.
- `f7823b24` tests `sema_declare.c`, `sema_class.c` and `sema_pattern.c`.
- `e8409f84` names the guards in `docs/notes/unreached-guards.md` and the
  commit of the fix of T266 in its row. One commit records the times in
  `CLAUDE.md` with this report.

## Findings

- T265, major, open. A field default of a class that is a literal of a
  struct or a class the file declares after the class, with the fields
  it names, `point: Point = Point { x: 5, y: 6 }`, is refused with
  `` `Point` has no field `x` ``. `declare_fields` of `sema_declare.c`
  checks each default while it declares the fields of its class, in the
  order of the file. The same program with the types first compiles.
  `matrix_classes_default_literal_of_a_type_declared_later` holds a
  struct, a class value and an array of class values, in its three
  modes. The probe of `holds_short_class` met it.
- T266, minor, fixed. A simd struct whose declaration refuses its lanes
  got a second message as the argument of a generic that needs `add`.
  `check_meets` of `sema_generic.c` now passes such a struct by in
  silence, as the operators of `sema_simd.c` do. The probe of the first
  guard of `sema_simd_meets` met it.

## The files

- `conf.c`: eleven cases of the table of `tests/CMakeLists.txt`, the keys
  `runtimes`, `injections` and `runtime` outside every table, a part
  after the dot that is no number, a table after an array, array values
  of three keys in a row, an empty value, an include by an absolute path,
  the option `--anti.abcd`, and a count of threads with a sign or a
  letter.
- `text.c`, `json.c` and `llvm_debug.c`: `unit_text`, `unit_json` and
  `unit_llvm_emit` call the functions with what no caller passes.
- `ir.c`: `unit_modules` gives both lanes of a simd struct of a library
  file the type of a pointer, which reaches `ir_lane_bytes` of a lane
  that is no number.
- `sema_declare.c`: `link linux` names with a backslash and a space, a
  struct on both sides of a `provides` line, fields of an unknown type
  marked `own`, `transient` and `inject`, the width of a bitfield as a
  text and from `size_of`, two values of one name before a value that
  names them, and a value of `text.Align` of another module in the
  values of an enum.
- `sema_class.c`: a `concrete fn` and an `operator fn eq` with a
  parameter of an unknown type, each with the one message of the type.
- `sema_pattern.c`: the block of an `if let` that takes the address of
  the pointer it binds.
- `sema_export.c` and `lower_error.c`: guards alone.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.45%, 1310 missed | 98.48%, 1291 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.67%, 1276 missed | 98.69%, 1253 missed |
| Branches | 91.98%, 4496 missed | 92.06%, 4452 missed |

| File | Lines missed before | after | Branches missed before | after |
|---|---|---|---|---|
| `conf.c` | 8 | 8 | 20 | 9 |
| `text.c` | 8 | 3 | 10 | 6 |
| `sema_declare.c` | 7 | 4 | 50 | 39 |
| `json.c` | 7 | 5 | 16 | 8 |
| `llvm_debug.c` | 6 | 0 | 15 | 8 |
| `sema_class.c` | 6 | 2 | 99 | 97 |
| `sema_pattern.c` | 6 | 4 | 26 | 25 |
| `ir.c` | 10 | 9 | 15 | 14 |
| `sema_export.c` | 6 | 6 | 32 | 32 |
| `lower_error.c` | 7 | 7 | 14 | 14 |

The lines that stay are guards, the `}` after a `return`, the line of
`holds_short_class` that waits for T265, and the name of a group past
`u64` in `sema_pattern.c`, which a damaged library file reaches. After
the fix of T266, the first guard of `sema_simd_meets` has no test, and
`sema_simd.c` stays at 7 lines. `rt.configure("")` is a side of a
condition that no test takes yet. The coverage suite ran 7487 tests, all
passed. `./c coverage` took 980 s.

## Suites

- The host suite ran before each of the five code commits, in the parts
  `-I 1,2100`, `-I 2101,4300` and `-I 4301,<last>`. The times stand in
  `CLAUDE.md`. Before the commit of T265, `fmt_canonical` failed on the
  wrap of the comment of the new program, which took the form of `anti
  fmt`, and the suite ran again whole.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7497. Every test passed. The last part took 579 s
  under ASan and 594 s under UBSan.
- The host runs 7499 tests and each sanitizer tree 7497.
  `tests/known-failures.txt` holds the two lines of `host=windows` and
  the three of T265.

## Questions for Eddie

1. T265. Every fix of it checks the defaults of a file once every type
   has its fields, by two passes or by declaring a type on demand as
   `sema_enum_values_ready` does for an enum. Either retires
   `complete_defaults` and `holds_short_class`, the DESIGN of T149, which
   evaluates a default again once every type has its fields. Should the
   fix check every default after the fields of every type of the file,
   and remove `complete_defaults`?
2. The questions of the guard of `compute` in `layout.c`, of the refusal
   of `(b.g, 1)` from 3k and of the form of T264 from 3l are still open.
