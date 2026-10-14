# The rules of modules and of C

The step built R121 to R135 of "Rules of 2026-10-13" in
`docs/decisions.md` in one session, on 2026-10-14. R134 waits for Eddie,
and the step did not touch it.

## The rules

A name `matrix_<name>` without `_refused` stands for the three tests
`_dev`, `_release` and `_memory_checks`.

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R121 | Keep | `matrix_modules_import_plain_and_direct_refused`, `matrix_modules_qualified_names` | before |
| R122 | Keep | `matrix_modules_file_clauses` | before |
| R123 | Keep | `matrix_modules_module_by_its_whole_path_refused`, `matrix_modules_qualified_names` | before |
| R124 | Keep | `error_modules_module_of_a_dependency` | before |
| R125 | Change | `anti_build_message_modules_import_each_other`, `anti_build_message_modules_import_in_a_cycle_of_three`, `anti_check_message_modules_import_each_other`, `anti_doc_message_modules_import_each_other` | `60fba7c5` |
| R126 | Change | `matrix_modules_extern_function_takes_a_slice_refused`, `_gives_a_slice_refused`, `_takes_a_tuple_refused`, `_takes_an_optional_value_refused`, `_callback_takes_a_tuple_refused`, `_takes_an_own_parameter_refused`, `matrix_modules_extern_function_takes_a_char`, the five tests of T089 | `50d8c5cf`, `b9517e41` |
| R127 | Change | `error_modules_extern_function_with_two_signatures`, `error_modules_extern_function_with_another_signature_than_a_library` | `94ea8614` |
| R128 | Change | `matrix_modules_variadic_argument_of_a_c_long`, `matrix_modules_variadic_argument_of_an_f32_refused`, four other `matrix_modules_variadic_argument_*_refused`, `unit_sema` | `00119f45`, `b9517e41` |
| R129 | Change | `matrix_modules_export_of_an_operator_function_refused`, `matrix_modules_export_of_an_operator_function_of_a_class_refused`, `error_overloads`, and `clib_header` for the kept half | `a85e5e85` |
| R130 | Change | `matrix_modules_export_constant_named_as_a_macro_of_stdint_refused`, `matrix_modules_export_function_named_as_a_macro_of_stddef_refused`, `matrix_modules_export_struct_named_as_a_type_of_stdint_refused`, `matrix_modules_export_names_beside_the_c_library` | `cae1afcc`, `beac2c74` |
| R131 | Change | `error_modules_library_exports_nothing`, `antic_message_library_exports_nothing_allowed`, `warning_names` | `e9190f61`, `beac2c74` |
| R132 | Change | `antic_message_library_shared_bundles_the_runtime`, `antic_message_library_static_without_the_runtime`, `antic_message_library_static_in_dev_mode`, `antic_option_bundle_shared` | `f94b581d` |
| R133 | Keep, message changed | `plugin_module_named`, and `plugin_module_not_imported` for the failed load | `b2717ff8`, `e2e60ec4` |
| R135 | Keep | `antic_message_plugin_name_with_both_quotes` | before |

Each test of a Change failed before the code changed, in
`mod-r125-red.log`, `mod-r128-red.log`, `mod-r129-red.log`,
`mod-r130-red.log`, `mod-r131-red.log`, `mod-r132-red.log`,
`mod-r127-red.log` and `mod-r133-red.log` of `build/drive/logs/`. The
refusals of R126 printed nothing in the session before the change.

## What changed

- R125. `unit_order` of `src/anti/units.c` reports the cycle as
  `a -> b -> a` and fails, and `anti build`, `anti check` and `anti doc`
  end with 1 before any module compiles.
- R126. `sema_check_extern_fn` refuses a `str`, a slice, a tuple and a
  `?T` in the signature, its pointers, arrays and callbacks, and an `own`
  parameter. An `extern fn` named `anti_rt_` keeps them.
- R127. The reader of a library file and lowering compare two
  declarations of one C function as C passes them.
- R128. `c_long` and `c_ulong` are variadic arguments. An `f32` gets a
  message that names `f64`.
- R129. `export` before an `operator fn` is refused at module level and in
  a class body.
- R130. An exported name that `<stdbool.h>`, `<stddef.h>` or `<stdint.h>`
  defines is refused.
- R131. The warning `nothing-exported` stands at 1:1, and the release
  build of a library refuses it.
- R132. `driver_run` refuses the three options before it compiles.
- R133. A plugin carries `anti_rt_plugin_modules`, and the loader reads it
  from the file when the platform refuses the library.

`docs/decisions.md` holds the readings of R126, R127, R130 and R133 under
"What the rules run added", each `[provisional]`.

## Findings

R126 closed T089, the `str` of decision 26. Its five lines left
`tests/known-failures.txt`, which holds 374 tests now, and its row names
`50d8c5cf`. No finding was opened.

## `emit_identity` and `link_identity`

`emit_identity` passes unchanged. `link_identity_macos-arm64` changed by
R133: the runtime archive holds other bitcode, ld64.lld derives the UUID
from its inputs, and the UUID, the build id the runtime keeps and the
signature differ. The code of `return42` is the same, as a link with the
old loader showed. `e2e60ec4` writes the new digest.

## Where the task and the code differed

- R126 names a slice, a tuple, an optional value and `own`. The standard
  library declares runtime functions that give a `str` or a `[]byte`, so
  a function named `anti_rt_` keeps them. The rule also refuses the `str`
  of decision 26, which closed T089, a finding the task left to the fix
  run.
- R131 makes a static library of a module that exports nothing fail,
  since the build of a library refuses every warning. The fixtures
  `shapes` and `units` carry `allow(nothing-exported)`, and the
  archiver case of the messages of antic compiles a module that exports.
- R133 holds where the platform names the symbol, glibc and dyld. A
  Windows plugin stops at its link.
- R121 says an alias with a direct import compiles. A direct import takes
  no alias, which `matrix_modules_direct_import_with_an_alias_refused`
  holds. The rule reads as a plain import with an alias beside a direct
  import, which `matrix_modules_qualified_names` holds.
- `antic -I . -c com/example/a.anti` gives the module `a`, which R147 of
  another step changes. The tests of this step pass absolute paths.

## Counts and checks

- Tests: 4878 before and 4910 after on the host. The sanitizer trees
  hold 4908.
- Host suite: every test passed, 374 of them expected failures, and the
  two `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 395 s
  and `-I 2001,4910` in 246 s, in `mod-host-suite-a.log` and
  `mod-host-suite-b.log`.
- ASan: `-I 1,1150`, `-I 1151,2250` and `-I 2251,4908` in 281 s, 382 s
  and 352 s, all passed, in `mod-asan-suite-a.log` to
  `mod-asan-suite-c.log`. ctest adds the setup tests of fixtures to a
  part, so the parts count 1150, 1112 and 2677.
- UBSan: the same parts in 253 s, 376 s and 278 s, all passed, in
  `mod-ubsan-suite-a.log` to `mod-ubsan-suite-c.log`.
- The three trees built with zero warnings, in
  `mod-host-build-final.log`, `mod-asan-build.log` and
  `mod-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every
  `.md` file the session touched.

## Not done

- Neither VM ran the suite, so no Linux or Windows host ran the new
  tests, and the glibc form of R133 ran on no host.

## Proof

`git log --oneline -3` after the push of the step:

```text
c08e0cb8 Record the step of the rules of modules and of C
e2e60ec4 Write the link digest of return42 for macos-arm64 after R133
b2717ff8 Name the module a plugin lacks in the message of its load, by R133
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main`
gave `c08e0cb80fce878567f50e6edf2bd86c210eba48` twice.
