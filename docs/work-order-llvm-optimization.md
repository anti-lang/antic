# Optimization facts in the LLVM text

Design and work order for giving LLVM every fact it can optimize with. The
reader is the session that implements a step. Eddie decided on 2026-10-05
that antic uses every optimization LLVM offers. The checklist is
[Performance Tips for Frontend Authors](https://llvm.org/docs/Frontend/PerformanceTips.html),
read against `src/antic/llvm_emit.c` on 2026-10-05.

This work order revises the provisional entry of `docs/decisions.md` that
leaves `noreturn cold` and the parameter attributes out of the LLVM text.
The revision stands under [Revision of decision 910](#revision-of-decision-910).

Eddie decided the open questions on 2026-10-05. They stand under
[Decisions](#decisions). One rule decides every choice between options:
the faster generated program wins, with three conditions. A choice that
no measurement settles yet goes to the step that measures it. One item
still needs Eddie's separate answer: shipping the runtime as bitcode.

Contents:

- [Why](#why)
- [How a fact is measured](#how-a-fact-is-measured)
- [Revision of decision 910](#revision-of-decision-910)
- [The checklist against antic](#the-checklist-against-antic)
- [Facts that end a path](#facts-that-end-a-path)
- [Memory effects of runtime functions](#memory-effects-of-runtime-functions)
- [Tables and dispatch](#tables-and-dispatch)
- [Parameters and results](#parameters-and-results)
- [Arithmetic, addresses and ranges](#arithmetic-addresses-and-ranges)
- [Decisions](#decisions)
- [Work order](#work-order)

## Why

`tests/bench/map_work` runs 1.46 to 1.50 times as long as its C twin on
macos-arm64. Its IR after `opt -passes='default<O2>'` shows the cause.

Every failure path of `HashMap.get`, `HashMap.set`, `HashMap.remove` and
`SlotTable.slot_of` reads like this:

```llvm
b1.i:
  %v0.i.i = call dereferenceable_or_null(120) ptr @malloc(i64 120)
  ; 25 lines that build an anti.lang.NoneDereference
  call void @anti_rt_hook(ptr nonnull %v0.i.i, i64 0)
  %v13.i = load ptr, ptr %v0.i.i, align 8
  %v15.i3 = getelementptr i8, ptr %v13.i, i64 168
  %v17.i4 = load ptr, ptr %v15.i3, align 8
  call void %v17.i4(ptr nonnull %v0.i.i) #9
  br label %"anti.collection.map.SlotTable$3c$28int$2c$20int$29$3e.entry.exit"
```

Slot 168 is `Error.fatal`, which always ends the program. LLVM cannot see
that, for three reasons:

- `anti_rt_exit` and `anti_rt_out_of_memory` are declared with `nounwind`
  alone. The C side marks `anti_rt_out_of_memory` `_Noreturn` and leaves
  `anti_rt_exit` unmarked.
- The call to `fatal` goes through the table. `anti_rt_hook` sits between
  the store of the table pointer and its load, so LLVM cannot fold the
  load and call `fatal` directly.
- The emitter writes no `unreachable`. The block branches back to the
  normal path after the call.

The failure block therefore belongs to every loop that calls these
methods. Three costs follow from that:

- The block adds about 35 instructions to each method. The inliner of
  `default<O2>` then keeps `get`, `set`, `remove` and `slot_of` out of
  `main`.
- The block calls functions that may write any memory. LLVM reloads the
  hash array, the entry array, the capacity and the seed of the map on
  every lookup. It recomputes `mix(seed)` on every lookup as well.
- Memory traffic stays. With every method inlined by `opt-inline`,
  Anti's `main` holds 57 loads and 89 stores. Clang's `main` holds 18
  loads and 6 stores. Most of the 89 stores build `NoneDereference`
  objects on the failure paths.

C ends the same path with `exit(1)`, which LLVM knows never returns.
Clang inlines every function of the twin into `main`.

The C twin is a fair reference. Its seed is a constant, which clang
folds. A twin that reads the seed at run time runs 1.8 ms slower, and
clang hoists `mix(seed)` out of its loops.

## How a fact is measured

`antic --opt <path>` runs a given program in place of the pinned `opt`.
A wrapper script edits the LLVM text and then runs the pinned `opt` on
it. Each candidate fact is measured this way before a step implements it.
No change to antic is needed for a measurement.

The wrappers of 2026-10-05:

| Wrapper | Edit of the text |
|---|---|
| `opt-inline` | adds `-inline-threshold=2000` |
| `opt_hash.py` | replaces `declare i64 @anti_rt_hash_seeded` with an internal definition of `mix(hash ^ mix(with))` |
| `opt_m1.sh` | replaces `"target-cpu"="generic"` with `"target-cpu"="apple-m1"` |
| `opt_noreturn.py` | adds `noreturn cold` to `anti_rt_exit` and `anti_rt_out_of_memory`, and writes `unreachable` after each of the 21 calls of `fatal` |

Each program ran 15 times. The runs of all builds alternate, and each
table gives the median. Every build printed `150009000000`, the result of
the C twin.

Hooks and checks, run A:

| Build | Median | Anti / C | To baseline |
|---|---:|---:|---:|
| baseline | 197.6 ms | 1.46 | 1.00 |
| `--no-hooks` | 198.2 ms | 1.47 | 1.00 |
| `--no-checks` | 198.1 ms | 1.47 | 1.00 |
| `opt-inline` | 166.2 ms | 1.23 | 0.84 |
| `opt-inline` and `--no-hooks` | 165.5 ms | 1.22 | 0.84 |
| C twin | 135.1 ms | 1.00 | 0.68 |

The hash call, run B:

| Build | Median | Anti / C | To baseline |
|---|---:|---:|---:|
| baseline | 196.1 ms | 1.47 | 1.00 |
| `opt_hash.py` | 196.2 ms | 1.47 | 1.00 |
| `opt-inline` | 165.3 ms | 1.24 | 0.84 |
| `opt-inline` and `opt_hash.py` | 160.4 ms | 1.21 | 0.82 |
| C twin | 133.1 ms | 1.00 | 0.68 |

The CPU and the seed, run C:

| Build | Median | Anti / C | To baseline |
|---|---:|---:|---:|
| baseline | 193.9 ms | 1.49 | 1.00 |
| `opt_m1.sh` | 196.4 ms | 1.51 | 1.01 |
| `opt-inline` | 162.5 ms | 1.25 | 0.84 |
| `opt-inline` and `opt_m1.sh` | 157.7 ms | 1.21 | 0.81 |
| C twin | 130.3 ms | 1.00 | 0.67 |
| C twin with the seed read at run time | 132.1 ms | 1.01 | 0.68 |

The end of the failure path, run D:

| Build | Median | Anti / C | To baseline |
|---|---:|---:|---:|
| baseline | 193.8 ms | 1.50 | 1.00 |
| `opt_noreturn.py` | 176.3 ms | 1.36 | 0.91 |
| `opt_noreturn.py` with the hash definition | 174.5 ms | 1.35 | 0.90 |
| `opt-inline` | 161.7 ms | 1.25 | 0.83 |
| `opt-inline` and `opt_hash.py` | 156.8 ms | 1.21 | 0.81 |
| C twin | 129.2 ms | 1.00 | 0.67 |

Size and compile time:

| Build | Object | Executable | Release compile |
|---|---:|---:|---:|
| baseline | 115,624 B | 229,968 B | 299.7 to 306.5 ms |
| `opt-inline` | 149,856 B | 262,416 B | 454.9 ms |
| `opt_noreturn.py` | 109,904 B | not recorded | 316.5 ms |

The compile time of `opt_noreturn.py` includes the start of Python. The
baseline does not.

`opt_noreturn.py` at the default threshold inlines `get`, `remove` and
`slot_of` into `main`. Its `main` holds 43 loads and 6 stores. With the
hash definition as well, it holds 22 loads and 6 stores. `HashMap.set`
stays a call in every Anti build. Clang inlines `set` and `resize` of the
twin. That call is the largest cost not yet measured.

## Revision of decision 910

The entry of `docs/decisions.md` reads:

> [provisional] The LLVM text leaves out `noreturn cold`, `nonnull`,
> `dereferenceable`, `noalias` and `signext` or `zeroext` on a result.
> Reason: the IR records no function that never returns, no Anti type of a
> parameter, no `own` parameter, no allocation function and no signedness
> of a result, and "Attributes and metadata" leaves out `readonly` and
> `nocapture` for the same lack.

The revised entry, which the step `noreturn` writes in its place:

> The IR records the facts that the LLVM text needs for every attribute
> of "Attributes and metadata" in `docs/work-order-llvm-back-end.md`, and
> the text writes each attribute. The facts are a function that never
> returns, the Anti type of a parameter, an `own` parameter, an allocation
> function and the signedness of a result. Eddie decided this on
> 2026-10-05, after `opt_noreturn.py` ran `map_work` in 0.91 of its time.
> `docs/work-order-llvm-optimization.md` adds memory effects, table facts
> and arithmetic facts.

The revision is no longer provisional. Each later step that writes a new
fact adds its own entry.

## The checklist against antic

The state of each item of the LLVM checklist on 2026-10-05:

| Item | State in antic | Where it goes |
|---|---|---|
| Data layout and triple | written | stays |
| Most private linkage | `internal` in a whole program | stays |
| Allocas in the entry block | one per temporary, in the entry block | stays |
| No aggregate values | `[2 x i64]` and `i128` only for ABI coercion | stays |
| Alignment on loads and stores | natural alignment | stays |
| `poison` over `undef` | `poison`. The emitter writes no `undef` | stays |
| `nounwind` | on every function and declaration | stays |
| Profile metadata on cold paths | on assertion and check failures | step `noreturn` adds the none guard |
| `noreturn` | never written | step `noreturn` |
| `unreachable` | never written | step `noreturn` |
| `readnone`, `readonly`, `argmemonly` on declarations | never written | step `effects` |
| `!invariant.load`, `!invariant.group` | never written | step `tables` |
| `nonnull`, `dereferenceable`, `noalias` | never written | step `params` |
| `noundef` | never written | step `params` |
| `nsw` and `nuw` | never written. Work order line 998 says Anti wraps | step `arith` |
| `inbounds` | never written on `ptradd`. DESIGN at `llvm_emit.c:739` | step `arith`, decision D5 |
| TBAA | none. Waits for the aliasing rules of `as` | steps `tbaa-rules` and `tbaa`, decision D6 |
| `!range` and `range()` | never written | step `arith` |
| Lifetime markers | never written | step `arith` |
| A pipeline for the language | `default<O2>` as it stands | step `config`, decision D8 |

Release builds already get `readonly` and `captures(none)` on internal
functions. The `function-attrs` pass of `opt` infers them, as the IR of
`HashMap.get` shows. The explicit attributes matter for declarations, for
dev-mode objects and for runtime functions in C.

## Facts that end a path

The IR gains three facts.

- `bool never_returns` in `struct ir_function` of `src/antic/ir.h`. It is
  true for a function that ends the program on every path.
- A column in `RT_FUNCTIONS` of `src/antic/rt_abi.h` with the same fact.
  It is true for `anti_rt_assert_failed`, `anti_rt_check_failed`,
  `anti_rt_cast_failed` and `anti_rt_out_of_memory`. The step reads every
  row against `src/rt/` and marks each function that calls `exit`,
  `abort` or a `_Noreturn` function on every path.
- A terminator `IR_UNREACHABLE` after the last enum value of the IR
  instructions. Lowering ends a block with it after a call of a function
  that never returns.

The emitter writes `noreturn cold` on the declaration of such a function.
It writes `unreachable` for `IR_UNREACHABLE`. `ir_verify.c` refuses an
instruction after `IR_UNREACHABLE` and a block that falls through a call
of a never-returning function.

The C side matches the IR. `anti_rt_exit` in `src/rt/io.c` and
`src/rt/std.h` becomes `_Noreturn`. The unit test `runtime_functions` of
`tests/unit/test_lower.c` already reads each row against the prototypes of
`src/rt/`. It gains a check that the column and `_Noreturn` agree.

`anti_rt_exit` is an `extern fn` in `src/std/anti/lang.anti:20` and
`src/std/anti/io.anti:7`. An Anti declaration has no way to say that a
function never returns. Decision D1 adds the result type `never`:

```anti
extern fn anti_rt_exit(status: c_int) -> never;
```

The checker refuses a `never` function with a body whose end is
reachable. It refuses a `return` in such a function. A call of a `never`
function ends its path for the checker, as `return` does. Lowering sets
`never_returns` from the type. The type has no values, so no variable,
field or parameter takes it. `docs/anti-syntax-overview.md` and
`docs/anti-language-additions.md` gain the type.

The guard of `catch fatal` calls `fatal` through the table. A class may
replace `fatal` with `concrete fn`, and nothing in `src/` or `tests/`
does. Decision D2 makes it `final fn fatal(self) -> never` in
`src/std/anti/lang.anti`. Lowering then calls `anti.lang.Error.fatal`
directly, and the guard ends with `IR_UNREACHABLE`.

The block of a none guard gets the weights of a cold block, as the
failure arms of `IR_FAIL_ASSERT` and `IR_FAIL_CHECK` do today. A new value
`IR_FAIL_GUARD` of `enum ir_fail` marks it. The DESIGN comment above
`enum ir_fail` says that each build drops a failure arm under its own
switch. `IR_FAIL_GUARD` is never dropped. The step extends the comment.

## Memory effects of runtime functions

Each row of `RT_FUNCTIONS` gains the memory effects of the function, as
LLVM spells them:

| Class | LLVM attributes | Example |
|---|---|---|
| none | `memory(none)` | `anti_rt_hash_seeded` |
| reads its arguments | `memory(argmem: read)` | `anti_rt_same_bytes`, `anti_rt_compare_bytes`, `anti_rt_hash_bytes` |
| writes its arguments and allocates | `memory(argmem: readwrite, inaccessiblemem: readwrite)` | `anti_rt_builder_append`, which grows its buffer |
| any | no attribute | `anti_rt_hook`, which runs the handler of `Trace` |

A function of the first three classes also gets `willreturn` when no
path of its body exits, and `nosync` and `nofree` where its body allows.

The step classifies every row by reading its C body in `src/rt/`. A
function that reads a global, takes a lock or calls a handler is `any`.
A unit test checks the class of each row against a table written from
the C bodies, as `runtime_functions` checks the types.

`anti.lang.seeded` in `src/std/anti/lang.anti` calls
`anti_rt_hash_seeded`, which computes `mix(hash ^ mix(with))`. The step
moves that computation into Anti, so the inliner sees it. Run B measured
4.9 ms with `opt-inline`. Run D measured 1.8 ms without it.

## Tables and dispatch

The table pointer of an object is written once, when the object is made.
In a whole program every table global is an `internal constant` in the
text already. Two
facts follow from that:

- Each store of the table pointer at construction and each load of it for
  dispatch carries `!invariant.group !N`. LLVM then forwards the stored
  table to the load across `anti_rt_hook`. Clang does the same under
  `-fstrict-vtable-pointers`.
- Each load of a slot from a table carries `!invariant.load`.

Together they let LLVM call `Error.fatal` directly in the guard of
run D. The same holds for any call through the table of an object whose
class LLVM sees.

The step lists every store of a table pointer in lowering and in
`src/rt/`. A store other than the one at construction needs
`llvm.launder.invariant.group` on the pointer after it. The report names
each store found.

## Parameters and results

The attributes of "Attributes and metadata" in
`docs/work-order-llvm-back-end.md`, with the facts the IR gains:

- `struct ir_param` gains `nonnull`, `deref_size` and `own`. Lowering
  fills them from the Anti type of the parameter.
- `struct ir_function` gains `result_ext` and `allocates`.
  `allocates` is true for `anti_rt_mem_alloc`, `anti_rt_copy_buffer` and
  `anti_rt_dup`.
- The emitter writes `nonnull dereferenceable(N)`, `noalias`, `signext`
  or `zeroext`, and `noundef`.

`noundef` goes on a parameter or result of scalar type. It does not go on
a coerced aggregate whose layout holds padding, since the padding bytes
are undefined. Clang follows the same rule.

## Arithmetic, addresses and ranges

Anti wraps on overflow in release mode, so a plain `add` carries no
`nsw`. Two cases rule out the wrap by the rules of the language:

- In a checked build, the operation on the path where `IR_BRANCH_OV`
  found no overflow. That path carries `nsw`.
- The step of a `for i in a..b` loop, where `i < b` holds before the
  step. It carries `nsw`, and `nuw` when `a` is at least 0.

`opt` already derives `nuw nsw` on the counters of `map_work`. The step
measures whether the explicit flags change any program of `tests/bench`.

`!range` goes on a load of a `bool` as `!{i8 0, i8 2}`, and on a load of
an enum tag with the range of its cases. `range()` goes on a result of
the same types.

`getelementptr inbounds` goes on the address of a field of an object
reached through a non-optional reference. The checker guarantees that
such an object exists and holds the field. `ptradd` keeps its form for
every other offset, as the DESIGN comment at `src/antic/llvm_emit.c:739`
says. The step extends that comment with the exception. A view through
`as` and an address computed from an integer never get `inbounds`.

`llvm.lifetime.start` and `llvm.lifetime.end` go on the `alloca` of each
`IR_SLOT` whose block of scope lowering knows. They let llc share stack
slots between locals that never live at the same time.

## Decisions

Eddie decided on 2026-10-05. The option that makes the generated program
faster wins. Speed is the run time of the programs of `tests/bench`,
measured as [How a fact is measured](#how-a-fact-is-measured) describes.
Three conditions bound the rule.

### Conditions

C1, ties. Two options whose medians lie within 2 percent of each other
on every program count as equal. The option with the shorter release
compile and the smaller object then wins. Every measurement also covers
`tests/bench/ablate/mixed_work.anti`, which the step `ablate` writes. It
imports at least six modules of `src/std/anti/` and does work in each.
Its object is at least five times the object of `map_work`. It shows the
growth of code that five small programs cannot show. No Anti program in
the repository is that large today. The largest file,
`src/std/anti/collection/sorted.anti`, has 1,678 lines.

C2, correctness first. A fact that LLVM cannot check goes into the text
only where the rules of the language guarantee it. This covers
`inbounds`, TBAA, `noalias`, `nonnull`, `dereferenceable`, `noundef`,
`!range`, `!invariant.group` and `nsw`. A wrong fact gives a wrong
program, and no test of speed shows it. Each fact comes with a program
test in release mode. The test runs the case at the edge of the
guarantee and checks its output. For `inbounds` that is a view through
`as` beside a field access. For `!invariant.group` it is a call through
the table after a hook that writes a field.

C3, opt-in and distribution. Profile-guided optimisation needs a run of
the user's program. It is an option a build asks for, never a default.
Shipping the runtime as bitcode changes what a distribution carries. That
change waits for Eddie's separate yes. The step `runtime-lto` measures it
before he answers.

### Choices

D1, a never-returning function. The result type `never`, as
[Facts that end a path](#facts-that-end-a-path) describes. It covers the
C functions a program declares itself, such as `exit` and `abort`.

D2, `catch fatal`. `final fn fatal(self) -> never` in
`src/std/anti/lang.anti`. Lowering calls it directly.

D3, the inline threshold of `opt` in `src/antic/llvm_run.c`. The step
`config` measures 225, 500, 1000 and 2000 and keeps the fastest under
C1. At 2000 on 2026-10-05, `map_work` ran in 0.84 of its time. Its object
grew from 115,624 B to 149,856 B and its release compile from 306.5 ms to
454.9 ms.

D4, tuning for a CPU. A `"tune-cpu"` attribute beside `"target-cpu"`. It
changes the scheduling model and keeps the instructions of the CPU level.
On macos-arm64 the step `config` measures `generic` and `apple-m1` and
keeps the faster under C1. Every Mac of that target has Apple silicon.
The other five targets keep `generic`. The machines their programs run on
are unknown. anti-linux, the reference machine of linux-arm64, is a VM on
Apple silicon and measures no other CPU.

D5, `inbounds` on the address of a field, under C2, as
[Arithmetic, addresses and ranges](#arithmetic-addresses-and-ranges)
describes.

D6, TBAA, under C2. The step `tbaa-rules` writes the aliasing rules of
`as` into `docs/anti-language-additions.md`. Eddie accepts them before
the step `tbaa` writes any `!tbaa` metadata.

D7, the runtime and profiles. The step `runtime-lto` builds the runtime
as bitcode and links it with the program through the LTO of lld. It
measures full LTO and ThinLTO. The default build changes only after
Eddie's yes under C3. The step `pgo` adds `--profile-generate` and
`--profile-use <file>`. It needs `llvm-profdata` in the pinned tools,
which `build/host/runtime/bin` lacks today.

D8, the pass pipeline. The step `config` measures `default<O2>`,
`default<O3>`, and `default<O2>` followed by `licm`,
`simple-loop-unswitch` and `irce`. It keeps the fastest under C1. The
step checks each pipeline string with `opt -passes=<string> -verify-each`
on the text of every program of `tests/bench`.

No test pins a value that a measurement chose. A later measurement may
choose another.

## Work order

The steps run one fresh headless session each, as the steps of
`drive-llvm.sh` do. A driver of the same shape runs them.

Every step writes the failing test first. Each step ends with the three
suites passing on the Mac, a push, and a report
`docs/reports/<date>-llvm-opt-<id>.md`. The report holds the table of
`tests/bench` and of `mixed_work` before and after, measured as
[How a fact is measured](#how-a-fact-is-measured) describes. A step that
changes `emit_identity` or `link_identity_<target>` re-pins them, with
the old and the new values in the report. A step that cannot finish
writes `BLOCKED:` with the reason as its last line.

The twelve steps, by id:

`ablate`, the measurement. The four wrappers go into `tests/bench/ablate/`
with `mixed_work.anti` of C1. A script there builds a program with each
wrapper, runs the builds alternately 15 times each and prints the
medians. Done when it reproduces the tables of run A to run D within
3 percent.

`noreturn`, the end of a path. The type `never` in the parser, the
checker, lowering and both language documents. The facts of
[Facts that end a path](#facts-that-end-a-path), `final fn fatal`,
`_Noreturn` on `anti_rt_exit`, and the revised entry of decision 910 in
`docs/decisions.md`. Tests:

- checker tests in `tests/errors` for a `never` function whose end is
  reachable, for a `return` in it, and for a variable of type `never`
- a `.ll` golden of `tests/dump` that holds `unreachable` after `fatal`
- a unit test of `ir_verify.c` for a block that falls through
- a program test where `catch fatal` on `none` prints the error and
  exits with 1

Done when the tests pass and `map_work` runs in at most 0.92 of its
baseline time.

`effects`, the memory effects. The column of
[Memory effects of runtime functions](#memory-effects-of-runtime-functions)
and `anti.lang.seeded` in Anti. Done when the unit test of the classes
passes and each program of `tests/bench` prints its old line.

`tables`, the table facts of [Tables and dispatch](#tables-and-dispatch),
under C2. Done when the program test of C2 passes. A call through the
table of an object of a known class must then be a direct call after
`opt`.

`params`, the facts of [Parameters and results](#parameters-and-results),
under C2. Done when `llvm_verify` accepts every program and the `.ll`
goldens show each attribute once.

`arith`, the facts of
[Arithmetic, addresses and ranges](#arithmetic-addresses-and-ranges),
under C2. Done when the program tests of C2 pass and the goldens show
each flag and each range.

`tbaa-rules`, the aliasing rules of `as`, written into
`docs/anti-language-additions.md`. The report lists each rule with the
programs of `tests/programs` that rely on it. The driver stops after this
step until Eddie accepts the rules.

`tbaa`, the `!tbaa` metadata from the accepted rules, under C2. Done when
the program tests of C2 pass and `tests/bench` is measured.

`runtime-lto`, the runtime as bitcode, under C3. The runtime archive
gains bitcode objects built by the pinned clang. A build links them with
the program through lld. The step measures full LTO and ThinLTO against
the default. The default stays until Eddie's yes. Done when both modes
pass the suite behind an option and the report holds the measurement.

`config`, the build configuration of D3, D4 and D8, under C1. It runs
after every step that changes the text, so it measures the final text.
Done when the report holds the measurement of each option and the
choice.

`pgo`, `--profile-generate` and `--profile-use <file>`, under C3. The
pins of `llvm-profdata` follow the step `pins` of
`docs/work-order-llvm-back-end.md`. Done when a program built with a
profile of its own run passes its test, and the report measures it
against the default on `tests/bench`.

`measure`, the numbers. The table of `tests/bench` on macos-arm64 and on
linux-arm64 of anti-linux, as the step `measure` of
`docs/work-order-llvm-back-end.md` records it. The report compares each
program with `docs/reports/2026-10-04-llvm-measure.md`. Numbers only, no
pass or fail.
