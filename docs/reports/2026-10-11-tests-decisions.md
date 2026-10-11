# The decisions of 2026-10-11

The first step of the test run, on 2026-10-11, on `main`. The task text
gives the step no id. The driver writes its log as `tests-decisions`, so
this report takes the name `tests-decisions`. The step changed
`docs/decisions.md` and nothing else.

## What the step added

`docs/decisions.md` holds a new section, "Decisions of 2026-10-11", before
"Open". It numbers the thirty decisions 1 to 30 under three headings: "The
fix run" with 1 to 18, "Language rules" with 19 to 27 and "The test run"
with 28 to 30. None is provisional. A reason stands where the third audit
gives one, and each such reason names its finding.

Twenty older entries said otherwise. Each now ends with one sentence that
names the decision that replaces it. Nothing was removed from any of them.

| Decision | Entries that point at it |
|---|---|
| 1 | The compile unit and C99, under "Debug information" |
| 2 | lld and `--llvm-mc`, the eight LLVM tools, the tools of a package, the driver options, `coff.c` |
| 3 | The glibc sysroots and the build steps of the README |
| 4 | `text()` of an error, the count a text builder cannot hold, `Builder.take_in` |
| 5 | The start flag of `init.c` |
| 6 | The value of `plugins` joined with `:` |
| 7 | "UTF-8 throughout" under "Core language" |
| 9 | `ANTI_SITE` and what step 9 rsyncs |
| 10 | `pool.Changed` and the two entries on `Snapshot<T>` |
| 14 | The form `src/rt/atomic.c` chooses |
| 15 | The cache key of a module |
| 25 | `Grid<T>.new`, which calls `count` a public field |
| 29 | The eight LLVM tools |

## Counts and checks

- Tests: 1701 registered before and 1701 after, by `ctest --test-dir
  build/host -N` in `build/drive/logs/tests-decisions-count.log`. The step
  added no test.
- Defects found: none. No id from T001 on is taken.
- `tests/known-failures.txt` does not exist yet, so the suite expects 0
  failures.
- The host suite and the sanitizer suites did not run. The task says the
  host suite need not run, and the commits change files under `docs/`
  alone.
- The docs-style checker reports `0 error(s), 0 warning(s)` on
  `docs/decisions.md`, in `build/drive/logs/tests-decisions-style.log`. Its
  first run found three sentences of 26 words, which were split.
- No test reads the text of the section. `installer_options` looks for five
  names in `docs/decisions.md`, which still stand, and `no_sdk_fetcher`
  passes the file over.

## What the step left as it stands

- The entries on the native back end. Decision 1 turns them into history
  in the fix run. The entry on C99 alone points at the decision.
- The entries that name a runner as `tests/run_*.cmake`. Decision 13 moves
  the files in the fix run, and the paths change with them.
- The entries on `alloc(T, n)` and `Allocator.alloc`, which give `none`
  when the memory has run out. Decision 4 names the library and the
  runtime, and these two hand the result to the program.
- The entry that keeps `-DANTIC_SYSTEM_COMPILER=ON` for a reader. Decision
  14 takes the branch a reader's build with Microsoft's compiler compiles.
- Every other document. The task says to change nothing else. `CLAUDE.md`
  asks that a decision on the language goes into the specifications and
  the overview in the same commit. Decisions 17 and 19 to 27 are such
  decisions, and decision 27 has the documents follow the code on `<` of
  an enum and on `+=`. `docs/distribution.md` still lists llvm-mc among
  the tools of a package, against decision 2.

Two words differ from the task text. Decision 4 says "the standard
library" for "the library", and decision 27 says `operator fn add` for "an
operator add".

## Rules the documents do not state

None. The step ran no program.

## Questions for Eddie

- Decision 4: does it reach `Allocator.alloc` and `alloc(T, n)`, or do
  both keep `none`?
- Decision 11: which document holds the written rule?
- Decision 25: do `width` and `height` of a `Grid` follow `count`?

## Proof

`git log --oneline -3` before the commit of this report:

```text
ca4da686 Record the decisions of 2026-10-11
552a0139 Report the read-through of the developer's guide
12a77f76 Name the chapter of three forms at their first use in the guide
```

`git status --short` printed nothing after the commit of this report. The
reply of the session holds the output of `git rev-parse HEAD origin/main`
after the push, since the hash of this commit is not known inside it.
