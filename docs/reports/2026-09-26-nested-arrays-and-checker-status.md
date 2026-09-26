# Nested array fields, and the exit status of the docs-style checker

The step builds Eddie's answer to the question of
`docs/reports/2026-09-26-tuples-arrays-and-regex-level.md`, recorded in
`docs/decisions.md` and `docs/anti-language-additions.md`. It also checks the
exit status of the docs-style checker. Status: BLOCKED on the findings in the
machine data of the audit, which the last section names.

## What was done

- `1daa0a2`: an array of more than one level has a descriptor with the length
  of each level. `serialize` writes such a field as nested JSON arrays, and
  `Object.deserialize` checks the count at every level. Test:
  `std/serialize_tuples.anti` and `records_type_ids`.
- `c112480`: the one finding of a Markdown file under `docs/`, a filler word
  in `docs/site/build-tool/index.md`.

## The exit status of the checker

`tools/docs-style/check_docs.py` already exits with 1 whenever it reports an
error, and with 0 otherwise, in `main`. A file with a sentence of 29 words
exits 1, and a file without findings exits 0. The copy does not differ from
what the gates assume, and it is unchanged.

The failure lay in the command the last session ran:
`check_docs.py <file> | tail -1 && git commit`. A pipeline has the status of
its last command, which was that of `tail`. The commit and the push of the
report went ahead past the error, which `99188c9` fixed. This session ran
the checker without a pipe and read its status.

## Proof

- Host: 1145 of 1145 passed at `c112480`, in `build/scratch/suite15.log`.
- ASan: 1144 of 1144 passed, in `build/scratch/asan-suite.log`.
- UBSan: 1144 of 1144 passed after a build from scratch, in
  `build/scratch/ubsan-suite.log`. The first run failed 866 tests while
  another session compiled at the same time. `build/ubsan` then had no
  `CMakeCache.txt` and listed no tests.
- Every `.md` file under `docs/`, with `CLAUDE.md`, `README.md` and
  `CHANGELOG.md`, gives 0 errors and 0 warnings, and the checker exits 0.

## Blocked: the machine data of the audit

Over all of `docs/` the checker reports 361 errors and 553 warnings, every one
in `docs/audit/data/*.txt`. Those files are the output of the tools of the
audit: the analyzer, the warnings of the compilers and the metrics. The
checker reads `.txt` as Markdown, since `MARKDOWN_EXT` in `check_docs.py`
holds it. The findings are words and marks of tool output, such as `arena`
and an exclamation mark from the analyzer.

Fixing them means one of three changes, and none is mine to take:

- Rewrite the tool output, which falsifies the record of the audit.
- Put `docs-style:ignore-file` in the first lines of each data file, which
  changes the data and any reader that parses it.
- Leave `docs/audit/data/` out of the checker, which changes the pinned copy
  of the rules.

`CLAUDE.md` puts every comment and every `.md` file under the rules and names
the audit data as machine data. Which of the three, or none, is wanted?
