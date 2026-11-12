# Batch 3d of the third phase of coverage

This step sorts the regions that `./c coverage` at the end of batch 3c
showed unreached in four files: `src/anti/build.c`, `src/antic/coff.c`,
`src/antic/lower_place.c` and `src/antic/llvm_emit_call.c`. It follows
the method of `docs/reports/2026-11-11-coverage-phase-3b.md`. A library
file written by hand is an input, as that report holds, so an IR that
such a file can carry is an input of the back end, and `unit_llvm_emit`
builds it in its place. `unit_coff` builds the objects of a join by
hand, so a damaged object is an input as well. A call of the system that
a fault case of `anti_faults` makes fail is an input of `anti build`, as
the fault cases of the earlier batches hold. Eddie's direction is the
Mac alone, so the VMs were not touched. The batch adds the two programs
of one modules test. The other cases extend two unit groups, two modules
tests and the table of the messages of `anti build`.

## Commits

- `92f44a47` lists T238, `f6cb09f2` fixes it.
- `4865628f` lists T239, `a314ea2e` fixes it.
- `15774535` lists T240, `b214a21f` fixes it.
- `b6250f23` tests `lower_place.c`.
- `036f1893` tests `coff.c`.
- `0b6fa0fe` tests `llvm_emit_call.c`.
- `dffd04c7` tests `build.c`.
- `40d2d87a` names the guards in `docs/notes/unreached-guards.md`.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the fixes in `docs/audit/tests-findings.md`.

## Findings

- T238, major, fixed. `d.f(3)` on a value, a pointer or `self` of a
  derived class, where `f` is a field of function type of the base, was
  refused with `` `Holder` has no function `f` ``. `callee_form` of
  `sema_call.c` looked the field up in the class of the receiver alone.
  `calls_field` now walks the chain, and a function of the name that the
  call reaches still takes the call. The test of `lower_place.c` met it,
  where a client called the `own fn` of a base of a library.
- T239, severe, fixed. The default `none` of an `own ?fn` field of a
  class of a library made a dev build of a client abort in `delete`.
  `lower_store_constant` copied the two words of the field from a
  constant of one pointer, so the context word took the bytes after it.
  The test of an `own fn` now comes before that of an aggregate, as its
  comment said, and stores the code and a null context. The release
  build read zeros there by chance. `modules_own_function_default` holds
  it.
- T240, minor, fixed. A second `anti build` that could not read the
  imports of a library file of its cache printed `antic: cannot open`
  for it, then compiled the module again and exited with 0. The check of
  `module_library` read them through `cache_imports`. It now reads them
  through `cache_imports_quietly`, as the check of `src/anti/test.c` does
  by its DESIGN, which names build.c as doing the same.

## lower_place.c

`modules_optional_class_field` gives the class of its library a
`?Handle` that holds a handle, a struct of a class value and bitfields in
two units and an `own fn`. A client writes each default from the library
file part by part. `modules_generic_atomic` takes a second library,
whose own copy of a generic of the first names the static field of the
first, so its object declares that field. After the fix of T239 the
store of an `own fn` constant is reached. Seven rows of guards stand in
the note.

## coff.c

`unit_coff` takes nine cases and three more parts of earlier ones. A
join of objects that define nothing looks a name up in an empty table.
Directives that start alike, directives parted by tabs and absolute
symbols of both kinds join. A COMDAT `any` after one of another
selection is refused. A dropped COMDAT takes its associated section and
an external symbol there with it, and a relocation to its section
symbol is refused. A precompiled type stream and a table of type hashes
count as debug information, and an empty type stream does not. A long
section name past its table, below its size field, without its NUL or
past 9999999 bytes of names is refused. The overflow form of a count of
relocations joins, and a file symbol keeps two auxiliary records. A weak
external whose default is dropped is refused. The archive exports skip
a common and an absolute symbol and refuse a damaged object. Four rows
of guards stand in the note.

## llvm_emit_call.c

`unit_llvm_emit` takes five cases. A scalar argument, the address of an
aggregate argument, a returned aggregate, the address and the value of
an atomic call and the context of a call through a function with its
context that do not translate fail the module, as does an aggregate
past the parameters of a variadic function. A call result of another
type than the callee declares is converted by `ptrtoint`, `trunc`,
`zext` and `bitcast`. An atomic call of three bytes stays a call, and a
store with a result writes none. A constant context takes the selects
in the argument where it is null and none otherwise, and a signature
without the context word passes it in its own type. The range of a
void or a coerced result and dereferenceable of a type of no bytes are
left out. Six rows of guards stand in the note.

## build.c

The table of `anti build` takes 28 cases, which the commit names, and
the case of T240. Each fault case names its count. The counts were
found on the Mac and checked in the host and the coverage trees. A
reuse case builds once in its setup and then removes or damages a file
of the cache before the build it checks. The message of a Windows link
without its PDB left `tests/diagnostics-open.txt`, since a case holds it
now. Six rows of guards stand in the note.

## Branches left

The sides of a condition that a guard of the note holds were not
chased. Three paths of `build.c` that an input reaches stay for a later
batch:

- The end of an absolute path in `/` in `name_program`, which needs a
  path dependency of a name ending in `/`, both calls of `realpath`
  failing and no name of its own.
- A runtime archive whose directory of a level cannot be read, which
  `cache_runtime` refuses with `anti: cannot read`. Its fault seam is
  held, and a case cannot remove an unreadable directory after itself.
- `anti build --release --profile-use` of a file that is missing, which
  gives `anti: cannot read`. It was found after the sanitizer suites of
  the push had run.

The `}` after a `break` and the `switch` over every value of an enum
belong to the measurement.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 98.22%, 1489 missed | 98.28%, 1445 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.45%, 1471 missed | 98.51%, 1409 missed |
| Branches | 91.42%, 4761 missed | 91.59%, 4668 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `build.c` | 97.94%, 28 missed | 98.97%, 14 missed | 90.79% | 97.57% |
| `coff.c` | 96.86%, 28 missed | 97.98%, 18 missed | 92.28% | 97.31% |
| `lower_place.c` | 94.20%, 28 missed | 97.72%, 11 missed | 88.93% | 91.22% |
| `llvm_emit_call.c` | 96.22%, 26 missed | 99.27%, 5 missed | 87.57% | 96.15% |

The lines that stay unreached in the four files are guards of the note,
regions of the measurement and the paths above. The coverage suite ran 7229 tests, all passed. `./c coverage` took 968 s.

## Suites

- The host suite ran before each code commit, in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`.
- The fault sweeps that found the counts of `build.c` ran on one core
  during the first suite, whose times are a few seconds slow.
- Both sanitizer suites ran after the last code commit, in the six parts
  of `asan`, up to test 7239. Every test passed. `asan` took 248 s,
  185 s, 260 s, 180 s, 448 s and 447 s. `ubsan` took 255 s, 190 s,
  266 s, 201 s, 473 s and 465 s. The new unit cases passed in both trees
  before their commits as well.

## Provisional lines

None. The three fixes change no rule. T238 and T239 make the code do
what the specification and its comment say, and T240 follows the
DESIGN of `src/anti/test.c`.

## Questions for Eddie

None. Every guard stays by "Answers of 2026-11-10, second set". The
hand-written library file counts as input by the method of batch 3b,
and the quiet check of T240 follows the DESIGN of `src/anti/test.c`.
