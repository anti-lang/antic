# Merge of the thread-safe collection lanes

Status: done on the Mac. The lanes of the synchronized and the concurrent
collections are on main, and this step brings their records together.

## What was done

- `e6ceb15` folds `docs/decisions-concurrent.md` and
  `docs/decisions-sync.md` into "Generics and collections" of
  `docs/decisions.md`, after the entry that names the module
  `anti.collection.synchronized`, and deletes both files. Each entry keeps its
  wording and its tag: 13 of the concurrent lane, then the 24 of the
  synchronized lane in the order of its three parts. A word count of the
  lane files against the added lines finds no word lost.
- One plain entry is new. The concurrent lane says `==` of a `ConcurrentMap`
  holds one lock at a time. Since Eddie's decision on `sync a, b` under
  "Concurrent classes", its `==` and hash take the locks of all its parts,
  which `src/std/anti/collection/concurrent.anti` does. The new entry says
  so beside the old one.
- `docs/anti-syntax-overview.md` marks the thread-safe collections built,
  names their two modules, and drops them from the missing parts of the
  standard library and of round five. The specifications carry no status
  lines, so nothing in them changed.
- `CLAUDE.md` names the six classes and carries the counts 1284 and 1283.
  The format version stays 73, as `ANTL_VERSION` in `src/antic/antl.h`
  has it.
- The identity manifests did not move: `emit_identity` and
  `link_identity_macos-arm64` pass on the merged tree. Every manifest line
  changed since the last reconcile, `09f7687`, comes with a commit that
  changed the compiler or the runtime. New lines are the new test programs
  of those commits, in six targets each. Changed lines are programs whose
  code those commits changed. The link digest of `return42` moved in
  `5901d18` and `c701e35`, which changed the runtime.
- The test that `--memory-checks` finds a pointer into a `List` kept across
  a growing push already stands. `b3738f6` added `memory_checks_list`, and
  it passes in all three suites. No second copy was added.

No `[provisional]` entry is new.

## Question for Eddie

"Thread-safe collections" in `docs/anti-language-additions.md` says every
thread-safe collection has versioned `set`. A `[provisional]` entry of the
concurrent lane leaves it out of `SpscRing`, which has no `set` and whose
positions go stale as the consumer takes elements. The overview lists it
under "Not built yet". Either the specification drops `SpscRing` from that
rule, or the ring needs a versioned form by criteria.

## Suites

Zero warnings in each of the three builds:
`build/drive/logs/merge-host-build.log`,
`build/drive/logs/merge-asan-build.log`,
`build/drive/logs/merge-ubsan-build.log`.

- host: 1284 of 1284, `build/drive/logs/merge-host-suite.log`
- ASan, `ctest --preset asan`: 1283 of 1283,
  `build/drive/logs/merge-asan-suite.log`
- UBSan, `ctest --preset ubsan`: 1283 of 1283,
  `build/drive/logs/merge-ubsan-suite.log`

The docs-style checker reports nothing on every file this step touched,
`build/drive/logs/merge-docs-style.log`.

## What failed

Nothing in the tree. Each sanitizer suite ran past the ten minutes of a tool
call. The session waited for its log to report the end before the next step.

## Proof of the push

After the push of `15049f2`:

```text
$ git log --oneline -3
15049f2 Report the merge of the thread-safe collection lanes
e6ceb15 Fold the decisions of the thread-safe collections into docs/decisions.md
c6db336 Record the equality of the synchronized collections and report the step
$ git status --short
$ git rev-parse HEAD origin/main
15049f25ec6c540fd17114b7c5ddd89096b036eb
15049f25ec6c540fd17114b7c5ddd89096b036eb
```
