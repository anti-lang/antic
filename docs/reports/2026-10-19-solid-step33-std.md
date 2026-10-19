# Step 33, the minor findings of the standard library

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. This session took the standard library,
the rows of the minor tables whose places lie in `src/std/`. The logs
stand in `build/drive/logs/step33-std/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 4, `map.anti:475` | `c1a67e35` | `shuffle` read no result of `anti_rt_entropy`, so without random bytes the walk order was one fixed order. `anti_rt_hash_draw` falls back to the clocks, as the seed did. `rt_hash_draw` holds it. |
| Rule 10, `os.anti:53` | `2ae9e5c0` | `variable` gave `getenv` the bytes of a `str` without its length. The name goes over as the text of a Builder now. |
| Rule 26, `list.anti:189` | `9d356f11` | `reserve` and `shrink` of `List` and `Deque` wrote the same bodies. |
| Rule 26, dead code, `map.anti:71` | `516791e1`, `d832e8bd` | `entry_hash`, an extern, a field and two tests of `none` were left by rewrites. `stop_into` read no `into` and stopped with the message of a template, which `trap_pattern_patch_into` holds. |
| Rule 26, duplicates, `args.anti:284` | `b99f70f4`, `63c8f49f`, `15814dc2`, `7830ff52` | Two growths of `args`, the failures of `anti.regex`, `read` and `open` of a TOML document and a literal 8 were written twice. `std_args_growth` grows both lists of a parser. |
| A walk three ways, `queue.anti:75` | `f81c6cab` | `QueueWalk` kept public fields and was built by a literal. `SlotsWalk` had taken the form of `IndexedWalk` by M349 before. |
| A lent key three ways, `set.anti:144` | `0a8e0d00` | The reasons stood in two entries of the decisions and no document set them side by side. |
| `destruct` two ways, `lang.anti:192` | `e1151eea` | Three classes kept `concrete fn destruct`. |
| No `-> never`, `io.anti:36` | `ce0fa771`, `d832e8bd` | `io.exit` and the stops of `anti.regex` had no result type. `std_io_exit` ends a function with a result in `io.exit`. |
| Numbers spelled again, `lang.anti:126` | `0f644fdc` | Four numbers were written beside the constant that defines them. The eight entries of `first_sink` went with `fd716910`. |
| `src/std/.gitkeep` | `49d030de` | Left from before the directory held a module. |
| Size and presence, `text.anti:174` | `1c3e1935` | Decision 11 asks for the rule to be written down. The site page of the library states it. |
| Stale sentences, `random.anti:5` | `28684285`, `68467844` | Six comments had not kept up. The two defects of antic that `error.anti` and `synchronized.anti` worked around are gone: `std_fatal` and the `sync_` tests take the plain forms. `fs.anti:5` went with `ba344114`, and `lang.anti:4` before this step. |
| Rule 27, `queue.anti:17` | `ec2bd287` | Two phrases of `tools/docs-style/banned-terms.md`. |

Fixed before this step and recorded: rule 14 of `args.anti:251` by
`fc960111`, `Changed` and `Snapshot` by `93ced37c`, the `""` of `args` by
`46e8d189`, and the site page of the library by `c5ca7d47`. `42b77c7c`
writes the commits into the summary.

## Findings left

- `patch_bytes` in `anti.regex`. The `[provisional]` entry on `into` under
  "Regular expressions" says a byte sequence calls `patch_bytes` of the
  module, and a provisional entry is binding. Question for Eddie: should
  the two move to `anti.text`, so a patch by bytes links no PCRE2?
- The helpers of `collection.anti:575` and `int_value` and `bool_value`
  of `anti.toml` stay `pub`. The overview lists "the writers of the text of
  a collection" and the two among the public items, and the entry on
  `int_value` gives them to `anti.config`. Narrowing them changes a
  documented interface.
- The three version stores of the thread-safe maps. `SyncMap` and
  `SyncSet` keep a `Map`, the parts of `ConcurrentMap` a `HashMap`, and
  `anti.collection` cannot import `anti.collection.map`, which imports it.
  Each store is two lines. `bump` of `Pool` counts a word of a slot and is
  no stamp of a clock.
- `moved` of `Collection` is no dead code. It is a protected helper that
  a collection outside the library calls, as `std_shared` does.
- The twelve `over` functions stay `pub`. A class function has no level
  between `pub` and the class itself, and each is called from another
  class of its file.

## The freeze

`QueueWalk<T>.over` is a new public function. The defect is that a
program could write the place of the walk. Private fields need a public
function that builds the walk from `PriorityQueue`, and it replaces three
public fields. `anti_rt_hash_draw` and `anti_rt_regex_stop_into` are
functions of the runtime and no item of a module.

## New findings, provisional entries, known failures

No new finding and no `[provisional]` entry. `tests/known-failures.txt`
has 29 lines before and 29 after, 10 tests and 8 of them on the Mac.

## Identity

- `emit_identity` changes in `ffd25f28`: ten programs on six targets, by
  `ce0fa771`, `d832e8bd`, `63c8f49f` and `516791e1`.
- `link_identity_macos-arm64` changes by `c1a67e35`, since the start of
  the runtime draws the seed through `anti_rt_hash_draw`. With the
  `hash.c` before it the old digest passes. `23b41bd2` writes the new one.

## Gates

- The three builds report no warnings: `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5140 tests, `-I 1,2000` 344 s and `-I 2001,5140` 275 s. All 2000
  and 3161 pass, a part counting the fixtures it pulls in. The 8 expected
  failures of the Mac fail as listed, and the two `mimalloc_environment`
  tests skip: `host-1.log` and `host-2.log`.
- ASan, 5138 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5138`,
  280 s, 364 s and 367 s. All 1150, 1113 and 2909 pass with the same
  skips: `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 257 s, 257 s and 294 s. All 1150, 1113 and 2909
  pass: `ubsan-1.log` to `ubsan-3.log`.
- The docs-style checker reports nothing on each `.md` file the session
  touched: `docs1.log` to `docs6.log`.

## Not run on the target

- `2ae9e5c0`, the name `variable` of `anti.os` gives `getenv` on Linux.
  `std_userdirs` on anti-linux reads it.
- `c1a67e35`, `rt_hash_draw` on Linux and Windows, whose platform files
  give the clocks of the fallback.

## State at the push

```text
$ git log --oneline -3
42b77c7c Record the standard-library minor findings of step 33 in the summary
23b41bd2 Write the digest of return42 for macos-arm64 again
ffd25f28 Write the digests of emit_identity again for the standard library of step 33
$ git status --short
$ git rev-parse HEAD origin/main
42b77c7c3fd4747e19f9074890b87e945ac43fb6
42b77c7c3fd4747e19f9074890b87e945ac43fb6
```

## Left for the next session

The other areas of step 33, one per session, in the order of the
summary: `anti`, the build and the tools, and the tests. Each takes the
rows of the minor tables in `docs/audit/summary.md` whose places lie in
its area and have no commit yet. The session for `anti` also takes the
comment on `WRAP_COLUMNS` in `src/anti/fmt.c`. The session of the tests
takes the chapters of the book in the comments of `tests/CMakeLists.txt`.

CONTINUE
