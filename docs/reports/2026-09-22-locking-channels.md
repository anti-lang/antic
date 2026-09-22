# Locking and channels

"Locking and channels" of `docs/anti-language-additions.md` is built:
`anti.lang.Mutex` with `Mutex.new()` and `m.destroy()`, `sync m { }`, which
unlocks on every exit of its block, `chan T` with `send`, `recv` and `close`,
and `select`. A nested `sync` on the mutex of an enclosing one in the same
function is refused with `` `sync m` inside `sync m` deadlocks ``. The
development Mac passes 688 of 688 tests, and the ASan and UBSan builds pass
687 each, without `no_paths`. The build has no warnings. The VMs did not run.

## What was done

1. `b13fca9`. The five reserved words are keywords, and the library format
   is version 45. `Mutex` and `chan T` are structs of `anti.lang` with one
   handle each, so the back end sees a struct of one pointer. The checker
   refuses a `sync` on a place that an enclosing `sync` holds, and lowering
   records the unlock as the first exit action of a scope around the block.
   `recv` gives a pointer to a slot of the frame. `select` passes the
   handles and one slot per arm to `anti_rt_select`. `rt/sync.c` holds the
   mutex, the channel ring and the select wait over the lock and condition
   variable of the platform, an SRWLOCK on Windows.
2. `244dfe4`. Library files carry both types, and `sync_modules_release`
   and `sync_modules_dev` take them across a module. The five programs also
   run in dev mode.
3. `21eda57`. An export item refuses both types as having no C form.
4. `0d3dc53`. A defect of `try` blocks, see below.
5. `36dfe6b`. The decisions gain "Locking and channels",
   `docs/notes/locking.md` holds the choices of the passes, and the
   specification, the overview and the handover follow.
6. The tests. `tests/unit/test_sync.c` holds the lexer, four parse trees,
   two syntax errors, 6 accepted sources, 3 types and 19 refusals. The
   programs `sync_exits`, `sync_workers`, `channels`, `select_two` and
   `channel_workers` run on ARM64, under Rosetta on x86_64 and in dev mode.
   `sync_workers` has four dispatched workers add 80000 times to one
   counter. `channel_workers` sends a thousand values through a channel of
   four to a dispatched consumer, and selects over two channels of two
   that dispatched producers fill. It runs with `ANTI_THREADS=4`, and the
   others run on a pool of any size. Each has a timeout of 120 seconds.

## What failed and how it was fixed

- antic crashed on the first program: a call of the runtime without a
  result has no temporary, and the lowering read one. Fixed in the helper.
- A class field of type Mutex named a descriptor that no module writes, and
  the link failed. Both types have no descriptor and the type id none now.
- The `try` block case of `sync_exits` hung. The first error of a `try`
  block jumped to the handler without running the `undo` and `defer`
  statements of the blocks it left, which the object model requires, and
  so without the unlock. `programs/try_block_exits.anti` showed the same
  for plain `defer` statements. The jump now runs the exit actions of every
  scope inside the `try` block. No other program changed its assembly.
- Two mutations checked the tests. Disabling the nested-`sync` check failed
  three unit checks, and skipping the unlock on non-local exits hung
  `sync_exits`.
- `emit_identity` gained the lines of the new programs and changed no
  other. `link_identity_macos-arm64` took the new digest of return42,
  whose build id covers the runtime archive.
- The host link prints `ignoring -lto_library`, as in every earlier session.

## Provisional entries

Under "Locking and channels" in `docs/decisions.md`: the handle inside a
Mutex, `destroy` and a cleared handle, the operand of `sync`, the sameness
of two operands, `recv` giving `?*T` to a slot of the frame, `chan T` as a
handle and `delete(c)`, both counting as pointer-free, `close` as a name,
the capacity, a send after `close`, the form of `select` and the channel it
takes, a pointer written `*p`, the type id none, C, and the missing-return
rule.

## Questions

- The specification wrote `recv(c) -> ?T`, and the task `?*T`. `?` stands
  before `*` and `fn` alone, so the specification now says `?*T`. Is a slot
  of the caller's frame the pointee you intended?
- The specification gives a channel no release. `delete(c)` frees it. Is
  that the word you want?
- The warning of the whole-program analysis on a field written inside
  `sync` and read outside it is not built. The IR marks no `sync` region
  and no read or write of a field by name, so it needs a pass of its own.
- `lang.Mutex` and `lang.Flags` do not resolve as qualified names, since no
  library file declares them. Should the qualified form work?

## Proof

Taken after the push of the code, before the commit of this report. Logs:
`build/drive/logs/test-final.log`, `asan-test.log` and `ubsan-test.log`.

```text
$ git log --oneline -3
36dfe6b Record locking and channels in the decisions and the handover
0d3dc53 Run the exits of every block an error leaves for a try block
21eda57 Refuse Mutex and channels in export signatures
$ git status --short
$ git rev-parse HEAD origin/main
36dfe6bfaa4033a17e2383890524f33cf862cfc7
36dfe6bfaa4033a17e2383890524f33cf862cfc7
```

- Host: 100% tests passed out of 688.
- ASan: 100% tests passed out of 687.
- UBSan: 100% tests passed out of 687.
