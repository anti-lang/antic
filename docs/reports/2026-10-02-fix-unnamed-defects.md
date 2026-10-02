# Fix step: the defects no rule names

This step fixes the defects the second audit lists outside its rules. There
are six in `docs/audit/front-end.md`, one in `docs/audit/rt.md` and four in
`docs/audit/anti-tool.md`. It also fixes S7 of the first audit. Every fix
has a test that failed first. Logs are in `build/drive/logs/un-*`.

## Findings

| Finding | Result |
|---|---|
| S7, `sema_const.c:694` | Fixed in `97426525`. The value of a class literal holds the base as a value of the base, and `CONST_DEFAULT` for a field it leaves out. Lowering writes it through the one object preparer, then runs `construct`. The library format is version 75. `programs/class_literal_defaults.anti` stopped antic with SIGSEGV, and `default_modules` refused the library file as damaged. |
| Text constants, `sema_const.c:527` | Fixed in `2117cca4`. `compare_text` orders two texts by their bytes, as `==` and `<` of `str` do at run time. `programs/const_text_compare.anti` printed `SAME` as 0 before. |
| Enum value past its base, `sema.c:2475` | Fixed in `82e5112e`. `errors/enum_values.anti` takes `u8`, `i8`, `u64` and `i64` past their tops. |
| `switch` arms, `sema_stmt.c:2515` | Fixed in `a7ab4085`. Each arm is evaluated once. `errors/switch_arm_values.anti` printed one refusal three and six times before. |
| Doc names of a library class, `sema_export.c:613` | Fixed in `2d775a15`. The code stands in `sema_doc.c` now. `silent_doc_library_class` warned on `append` of `text.Builder` before. |
| Doc comment in `f"{ }"` | Fixed in `6f7a6978`. `keep_tokens` and `drop_untaken` hold the rule for the module and for every placeholder. `warning_format_doc` failed with "expected an expression" before. |
| Raw NUL in `'...'`, `lexer.c:918` | Fixed in `46f68f87`. The unit test of the lexer reported no error before. |
| `rt.configure` and `[injections]`, `rt.md` | Fixed by step 31 in `9383cd04`, as Eddie decided. `conf_configure_injections` and `conf_configure_injections_include` pass. No change here. |
| `manifest_inject_read`, `manifest.c:92` | Fixed in `984be114`. The unit test `manifest_inject_order` got the provider of `[inject]` before. |
| Resolver keeps every requirement | Fixed in `338e83fd`. A requirement belongs to the pick that made it, and `unpick` takes a pick back with them. `anti_build_resolve` was refused before. |
| `resolve_from_repo`, `deps.c:649` | Fixed in `09ab37dd`. A repository answers in one of three ways, and the search stops where one gives no answer. `anti_build_resolve` built from the other repository before. |
| `fmt_run`, `fmt.c:1896` | Fixed in `8d078d5e` and `8e40cdae`. `files_replace` writes beside the file and renames. Under `ulimit -f 1`, `anti_fmt_write` found the source cut to 1024 bytes before. |

The second repository search showed a leak in `resolve_from_repo`. It is
the name of the kept candidate, zeroed and never freed. LeakSanitizer found
it, and `1662bc49` fixes it.

`anti_fmt_write` runs on the POSIX hosts alone, since Windows has no limit
on the size of a written file. `platform_replace` for Windows is compiled
on Windows alone and has not been built in this session.

## Provisional entries added

- The value of a class literal that defaults a field or a parameter, with
  `CONST_DEFAULT`, library format 75 and the teardown of a default argument.
- A value of an enum without `=` must fit the base.
- A requirement of the walk belongs to the pick that made it.
- A repository holds an index, holds none or gives no answer, and the
  search stops at no answer.
- `anti fmt` writes `<file>.new` and renames it over the source.

The entry on constants that hold a class now names parameter defaults too.

## Gates

The build has no warnings in the three trees. The docs-style checker
reports nothing on `docs/decisions.md`, `CLAUDE.md` and this report.

| Suite | Passed | Log |
|---|---|---|
| host | 1377 of 1377 | `build/drive/logs/un-final-host.log` |
| asan | 1376 of 1376 | `build/drive/logs/un-asan.log` |
| ubsan | 1376 of 1376 | `build/drive/logs/un-ubsan.log` |

## State

```text
$ git log --oneline -3
1662bc49 Free the name of the candidate the repository search keeps
8e40cdae Capture the output of anti_fmt_write as bytes
8d078d5e Replace a formatted source only once its new form is written
$ git status --short
$ git rev-parse HEAD origin/main
1662bc49dd791ec3751a99bf79b6e947af875b33
1662bc49dd791ec3751a99bf79b6e947af875b33
```

That block shows the tree before this report was committed. The reply
of the session gives it after the report.
