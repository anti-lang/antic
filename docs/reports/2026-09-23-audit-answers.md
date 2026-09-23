# Eddie's answers to the open audit questions

Documents only. The section "Open for Eddie" of `docs/audit/summary.md` is
now "Answered by Eddie", and the fix steps that waited for an answer name
it.

## Findings

- S32. Answered, left for fix step 13. The two entries under "Object model"
  in `docs/decisions.md` now write a pointer, a slice or a function pointer
  the object does not own as `null`, and `deserialize` accepts only `null`
  there. The reason is recorded. The code still reads addresses until step
  13 changes `src/rt/registry.c`.
- M29. Answered, left for fix step 19. Rule 22 of `docs/c-guidelines.md`
  and "Repository layout" in `docs/decisions.md` name the platform files.
  None of them exists yet.
- Rule 25. Answered, left for a later minor step. Rule 25 now names
  `anti_rt_` as the one runtime prefix.
- Rule 15, M31. Answered. "Severity" in `docs/c-guidelines.md` grades a
  reader without a malformed-input test as major, which is the grade M31
  already had.

## Left

- `docs/anti-object-model.md` still says that `serialize` writes other
  pointers as addresses. The step names `docs/decisions.md` alone, which
  outranks the specification, and its entry records that Eddie replaced
  that sentence. Whether the specification and
  `docs/anti-syntax-overview.md` change with it is Eddie's call.
- The task named a guideline test. No test reads `docs/c-guidelines.md`,
  and the step named none to write, so none was added.

No `[provisional]` decision was made.

## Gates

- Docs-style checker: 0 errors, 0 warnings on every touched file,
  `build/drive/logs/docs-style-answers.log`.
- Suites, run although only documents changed: host 792 of 792, ASan
  791 of 791, UBSan 791 of 791. Logs in
  `build/drive/logs/answers-ctest-{host,asan,ubsan}.log`.

## Proof

Taken after the push of the answers, before the commit of this report.

```
$ git log --oneline -3
a4d4a79 Record Eddie's answers to the open audit questions
895688a Report the applied provisional review
8c3160f Add the compound assignments of the wrapping and saturating operators
$ git status --short
?? docs/reports/2026-09-23-audit-answers.md
$ git rev-parse HEAD origin/main
a4d4a79778332b15a3f72bba1eb3a2ff20bba7c5
a4d4a79778332b15a3f72bba1eb3a2ff20bba7c5
```
