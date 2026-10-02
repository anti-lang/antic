# Fix step 40, the minor findings of anti

Step 40 of "Fix steps" in `docs/audit/summary.md`, the session for `anti`:
the rows of the minor table whose places lie in `src/anti/`. The detail is
in `docs/audit/anti-tool.md`, and the row on names spelled twice in
`docs/audit/structure.md`. Logs are in `build/drive/logs/s40a-*`.

Status: green and pushed.

## Findings

| Rule or question | Result |
|---|---|
| Q1, second purposes, `src/anti/syms.c:1251` | Fixed in `3f028a0d`, except `files.c`. The run of a `--memory-checks` program is `memreport.c`, the cursor over a foreign line is `cursor.c`, the binding model is `bindmodel.c` and `anti new` is `project.c`. The grammar of a version moved to `repo.c`, so `repo.c` and `manifest.c` no longer include `deps.h`. `manifest_read` reads `anti.toml` once, and the default layout stands once. |
| Q1, `files.c` | Left. Two provisional entries in `docs/decisions.md` keep the allocation helpers in `files.c` under the prefix `files_`: "Every allocation of `anti` goes through `files_array`, `files_resize` or `files_grow` of `files.c`". |
| Q3, names spelled twice, `src/anti/syms.c:32` | Fixed by step 29 in `fa5a8dc5`. |
| 1, `src/anti/bindmodel.h:143` | Fixed in `86744666`. `bind_warn` and `refuse` take `ATTRIBUTE_PRINTF`, and `refuse` takes its arguments as printf does. |
| 5, `src/anti/jsontree.c:82` | Fixed in `1eea6716`. The JSON tree checks the product before `realloc` and keeps its own growth, as the provisional entry on the allocations of `anti` says. `bind_list_add` and `arg_add` had moved to `files_grow` before this step. |
| 12, `src/anti/check.c:474` | Fixed in `7869e53e`: `check_run`, `syms_check`, `syms_resolve`, `compile_imports` and `fmt_source`. `manifest_read` and `bind_read_api` followed in `3f028a0d` and `dcbcb75e`. |
| 14, `src/anti/bindwrite.c:263` | Fixed in `dcbcb75e`. A name of an API description that is no C identifier is refused with its kind. `anti_bind_api_malformed` failed before (`s40a-red-names.log`). |
| 18, `src/anti/check.c:441` | Left for step 41, which takes the splits, as the other step 40 sessions did. |
| 24, `src/anti/test.h:21` | Fixed in `57105b7f`. The lists of `struct options` are `const char *const *`, and so is every list `anti` passes on. antic's `main` fills arrays of its own, and three copies that put the input before the libraries are one helper. |
| 25, `src/anti/jsontree.h:48` | Not a defect. The provisional entry on the prefixes of `anti` reads "The module `jsontree` keeps `json_`, which all its names share", and it binds until Eddie changes it. |
| 26, `src/anti/deps.c:457` | Fixed in `840aba91`: one TOML lookup, one suffix test and one suffix cut, the type words of the lexer, and one reader of the link names. `library_module` and the exits on an unknown host were fixed before this step. Step 27 moved two of the three name comparisons into antic, and `same` of `doc.c` is the one left in `anti`. |
| 27, `src/anti/doc.c:780` | Fixed in `664a7bc6`, with the DESIGN comment of a signature that step 27 moved to `src/antic/docpage.c`. The comment of `units.c:1` was fixed before. |

The four defects of `anti-tool.md` outside the rules were fixed by
`docs/reports/2026-10-02-fix-unnamed-defects.md`, except the resolver
that keeps every requirement, which the first audit left open.

## Provisional entries added

- An API description is refused as a whole when a name it gives is no C
  identifier, under "The bind command".

## Gates

The build has no warnings in the three trees (`s40a-final-build.log`,
`s40a-asan-build.log`, `s40a-ubsan-build.log`). The docs-style checker
reports nothing on `docs/decisions.md`, `docs/notes/bind.md` and this
report.

| Suite | Passed | Log |
|---|---|---|
| host | 1382 of 1382 | `build/drive/logs/s40a-final-host.log` |
| asan | 1381 of 1381 | `build/drive/logs/s40a-asan.log` |
| ubsan | 1381 of 1381 | `build/drive/logs/s40a-ubsan.log` |

## State

Taken after the push of the code and before the commit of this report.

```console
$ git log --oneline -3
3f028a0d Give each file of anti one purpose
664a7bc6 Put the comments of anti doc above the code they describe
840aba91 Write the small helpers of anti once
$ git status --short
$ git rev-parse HEAD origin/main
3f028a0db80c645e1635f4b06a9481ede18d918c
3f028a0db80c645e1635f4b06a9481ede18d918c
```
