# The review of the compile-time checks

The step reviewed E1, E2 and C1 to C6 of "Compile-time checks of
2026-10-20" in `docs/decisions.md` without trusting the steps that built
them. The probes stand under `build/checks-review/probes/` and their
results in `build/drive/logs/checks-review/probes-*.log`.

## What the checks refuse

One row per clause: the test that refuses it and the test that lets its
neighbour pass. A name without a directory is a file of `tests/matrix/`.

| Clause | Refused by | Neighbour allowed by |
|---|---|---|
| E1 returned, a parameter, a part, a field, an index, a slice, a view | `ownership_reference_to_a_local_returned`, `..._to_a_parameter_returned`, `..._to_a_part_of_a_local_returned`, `..._through_a_field_access_returned`, `..._through_an_index_returned`, `ownership_slice_of_a_local_array_returned`, `..._through_a_view_returned` | `ownership_reference_to_a_local_where_it_stays` |
| E1 yielded | `ownership_reference_to_a_local_yielded` | `ownership_reference_to_a_local_yielded_or_kept_where_it_lives`, new |
| E1 a local of an outer block, a field, a global, an element, an out parameter | `..._stored_into_an_outer_local`, `..._into_a_field`, `..._into_a_global`, `..._into_an_element`, `..._stored_through_a_parameter` | `..._where_it_stays`, `ownership_alloc_and_free_kinds` |
| E1 a collection | `..._stored_into_a_collection` | `..._yielded_or_kept_where_it_lives`, new |
| E1 a channel | `threads_channel_of_pointers`, by its type | none needed |
| E1 `dispatch`, `parallel` | `..._passed_to_dispatch`, `..._passed_to_parallel` | `..._where_it_stays`, an argument of a call |
| E1 a closure returned, stored, at a `concurrent` parameter | `errors/closures_modules`, `errors/closures`, `..._captured_by_a_closure_at_a_concurrent_parameter` | `..._where_it_stays` |
| E2 `delete`, `destroy`, a move into an `own` parameter or place | `ownership_use_after_delete`, `..._destroy_of_a_pointer`, `..._a_move_of_a_pointer`, `..._a_store_into_an_own_field` | `ownership_use_after_free_allowed` |
| E2 a read, a field, a call, an index, a second `delete` or `destroy`, a move | `ownership_use_after_delete`, `ownership_call_after_delete`, `ownership_index_after_destroy`, `ownership_second_delete`, `ownership_second_destroy`, `ownership_move_after_delete` | `ownership_use_after_free_allowed` |
| E2 the join, a new value, the loop | `ownership_delete_in_one_branch_then_use`, `ownership_delete_inside_a_loop`, `..._after_a_new_value` | `ownership_use_after_free_allowed` |
| E2 an `own` field of `self` | `ownership_own_field_of_self_after_delete`, `ownership_own_field_moved_then_read` | `ownership_use_after_free_allowed`, which counts each teardown |
| C1 a constant index, a computed index, one path, a loop | `ownership_uninitialized_read_at_a_constant_index`, `..._at_a_computed_index`, `..._written_on_one_path`, `..._before_the_write_in_a_loop` | `ownership_uninitialized_read_allowed` |
| C2 a constant, a local, `for` and its step, a fixed array, a slice | `values_index_out_of_range_*`, seven refusals | `values_index_out_of_range_allowed`, `traps/bounds_unproved` |
| C3 the five kinds | `flow_paths_{result,may_fail,never,handler,let_else}_after_*`, 40 refusals | `flow_paths_that_end_nested`, `flow_exit_from_a_handler_in_a_loop_condition` |
| C4 `sync`, a synchronized function, calls, modules | `threads_lock_order_*`, four, `errors/modules_lock_order*`, four | `threads_lock_order_allowed`, `traps/lock_order_unfollowed` |
| C5 each wait, direct and through a call | `threads_blocking_*`, 20 refusals | `threads_blocking_outside_a_lock` |
| C6 the hash, `mul_high`, the runtime | `values_unused_result_*`, 16 refusals | `values_unused_result_of_calls_with_effects` |

`errors/<check>` holds the dev build and `unchecked` or `allow` of each
check, and the 88 `checks_*` files the eleven contexts.

## The probes

123 programs. 117 try to slip an error past a check where its rule says
it proves it, and six are neighbours that must compile: `e09`, a store
into a parameter by value, `p06`, `p07`, `p11`, `p12` and `n01`, which
became the new neighbour test. Every attempt at E2, C1, C2, C3, C4, C5
and C6 was refused: 23 of E2, 17 of C1, 22 of C2, 10 of C3, 9 of C4, 6
of C5 and 6 of C6. Of 24 attempts at E1, 7 passed. `e11` stores through
a pointer the function read, which the run time keeps by its entry, and
the other six are three findings.

- T197, `e03`, `e20`: `return q ?? &x;` and `p = q ?? &x;`. `root_of`
  follows no `EXPR_BINARY`.
- T198, `e01`, `e21`, `e22`: a store through `let q = h`, `let me = self`
  or `if let h = m` of a parameter. `anchor_of` sees a parameter alone
  when it names the place directly.
- T199, `e23`: `Some s => { return s.p; }`. `walk_arms` declares the
  binding with no reference, although `if let` and `let ... else` carry
  one.

Each finding is a row of `docs/audit/tests-findings.md`, and its tests
are listed in `tests/known-failures.txt`. They are left open for the
step of E1. `new-tests-v.log` shows each test failing as its row says.

## False positives

- T200, `p06`, now `flow_paths_do_while_body_that_returns`:
  `do { return k; } while k > 0` as the last statement is refused with
  "can reach its end", which no path does. The other five neighbours compile. R003 names a `switch` and a `while true` and no `do`.
  Recorded open for the step of C3.
- `param_facts` of `tests/programs/` reads a node after its store into an
  `own` field that keeps it alive. Its `unchecked` stands, and the
  question of the step of E2 to Eddie stands with it.
- The 118 `parallel` and `dispatch` places of E1. The question of the
  step of E1 stands.

## The `unchecked` and `allow` clauses left

166 lines name one of the seven checks. 95 `parallel` reasons hold, since
`parallel` waits for every chunk. All 23 `dispatch` places join their job
in the block of the local, with no `return`, `fail` or `try` between the
two. The 5 `lock-order` places of `traps/` and the guide take both orders
on purpose. The `allow` clauses of `threads_channel` and `threads_select`
hold, since the channels are local and never wait. In `threads_dispatch` the
reason said each worker ends "after one addition", while `keep` assigns,
and `f5ed662c` makes it "take no lock and wait for nothing".
`ownership_delete_of_a_channel_never_made` reads zeroed memory on purpose,
for R075. The clauses of `errors/` test the clause itself. The rest are
prose of the guide, the reports and comments.

## Left to the run time

The documented cases stay, and the probes confirm each: memory the
function allocated, a place behind a pointer the function read
(`e11`), the value of a call, the order of the text for a local that a
later `=` clears, a `defer` that ends a value, the arguments of a worker
(T193), a closure (T194 to T196) and two places that hold one object. A
static `Mutex` cannot be declared, since a static field must be
`atomic`, so that part of the provisional C4 entry on lock names reaches
no program.

## Commits, counts and gates

- `0d4c32a8` T197, `8b3da0e9` T198, `66b681bf` T199, `0af06d3f` T200,
  `f5ed662c` the reason, `02ee20ec` the format of four new tests,
  `3aa822f0` the neighbour test.
- No finding closed. Four recorded, T197 to T200, all open.
- `tests/known-failures.txt`: 13 entries before, 22 after.
- No `[provisional]` entry added.
- Not run on the target: nothing. Every change is a test of the front end
  or a program of the host.
- No warning: `build1.log`, `build2.log`, `final-build-asan.log`,
  `final-build-ubsan.log`.
- Host, `final-host-1.log` to `-3.log`: 6659 tests, 6657 passed, 2
  skipped, with the 20 expected failures of the Mac among the passes.
- ASan, `final-asan-1.log` to `-6.log`, and UBSan, `final-ubsan-1.log` to
  `-5.log`: 6657 tests each, 6655 passed, 2 skipped, the same 20 among
  them.
- `emit_identity` and `link_identity_macos-arm64` passed unchanged.

Logs are under `build/drive/logs/checks-review/`.

## Questions for Eddie

1. Should a `do ... while` whose body ends the path end the path, as T200
   reads C3?

## State before the commit of the report

```text
$ git log --oneline -3
3aa822f0 Test the neighbours of the yield and the collection of escaping-local
02ee20ec Write the comments of four new matrix tests as anti fmt wraps them
f5ed662c Give the waits of two workers under a sync block their exact reason
$ git status --short
 M CLAUDE.md
?? docs/reports/2026-10-24-checks-review.md
$ git rev-parse HEAD origin/main
3aa822f0cf0c3c971faf05ebae7e0882257e6b75
f8d787b074cd00f80803032fed4517b4a4868cf4
```

The two differ because the seven commits are not pushed yet. The push
follows the commit of the report and `CLAUDE.md`, and the reply of the
session holds the state after it.
