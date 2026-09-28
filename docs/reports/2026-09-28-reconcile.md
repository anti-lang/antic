# Reconcile of the round five lanes

Status: done on the Mac. The reconcile session of `drive-additions-5.sh`
committed its work and stopped while the ASan suite ran in the background.
Its gates, its report and its push were not done. A second session ran
the three suites in the foreground, wrote this report and pushed.

## What was done

- `840062b` takes the type arguments of a generic class named with its
  module. `map.HashMap<str, bool>.new()` dropped the arguments after the
  class and refused the call, since nothing in its arguments gave `K`.
  The branch of `m.T.f(args)` in `src/antic/sema_call.c` now names the copy
  and gives it to inference as the owner, as the branch of `T.f(args)`
  does. `std_direct_imports` failed on it first,
  `build/drive/logs/rc-direct-red.log`, and passes,
  `build/drive/logs/rc-direct-green.log`.
- `b3738f6` adds `memory_checks_list`. `tests/traps/memory_checks_list.anti`
  keeps a pointer into a `List` across a push that grows it, and
  `--memory-checks` reports the use after free.
- `8a0332d` compares two pools, two queues and two trees with `==`, in both
  modes. The handles lane had refused it while a generic operator read from
  a library file lost its mark. The first run of the new cases is
  `build/drive/logs/rc-eq-red.log`, the passing one
  `build/drive/logs/rc-eq-green.log`.
- `bc0fa0d` folds `docs/decisions-seq.md`, `-sorted`, `-hashed`, `-handles`
  and `-memcheck` into "Generics and collections" of `docs/decisions.md`
  with their wording and tags, and deletes the five files. It updates the
  status lines of `docs/anti-syntax-overview.md` and the "State" section of
  `CLAUDE.md`: the collections and `--memory-checks` are built, and the
  counts are 1198 and 1197. The format version stays 72, as
  `ANTL_VERSION` in `src/antic/antl.h` has it. The direct imports example of
  the overview drops `import anti.regex.{Regex};`, which the decisions
  refuse, and now compiles.
- The identity manifests did not move. `emit_identity` and the other pinned
  outputs pass on the merged tree.

No `[provisional]` entry is new. Every one that `bc0fa0d` adds to
`docs/decisions.md` comes from a lane file, with its tag. The two new plain
entries record that `==` of a generic class from a library file reaches a
program since format version 71.

## Suites

All three ran in the foreground at `bc0fa0d`, with zero warnings in each
build:

- host: 1198 of 1198, `build/drive/logs/fin-host-suite.log`
- ASan, `ctest --preset asan`: 1197 of 1197,
  `build/drive/logs/fin-asan-suite.log`
- UBSan, `ctest --preset ubsan`: 1197 of 1197,
  `build/drive/logs/fin-ubsan-suite.log`

The ASan and UBSan suites ran for 15.5 and 12.7 minutes. A tool call of
the session ends its wait at ten minutes. Each suite was started in the
foreground and ran on past that point, and the session waited for it to
end before the next step.

## What failed

The first reconcile session ended while it waited for the ASan suite it
had started in the background, `build/drive/logs/rc-suite-asan.log`, which
stops at `emit_identity`. Nothing in the tree failed.

## Proof of the push

After the push of `09f7687`:

```text
$ git log --oneline -3
09f7687 Report the reconcile of the round five lanes
bc0fa0d Fold the decisions of the round five lanes into docs/decisions.md
8a0332d Compare two pools, two queues and two trees with ==
$ git status --short
$ git rev-parse HEAD origin/main
09f7687514ac65686974c0e6c8a698a8ada635ae
09f7687514ac65686974c0e6c8a698a8ada635ae
```
