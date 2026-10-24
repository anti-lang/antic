# The step sweep of the compile-time checks

The step read every place that the steps E1, E2, C1 and C4 marked with an
`unchecked` whose reason was `"until the step sweep"`. No such clause is
left. Logs are under `build/drive/logs/checks-sweep/`.

## The places

- 117 places of E1 `escaping-local`: 93 before a `parallel`, 23 before a
  `dispatch` and one before a store into a static field.
- One place of E2 `use-after-free`, one of C1 `uninitialized-read`, and
  five of C4 `lock-order`.

## Defects

- The store into a static field. `classes_typed_static_of_a_generic_class`
  of `tests/matrix/` stored `&n` into `Box<int>.last`, which kept the address
  after `main` returned. Commit `41c79bcc` gives the static memory of
  `alloc`, clears it and frees it. It is T192, fixed. The cause was the test
  itself, and antic needed no change.

## False positives of the rules

Each place kept its `unchecked`, with a reason that says why the code is
correct. The rules name each case, so the check stays as it is.

- `parallel`, 93 places, commit `a4d52f09`. Reason: `"parallel waits for
  every chunk while the array lives"`. The fuzzer writes the same clause,
  and the chapter on ownership of the guide says why the examples carry it.
  `build/sweep-scratch/par.anti`:

  ```anti
  worker fn sum(xs: []int) -> int
  {
  	return xs[0];
  }

  fn main() -> int
  {
  	let a = [1, 2];
  	let parts = parallel a[0..2] by 1 -> sum;
  	free(parts.ptr);
  	return 0;
  }
  ```

- `dispatch`, 23 places, commit `2db077de`. Each joins its job in the same
  block before the local ends. Reason: `"the block joins the job before the
  local ends"`. `build/sweep-scratch/disp.anti` passes `&c` of a local
  `Cell` to `dispatch` and returns `join(j)`.
- E2, `param_facts` of `tests/programs/`, commit `a3a987cf`. The line
  `n.v = 1;` after `h.node = n;` writes a node that `h` owns and keeps alive. Reason: `"h owns
  the node, and n reaches it while h lives"`. `build/sweep-scratch/moved.anti`
  is the function `adopt` alone. The question of the step of E2 to Eddie
  stands: should a store into an `own` field leave the pointer readable?
- C1, `ownership_delete_of_a_channel_never_made` of `tests/matrix/`, commit
  `a5d0439d`. The element holds a class value, so `alloc` zeroes it, and the
  test deletes the channel never made by R075. Reason: `"alloc zeroes an
  element that holds a class, so its channel was never made"`.
  `build/sweep-scratch/zeroed.anti` holds it without the counter.

## Kept on purpose

C4, commit `6f8d9fd7`. `traps/lock_order`, `traps/lock_orders`,
`traps/lock_order_once` and the example `orders` of chapter 14 take two
locks in both orders. The inversion is real, and these programs exist to
show the report of the check of dev mode. They take the two orders one
after the other, so none hangs. Reason: `"both orders on purpose, for the
check of dev mode"`.

## New finding

T193, open, commit `b78a5ca3`. E1 checks the object of `dispatch` and the
array of `parallel`, and lets the address of a local pass as an argument of
their worker. `sync_map_threads` passes `&m` to `parallel` that way. Two
refusals of `tests/matrix/` show it, both listed. They fail for that reason
alone, `t193.log`. Left for the step of E1. A fix refuses more correct code
of `tests/std/`, so the question of the step of E1 on `parallel` decides
its size.

## Question for Eddie

The question of the step of E1 stands. `parallel` waits for every chunk,
and so does a `dispatch` joined in the block of its local. Should either
leave the list of E1? 116 of the 117 places of E1 were such code.

## Gates

- `tests/known-failures.txt`: 4 lines before, 6 after.
- No `[provisional]` entry added.
- Not run on the target: nothing. Every change is a test program or the
  front end, which is the same on every host.
- No warning: `build-host.log`, `build2-asan.log`, `build2-ubsan.log`.
- Host, `host2-1.log` to `host2-3.log`: 6538 tests by name, 6536 passed,
  the four expected failures among them, and 2 skipped.
- ASan, `asan2-1.log` to `asan2-5.log`, and UBSan, `ubsan2-1.log` to
  `ubsan2-5.log`: 6536 tests by name each, 6534 passed, 2 skipped. Parts
  `-I 1,1150`, `1151,2250`, `2251,4000`, `4001,5200` and `5201,7000`.
- `emit_identity` and `link_identity_macos-arm64` passed unchanged.
- The docs-style checker reported nothing on every `.md` file touched.

## State before the commit of the report

```text
$ git log --oneline -3
b78a5ca3 Record T193, the arguments of a worker that escaping-local lets pass
6f8d9fd7 Give the lock orders taken both ways on purpose their reason
a5d0439d Give the read of a channel never made the reason of its unchecked
$ git status --short
$ git rev-parse HEAD origin/main
b78a5ca3b54cd0bdd7995712a46d792964a3d107
b78a5ca3b54cd0bdd7995712a46d792964a3d107
```

The commit of the report adds the report, the commit of T192 and the
counts of the suites in `CLAUDE.md`.
