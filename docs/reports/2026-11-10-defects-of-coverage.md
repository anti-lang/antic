# The defects of the second phase of coverage

This step builds the five answers of "Answers of 2026-11-10" in
`docs/decisions.md`, which answer questions 1, 2, 6, 8 and 11 of
`docs/reports/2026-11-10-coverage-phase-2b.md`. Each answer fixes a
defect, so the feature freeze lets each change in. Eddie's direction is
the Mac alone, so the VMs were not touched.

## The interruption

A reboot of the Mac stopped the first session of this step. The tree then
held the fix of T219 uncommitted, and no suite had run on it. The second
session read the diff against Q1 before it went on. The record of a type
argument that is an array of arrays carries the descriptor of the array
with its levels, which `array_descriptor` of `src/antic/lower_desc.c`
writes for a field. `anti_rt_element_serialize` and
`anti_rt_element_text` of `src/rt/object.c` write such an element as they
write a field. The descriptors stand in the object of each module and not
in the library file, so the format of the library file did not change.
The session rebuilt `build/host` from the state the reboot left, ran the
two tests of T219 and the identity tests alone, then the whole suite, and
committed the diff as it stood.

## Commits

- `72374330` fixes T219. The runtime changed, so the digest of
  `tests/link-identity/return42.macos-arm64.sha256` is written again.
- `73fb0978` fixes T221.
- `b101e693` tests Q6 and lists T223 and T224.
- `98612db3` fixes T223 and T224.
- `c0367a25` tests Q8 and lists T225, T226 and T227.
- `512a08fd` fixes T225, with a provisional line.
- `d08e4406` fixes T226.
- `2114555a` fixes T227.
- `a53d70f9` builds Q11.
- This report, with the times and the counts in `CLAUDE.md` and the
  commits of the fixes in `docs/audit/tests-findings.md`.

## The answers

- Q1, T219. `to_text` of a `List<[2][2]int>` writes `[[[1, 2], [3, 4]]]`,
  and `serialize` writes `[[[1,2],[3,4]]]`.
- Q2, T221. `struct proved` of `src/antic/sema_checker.h` holds the names
  a condition proves in a list that grows with `alloc_grow`, and
  `sema_proved_free` gives it back. `PROVED_MAX` is gone, and the four
  callers take any count.
- Q6, T223 and T224. The tuple literal of `src/antic/sema_expr.c` refuses
  an element of a `void` call with the message of a `let` of a `void`
  value, `` `nothing` returns no value ``. It refuses an element of an
  abstract class with the message of `let s = *sp;`. A `let` whose value
  is the literal names its local, `` `Shape` is abstract and has no
  complete value, and this local needs one ``. Every other context names
  the tuple, `and a tuple needs one`. A `return` and an assignment gave
  the message of `void` already, through the type of their context, and
  two refusals of the matrix now hold it. Neither can hold an abstract
  element without a second mistake, since a declaration refuses a tuple
  type of an abstract class.
- Q8, T225. `--lib shared --dev` is refused with `` --lib shared builds in
  release mode, without --dev ``, the message of `--lib static --dev`.
- Q8, T226. `--dev -c lib.antl` is refused with `` -c compiles a source,
  and lib.antl is a library file ``.
- Q8, T227. `platform_suffix_alone` of the platform layer tells a file
  whose name is a dot and a suffix alone. The arguments of antic, the
  input of `driver_run` and the files of `anti doc` refuse such a file
  with `` .antl: the file has no name, only its suffix ``. The walks of a
  project leave every name that starts with a dot alone already.
- Q11. `if let Circle c = p` on a value that is no variant says
  `` `if let Circle c` takes a variant, found `?*Shape` ``. A bare name,
  `if let m = n`, keeps the message that names both forms, and a new
  refusal of the matrix holds it.

## Findings

T219, T221, T223, T224, T225, T226 and T227 are fixed. No defect of this
step stays open, and `tests/known-failures.txt` holds the two lines of
Windows, T136 and T138, alone.

## Provisional lines

- Q8 refuses `--lib shared --dev`, under "Answers of 2026-11-10". No
  specification lets a dev build make a shared library.

## Suite times

The host suite ran as `ctest -I 1,2100`, `-I 2101,4300` and
`-I 4301,<last>` before each commit of code.

| Commit | Last test | Times |
|---|---|---|
| T219 | 7157 | 377 s, 262 s, 299 s |
| T221 | 7157 | 379 s, 265 s, 323 s |
| tests of Q6 | 7163 | 386 s, 267 s, 342 s |
| T223 and T224 | 7163 | 384 s, 264 s, 304 s |
| tests of Q8 | 7171 | 414 s, 264 s, 310 s |
| T225 | 7171 | 396 s, 268 s, 308 s |
| T226 | 7171 | 389 s, 265 s, 312 s |
| T227 | 7171 | 378 s, 261 s, 299 s |
| Q11 | 7172 | 391 s, 265 s, 303 s |

The first run of the tests of Q6 failed `fmt_canonical`, since four new
programs wrapped their first comment where `anti fmt` does not. The files
took the canonical form, and the suite ran again.

Each sanitizer suite ran in the six parts of CLAUDE.md, up to test 7170,
and every test passed.

| Suite | Times |
|---|---|
| `asan` | 243 s, 183 s, 260 s, 182 s, 437 s, 442 s |
| `ubsan` | 261 s, 186 s, 257 s, 215 s, 474 s, 499 s |

## Questions for Eddie

1. Q6 names the message of `let s = *sp;`. A `let` of the literal gives
   it word for word. An argument has no local, so its message names the
   tuple, `and a tuple needs one`. Is that the form you want there?
2. The message of T227 is `<path>: the file has no name, only its suffix`.
   antic ends with 2 and `anti doc` with 1, as the refusals beside each
   do. Does it read as you want?
