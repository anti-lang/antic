# Solid: the findings of the area ownership

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_ownership_`, in one session on 2026-10-14. Every
one is fixed apart from T127, which the feature freeze keeps as it is. The
logs lie in `build/drive/logs/solid-ownership/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T032 | severe | `c4e82b7c` | `alloc(T, n)` multiplied the count by the size in the program, so a product past 64 bits wrapped to a small block. |
| T033 | severe | `c4e82b7c` | LLVM removed a `malloc` of the program whose memory went to `free` alone and took its result as memory. |
| T034 | severe | `c4e82b7c` | AddressSanitizer ended the program at a `malloc` it could not serve instead of giving null. |
| T037 | severe | `31283be5` | `return` copied the bytes of a field, `*p` or an element that owns something, which stayed in its place as a second owner. |
| T038 | severe | `31283be5` | `yield` copied the bytes of a local, and a yielded `own` parameter was torn down by the function as well. |
| T039 | severe | `e47252c6` | Nothing tore down a fresh argument of a `dispatch` to a parameter that is not `own`. |
| T040 | severe | `e47252c6` | The checker noted no move of a local into an `own` parameter of a `dispatch`, so the caller tore it down after the worker. |
| T041 | severe | `92f57ef2` | `is`, a test for `none` and an index read a call result without keeping it to the end of the statement. |
| T042 | severe | `cc790582` | The out slot of a failing call was cleared by a load of the tag of a variant it did not hold yet, which a release build took as unreachable. |
| T012 | major | `2f3ae317` | The tree of a constant of a `?T` read the value of `T` as the fields of the struct of the `?T`. |
| T028 | major | `df50c128` | The checker read the defaults of fields before the types of functions, so a call found no type and no message, and `??` gave its right side in a constant. |
| T031 | major | `0e3773d0` | `dup` took no array, while `=` refused an owning array and named `dup`. |
| T036 | major | `dcb38e36` | The checker typed the result of `as?` as `*T`. |
| T035 | minor | `3cd16c49` | After the message that a `?T` may be `none`, the check of `dup` went on with its value and refused it as well. |
| T108 | minor | `b7aedb1f` | The refusal of a copy named the element of a tuple by the name of the compiler, `_0`. |

- T012 closes with the six tests of `matrix_values_const_optional` too,
  which share the cause.
- T028 closes `matrix_classes_call_as_the_default_of_a_field_refused`, and
  the same commit fixed T106 of the area values, which had the cause.

## Findings left

- T127, R081. The freeze of 2026-10-14 keeps it and its three tests.

## New findings

- T128, severe, fixed in `e47252c6`. A fresh argument of a `parallel` was
  never torn down, the cause of T039. `matrix_threads_parallel_of_a_fresh_owning_value`
  holds it.
- T129, severe, fixed in `cc790582`. The unset fields of a new object whose
  `construct` may fail were cleared by the same load of a tag as in T042.
  `matrix_errors_construct_fails_before_an_owning_variant_field` holds it.

## Tests corrected

- `matrix/ownership_return_of_an_element_copies.err` expected the column of
  the parenthesis of `(*keys)[0]`. antic places a grouped value at its first
  token inside, which four other tests of messages hold, so the expected
  column is 10.
- `matrix/ownership_none_or_of_none_at_module_level.err` expected line 7, an
  empty line. The constant stands on line 6.
- `programs/arith_facts.anti` read the elements of `alloc(Box, 2)` before
  it wrote them, and `alloc` writes nothing into a struct. It held zeros on
  the Mac alone once `alloc(T, n)` called the runtime. The program now
  zeroes the memory with `memset` (`bd87461c`).

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- Every `alloc(T, n)` calls `anti_rt_alloc_aligned`, which stays out of line
  under LTO and checks the count before any call.
- `yield v` gives the value of the `catch` as `let` gives a variable its
  value.
- The job of a `dispatch` holds its arguments as a call holds its own.
- The caller of a `parallel` tears down a fresh argument once every chunk
  has run.

`ANTI_RT_NOINLINE` in `src/rt/platform.h` and `allocator_may_return_null=1`
in the options of `--memory-checks` are no feature, option or public item.
The emit-identity manifest and the link-identity digest changed with the
fix of T032 to T034, and the manifest again with `arith_facts`.

## Counts and gates

`tests/known-failures.txt` held 387 lines, 374 of them tests, before the
step. It holds 321 lines, 308 of them tests, after it.

- The builds of `host`, `asan` and `ubsan` have no warning of our code.
- `host`: 4953 tests, every one passes, 308 of them as expected failures
  and 2 skipped (`mimalloc_environment`). Logs `gate-host-1.log` and
  `gate-host-2.log`.
- `asan` and `ubsan`: 4951 tests each in three parts, every one passes with
  the same 2 skipped. Logs `gate-asan-*.log` and `gate-ubsan-*.log`.
- The docs-style checker reports nothing on every `.md` file the step
  touched.
- `emit_identity` and `link_identity_macos-arm64` pass with the digests the
  fix of T032 to T034 and `arith_facts` wrote.

## The VMs fail on tests older than the step

The step changed `src/rt/platform.h` and `src/rt/mem.c`, so both VMs ran
the suite at `bd87461c`, the last commit of code.

- anti-linux: 4875 tests, 17 fail. Logs `vm-linux-1.log` to
  `vm-linux-4.log` and `vm-linux-arith.log`.
- anti-windows: 4853 tests, 42 fail. Logs `vm-win-1.log` to `vm-win-4.log`.

The base commit `5e559b35`, built on each VM, fails the same 17 and the
same 42 (`vm-linux-base-t.log`, `vm-win-base-t.log`). The step adds no
failure on either VM, and neither suite passes. The Linux ones are
`antic_option_bundle_static`, `guide_examples` (the program `checked`
of the guide stops with a segmentation fault) and 15 of `anti bind
--clang`. 14 of the 15 pass with the pinned clang first on `PATH`, which
the ssh session of the VM lacks, and
`anti_bind_message_clang_type_does_not_parse` fails either way. The
Windows ones are the tests of `anti test`, messages of `anti build`,
`anti symbols`, `anti check`, `anti license` and `anti bind`, options of
`anti check` and `anti bind`, `antic_option_bundle_static`,
`antic_option_output_program`, `antic_option_soname_linux`,
`anti_bind_huge_enum`, `variant_wide_tag` and `guide_examples`. The list
stands in `win-failed.txt`. Neither VM had run the suite since
2026-10-12.

A suite of a VM that the step needs is a gate, and these failures belong
to other areas. The step therefore stops before the push: its eleven
commits of code and this report stand on `main` of the Mac alone. Eddie
decides whether the step pushes with the VM failures recorded, or waits
for the steps that own them.

## State before this report

The step did not push, so `HEAD` and `origin/main` differ. The commands
gave, before the commit of this report:

```text
$ git log --oneline -3
1c8abd52 Record the commits of the findings the step fixed
bd87461c Zero the memory arith_facts reads as an enum of value 0
b7aedb1f Name an element of a tuple by its number in the refusal of a copy
$ git status --short
?? docs/reports/2026-10-14-solid-ownership.md
$ git rev-parse HEAD origin/main
1c8abd52dde31727c8c5a9c31741b1d8836e10cd
5e559b358e82ded3895b5c68b04cd62729b721ae
```
