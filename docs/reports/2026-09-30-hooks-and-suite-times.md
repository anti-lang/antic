# The pre-commit hook and the times of the suites

The step asked for two safeguards. A pre-commit hook runs the docs-style
checker, and each suite finishes in under 9 minutes at `ctest -j` set to the
cores of the Mac, 14. It names no finding of `docs/audit/`, so
no finding id is fixed or left here.

## The pre-commit hook

`tools/hooks/pre-commit` writes the staged text of every staged `.md` file
into a scratch directory with `git checkout-index` and runs
`tools/docs-style/check_docs.py` over it. A non-zero exit refuses the
commit. With no `.md` file staged it exits before it looks for the checker.
`tools/git-hooks.cmake` names `tools/hooks` once, and the configure step
sets it as a relative `core.hooksPath` of the checkout.

The test `pre_commit_hook` failed before the hook existed, "the hook let
through: a failing .md file", and passes with it. It commits in a scratch
repository and checks:

- an `.md` file without findings is committed, a failing one is refused
  with its finding;
- a fix left unstaged does not let the failing staged text through;
- a failure left unstaged does not stop a staged text without findings;
- `UPPER.MD`, a name with a space and `commit -a` are checked;
- a deletion and a commit without an `.md` file pass. The second runs a
  copy of the hook that has no checker, so it never ran one;
- the checkout's own `core.hooksPath` is `tools/hooks`.

`raw_output` then refused the captured `execute_process` calls of the two
new scripts without `ENCODING NONE`, fixed in `f6c5bc2`.

## The times of the suites

All three already finish in under 9 minutes, so no test was changed.

| Suite | Tests | Wall time | Longest tests, seconds |
|---|---|---|---|
| host | 1285 | 120 s, 185 s on the first run after a build | `checks` 158, `package_keys` 90, `deps_dir` 72 (first run) |
| asan | 1284 | 300 s | `emit_identity` 299, `package_keys` 128, `unit` 112 |
| ubsan | 1284 | 199 s | `unit` 144, `emit_identity` 125, `checks` 122 |

`emit_identity` sets the ASan time alone, as the tests audit found. If ASan
nears the limit, one test per target in `run_emit_identity.cmake` is the
first change to make.

## Decisions

Two entries under "Repository layout" in `docs/decisions.md` record what
Eddie asked for. One `[provisional]` entry records the choices of the hook:
the directory `tools/hooks`, a relative `core.hooksPath` set over any earlier
value, the staged text as the input, `python3` from the search path, and
SKIP of the test on a host without `python3`.

## Gates

The build had zero warnings in all three trees. The suites passed: host
1285 of 1285, ASan 1284 of 1284, UBSan 1284 of 1284. The logs are in
`build/drive/logs/hooks-and-times/`: `ctest-host-2.log`,
`ctest-asan-1.log`, `ctest-ubsan-1.log`, the builds in `build-*.log`. The
sanitizer suites ran at `f6c5bc2`. The one later commit, `5613dfc`,
changes `CLAUDE.md` and `docs/decisions.md` alone. The checker reported
nothing on both, and on this report.

State after the push of `5613dfc`, before this report was committed:

```text
$ git log --oneline -3
5613dfc Record the pre-commit hook and the time limit of the suites
f6c5bc2 Read the output of git in the hook scripts without decoding it
01765ef Refuse a commit whose staged .md files fail the docs-style checker
$ git status --short
$ git rev-parse HEAD origin/main
5613dfc2003491c58bb243c55bf79776887c970a
5613dfc2003491c58bb243c55bf79776887c970a
```
