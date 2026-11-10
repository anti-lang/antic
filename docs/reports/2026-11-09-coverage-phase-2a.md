# The third answers of 2026-11-09 and the first batch of coverage

Eddie accepted the three recommendations of
`docs/reports/2026-11-09-coverage-phase-1.md` on 2026-11-09. This step
records them and builds them. It then sorts every region that the coverage
report shows unreached in `src/antic/llvm_emit_function.c`,
`src/antic/sema_index.c` and `src/rt/trace.c`. Eddie's direction is the
Mac alone, so the VMs were not touched.

## Commits

- `2a7673c7` records the answers as "Answers of 2026-11-09, third set" in
  `docs/decisions.md`, with a line in the entry of the feature freeze.
- `e9a7e690` removes the three functions without a caller.
- `6ff1b1f4` tests the stub of the loader of plugins.
- `a739eaa2` names the tool that runs when memory runs out in `alloc.c`.
- `392f366c` tests the forms of `llvm_emit_function.c`.
- `26dc2018` tests the values that the check of an index follows.
- `842f0a0d` tests the paths of `trace.c` that the Mac reaches.
- This report, with the times and the counts in `CLAUDE.md`.

## The answers

- Q1. `diagnostics_check`, `lower_calloc_function` and
  `anti_rt_fault_fflush` with `ANTI_FFLUSH` are gone, with their
  declarations. The row of `diagnostics_check` left `TEXTS_ANTIC` of the
  guard of the diagnostics, and `docs/notes/test-matrix.md` names the
  three functions that remain. No list of the seam named the others. The
  first wording of the entry named each function with its file. The test
  `note_claims` refuses such a claim once the name left its file, so the
  suite of the removal failed there, and the entry now names the parts of
  antic instead. `note_claims` then passed alone.
- Q2. `unit_plugin_stub` runs `tests/unit/test_plugin_stub.c`. It loads
  `plugin_bad` against an interface of one function more, in a host that
  calls through reflection, and calls slot 1 of the sub-object. The stub
  stops the program with ``anti: `Impl` 1.0.0 carries no such function
  of `host.Service` ``. Three more cases call the slot with no object,
  with an object without a table and with a table the loader did not
  write, and get the second message. With reflection off the loader wrote
  no table of stubs, and the test failed for that reason first.
- Q3. `platform_tool` of the platform layer gives the name that
  `platform_set_tool` set, and both messages of `alloc.c` take it. Two
  cases of `tests/anti-build/messages.cmake` failed first with `antic:`.
  `fault_out_of_memory_in_the_manifest_reader` refuses the fourth
  allocation and gives `anti: out of memory for 80 bytes`.
  `fault_out_of_memory_in_the_code_of_antic` refuses the tenth and gives
  `anti: out of memory`. The report before gave the eleventh allocation
  for the reader of the manifest, in another directory. antic still says
  `antic:` in `unit_alloc_oom` and `antic_message_fault_out_of_memory`.

## llvm_emit_function.c

Tested in `unit_llvm_emit`:

- an operation of an aggregate type in each kind of operation, and a copy
- an operand of kind none or block in each kind that reads one, and a
  second failure after the first
- the lane operation and the fold that vbinary and vreduce lack
- a flag read and a branchov away from their operation, and a vselect
  without its second operand
- the lane shifts `ashr` and `lshr` of vbinary, which the lowering writes
  as checked lanes alone, and a fold of float lanes by an integer operation
- the temporaries of void, `c_long`, `c_wchar` and the word of a Mutex
  without `layout_resolve`

`error_frame_limit_arm64` gives the message of the frame for ARM64.

Unreachable, questions 1 and 2: `llvm_emit_frame_add` after a failure and
with the alignment 0, and the definition whose signature fails.

## sema_index.c

`tests/matrix/values_index_out_of_range_followed` holds 15 functions whose
indices the check proves in range or cannot follow. Four refusals give the
texts `` `s.len + 1` `` and `` `s.len - 2` `` and the bases without a name,
a literal `str` and an array field.

Unreachable, question 3. An index is an `int`, so the branches for the
other integer types in `fits`, `name_range` and `range_of` take no input.
Neither does a range that leaves `int`, nor a name without a symbol or an
operand without a type. A `try` before no call stays marked only with an
error, and an error stops the pass first. A body that is missing or that
the checker checks quietly reaches no line of the pass either.

## trace.c

Tested:

- `clib_forged_notice` links a library for C into three C programs, each
  with a notice `anti_licenses` of its own. The form antic writes gives
  `[1234]`. A notice without the begin marker or without the line feed of
  its build line gives `[]`. Windows reads the notice of a module that
  exports it, so the case skips there.
- `trace_outside` takes the raw frames of the addresses 0 and 16 from
  `anti_rt_trace_frame`.
- `std_faults_trace_fault_known_memory` refuses the eighth allocation, the
  record of the build id of the image.

Other host: `elf_id`, `symbolize_elf` and the ELF cases of `module_id` and
`anti_rt_trace_symbolize` run on Linux. `symbolize_pe` and the PE cases
run on Windows. A notice that the platform layer read, `notice_read`, is
set on Linux and Windows alone.

Unreachable on the Mac, question 4: an image without `__TEXT` in
`image_slide`, and one without `LC_SYMTAB` or `__LINKEDIT` in
`symbolize_macho`. Every image dyld maps has both.

Unreachable, question 5. Every line of `anti_rt_trace_text` is at most 60
bytes of formatted numbers, so `add_line` never fails. `add` then never
meets the freed text, and the three returns of NULL never run. The last
`return -1` of `module_number` follows a loop that returns for every frame
with a base.

No test showed a defect, so no finding.

## Coverage

| | Before | After |
|---|---|---|
| Regions | 97.73%, 1898 missed | 97.90%, 1755 missed |
| Functions | 99.78%, 10 missed | 99.87%, 6 missed |
| Lines | 97.88%, 1998 missed | 98.09%, 1806 missed |
| Branches | 90.38%, 5321 missed | 90.57%, 5215 missed |

| File | Lines before | Lines after | Branches before | Branches after |
|---|---|---|---|---|
| `llvm_emit_function.c` | 94.76%, 83 missed | 99.62%, 6 missed | 87.39% | 94.26% |
| `sema_index.c` | 87.23%, 93 missed | 96.70%, 24 missed | 74.81% | 81.67% |
| `trace.c` | 76.82%, 89 missed | 79.17%, 80 missed | 66.02% | 67.96% |

The three removed functions and `stub` account for the four functions
fewer. The brief gave 99 missed lines for `llvm_emit_function.c`. The
report on disk, written after the test of `unexpected`, gave 83. The
coverage suite ran 7007 tests, all passed, and `./c coverage` took 974 s.

## Suites

- The host suite ran before each code commit in the parts `-I 1,2100`,
  `-I 2101,4300` and `-I 4301,<last>`. The times stand in `CLAUDE.md`.
  Every test passed apart from the two skips of `mimalloc_environment`,
  and apart from `note_claims` before the commit of the removal, as Q1
  says.
- Both sanitizer suites ran after the last code commit in the six parts
  of `asan`, up to test 7017, and every test passed. `asan` took 250 s,
  187 s, 260 s, 185 s, 435 s and 456 s. `ubsan` took 254 s, 184 s,
  259 s, 200 s, 467 s and 465 s.
- A run of the unit tests of the coverage tree by hand, in the root of
  the checkout, wrote their scratch files there. They were removed before
  the next commit, and no commit holds one.

## Provisional lines

No new provisional line.

## Questions for Eddie

1. `llvm_emit_frame_add` returns at once after a failure and takes the
   alignment 0 as 1. Every caller stops at a failure first, and no layout
   gives the alignment 0. Keep both guards, or remove them?
2. `llvm_emit_definition` returns when `llvm_emit_signature` fails. A
   signature fails only for an aggregate value type, and an aggregate never
   passes as `ABI_DIRECT`. Keep the return, or remove it?
3. In `sema_index.c` the paths of question 3 above take no input. Keep
   them as guards, or remove them?
4. `image_slide` and `symbolize_macho` guard against a mapped Mach-O image
   without `__TEXT` or without a symbol table. Keep them?
5. `add_line`, the start of `add`, the three returns of NULL in
   `anti_rt_trace_text` and the last line of `module_number` take no
   input. Keep them as guards, or remove them?
