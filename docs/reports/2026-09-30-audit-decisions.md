# Eddie's answers to the decisions of the second audit

A documents-only step. It records Eddie's five answers to "Decisions a
fix step needs" of `docs/audit/summary.md`. No code changed.

## What changed

- S29. A program that loads plugins cannot be built with `--no-hooks`,
  and antic refuses the combination with a message that names both. In
  `docs/decisions.md` under "Hooks and tracing", in "Hooks and tracing"
  of `docs/anti-language-additions.md` and in the overview, whose "Not
  built yet" line names the refusal. Fix step 10 builds it.
- M01. The `List`, `Map` and `IntMap` over objects of `anti.collection`
  are removed and every use moves to the generic collections. The entry
  under "Generics and collections" that kept them now says so. The
  specification already lists no such collection. Fix step 38 does it.
- `rt.configure` refuses a table `[injections]` with an error that says
  injections are fixed at start, through `--anti.conf` or `ANTI_CONF`.
  In `docs/decisions.md` under "Runtime configuration", in "Runtime
  configuration" of the additions and in the overview, whose "Not built
  yet" line names it. Fix step 31 builds it, and `conf.c` joins its
  files.
- Rule 25 of `docs/c-guidelines.md` gains its one exception: a C
  function that implements an item of an Anti module carries that
  module's mangled name, as `anti_lang_Object_*` does for
  `anti.lang.Object`. `docs/decisions.md` records it under "Names and
  shared code of the runtime". The rule 25 row of the summary marks the
  root's names as answered.
- M25. The runtime and `anti` keep their own walks of the includes, and
  no code is shared across the boundary. One shared constant holds the
  limit, and both give one wording of the message. In `docs/decisions.md` under
  "Runtime configuration". Fix step 29 builds it, and `src/rt/conf.c`
  joins its files.

The summary's section now gives each answer, its date and its fix step.

## Findings

No finding was fixed in code. S29, M01, M25 and the `rt.configure`
finding are left to fix steps 10, 38, 29 and 31. The rule 25 finding on
`anti_lang_` of the root is no defect under the amended rule.

No `[provisional]` entry was added. Every entry records a decision of
Eddie's.

## Gates

- docs-style checker on the five changed files: exit 0,
  `build/drive/logs/eddie-answers-docs-style.log`.
- host build without warnings, 1284 of 1284 tests pass,
  `build/drive/logs/eddie-answers-host-build.log` and
  `eddie-answers-host-ctest.log`.
- ASan: 1283 of 1283 pass, `eddie-answers-asan-build.log` and
  `eddie-answers-asan-ctest.log`.
- UBSan: 1283 of 1283 pass, `eddie-answers-ubsan-build.log` and
  `eddie-answers-ubsan-ctest.log`.

## State after the documents commit

The report was written after the commit below was pushed, so these
lines show the tree before the report's own commit.

```text
$ git log --oneline -3
b08f386 Record Eddie's answers to the decisions the fix steps need
1f8b5fc Add the summary of the second audit
75e54d0 Add the audit of the tests and their scripts
$ git status --short
$ git rev-parse HEAD origin/main
b08f3862adb6590614de45ded2dee6c47acc61b0
b08f3862adb6590614de45ded2dee6c47acc61b0
```
