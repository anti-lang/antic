# The scope of the docs-style checker

The step puts Eddie's answer on the scope of `tools/docs-style/check_docs.py`
into the checker, `docs/decisions.md` and `CLAUDE.md`. The checker reads
Markdown documents, `.md` files, and nothing else.

## What was done

- `7ea9328`: `MARKDOWN_EXT` holds `.md` alone, and a file of any other kind is
  not read, named or found in a directory. The code that read comments out of
  source files is gone with the tables of comment markers it used. The entry
  in `docs/decisions.md` that kept CMake files out now states the whole scope,
  and the two rules on the checker in `CLAUDE.md` name `.md` files and say to
  read the exit status without a pipe.
- `3924247`: five warnings on a filler word in
  `docs/reports/2026-09-26-warnings.md`, a report of another session, now read
  "from scratch", so the checker reports nothing over `docs/`.

## Checks

- Over `docs/`, `CLAUDE.md`, `README.md` and `CHANGELOG.md` the checker
  reports 0 errors and 0 warnings and exits 0.
- A file with a sentence of 29 words exits 1, and a file without findings
  exits 0. The audit's data files, `tests/CMakeLists.txt` and a `.c` file are
  not read and give 0.

## What happened on the way

- Another session worked in the same checkout. Its uncommitted changes to
  `tests/run_clib.cmake` and five other files stood in the tree while the
  sanitizer suites ran, and `clib_bundle` failed in both. The session left
  those files alone and did not push.
- That session then committed on top of the two commits of this step and
  pushed all of them, `259af73`, `ba2d68d` and `f7fa937` among them.
  `ba2d68d` links the Mac's host programs with the pinned `ld64.lld`.

## Proof

- At `f7fa937` the host suite passed 1146 of 1146, and the ASan and UBSan
  suites 1145 of 1145 each, `clib_bundle` included.
- The checker reports nothing over `docs/` at `f7fa937` and exits 0.
