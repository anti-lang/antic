# The syntax overview and round five

The step brings `docs/anti-syntax-overview.md` in line with round five as built:
generics, optional values, ownership at a call, lending, walking, hashing, direct
imports and the collections.

## What changed

- `overview_examples` passed at the start, so no checked example was broken. Two
  blocks were marked `not-built` for features that are built. The example of
  `Shared<T>` in "Ownership" now compiles, with `modify` added, and the prose
  names `read(f)` and `modify(f)`. The whole example of "Collections" had been
  skipped for its named arguments alone.
- "Collections" now holds three checked examples. The first is the old example
  with `capacity` and `from` by position, `read` and `for x in &c` over a list
  and a map. The second covers `sort`, `map<int, str>`, `Set` with `union`,
  `BitSet`, `Pool` with `get_versioned` and `set_if_version`, `PriorityQueue`
  and `Grid`. The third covers `SyncMap`, `ConcurrentMap` and `SpscRing`. A
  two-line `not-built` block keeps the named form of the specification. The
  status line names `set_if_version`, `pool.Changed` and `map<T, U>`, from the
  `[provisional]` entries of "Generics and collections".
- "Generics" uses the `List` of `anti.collection.list` in place of a stand-in
  class, shows a generic interface, `abstract class Source<T>`, and gains an
  example of the constraints `iter`, `collection.Iterable<int>` and
  `eq + hash`.
- "Optional values" reads a real `Map` and `List` in place of two stand-in
  classes.

Direct imports, walking, hashing and the status lines of those sections
already agreed with `docs/decisions.md` and the tests they name. Every test
file a round-five status line names exists.

## What failed and how it was fixed

- A hidden context held a class, and the shown block after it held imports.
  The module then had an import after an item, which the parser refuses. The shown block now declares `Person` after its imports.
  `build/drive/logs/ov-test1.log`, `ov-test2.log`.
- `let (a, b) = e else { }` is no form of the language. The `Pool` example binds
  the tuple and reads `.1`. `build/drive/logs/ov-test3.log` is the pass.

## Found and not fixed

- antic crashes in the front end on `&A.make`, where `make` is a function
  without `self` named through its class, in `sema_is_place` of
  `src/antic/sema_expr.c` at line 33. The base of the field expression has no
  type. `build/scratch/r5ov/seg5.anti` reproduces it, and
  `build/drive/logs/ov-seg5-bt.log` holds the backtrace. The message the
  expression should give is not in the documents, so it is left for a step of
  its own.

## Questions

- "Direct imports" of `docs/anti-language-additions.md` still gives
  `import anti.regex.{Regex};` as an example, which a `[provisional]` entry of
  `docs/decisions.md` refuses, since `Regex` is declared in `anti.lang`. The
  overview follows the entry. The specification was not changed.

No `[provisional]` decision was made.

## Gates

- Build with zero warnings: `build/drive/logs/ov-host-build.log`,
  `ov-asan-build.log`, `ov-ubsan-build.log`.
- Host: 100% tests passed out of 1284, `build/drive/logs/ov-host-test.log`.
- ASan: 100% tests passed out of 1283, `build/drive/logs/ov-asan-test.log`.
- UBSan: 100% tests passed out of 1283, `build/drive/logs/ov-ubsan-test.log`.
- The docs-style checker reports nothing on both files.

At the push of the overview commit:

```text
$ git log --oneline -3
71594be Bring the round five examples of the syntax overview to the built collections
bae062f Add the proof of the push to the report of the lane merge
15049f2 Report the merge of the thread-safe collection lanes
$ git status --short
?? shared.anti
$ git rev-parse HEAD origin/main
71594be8b4fb2c10dc8f69097ba5a11924c2cb56
71594be8b4fb2c10dc8f69097ba5a11924c2cb56
```

`shared.anti` was a scratch file of this session, written into the root by a
`cd` that failed. It was deleted after the push and was never committed.
