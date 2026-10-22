# The solid run, 2026-10-14 to 2026-10-22

The run started after `a0a7c501`, with the feature freeze of `5e559b35`, and
ends at `26585396`: 968 commits. It ran fix steps 1 to 34 of
`docs/audit/summary.md`, the steps of the answers of 2026-10-18, the fault
tests, the coverage of the runtime, 20 sessions of runtime tests, the fuzzer
and the probe. This step changed no code. Its logs are under
`build/drive/logs/summary/`.

## The expected failures

| | Start, `a0a7c501` | End, `26585396` |
|---|---|---|
| Lines of `tests/known-failures.txt` | 387 | 133 |
| Entries, one per test | 374 | 114 |
| Expected to fail on the Mac | 374 | 112 |
| Rows of `docs/audit/tests-findings.md` | 127 | 188 |
| Open rows | 121 | 38 |

The run closed all 121 findings open at its start: 120 fixed, and T127 closed
as no defect by R081 of "Answers of 2026-10-18". It recorded 61 new ones,
T128 to T188, and fixed 23 of them.

The task expects only the cases the freeze keeps. The freeze keeps none now.
Eddie answered the ten cases and R081 on 2026-10-18. The steps
`answers-owned` and `answers-language` built them, and no line of the list
names one. The 114 entries are 38 open findings that no step of the run was
given: 8 severe, 13 major and 17 minor.

Each was recorded by a step whose task was something else. That task sent a
new defect to "the step of its area", and the run held no later step for
it. `fuzz-fix` and `probe-fix` took their own findings alone. No row names
the freeze as the reason it stays.

| Finding | Grade | Lines | Recorded by | Why it is left |
|---|---|---|---|---|
| T136 | severe | 1 | step 1 | Windows alone. The fix lies in the link of a plugin, outside the runtime of step 1. The VMs are out of service. |
| T138 | minor | 1 | step 2 | Windows alone. Step 25 of the runners fixed its three audit findings and not this one. |
| T139 | severe | 37 | step 3 | About forty places of the runtime. `regex.c` and `toml.c` build into antic and anti, which need a stop of their own first. |
| T141 | major | 1 | step 17 | Sent to the step of the back end, which took the minor audit findings alone. |
| T142 | severe | 1 | step 17 | The same. |
| T145 | severe | 1 | step 29 | Sent to the step of the runtime. None followed. |
| T146 | minor | 1 | step 32 | Sent to the step of its area. None followed. |
| T148 | major | 2 | `answers-language` | Met while building R017. The task held the answers alone. |
| T149 | major | 2 | `answers-language` | The same. |
| T150 | severe | 3 | `answers-language` | Met while testing R051. |
| T151 | major | 1 | `faults` | A step of tests. It fixed nothing. |
| T152 | major | 3 | `faults` | The same. |
| T153 | minor | 17 | `faults`, `rt-tests` | The same. Later sessions added tests. |
| T154 | minor | 1 | `faults` | The same, `host=macos`. |
| T155 | minor | 1 | `faults-2` | The same, `host=macos`. |
| T156 | minor | 4 | `faults-2` | The same. |
| T158 | severe | 1 | `rt-tests` | A step of tests. Sent to the step of the runtime's text. |
| T159 to T179 | 2 severe, 8 major, 11 minor | 36 | `rt-tests`, sessions 2 to 17 | A step of tests. Each sent to the step of its area. |

The severe ones among T159 to T179 are T166 and T175. T166 goes against
the `[provisional]` entry on a `Regex` without a pattern. The entry says the
search stops with a message.

## Coverage

Measured at `26585396` with `./c coverage part 1/5` to `5/5`, `cov-1.log` to
`cov-5.log`, in which every test passed. The summary is `cov-summary.txt`.
The runtime inside every program of the suite counts since `fb53310a` of
step `rt-coverage`. The test run's figures are those of
`docs/reports/2026-10-13-tests-summary.md`, which measured `src/rt/` only
where antic, anti and the unit tests compile it.

| Directory | Lines, test run | Lines, end | Regions, test run | Regions, end | Branches, test run | Branches, end |
|---|---|---|---|---|---|---|
| `src/antic/` | 98.04% of 58715 | 98.24% of 63669 | 97.90% of 50887 | 98.16% of 55785 | 91.05% | 91.18% of 38194 |
| `src/anti/` | 96.46% of 12727 | 99.14% of 13558 | 96.64% of 10956 | 99.13% of 11672 | 86.67% | 90.88% of 7288 |
| `src/rt/` | 74.21% of 5300 | 96.25% of 9589 | 75.63% of 5076 | 96.08% of 9252 | 67.56% | 88.35% of 5280 |
| All three | 96.13% of 76742 | 98.16% of 86816 | 96.01% of 66919 | 98.06% of 76709 | 88.76% | 90.84% of 50762 |

- Regions not reached: 1026 of `src/antic/`, 101 of `src/anti/` and 363 of
  `src/rt/`.
- Functions not reached: 65 of 3575 in the test run, 11 of 4227 now. They
  are 5 of antic, none of anti and 6 of the runtime.
- The last session of `rt-tests` measured 1000 regions of antic not reached.
  The 26 more came with the code of `fuzz-fix` and `probe-fix`.
- A program that ends through a trap or a signal writes no profile, so
  `check.c`, `cast.c` and the abort paths of `assert.c` read as unreached.

## Rates of new defects

The fuzzer, by step:

| Step | Generator | Valid programs | Causes in antic | Per thousand |
|---|---|---|---|---|
| `fuzz` | built in the step | 1300 | 3, T180 to T182 | 2.3 |
| `fuzz-campaign` | `cf4fc31a`, steers around T180 to T182 | 9040 | 0 | 0 |
| `fuzz-fix` | `679d68d2`, steers around nothing | 1153 | 0 | 0 |

The rate falls from 2.3 to 0 per thousand programs. The cleaner zero is that
of `fuzz-fix`, whose generator avoids no finding. The campaign's 150 seeds of
size 40 failed on a fault of the generator, `fn f16`, and count in neither.

The probe wrote 200 tests, which register 548 tests of ctest. It found 6
defects, 3.0 per hundred tests, and 2 of them severe. The test run found 124
in about 2950, 4.2 per hundred. The rate falls by about a third. It is one
sample, and its defects stand where generics, ownership and classes meet.
`probe-fix` fixed all six.

## The `[provisional]` entries the run added

`docs/decisions.md` held 738 `[provisional]` lines at the start and holds
816. 79 entries are new. The run reworded 74 older ones to match the code.
Among them, the entries on the native back end are now in the past tense. The new ones, by section:

- Core language: a Windows file name as UTF-8, a lone surrogate as three
  bytes.
- Object model: `Object.deserialize` gives `none` when its allocator does.
- Libraries and runtime: `.installed/<target>` of the sysroots, no sysroot
  under `ANTIC_SYSTEM_COMPILER`, and the flags of the native libraries.
- Standard library phase: the kept text of a `SystemError`, and the file a
  `FileSink` shares with its copies.
- `anti.mem`: `dup` of an `ArenaAllocator`.
- Plugins: an empty directory in `plugins`, an object of a library alive
  from its lookup, and a second `plugin.load` of an open library.
- Versions: the table of stubs of a sub-object, and the bounds of the image
  of a plugin.
- Errors, warnings and checks: the three forms of a message of antic.
- The build command: redirects, a path dependency of library files alone,
  the key of a library file, and decision 15 for a dev object.
- The bind command: a trailing `_` on a word of Anti, a header refused as a
  whole, and the fixed base of a C23 enum.
- Regular expressions: a `Regex` without a pattern stops the program.
- Repository layout: the seam of fault injection, where a fault test
  stands, and the rows of `docs/notes/source-tree.md`.
- Files of the checker: twenty files with `sema_checker.h`, `sema_closure.c`,
  `sema_declare.c`, and the prefix of the exported functions.
- Files of the LLVM text: four files with `llvm_emit_parts.h`.
- Generics and collections: `reserve` and `shrink` as a change, the place
  functions on an empty place, where the memory checks go in, the shared
  messages of `NotOne` and `Changed`, `dup` of a `Snapshot<T>`, and room
  past an `int`.
- Rules of 2026-10-13: the lock file records what each package needs.
- What the solid run decided, 42 entries:
  - Ownership: `alloc(T, n)` through `anti_rt_alloc_aligned`, `yield v`
    in a `catch`, the arguments of `dispatch` and `parallel`, `dup` over
    a replaced `copy`, and `=` to an `own` field.
  - R074 on a channel, R083, R017, R054, R066 and R100, each as the code
    builds the answer.
  - Errors: the handler of a call statement, `?U` where `??U` stands, and
    the call of an arm of `switch` or `select`.
  - Constants: `n as E`, a constant index into an array of `size_of`, an
    enum value from `size_of`, and the flags form of T180.
  - Threads: the operands of the atomic operations and a second `join` in
    a block. What an atomic field holds, `packed` beside an atomic or a
    Mutex, and the mark of a joined job.
  - Values: a closure in an `if let` block, the module that writes
    `count`, `v is Maybe<int>.Some`, and the two checks of `a[lo..hi]`.
  - Plugins: `unload` counts heap objects, `delete` and the count of a
    library, and a host keeps every function of its modules.
  - Libraries and headers: `#pragma pack` of a packed class and `export
    fn` of a static musl program. Constants and statics in the library
    file, and a field default that names a function.
  - The library: `args.Parser` and its three codes, which are public items
    under the freeze, and the unguarded fields of `anti.log.Logger`.
  - Tooling: the unit group `contract`, `host=` with more than one system,
    `text output windows` in the guide, and the two kinds of find of the
    fuzzer.
  - Grammar: the condition of `do { } while` ends at a line break, T182.

## Questions that wait for Eddie

The ten cases of the rules and R081 wait no more. "Answers of 2026-10-18"
holds the answers, and the code builds them. The run raised these:

1. `CLAUDE.md` still says the ten cases wait and that T127 holds R081, and
   its State gives 4947 tests. `answers-record`, `answers-owned` and
   `answers-language` left it, since their tasks did not name it.
2. `answers-record`: the rule of `CLAUDE.md` puts a decision that changes
   the language into the specifications and the overview in one commit,
   and the record went into `docs/decisions.md` alone.
3. Step 8: the entry on `anti.log` reads the logger file before `main` with
   `[module]` thresholds. The code reads it at the first line written.
4. Step 9: a setter under a lock for the fields of `Logger`, or the fields as
   the `[provisional]` entry keeps them.
5. Step 22: a repository whose index holds the package without a version
   that answers now passes to the next repository. Refusing is the other
   reading.
6. Step 23: `-mno-outline-atomics` and frame records for the Linux native
   libraries, a choice of the build.
7. Step 33, anti: should the work directories of `anti check`, `anti doc`
   and `anti test` follow `[layout]`.
8. Step 33, build: `ASAN_OPTIONS` and `-g` of the `ubsan` presets, `cl
   main.c` in the link line of a Windows library, and `ANTI_DEPS_DIR` for
   the cache paths.
9. Step 33, tests: one kind per directory for `tests/dump/` and
   `tests/unit/`, and `matrix_values_aggregates_dev` at 34 s of its 60
   under UBSan.
10. Step 33, std: `patch_bytes` into `anti.text`, so a patch by bytes links
    no PCRE2.
11. `faults`: `ANTIC_FAULTS` is on in the presets `host`, `asan`, `ubsan`
    and `coverage`, a choice of the build.
12. `rt-coverage`: a profile pattern per compile replaced
    `LLVM_PROFILE_FILE`, and with it a DESIGN comment.
13. `rt-tests`, session 3: should a generic copy refuse `<<%` by a count
    below zero, as a constant does.
14. `rt-tests`, session 7: the message of `anti bind --clang` for a clang
    whose version it cannot read, today `clang is clang -1`.
15. `rt-tests`, session 15: what `file:///C:/repo` means on macOS and Linux.
16. `probe`: decision 27 against the overview and the object model on
    compound assignments.
17. `probe`: no form moves an object out of one `own` field into another.
18. `probe`: a simd struct does not meet `add`.
19. `probe`: a call does not infer `T` from a derived class.
20. `probe`: the bare name of a `const` of a class body inside the body.

## Not run on the target

The VMs are out of service since 2026-10-18. About 64 lines in 30 reports
since then name a change that only Linux or Windows runs, each under its
own heading "Not run on the target". T136 and T138 show on Windows alone.

## Left for later

- The 38 open findings above.
- `rt-tests` ended its twentieth session with CONTINUE. Its list of regions
  without a reason stands in `docs/reports/2026-10-22-solid-rt-tests-20.md`.

## Gates of this step

The step adds this report alone, so `CLAUDE.md` asks for the docs-style
checker and no suite. The suites ran to give the counts:

- Host, 6362 tests, `ctest --preset host -j14` as `-I 1,2100`, `2101,4200`
  and `4201,6362`: 2100, 2127 and 2169 passed in 293 s, 150 s and 275 s,
  `host-1.log` to `host-3.log`. The later parts run fixtures again. The 112
  expected failures fail as listed, and the two `mimalloc_environment` tests
  skip. The build of `build/host` has no warning, `host-build.log`.
- Coverage, the five parts above: 1272, 1287, 1301, 1284 and 1264 passed.
- ASan and UBSan ran at `797f4be6` in `probe-fix`, 6360 tests each, all
  passing. The code is the same at `26585396`.

The state before the commit of this report:

```text
$ git log --oneline -3
26585396 Report the step probe-fix
797f4be6 Record the commits of T183 to T188
0fcb621c Write the digests of generic_variants again after the fix of T183
$ git status --short
$ git rev-parse HEAD origin/main
2658539648b0788b4ee422c3e678cf750d899fbd
2658539648b0788b4ee422c3e678cf750d899fbd
```
