# Errors antic finds at run time alone

The survey lists every kind of error that a program shows only while it
runs, at commit `4a05672b`. For each kind it says whether a check could
prove it in one function or in one program. It says how often the check
would refuse correct code, and what it would cost. It is a proposal for Eddie. The
step built nothing and changed no file outside this report.

## What the survey read

- The checks of a dev build in `src/antic/lower_check.c`,
  `lower_expr.c`, `lower_stmt.c`, `lower_worker.c` and `lower_simd.c`.
- Every stop of the runtime: `anti_rt_fail_abort` and
  `anti_rt_fail_exit` in `src/rt/`, and `fatal()` of `src/rt/sync.c`.
- The stops of the standard library: `no_element` of
  `src/std/anti/collection.anti`, `out_of_order` of
  `src/std/anti/collection/sorted.anti` and the stops of
  `src/std/anti/regex.anti`.
- The rows of `docs/audit/tests-findings.md` graded `severe`, the trap
  programs of `tests/traps/` and the reports of the test run, the solid
  run and the checks run.
- "Compile-time checks of 2026-10-20" in `docs/decisions.md`, so no
  proposal repeats E1, E2, C1, C2, C4, C5 or C6.

## The probes

Each probe is a small program under `build/checks-survey/`, built by
`build/host/antic` in dev mode, in release mode and with
`--memory-checks` where the table says so. The logs are
`build/drive/logs/checks-survey/probes1.log` to `probes4.log`. Every probe
below compiles without a message today.

| Probe | The mistake | What the run does |
|---|---|---|
| `p01_overflow_let` | `let a: i8 = 100; a + a` | dev: `overflow in +` |
| `p02_div_let_zero` | `let n = 0; 10 / n` | dev: `division by zero in /` |
| `p03_div_const_zero` | `x / 0` with `x` a parameter | dev: `division by zero in /` |
| `p04_shift_const` | `x << 64` with `x` a parameter | dev: `shift count out of range` |
| `p05_shift_let` | `let s = 64; x << s` | dev: `shift count out of range` |
| `q43_const_operands` | `x << -1`, `x % 0`, `x << 8` on an `i8` | compiles |
| `p06_narrow_let` | `let v = 300; v as u8` | dev: `value out of range for byte` |
| `q37_enum_let` | `let v = 5; v as Level` | dev: `value not declared by Level` |
| `p07_chan_zero` | `chan int(0)` | every mode: `a channel holds one value or more` |
| `p13_slice_const` | `xs[1..7]` of a `[3]int` | dev: `slice end out of bounds` |
| `p16_bitfield_let` | `let v: u8 = 13; x.a = v` into 3 bits | dev: `value out of range for the 3 bits` |
| `p18_chunks_let` | `let k = -3;` as the count after `by` of a `parallel` | dev: `count of chunks below zero` |
| `p21_mul_let` | `let big = 9223372036854775807; big * 2` | dev: `overflow in *` |
| `p27_negate_least` | `let m: i8 = -128; -m` | dev: `overflow in -` |
| `p10_free_twice` | `free(p); free(p);` | dev: status 133 in malloc, ASan: double-free |
| `p26_free_then_read` | `free(p); return p[0];` | dev and release: status 7, no message |
| `p14_leak` | `alloc(int, 4)` never freed | dev and release: nothing, LeakSanitizer: leak |
| `p28_text_leak` | `f"n = {n}"` never freed | dev and release: nothing, LeakSanitizer: leak |
| `p12_walk_change` | `l.push(n)` in `for n in l` | release: runs on, status 0 |
| `q35_free_local` | `free(&x)` of a local | dev: status 134, no message of Anti |
| `q36_delete_local` | `delete(&c)` of a local object | dev: status 134, no message of Anti |
| `p09_recursion` | `fn f(n) { return f(n + 1); }` | dev: SIGSEGV 139, release: waits for ever |
| `p22_join_loop` | `join(j)` in a `for` of two turns | dev: `join of a job joined already`, release: waits for ever |
| `q39_delete_before_join` | `delete(a)` before `join(j)` | dev and release: SIGSEGV 139 |
| `p19_unjoined` | a job that no path joins | ASan: heap-use-after-free in the worker |
| `p17_sync_self` | `sync self.m` calls `self.get()` that syncs `self.m` | dev: `takes a Mutex that its thread holds`, release: status 137 |
| `q42_sync_held_local` | `sync m` holds `sync p` with `p = &m` | as `p17_sync_self` |
| `p11_send_closed` | `close(c); send(c, 2);` | every mode: `send on a closed channel` |
| `q31_send_full` | two `send` on `chan int(1)`, no receiver | dev and release: waits for ever |
| `q32_recv_empty` | `recv` on an empty open channel, no sender | dev and release: waits for ever |
| `p23_div_param_checked_zero` | `if d == 0 { return x / d; }` | traps on every run that takes the branch |
| `p24_index_after_check` | `if xs.len == 0 { return xs[0]; }` | as `p23_div_param_checked_zero` |
| `q41_cast_alloc_square` | `alloc Square { }` read `as *Circle` | every mode: `cast failed` |

Status 137 of `p17_sync_self` and `q42_sync_held_local` is the
`os_unfair_lock` of macOS, which kills a thread that takes it twice. The
Mac alone ran the probes, since the VMs are out of service.

The constant rules R026 to R032 refuse an expression whose operands are
all constants. A constant operand beside an unknown one passes, as
`p03_div_const_zero`, `p04_shift_const` and `q43_const_operands` show.

## Every run-time error of today

The last column names the proposal of the next section that would catch
the kind, or says why none does.

| Kind | Where it stops | Proposal |
|---|---|---|
| Signed `+`, `-`, `*`, unary `-` and the carry forms out of range | `lower_binary_checks`, `lower_negate_checked`, `lower_carry` | P1 |
| Division or remainder by zero, the least value by -1 | `lower_binary_checks` | P1 |
| A shift count outside the width | `lower_binary_checks` | P1 |
| A narrowing `as`, to an integer, `char` or an enum | `lower_narrow_check` | P1 |
| A bitfield written past its width | `lower_bitfield_check` | P1 |
| The bounds of a part `a[lo..hi]` | `lower_slice_bounds_check` | P1 |
| The index of a simd `load` or `store` | `lower_simd.c` | P1 |
| A count of chunks below zero | `chunk_count` of `lower_worker.c` | P1 |
| A channel made for fewer than one value | `anti_rt_chan_new` | P1 |
| An index past the length | `lower_bounds_check` | C2, built. T196 is open |
| Use after `free`, a second `free` | `--memory-checks` alone | P2 |
| Memory the function made and never frees or hands on | `--memory-checks` alone | P3 |
| A collection changed while a `for` walks it | `anti_rt_walk_changed` | P4 |
| `free` or `delete` of the address of a local | the C library | P5 |
| A function that calls itself on every path | the stack | P6 |
| `join` of a job joined already | `check_not_joined` of `lower_worker.c` | P7, and decision 23 in one block |
| `delete` of an object a job still holds, a job never joined | `--memory-checks` alone | P7 |
| `sync` on a Mutex its thread holds | `held_already` of `src/rt/lock.c` | P8 |
| `send` on a closed channel | `anti_rt_chan_send` | P9 |
| `send` on a full channel or `recv` on an empty one that no thread fills | nothing, the program waits | P9 |
| A value that a condition fixed, as `d == 0` | the checks above | P10 |
| `p as *T` of an object of no class below `T` | `anti_rt_cast_failed` | P11 |
| A group, a template or `into` of a pattern held in a local | `anti_rt_regex_stop_*` | P12 |
| Locks in opposite orders | `src/rt/lock.c` | C4, built. Calls through a table stay at run time |
| The table of an object never built or torn down | `anti_rt_table_unset` | E2 and C1, built |
| A search over the match limit | `anti_rt_regex_stop_limit` | None: the input decides |
| A `Regex` field that holds no pattern | `src/rt/patterns.c` | None: the value comes from data, T166 |
| A place of a collection that holds no element, a capacity no `int` holds | `no_element`, `anti_rt_out_of_memory` | None: one function does not know the count |
| A lent element of a sorted collection moved out of its order | `out_of_order` | None: the order is `operator fn lt` of the program |
| A slot that a plugin lacks | `stub` of `src/rt/plugin.c` | None: the library is not in the program |
| Memory that runs out, a processor below the level, the runtime configuration, `[injections]` | `src/rt/` at start | None: the machine and the files decide |
| `catch fatal` on `none`, `e.fatal()`, `io.exit` | `Error.fatal` | None: the program ends on purpose |
| A failed `assert` | `anti_rt_assert_failed` | None: `assert(false)` marks a place no run reaches, 5 times in `tests/` and `src/std/` |
| Two places that hold one object, one of which frees it | `--memory-checks`, the versions of a `Pool` | None: outside the ownership checks by decision |

## The proposals, ordered by the errors they would catch

The order weighs the kinds a proposal covers against how often a program
writes the mistake. The trap programs, the findings and the guide show
that. The grades:

- Proof: one function, or one program through the calls it names.
- False refusals: none means the check refuses only a place that fails
  on every run that reaches it. Rare means a correct pattern exists, and
  an `unchecked` or an `allow` with a reason covers it.
- Cost: the walks of E1, E2, C1, C2 and C4 in `src/antic/sema_*.c` hold
  950 to 1330 lines each. Small is a rule inside one of them, under about
  300 lines with its tests. Medium is 300 to 800 lines, or a new record
  of the library file. Large is a new analysis of about 1000 lines or
  more.

### P1, the ranges of C2 at every check of dev mode

- Errors: the nine kinds of `lower_check.c`, `lower_simd.c`,
  `lower_worker.c` and `anti_rt_chan_new` in the table above. Probes
  `p01` to `p07`, `p13`, `p16`, `p18`, `p21`, `p27`, `q37` and `q43`.
- Proof: one function. `range_of` of `src/antic/sema_index.c` gives the
  value of a constant, of a local that never changes and of `.len`
  already. The check refuses an operation whose operand ranges make it
  fail on every run that reaches it.
- False refusals: none. A width or a length that differs between targets
  keeps its check of dev mode, as the `[provisional]` entry of C2 on
  `size_of` does.
- Cost: small per kind, medium for the nine.
- Recommendation: build it first, as one safety check with the words of
  each message of dev mode. It catches the most kinds for the least code.

### P2, `free` as a death of E2

- Errors: a read, an index and a second `free` after `free(p)` of memory
  of `alloc(T, n)`. `p26_free_then_read` returns 7 with no message in
  dev and in release, so the mistake shows nowhere without
  `--memory-checks`.
- Proof: one function, with the walk of `src/antic/sema_free.c`.
- False refusals: none, if `s.len` after `free(s.ptr)` stays allowed and
  a copy of the pointer in another local is not followed. The entry of E2
  that lets `free(p)` pass after `destroy` stays as it is.
- Cost: small.
- Recommendation: build it, under the name `use-after-free`.

### P3, the warning `unfreed-memory`

- Errors: memory that `alloc(T, n)`, `alloc C { }`, `f"..."` or
  `parallel` gave a local, which reaches the end of the function on a
  path where nothing freed it, returned it, stored it or passed it to a
  call. Probes `p14_leak` and `p28_text_leak`. The guide asks for
  `free(s.ptr)` after every `f"..."`, and `tests/` holds 1061 of them.
- Proof: one function, with the rules of C1 for a buffer that leaves.
  The path where `alloc` gave `none` holds no memory and passes.
- False refusals: rare. Memory that `main` keeps until the exit on
  purpose warns, and so does a path that two conditions on one flag
  make impossible.
- Cost: medium.
- Recommendation: build it as a warning, since a leak changes no output.
  A warning stops a release build, so `allow` with a reason covers the
  rare case.

### P4, a collection changed in its own walk

- Errors: a call that changes the collection a `for` walks, inside its
  body, on a path that reaches the next turn. `tests/traps/` holds 19
  programs of this stop, and `p12_walk_change` runs on in release.
- Proof: one function, and one record per function of a collection:
  whether it reaches `resized` of `src/std/anti/collection.anti`. The
  library file carries it, as it carries the first wait of C5.
- False refusals: none. A change followed by `break` or `return` on every
  path passes.
- Cost: medium.
- Recommendation: build it, as a safety check.

### P5, `free` or `delete` of the address of a local

- Errors: `free(&x)` and `delete(&c)`, probes `q35` and `q36`. The C
  library aborts, and Anti prints no line.
- Proof: one function. E1 knows every local reference already.
- False refusals: none. No correct program frees memory of the stack.
  `destroy(&c)` stays allowed.
- Cost: small.
- Recommendation: build it as an error of the checker, since no
  `unchecked` can make it correct.

### P6, a function that calls itself on every path

- Errors: `p09_recursion`. A dev build ends with SIGSEGV and no message,
  and a release build runs for ever, so the two modes differ.
- Proof: one function for a direct call, on the paths that C3 walks. A
  cycle through other functions needs the call graph C4 builds.
- False refusals: none for a direct call.
- Cost: small for direct calls, medium with the call graph.
- Recommendation: build the direct form, as a safety check.

### P7, the life of a job

- Errors: three kinds. A `join` in a loop of a job made before the loop,
  `p22_join_loop`, which a release build waits on for ever. A `delete` of
  the object of a job before its `join`, `q39`. A job that no path joins
  while its handle never leaves the function, `p19_unjoined`.
- Proof: one function. The walk of E2 makes a job dead at its `join`, by
  its rule for loops as well. The object of a `dispatch` is lent to the
  job until the `join`.
- False refusals: none for the first two. The third is rare and joins P3
  as a warning.
- Cost: small for the loop, medium for the other two.
- Recommendation: build the first two. Decision 23 then reaches a loop,
  which its check of the order of the text does not.

### P8, a Mutex its own thread takes again

- Errors: `p17_sync_self`, where `sync self.m` calls `self.get()` that
  takes `self.m`, and `q42_sync_held_local`, through `p = &m`.
- Proof: one program. C4 names a lock by its class and field and skips an
  edge from a lock to itself, since it cannot tell two objects apart.
  When the receiver of the call is `self` in both places, or a local
  reference of E1, the object is one.
- False refusals: none. A `Mutex` is not taken again by its thread, and
  the hidden lock of a synchronized class stays out.
- Cost: small to medium.
- Recommendation: build it, with the name `lock-order`.

### P9, a channel that one thread alone holds

- Errors: `send` after `close`, `p11_send_closed`, and a `send` to a full
  channel or a `recv` from an empty open one, `q31` and `q32`. The last
  two wait for ever in every mode and print nothing.
- Proof: one function, for a channel local that no call, `dispatch`,
  store or closure gets. The count of held values follows the constant
  capacity on paths without a loop.
- False refusals: none.
- Cost: medium, a new walk with counts.
- Recommendation: build it after P1 to P8. Programs that hold a channel in
  one thread are mostly tests.

### P10, facts of a condition

- Errors: `p23` and `p24`, an operation inside a branch whose condition
  fixes the value that makes it fail.
- Proof: one function.
- False refusals: none.
- Cost: large. Each branch adds a fact, a join merges them and an
  assignment ends them, which neither C2 nor P1 does.
- Recommendation: not now. Measure what P1 catches first.

### P11, a failed `as *T` of a known class

- Errors: `q41_cast_alloc_square`, an object that `alloc Square { }` made
  in the function, read `as *Circle`.
- Proof: one function, by the class of each `alloc` through the locals.
- False refusals: none.
- Cost: medium.
- Recommendation: no. `as?` exists for an unknown class, and a program
  that knows the class writes no cast.

### P12, a pattern held in a local

- Errors: a group, a template or `into` that a pattern of a local lacks,
  where the local holds a literal. `src/antic/sema_pattern.c` refuses
  each at a literal of the call already.
- Proof: one function, with the one value of a local of P1.
- False refusals: none.
- Cost: small.
- Recommendation: not now. Few programs hold a literal in a local before
  the search.

## The open findings of the checks built

T193 to T196 are gaps of E1, E2, C1 and C2 that the step of the matrix
recorded. They belong to the steps of their checks. Each of them catches
mistakes of the same kind as P2 and P5, at a smaller cost, so they come
before any proposal here.

## Questions for Eddie

1. Which of P1 to P12 to build, and in which order.
2. Whether P3 is a warning, as proposed, or a safety check.
3. Whether P5 is an error of the checker or a part of `escaping-local`.

## Gates

The step changed no code, so the suites ran on the tree of `4a05672b`.

- No warning: `build-host.log`, `build-asan.log` and `build-ubsan.log`.
- Host, `host-1.log` to `host-3.log`, parts `-I 1,2100`, `2101,4300` and
  `4301,6647`: 6647 tests by name, 6645 passed, the 11 expected failures
  among them, and 2 skipped, the `mimalloc_environment` tests.
- ASan and UBSan, `asan-1.log` to `-6.log` and `ubsan-1.log` to `-6.log`,
  parts `-I 1,1150`, `1151,2250`, `2251,4000`, `4001,5200`, `5201,6000`
  and `6001,7000`: 6645 tests by name each, 6643 passed, the same 11
  expected failures among them, and 2 skipped.
- `emit_identity` and `link_identity_macos-arm64` passed unchanged.
- `tests/known-failures.txt` held 13 entries before and after. No finding
  was recorded or closed, and no `[provisional]` entry was added.
- Not run on the target: nothing. The step changed no code.

Every log is under `build/drive/logs/checks-survey/`.

## State before the commit of the report

```text
$ git log --oneline -3
4a05672b Report the step of the checks in their contexts
0596137e Test the compile-time checks in every context of the matrix
5b0ff926 Record T196, an index in a closure that index-out-of-range never reads
$ git status --short
?? docs/reports/2026-10-24-checks-survey.md
$ git rev-parse HEAD origin/main
4a05672be58f19572cf630550d7464048b327cfb
4a05672be58f19572cf630550d7464048b327cfb
```

The commit of the report adds the report alone. The reply of the session
holds the state after its push.
