# Solid: decision 13, the runners in tests/runners/

The step built decision 13 of "Decisions of 2026-10-11": the 198 runners
`run_*.cmake` moved from `tests/` into `tests/runners/`. It ran in one
session on 2026-10-17 from `e4a15a38`. The logs lie in
`build/drive/logs/`, under names that start with `d13-`. The task named
no id for the step, so this report takes `decision13`.

## What changed

`b906e240` holds the whole move. `git mv` took each runner, and git
records 198 renames.

- The root `CMakeLists.txt` sets `ANTIC_RUNNERS` once, before
  `add_subdirectory(tests)`. `tests/CMakeLists.txt` and `src/native/`
  name every runner through it.
- A runner reaches `program_output.cmake`, `pinned_clang.cmake`,
  `build_id.cmake`, `known_failures.cmake`, `binary_stdio.h` and `tools/`
  one directory higher. `run_linux_libc.cmake` finds the root two
  directories up. `run_package.cmake` and `run_raw_output.cmake` call
  the runner beside them through `CMAKE_CURRENT_LIST_DIR`.
- Six scans read `tests/*.cmake` without recursion and would have lost
  the runners: `errors_registered`, `pinned_compiler`, `raw_output`,
  `warning_set`, `target_triples` and `linux_link_recipe`. Each globs
  `tests/runners/` as well. A scratch script counted 198 runners and 204
  scripts in the two directories.
- `repo_layout` lists `runners` among the directories of `tests/` and
  refuses a runner at `tests/run_*.cmake`. It failed on the moved tree
  before the list changed, `d13-red-layout.log`. A tree with
  `tests/run_stray.cmake` passed before the refusal and failed after,
  `d13-red-stray.log` and `d13-green-stray.log`.
- The Layout table and the layout rule of `CLAUDE.md` name
  `tests/runners/`. Every comment, script and document names a runner at
  its new place, the trees the self-tests write among them.

`f5a234dd` records the move in the row of `docs/audit/summary.md` that
asked for the directory.

## Left as it stands, and why

- `docs/audit/data/back-end-mentions.txt` and `named-files.txt` still
  name 20 runners at `tests/`. `CLAUDE.md` says the tool output under
  `docs/audit/data/` stays as the tools wrote it, and that rule wins over
  the sentence of the task that no file names a runner at its old place.
- `docs/reports/2026-10-11-tests-decisions.md:57` names the pattern
  `tests/run_*.cmake` as the subject of decision 13. It names no runner.

## VMs

Neither VM ran. The step changes no file of the platform layer, no
Windows or Linux path of the runtime, no link, sysroot or package. Every
path a runner builds from its own directory was checked to exist on
disk, and so was every `${ANTIC_RUNNERS}` path. That covers the branches
that run on Linux or Windows alone.

## Findings and provisional entries

No finding was closed or recorded, and no `[provisional]` entry was
added. `tests/known-failures.txt` holds 27 lines before and after.

## Gates

- The builds warn in no code of ours, `d13-host-build3.log`,
  `d13-asan-build.log` and `d13-ubsan-build.log`.
- Host, 5123 tests through the preset at `f5a234dd`, `-I 1,2000` and
  `-I 2001,5123`: 2000 and 3144 pass, a part counting the fixtures it
  pulls in. The two `mimalloc_environment` tests skip. The 27 lines of
  `tests/known-failures.txt` fail as listed. `d13-host-3.log` and
  `d13-host-4.log`.
- ASan, 5121 tests in three parts: 1150, 1113 and 2892 pass,
  `d13-asan-1.log` to `d13-asan-3.log`. UBSan, the same parts and counts,
  `d13-ubsan-1.log` to `d13-ubsan-3.log`.
- `emit_identity` and `link_identity_macos-arm64` pass, unchanged.
- The docs-style checker reports nothing on the 99 `.md` files the step
  touched, `d13-docs-1.log` and `d13-docs-2.log`.

## State before this report

```text
$ git log --oneline -3
f5a234dd Record the move of the runners in the summary
b906e240 Move the test runners into tests/runners/
e4a15a38 Report the solid step 24, the release scripts
$ git status --short
$ git rev-parse HEAD origin/main
f5a234dd6b2ee17aac848b6307ea4c18bbaf3507
f5a234dd6b2ee17aac848b6307ea4c18bbaf3507
```
