# docs/audit/

Eddie's decision: `docs/audit/` is allowed directly under `docs/`. It holds
the reports of the code audit and their machine data.

## What changed

Commit `05ded9f` changes three files and nothing else.

| File | Change |
|---|---|
| `tests/run_repo_layout.cmake` | `docs_directories` lists `audit` |
| `CLAUDE.md` | "Repository layout" lists the directories of `docs/` and names `docs/audit/` as the place of an audit report and its data |
| `docs/decisions.md` | The first entry of "Repository layout" lists the directories of `docs/` and records the decision |

## Proof

Before the change, `repo_layout` failed on a tracked `docs/audit/probe`,
staged with `git add -N`. After it, the same tree passed. The probe was
removed before the commit.

| Suite | Result |
|---|---|
| host | 788 of 788 passed |
| asan | 787 of 787 passed |
| ubsan | 787 of 787 passed |

## Questions

None.
