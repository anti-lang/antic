# Batch 3z of the third phase of coverage

By answer 5 of "Answers of 2026-11-17" in `docs/decisions.md`, this batch
is chosen by the branches the suite misses, and it runs the fuzzer. The
summary of `./c coverage` at the end of batch 3y gave the ranking, and
the code had not changed since. The batch took `src/anti/deps.c` with 38
missed branches, which batch 3y named, and `src/anti/main.c` with 58,
which no recent batch had probed. It probed `lower.c`, `sema_value.c`,
`lower_desc.c`, `docpage.c`, `src/rt/symbols.c` and `src/anti/fmt.c` as
well. Each side ran first through the antic or the anti of the host or of
the coverage tree, with a profile of its own. It swept the readers of the
manifest and of the lock file with the ASan anti, which no batch had
done. Eddie's direction is the Mac alone, so the VMs were not touched.
The batch found three defects, T298 to T300, all in `src/anti/deps.c`.
The next free id is T301.

## Commits

- `7603debe` lists T298, `0b221ce7` fixes it.
- `5f0384b2` tests the sides: the case `path_chain_mixed`, eight cases of
  the usage of anti, `anti_lto_library` and two programs of the matrix.
- `df1d99e5` lists T299, `a03eaa3a` fixes it.
- `2c71f97a` lists T300, `9c7157f9` fixes it.
- `8cc4de4b` names the guards.
- One commit records the times and the counts in `CLAUDE.md`, the commits
  of the fixes in `docs/audit/tests-findings.md` and this report.

## Findings

- T298, minor, fixed. A dependency of a path that the manifest moves to a
  directory that holds the same library keeps its digest, and
  `graphs_equal` compared no path, so `anti.lock` kept the old directory.
  The comparison takes the path now. The path the lock file holds is the
  `dist/` directory of the target and the mode, so a dev build after a
  release build writes the lock file again. A `[provisional]` entry
  records it.
- T299, major, fixed. `lock_read` took a package without its version,
  and a module without its digest, for the end of its list. The graph
  then answered the manifest without the entries after it. The build kept
  the lock file and wrote a notice without the dropped packages, or could
  not find a dropped module. `entry_at` tells such an entry from the end
  of the array through the shape of its path. The lock file then reads
  as none, as one with a value its kind refuses does. No message names the
  entry, since the fix needs none under the feature freeze.
- T300, major, fixed. The readers of an index did the same with a
  version without its version, a module without its digest and a
  dependency without its name. A dependency then left the graph without
  a word. Each now gives `` anti: <index> is no index that anti reads ``.
  An index that is no TOML gives that line, so the fix adds no message.

## ASan sweep of damaged manifests and lock files

The projects `diamond`, `app` and `back` of the fixture of
`anti_build_resolve`, copied under `build/sweep-3z/`, went to the anti of
the ASan tree with `ASAN_OPTIONS=detect_leaks=1`. Their `anti.toml` and
`anti.lock` were damaged in two ways at every offset: cut there, and
with the byte there inverted. That gave 8570 runs of `anti build` in ten
workers, each with its own copy and cache, in 7 minutes. 2759 ended with
0 and 5811 with 1. None showed a report of a sanitizer or ended by a
signal. The sweep did not show T299, since a damaged lock file of it
builds and ends with 0. A read of `lock_read` found it.

## Fuzzer

The seeds run before are 1 to 200, 1000 to 1299, 2000 to 2499, 3000 to
3299, 10001 to 15840, 20001 to 23200, 30001 to 30150 and 40001 to 43200.
`cmake -DFIRST=43201 -DCOUNT=200 -P tools/fuzz/run.cmake` ran 43201 to
43400 at the default size in 161 s. No seed showed a find.

## Coverage

| | Before | After |
|---|---|---|
| Branches | 93.37%, 3740 missed | 93.39%, 3728 missed |
| Regions | 98.52%, 1265 missed | 98.52%, 1264 missed |
| Functions | 99.87%, 6 missed | 99.87%, 6 missed |
| Lines | 98.72%, 1227 missed | 98.73%, 1224 missed |

| File | Branches missed before | after |
|---|---|---|
| `deps.c` | 38 | 33 |
| `main.c` of `anti` | 58 | 48 |
| `lower.c` | 57 | 56 |
| `sema_value.c` | 59 | 58 |

The fixes added 10 branches to `deps.c`, all of which the tests take.
`src/rt/mem.c` and `src/rt/platform_posix.c` lost one and four, since the
threads of the suite take other paths from run to run. The coverage run
had 7853 tests, all passed, and its suite took 1015 s.

## Suites

- The host suite ran in the parts `-I 1,2100`, `-I 2101,4300` and
  `-I 4301,<last>` before each commit of code. Every test passed.
- Both sanitizer suites ran once before the push, in the six parts of
  `asan`, up to test 7863. Every test passed.
- The host runs 7865 tests and each sanitizer tree 7863.
  `tests/known-failures.txt` holds the two lines of `host=windows`, as
  before.

## Not reached

The sides of `deps.c` left are the paths of a drive letter on a Mac, a
dependency of a library file without a constraint, a lock entry whose
names differ from those of the graph at one place, and a lock that lacks
a dependency the manifest adds. The rest of `main.c` is the runtime
archive of the user that is not found, and the reads of the manifest that
`anti test` makes after the build. `sema_value.c`, `lower.c`,
`lower_desc.c`, `symbols.c` and `fmt.c` showed null guards and the
refusals of damaged files, and each probe gave the output the code
describes. `docpage.c` orders two items of one line by their column, and
a fixture of it cannot stand in `tests/`, whose files `fmt_canonical`
holds to one item per line.

## Questions for Eddie

- T298: should the lock file name the directory the manifest gives, as
  `../dep`, in place of the `dist/` directory of a target and a mode?
  The fix keeps the directory as it was, which a dev build after a
  release build then writes again.
- T299: should a lock entry that lacks a key get a message of its own?
  The fix reads the lock file as none without one, under the freeze.

Seven questions stay open from before, and nothing here acts on them.
