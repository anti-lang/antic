# Step switch of the LLVM back end: blocked

Step `switch` of `docs/work-order-llvm-back-end.md` stopped before its first change.
The step deletes `src/antic/coff.c` and `coff.h` as part of the native back end, and
`docs/decisions.md` requires that file. No code, test or document other than this
report changed. Nothing was built and no suite ran.

## The conflict

"What stays and what goes" lists `coff.c` among the files of the native back end. It
is not one of them. It writes no code and no assembly, and neither back end calls it.
It holds two functions that the link of a Windows target uses under either back end:

- `coff_join` joins COFF objects into one relocatable object. `join_coff` of
  `src/antic/driver_library.c` calls it for `--bundle-runtime` on both Windows targets.
  No linker of the pinned release writes a relocatable COFF object.
- `coff_archive_exports` reads which members of the runtime archive a program links.
  `host_exports` of `src/antic/driver_link.c` calls it to write the `.def` file of a
  Windows program that can host a plugin, which gives the import library its plugins
  link against.

What depends on them:

- The entry on `--bundle-runtime` in `docs/decisions.md`, line 851: "COFF with the join
  of `src/antic/coff.c`". "Documentation changes" does not list that line, and "Questions
  an implementer asks" allows no other change of `docs/decisions.md` in this step.
- `docs/notes/plugins.md`, line 86, on the exports of a Windows host.
- The tests `clib_bundle_<target>` and `llvm_coff_plugin_<target>` of both Windows
  targets, the plugin tests of the Windows VM, and the unit test `tests/unit/test_coff.c`.

Deleting the file as written removes `--bundle-runtime` on Windows and the plugin host
on Windows, and the VM run of this step then cannot pass. Keeping it disagrees with the
work order and with the prompt of the step. Neither choice is mine to take.

## Question

Which of the two holds?

1. Keep `src/antic/coff.c`, `coff.h` and `tests/unit/test_coff.c`. The step deletes the
   other eight files with their headers, `target_desc.h` included. Line 105 of the work
   order then drops `coff.c`, and its count of 12,000 lines becomes about 9,800.
2. Delete `coff.c` as written. Windows `--bundle-runtime` and the Windows plugin host
   then need another design, and the entry of line 851 and the tests above change with it.

Under either answer, `struct debug_spans` and `debug_spans_free` move from
`src/antic/debug.h` to `src/antic/llvm_debug.h`. The build id and `llvm_debug.c` read them.

## State

```text
$ git log --oneline -3
38e9abd1 Report step vm of the LLVM back end
fd2bc8b6 Read line 0 of CodeView as no position
f62091a7 Read the line of an inlined callee from DbgHelp's inline context
$ git status --short
$ git rev-parse HEAD origin/main
38e9abd1f79cbefbd393003b267ccd9606665d70
38e9abd1f79cbefbd393003b267ccd9606665d70
```

No suite ran in this session, so it has no pass counts.

## Decision

Eddie decided on 2026-10-04 for option 1. `src/antic/coff.c`, `coff.h` and `tests/unit/test_coff.c` stay. "What stays and what goes" of the work order now lists what goes and what stays, file by file, with the unit tests, the goldens and `--llvm-mc`. The entry stands in `docs/decisions.md` after the entry on frame records. The redo of this step follows the work order as it stands now.
