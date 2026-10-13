# The regions of the rest of antic and of anti that no test reached

Five sessions of a step of the test run, all on 2026-10-13, on `main`. The
step does for the rest of `src/antic/` and for `src/anti/` what the step of
`docs/reports/2026-10-13-tests-coverage-front-end.md` did for the front
end. Each session ran `./c coverage` at its start and at its end, read the
regions the suite did not reach and wrote tests for them. No file under
`src/` changed. The step is not finished, and the last section lists the
rest.

## What the first session added

- 18 programs in `tests/matrix/`, 54 tests. 14 are of the first commit, for
  regions of `lower*.c`: a bound function that takes a closure, called
  twice, `as c_wchar`, the operators of a simd struct with `f16` lanes, the
  loops of one above the vector cap, a default of a parameter that holds a
  class, a constant aggregate of `size_of`, `none == v` of a `?int`, a
  constant of `size_of` shifted and widened, a walk over an array of a
  `size_of` length, a range of `c_long`, the flags form into names in
  memory, array fields of enums in a descriptor and two programs with a
  `defer` that never returns.
- The other four: `sync` on a synchronized value in place, `==` and hash of
  a struct with a Mutex, the folds of the back end over `size_of`, and
  `classes_changed_hook_of_the_second_field`.
- `dump_ast_nodes` and `dump_types_checked_nodes` in `tests/dump/`. The
  first holds the nodes of the parsed tree that no other dump held, in a
  source the checker would refuse. The second holds the nodes the checker
  writes. The two reach 96 of the 105 unreached regions of `ast_dump.c`.
- `program_hook_trace_module` passes the module of `hook_trace.anti` as the
  value of `--trace`. `matrix_threads_jobs_no_hooks` joins jobs under
  `--no-hooks`.
- `matrix_classes_changed_hook_of_the_second_field_trace_writes` runs the
  matrix program under `--trace writes`. Its expected file is
  `tests/programs/hook_changed_fields.writes.expected`.

## What the second session added

- `tests/antic_messages.cmake` with 19 cases, each a test whose name
  starts with `antic_message_`. The runner of the messages of anti runs
  antic for this table.
- Of the 19, three hold the messages of a package header. Two hold an
  output whose directory is not there. Five hold a library file that is
  missing, damaged, of the module compiled or part of a cycle.
- The other nine hold a build without the runtime archive and a tool
  that does not start. They also hold a library file as the input of
  `--dev`, a source name and a plugin name the index cannot hold.
- Five cases of `tests/run_antic_options.cmake`: `output_static`,
  `output_shared`, `output_dev_library`, `output_plugin` and `linker_lld`.
- `bitfield_defaults_modules` and `function_default_modules` in
  `tests/modules/`, and the matrix program
  `classes_default_of_a_parameter_names_a_bitfield`.
- `clib_forms`, `clib_never` and `clib_never_cpp` with the libraries
  `forms` and `never` of `tests/clib/` and their headers in `tests/dump/`.
  `anti_bind_header` writes both headers from the library files.
- `anti_doc_forms_html` and `anti_doc_forms_private` over the module
  `forms` of `tests/doc/`, through `tests/run_anti_doc_page.cmake`.
- Four cases of `tests/check/messages.cmake`, two usage cases of
  `anti new` and three lines of `tests/unit/test_userdirs.c`.

## What the third session added

- `damaged_bytes` in `tests/unit/test_modules.c`. It changes one byte at
  every position of two library files, to two values each, and reads the
  file as the driver does. No change may end the process, and the
  sanitizer builds check each read. The group reaches 150 regions of the
  reader, the tree reader and the two verifiers that no poke had reached.
- 11 cases of `tests/anti-symbols/messages.cmake`. Six are of
  `inventory` and `check`: a configuration and an include that are not
  there, a directory without a program, `plugins` as one text and an
  injection that an include holds as well.
- The other five are of `resolve`. Three hold a trace that is not there,
  a map without its build line and a trace with the line ends of Windows.
  Two hold an archive of a map alone and an index that names no module.
- `broken_blocks` in `tests/unit/test_zip.c`: sixteen deflate streams
  that the inflater refuses at one place each, and a stored block inside
  a deflate entry. `directory_out_of_order` reads a central directory in
  the other order than its local headers.
- `anti_bind_forms`, `anti_bind_values` and `anti_bind_functions` with the
  headers `forms.h`, `values.h` and `functions.h` of `tests/bind/`. Each
  holds its module and its warnings, and the first and the third compare
  the two probes.
- `anti_bind_huge_enum` and `anti_bind_nested_enum` with `huge.h` and
  `tagged.h`, and two cases of `tests/bind/messages.cmake`. The four show
  T117 to T120.

## What the fourth session added

- Three refusals in `tests/errors/`. `inject_cycle_outside` holds a cycle
  of providers that the walk reaches from a provider outside it, and
  `inject_plugin_empty` the provider `plugin:` without a library.
  `layout_divide` holds a size expression that divides by zero on its
  target.
- `antic_message_provider_names_two_functions`: a provider whose path is
  the `get` of a singleton and a function of a module below it.
- Two programs in `tests/matrix/`, six tests.
  `generics_copies_with_the_same_code` holds copies of a generic with a
  float constant and with a length computed from `size_of`.
  `values_c_wchar_operators` takes each operator of a `c_wchar` at run
  time.
- `llvm_coff_plugin_f16` with the library `halves` of `tests/plugin/`,
  which shows T121. `tests/run_llvm_coff_plugin.cmake` takes the library
  and the level of the processor as values.
- `dump_llvm_wchar_constant_macos-arm64`, which shows T122, and
  `dump_llvm_wchar_constant_linux-arm64`, which passes.
- `odd_sources` in `tests/unit/test_fmt.c`, twelve sources of one rule
  each, and `platform_links` in `tests/unit/test_link.c`, five commands
  of `--linker platform` and of a DLL whose name has no dot.
- Five cases of `tests/anti-build/messages.cmake`: four lock files that
  `anti build` does not read, and a repository named as
  `file://localhost/`.
- `anti_bind_api_parts`, `anti_bind_api_callbacks` and
  `anti_bind_api_literals`, each with a description, a module and its
  warnings in `tests/bind/`.

## What the fifth session added

- Five cases of `tests/run_anti_options.cmake`. `targets_default` builds
  for the targets that `default` of `[targets]` names. `release_path`
  builds a path dependency under `--release`. `library_root` builds a
  library project without the module of the package name.
  `symbols_target` runs the inventory beside a program of Linux and one
  of Windows, and `bind_target` binds one header for three targets.
- Eight cases of `tests/anti-build/messages.cmake`. Three are a path
  dependency: its manifest is no TOML, it is named from the root, or its
  library file is damaged. Three are a module: one does not parse, the
  name of one is no identifier, and two import each other. The last two
  are a lock file that is no TOML and `anti run --memory-checks` over
  lines that are no frames.
- Four cases of `tests/anti_messages.cmake`, three of
  `tests/bind/messages.cmake` and four of `tests/doc/messages.cmake`.
  They hold a program and an archive that are not there, a member of an
  archive without its size, a version of the SDK with another sign, a key
  that is a malformed string, a part that is `null` and a damaged library
  file as the input of `anti doc` and of `anti bind --header`. One of the
  cases of `anti doc` shows T123.
- `refused_forms` in `tests/unit/test_ir.c`: nineteen instructions and
  records that the verifier of the IR refuses, each changed in one value.
- `global_reloc` in `tests/unit/test_optimize.c`: a global that the bytes
  of another global name stays, under its new index.
- In `tests/unit/test_coff.c`, `test_damaged_bytes` sets every byte of
  an object to three values. `test_codeview_kinds` and six more
  refusals stand beside it, among them a big object and a weak external
  without its default.
- `clib_views` with the library `views` of `tests/clib/`, its header in
  `tests/dump/views.h` and a C program. The library holds sixteen forms of
  the header that no other one held.
- `plugin_refused_needy` and `plugin_refused_holder`: a library whose
  provided class the loader cannot build without arguments.
- The matrix program
  `values_float_the_optimizer_knows_out_of_unsigned_range`, three tests.

## The coverage of each file

The table holds every file of `src/antic/` outside the front end and every
file of `src/anti/`, with the regions that no test reached at the start of
the step and after each of the five sessions, and the share of regions
and of lines the suite reaches. `build/drive/logs/` holds the rows in
`c4-before-summary.txt` and in `c5-`, `c6-`, `c7-` and `c8-` with
`before-summary.txt` and `after-summary.txt`. One list of unreached
regions per file stands in the directories `c5-` to `c8-` with
`before-missed/` and `after-missed/`.

| File | Regions | Missed at the start | After session 1 | After session 2 | After session 3 | After session 4 | After session 5 | Regions at the start | Regions now | Lines at the start | Lines now |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `anti/bind.c` | 91 | 12 | 12 | 12 | 8 | 8 | 7 | 86.81% | 92.31% | 88.68% | 93.71% |
| `anti/bindapi.c` | 527 | 46 | 46 | 46 | 46 | 14 | 14 | 91.27% | 97.34% | 89.29% | 97.69% |
| `anti/bindclang.c` | 1250 | 100 | 100 | 100 | 61 | 61 | 41 | 92.00% | 96.72% | 93.29% | 97.63% |
| `anti/bindexpr.c` | 507 | 51 | 51 | 51 | 12 | 12 | 12 | 89.94% | 97.63% | 89.21% | 97.26% |
| `anti/bindmodel.c` | 32 | 2 | 2 | 2 | 2 | 2 | 2 | 93.75% | 93.75% | 88.68% | 88.68% |
| `anti/bindtype.c` | 462 | 24 | 24 | 24 | 24 | 24 | 24 | 94.81% | 94.81% | 93.44% | 93.44% |
| `anti/bindwrite.c` | 501 | 48 | 48 | 48 | 35 | 35 | 35 | 90.42% | 93.01% | 92.56% | 94.46% |
| `anti/build.c` | 563 | 39 | 39 | 39 | 39 | 39 | 26 | 93.07% | 95.38% | 94.82% | 95.86% |
| `anti/check.c` | 271 | 22 | 22 | 18 | 18 | 18 | 18 | 91.88% | 93.36% | 90.31% | 91.84% |
| `anti/cursor.c` | 63 | 2 | 2 | 2 | 1 | 1 | 1 | 96.83% | 98.41% | 96.61% | 96.61% |
| `anti/deps.c` | 775 | 45 | 45 | 45 | 45 | 39 | 33 | 94.19% | 95.74% | 93.40% | 95.67% |
| `anti/doc.c` | 550 | 75 | 75 | 24 | 24 | 24 | 19 | 86.36% | 96.55% | 85.74% | 94.99% |
| `anti/files.c` | 259 | 21 | 21 | 21 | 21 | 21 | 21 | 91.89% | 91.89% | 91.53% | 91.53% |
| `anti/fmt.c` | 1335 | 36 | 36 | 36 | 36 | 19 | 19 | 97.30% | 98.58% | 97.02% | 98.47% |
| `anti/jsontree.c` | 223 | 13 | 13 | 13 | 13 | 13 | 10 | 94.17% | 95.52% | 91.63% | 94.14% |
| `anti/licensing.c` | 251 | 6 | 6 | 6 | 6 | 6 | 3 | 97.61% | 98.80% | 98.00% | 99.33% |
| `anti/main.c` | 759 | 19 | 19 | 17 | 17 | 17 | 17 | 97.50% | 97.76% | 96.43% | 96.68% |
| `anti/manifest.c` | 237 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `anti/memreport.c` | 151 | 12 | 12 | 12 | 12 | 12 | 2 | 92.05% | 98.68% | 92.72% | 99.34% |
| `anti/platform.c` | 73 | 1 | 1 | 1 | 1 | 1 | 1 | 98.63% | 98.63% | 96.43% | 96.43% |
| `anti/project.c` | 26 | 3 | 3 | 3 | 3 | 3 | 3 | 88.46% | 88.46% | 89.09% | 89.09% |
| `anti/repo.c` | 287 | 20 | 20 | 20 | 20 | 19 | 19 | 93.03% | 93.38% | 89.73% | 90.33% |
| `anti/sdk.c` | 154 | 15 | 15 | 15 | 15 | 15 | 13 | 90.26% | 91.56% | 92.24% | 93.10% |
| `anti/symmap.c` | 271 | 26 | 26 | 26 | 26 | 26 | 20 | 90.41% | 92.62% | 94.93% | 96.42% |
| `anti/syms.c` | 662 | 68 | 68 | 68 | 34 | 34 | 23 | 89.73% | 96.53% | 91.35% | 96.47% |
| `anti/test.c` | 148 | 20 | 20 | 20 | 20 | 20 | 20 | 86.49% | 86.49% | 87.17% | 87.17% |
| `anti/units.c` | 137 | 14 | 14 | 14 | 14 | 14 | 10 | 89.78% | 92.70% | 91.19% | 95.60% |
| `anti/zip.c` | 391 | 35 | 35 | 35 | 3 | 3 | 3 | 91.05% | 99.23% | 93.44% | 99.23% |
| `antic/abi.c` | 231 | 5 | 5 | 5 | 5 | 5 | 5 | 97.84% | 97.84% | 100.00% | 100.00% |
| `antic/alloc.c` | 39 | 2 | 2 | 2 | 2 | 2 | 2 | 94.87% | 94.87% | 100.00% | 100.00% |
| `antic/antic.c` | 112 | 4 | 4 | 4 | 4 | 4 | 0 | 96.43% | 100.00% | 97.24% | 100.00% |
| `antic/antl.c` | 87 | 1 | 1 | 1 | 0 | 0 | 0 | 98.85% | 100.00% | 98.37% | 100.00% |
| `antic/antl_io.c` | 91 | 3 | 3 | 3 | 3 | 3 | 3 | 96.70% | 96.70% | 96.88% | 96.88% |
| `antic/antl_read.c` | 930 | 77 | 77 | 77 | 20 | 20 | 20 | 91.72% | 97.85% | 88.68% | 98.25% |
| `antic/antl_read_ir.c` | 988 | 61 | 61 | 61 | 31 | 31 | 31 | 93.83% | 96.86% | 90.23% | 94.71% |
| `antic/antl_tree.c` | 1117 | 82 | 82 | 82 | 41 | 41 | 41 | 92.66% | 96.33% | 94.42% | 97.70% |
| `antic/antl_verify.c` | 776 | 37 | 37 | 37 | 19 | 19 | 19 | 95.23% | 97.55% | 94.66% | 97.33% |
| `antic/antl_write.c` | 615 | 21 | 21 | 21 | 21 | 21 | 21 | 96.59% | 96.59% | 97.56% | 97.56% |
| `antic/applesdk.c` | 59 | 3 | 3 | 3 | 3 | 3 | 3 | 94.92% | 94.92% | 96.15% | 96.15% |
| `antic/arena.c` | 23 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/arith.c` | 228 | 6 | 6 | 6 | 6 | 6 | 6 | 97.37% | 97.37% | 100.00% | 100.00% |
| `antic/ast_dump.c` | 685 | 105 | 9 | 9 | 9 | 9 | 9 | 84.67% | 98.69% | 81.40% | 98.78% |
| `antic/ast_walk.c` | 134 | 3 | 3 | 3 | 3 | 3 | 3 | 97.76% | 97.76% | 98.79% | 98.79% |
| `antic/coff.c` | 897 | 47 | 47 | 47 | 47 | 47 | 27 | 94.76% | 96.99% | 94.59% | 96.69% |
| `antic/cpu.c` | 18 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/diagnostic.c` | 24 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/docpage.c` | 486 | 49 | 49 | 14 | 14 | 14 | 14 | 89.92% | 97.12% | 90.54% | 98.25% |
| `antic/driver.c` | 1261 | 101 | 101 | 55 | 55 | 55 | 50 | 91.99% | 96.03% | 89.99% | 94.24% |
| `antic/driver_index.c` | 103 | 10 | 10 | 4 | 4 | 4 | 4 | 90.29% | 96.12% | 93.15% | 97.26% |
| `antic/driver_library.c` | 259 | 31 | 31 | 14 | 14 | 14 | 14 | 88.03% | 94.59% | 91.23% | 94.79% |
| `antic/driver_link.c` | 328 | 57 | 57 | 56 | 56 | 56 | 56 | 82.62% | 82.93% | 85.33% | 85.33% |
| `antic/driver_search.c` | 248 | 21 | 21 | 12 | 12 | 12 | 12 | 91.53% | 95.16% | 89.59% | 93.68% |
| `antic/header.c` | 1068 | 63 | 63 | 45 | 45 | 45 | 29 | 94.10% | 97.28% | 95.88% | 98.28% |
| `antic/ir.c` | 415 | 9 | 8 | 8 | 8 | 8 | 8 | 97.83% | 98.07% | 98.29% | 98.55% |
| `antic/ir_fold.c` | 98 | 3 | 0 | 0 | 0 | 0 | 0 | 96.94% | 100.00% | 97.59% | 100.00% |
| `antic/ir_print.c` | 368 | 6 | 6 | 6 | 6 | 6 | 6 | 98.37% | 98.37% | 99.63% | 99.63% |
| `antic/ir_verify.c` | 724 | 30 | 30 | 30 | 23 | 23 | 1 | 95.86% | 99.86% | 91.69% | 99.71% |
| `antic/layout.c` | 485 | 34 | 33 | 33 | 33 | 18 | 18 | 92.99% | 96.29% | 92.97% | 95.86% |
| `antic/linker.c` | 407 | 12 | 12 | 12 | 12 | 3 | 3 | 97.05% | 99.26% | 97.90% | 99.44% |
| `antic/llvm_debug.c` | 201 | 4 | 4 | 4 | 4 | 4 | 4 | 98.01% | 98.01% | 97.30% | 97.30% |
| `antic/llvm_emit.c` | 1920 | 87 | 87 | 87 | 87 | 86 | 86 | 95.47% | 95.52% | 96.05% | 96.05% |
| `antic/llvm_run.c` | 85 | 6 | 6 | 6 | 6 | 6 | 6 | 92.94% | 92.94% | 94.96% | 94.96% |
| `antic/llvm_target.c` | 13 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/lower.c` | 888 | 11 | 9 | 9 | 9 | 9 | 9 | 98.76% | 98.99% | 99.45% | 99.45% |
| `antic/lower_check.c` | 88 | 3 | 3 | 3 | 3 | 3 | 3 | 96.59% | 96.59% | 100.00% | 100.00% |
| `antic/lower_class.c` | 251 | 14 | 14 | 13 | 13 | 13 | 13 | 94.42% | 94.82% | 97.27% | 97.48% |
| `antic/lower_desc.c` | 819 | 20 | 17 | 17 | 17 | 17 | 17 | 97.56% | 97.92% | 97.73% | 98.05% |
| `antic/lower_eq.c` | 178 | 4 | 3 | 3 | 3 | 3 | 3 | 97.75% | 98.31% | 97.49% | 97.91% |
| `antic/lower_error.c` | 114 | 3 | 3 | 3 | 3 | 3 | 3 | 97.37% | 97.37% | 97.87% | 97.87% |
| `antic/lower_expr.c` | 1256 | 29 | 26 | 26 | 26 | 26 | 26 | 97.69% | 97.93% | 98.85% | 98.85% |
| `antic/lower_function.c` | 225 | 1 | 1 | 1 | 1 | 1 | 1 | 99.56% | 99.56% | 99.02% | 99.02% |
| `antic/lower_hash.c` | 160 | 8 | 7 | 7 | 7 | 7 | 7 | 95.00% | 95.62% | 96.08% | 96.64% |
| `antic/lower_hook.c` | 120 | 14 | 10 | 10 | 10 | 10 | 10 | 88.33% | 91.67% | 88.60% | 92.98% |
| `antic/lower_owning.c` | 330 | 11 | 10 | 9 | 9 | 9 | 9 | 96.67% | 97.27% | 97.15% | 98.10% |
| `antic/lower_pattern.c` | 33 | 1 | 1 | 1 | 1 | 1 | 1 | 96.97% | 96.97% | 100.00% | 100.00% |
| `antic/lower_place.c` | 331 | 29 | 13 | 13 | 13 | 13 | 13 | 91.24% | 96.07% | 88.43% | 95.37% |
| `antic/lower_simd.c` | 202 | 25 | 6 | 6 | 6 | 6 | 6 | 87.62% | 97.03% | 91.22% | 97.23% |
| `antic/lower_stmt.c` | 1053 | 18 | 12 | 12 | 12 | 12 | 12 | 98.29% | 98.86% | 97.94% | 98.51% |
| `antic/lower_sync.c` | 118 | 3 | 2 | 2 | 2 | 2 | 2 | 97.46% | 98.31% | 100.00% | 100.00% |
| `antic/lower_worker.c` | 127 | 5 | 3 | 3 | 3 | 3 | 3 | 96.06% | 97.64% | 100.00% | 100.00% |
| `antic/main.c` | 400 | 6 | 6 | 5 | 5 | 5 | 5 | 98.50% | 98.75% | 98.59% | 98.80% |
| `antic/memcheck.c` | 158 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/modpath.c` | 121 | 3 | 3 | 1 | 1 | 1 | 1 | 97.52% | 99.17% | 97.92% | 100.00% |
| `antic/notice.c` | 58 | 1 | 1 | 1 | 1 | 1 | 1 | 98.28% | 98.28% | 100.00% | 100.00% |
| `antic/optimize.c` | 1233 | 20 | 20 | 20 | 20 | 20 | 17 | 98.38% | 98.62% | 98.10% | 98.27% |
| `antic/platform.c` | 274 | 49 | 49 | 47 | 47 | 47 | 46 | 82.12% | 83.21% | 84.32% | 84.59% |
| `antic/ptrset.c` | 81 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/reach.c` | 160 | 1 | 1 | 1 | 1 | 1 | 0 | 99.38% | 100.00% | 98.75% | 100.00% |
| `antic/rt_abi.c` | 14 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/sha256.c` | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/target.c` | 54 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/text.c` | 72 | 6 | 6 | 6 | 6 | 6 | 6 | 91.67% | 91.67% | 91.94% | 91.94% |
| `antic/userdirs.c` | 133 | 11 | 11 | 7 | 7 | 7 | 7 | 91.73% | 94.74% | 93.89% | 96.95% |
| `antic/warnings.c` | 98 | 0 | 0 | 0 | 0 | 0 | 0 | 100.00% | 100.00% | 100.00% | 100.00% |
| `antic/whole.c` | 767 | 42 | 42 | 42 | 42 | 37 | 37 | 94.52% | 95.18% | 95.26% | 96.02% |
| `antic/whole_tables.c` | 997 | 46 | 46 | 46 | 46 | 41 | 39 | 95.39% | 96.09% | 96.28% | 97.25% |
| All 96 | 37400 | 2139 | 1979 | 1779 | 1463 | 1372 | 1214 | 94.28% | 96.75% | 94.51% | 96.98% |

All files of the measurement, `src/rt/` and the front end included, went
from 3685 unreached regions of 66919 to 3526, to 3324, to 3002, to 2913
and then to 2752. That is 94.49%, 94.73%, 95.03%, 95.51%, 95.65% and
95.89% of the regions, and 94.51%, 94.83%, 95.11%, 95.63%, 95.78% and
96.02% of the lines. The start of the fifth session measured 2911, two
regions of `rt/conf.c` fewer than the end of the fourth. The
start of the fourth session measured the 3002 of the end of the third.
The start of the third measured the 3324 of the end of the second. The start of the second session measured 3526, and the end of
the first had measured 3524. Nothing under `src/` or `tests/` changed
between the two.

## The findings

- T114, severe, of the first session. Under `--trace writes` the `changed`
  hook gets the record of the first field of the class for a write of any
  field.
- T115, major. A field of a library class has a function as its default,
  and a literal that omits the field is refused in the importing module.
  The library file carries no default for the field.
- T116, major: the header of an export function that returns `never`
  declares it `_Noreturn`, which is no C++. The header compiles as C11.
- A defect without a row: antic does not free the text of
  `--license-text` when it refuses the file for a NUL byte, at line 1095
  of `src/antic/driver.c`, and LeakSanitizer reports 8 MB. antic ends
  with 1 and the right message. The run that showed it wrote
  `c5-suite-asan-1.log`, which the run after the change replaced.
  `build/asan/antic -c --license-text build/asan/antic` on any module
  shows it again. See "Where the task and the code differ".

- T117, severe, of the third session. `anti bind --clang` gives an enum
  with a value above the signed 64-bit range the base `c_int`. The struct
  that holds it has 8 bytes in Anti and 16 in C.
- T118, major. The enumerators of an enum without a name inside a struct
  are no constants of the binding.
- T119, major. `#define BIG 1e20` ends `anti bind` with 1 and `bind wrote
  api.anti, and it does not lex`. The float is written as `1e+20`.
- T120, major. `#define LOW ((int)-3)` is skipped as no constant.
- The library of the byte sweep first held `pub maybe: ?int = none` and a
  parameter `c: ?int = none`. The first is T085 and a call that leaves
  the second out is T012, so neither got a new id, and the library holds
  neither.

- T121, major, of the fourth session. A library built with
  `--no-runtime` that converts an `f16` below x86-64-v3 takes the two
  conversions of the runtime from its host. A host that converts no `f16`
  holds neither. The link of the library fails for windows-x86_64, and
  on macos-x86_64 the host fails when it loads the library.
- T122, severe. A constant of `c_wchar` computed from `size_of` folds
  with the unsigned operation on macOS and Linux x86_64, where `wchar_t`
  is signed. The same operation on a variable is signed there, so
  `LOW / 2` is 2147483644 as a constant and -4 at run time.

- T123, minor, of the fifth session. `anti doc` prints the message of a
  source that does not parse twice. The outline of the source reports it,
  and the front end of the driver reports it again.

`docs/audit/tests-findings.md` holds the rows of the ten ids.

## Regions no input reaches

A guard is a test for `NULL`, for a refused type or for a kind that no
caller passes.

- `lower.c` 145: the one caller passes `true`. 878 and 992 are guards.
- `lower_class.c` 177 and 391: the global and the thunk are asked for
  once. 503: an aggregate result is a pointer in the thunk already. 606:
  the checker refuses `T(args)` of a class without `construct`.
- `lower_place.c` 259 and 263: a static field is atomic, so it is read
  with `load()` and written with `store(v)` and is no place. 97, 202 and
  213 are guards.
- `lower_stmt.c` 919: `_` is an ordinary name, so every name of the form
  has a symbol. 980 and 1101: the checker refuses an `else` of a `let`
  that does not leave. 765, 957, 1169 and 1828 are guards.
- `lower_expr.c` 743, 745, 2145 and 2147: `lower_place` finds a place for
  every node these four pass.
- `lower_owning.c` 236, 280, 342 and 395: a struct and a tuple hold no
  `use` field. 249 and 292 are the `default` of a switch over the kinds
  with a teardown.
- `lower_worker.c` 362: `send` and `recv` refuse a pointer to a channel.
- `ir.c` 127, 429 and 577 end antic when a count leaves 32 bits. 236 is a
  `default`.
- `ir_print.c` 167 and 259: the parser refuses a struct without a field
  and a variadic function without a fixed parameter. 176 and 426 are
  guards.
- `ast_dump.c` 111 is a guard. 278: neither the parser nor the checker
  makes a descriptor node. 1030 to 1036: a function at module level has
  no contract and no `self`.
- `lower_function.c` 89: no code sets `home_file` of an item.
- `lower_class.c` 83: a library file brings the declaration of the init
  function of each of its classes, so the search finds it.
- `lower_place.c` 454: the checker folds no constant for the name of a
  function, and refuses `none` as the default of an `own` field of
  function type.
- `driver.c` 1736 and `main.c` 235 and 508: `main.c` refuses the level of
  another architecture first, and every host of the suite is a target.
- `driver_index.c` 213, `driver_library.c` 54 and 167, `platform.c` 645 to
  951 and `text.c` 55 to 87 stand behind a call of the system that fails.
- `header.c` 188: an export signature takes no slice.
- `zip.c` 145 to 148: an archive whose entries pass 4 GiB together.
- `llvm_emit.c` 3830: a notice after a bundle marker. The driver writes
  the marker for a static library alone and the notice for every other
  output. 1448: the one caller of `type_suffix` passes no float.
- `llvm_emit.c` 2595 to 2606: lowering gives a call the type its callee
  declares, apart from a pointer that the runtime returns as an `i64`.
- `driver_link.c` 23 to 53, 166 to 176 and `linker.c` 62 stand behind
  `--linker platform` on a host of Linux or a Mac without `xcrun`.
  `driver_link.c` 133 to 145 needs a host without Apple's SDK.


Of the fifth session:

- `header.c` 667 and 895: an export simd struct with `char` lanes is
  refused, and a class body declares no union.
- `llvm_emit.c` 1381 and 1433: `lane_op_ok` of the verifier takes no
  shift on the lanes of a simd struct, so no verified IR holds one.
- `reach.c` 90 and `optimize.c` 1451 are reached by `global_reloc` and by
  no program. Layout writes the relocations of a global after the
  optimizer ran.
- `coff.c` 698 to 849 stand behind an object past 4 GiB or with too many
  long section names. 979 is an internal error.
- `whole_tables.c` 1506: the checker refuses a `provides` line whose class
  is no such interface, which `error_provides_lines` holds.
- `files.c` of anti, all 21: a count past the memory at 21, 37 and 53, and
  a call of the system that fails at 142 to 152, 224 to 246 and 328.
- `jsontree.c` 58 to 180 and `alloc.c` 36 and 46 stand behind memory that
  runs out or a size of zero. `licensing.c` 230 needs a size of more than
  ten digits, which the field of an archive cannot hold.
- `sdk.c` 135 to 149, `units.c` 33 and `build.c` 1024 to 1027 and 1116
  need a host that is no Mac or no target. `sdk.c` 177 to 235 and
  `build.c` 257, 305 and 807 to 812 stand behind a call of the system
  that fails. `build.c` 689 is a Windows link that wrote no PDB.
- `llvm_run.c` 66, 71, 214 and 231 stand behind opt or a path that fails.
  `driver.c` 84, 129 and 685 are a file the system refuses, and 562 and
  631 are internal errors.
- `applesdk.c` 63 and 77 follow the SDKs the Mac holds.

## Probes that reached nothing new

A later session need not repeat them.

- The address of a name that `if let` binds, and of one that a `catch`
  with a block yields, for `lower_expr.c` 420 and `lower_stmt.c` 340.
- `h as f16` of an `f16`, for `lower_expr.c` 1577.
- `yield 5;` in the handler of a call that stands as a statement, for
  `lower_stmt.c` 1499.
- An interface function that returns a struct, for `lower_class.c` 503.
- `==` and `hash()` of a class with an interface and a bitfield, and
  `hash()` of a struct with bitfields, for `lower_hash.c` 458.
- `>>` and `<<` of two simd structs with `i32` and `u32` lanes, in release
  and in dev mode, for `llvm_emit.c` 1381 and 1433.
- An `own` field of function type with a function or `none` as its
  default, in a class of a library, for `lower_place.c` 454.
- Two libraries that each hold `link framework "CoreFoundation";` under
  one program, for `driver_search.c` 210 and 214.
- A warning under `anti check`, for `check.c` 494: the count stayed 0.
- `size_of(c_long) +| 1` and `mul_high(size_of(c_long), 2)` as constants,
  for `layout.c` 421: both fold.
- A worker that reads `part[0].both[1].b` of a slice of structs, for
  `whole.c` 576. The program holds a singleton with a `mutable` field.
  Lowering writes no symbolic operation as the offset of an address.
- A doc line with 200 spaces before its text, for `fmt.c` 1189, and a
  comment between two arguments, for `fmt.c` 905.

- `license_text` of `anti.toml` that names a file which is not there, for
  `build.c` 389. `anti build` ends with `antic: cannot open ./NOPE.txt`.
- `"Version": "15."` in `SDKSettings.json`, for `sdk.c` 72. The case of
  `../14` reached the same line.
- `anti doc` of a source that parses and names an unknown name, for
  `driver.c` 1491.

## Rules the documents do not state

- The default `==` and the default hash of a struct that holds a Mutex
  pass over the lock and take the other fields.
- A `defer` whose block calls a function that returns `never` is taken.
  The function ends inside the defer, on `return;` and on `return v;`.
- `as c_wchar` of an `i64` and of a `u16` is taken, and `as` back to each.
- A default of a parameter may be a literal that holds a class value. An
  array of classes, a struct with a class field and a tuple with one are
  taken.
- `(r, f) = e;` takes a name `r` whose address the program took.
- A length computed from `size_of` is refused in the field of a class and
  in the default of one. The field of a struct that the class holds takes
  it.
- `antic -I . -c com/example/lib.anti` gives the module the path `lib`.
  The root and the source agree when both are absolute.
- A search root that holds the library file of the module antic compiles
  is refused with `holds module ..., which this command compiles`.
- `-o` of a program in a directory that is not there names
  `<output>.ll` in its message, the first file of the build.
- `pub own call: ?fn(int) -> int = none` is refused with
  `` an `own fn` value is not copied by `=`, use `dup` ``.
- A fence of a doc comment whose tag is `anti` and a space is an `anti`
  block, and `anti fmt` leaves the space. A fence that nothing closes
  holds no block, and `anti check` says nothing of it.
- `anti doc` links `` `com.example.forms.Level` `` and the name of an
  import, and leaves `` `forms.Level` `` of the module's own last segment
  as text. It writes `u8` as `byte`, `f64` as `float` and `c_int` as
  `i32`.
- The plugin index refuses a library name with both kinds of quote.
- A default of a parameter that is a case of a variant, `s: Shape =
  Shape.Empty`, is refused with `a variant is not a constant expression`.
- `anti symbols resolve` answers no frame from an archive that holds a
  map and no debug link. It prints each line without the CR of a line of
  Windows, and gives the last line the newline it lacks.
- An index of a symbols archive may hold a key outside its `module`
  tables, and a key of a table before the first `module` line. Both name
  nothing.
- A text of `plugins` in the runtime configuration may hold an empty
  directory between two `:`, which `anti symbols` passes over.
- `anti bind` skips a macro with `!`, as `(!0)`, and a text with an escape
  other than `\t`, `\n`, `\r`, `\\`, `\"` and `\'`, as `"bell\a"`.
  "Constants" in `docs/notes/bind.md` names the unary operators and the
  literals of C without a list.
- `anti bind` takes a compound literal with a comma after its last value,
  skips two macros that name each other, and drops a macro that `#undef`
  takes back. `#pragma pack()` ends a pack of 1.
- A struct without a name that is the type of a field `plain` of `Outer`
  is bound as `Outer_plain`. An array of two levels as a parameter is
  `?*byte`, and a signed bitfield of one bit is `c_int : 1`.
- The inflater of `anti` takes a stored block inside a deflate entry, and
  a central directory in another order than the local headers.

- A class may have a name in small letters, as `singleton class console`.
- A provider whose path is the `get` of a singleton and a function of a
  module below it is refused as naming more than one function.
- The formatter keeps a comma before the first name of a direct import
  and sorts the names after it. A file that ends after a `<` gets its
  newline, and `x<int>(y);` as the first tokens of a file is a call.
- A lock file whose version no longer answers the manifest, or whose
  repository is another one, is passed over without a message.
- A description of rlparser may hold a function without its name or its
  result, a struct without its name and an enum without its values. Each
  is passed over without a warning. So is a define without its name, its
  kind or its value. A function without `params` takes none.
- A field without its name or its type is passed over as well, and the
  struct binds without it. Its layout then differs from the one of C,
  and no warning says so.
- A define of the kind `FLOAT` whose value is a whole number is written
  with `.0`.
- Under `--linker platform`, a Linux program that hosts plugins links
  with `--export-dynamic`. A Windows program that writes a profile links
  with `/NODEFAULTLIB:libcmt.lib`. A plugin of link.exe links
  `libvcruntime.lib`, `ucrt.lib` and `legacy_stdio_definitions.lib`
  without `msvcrt.lib`.
- The import library of a DLL `out.d/geo` is `out.d/geo.lib`.


Of the fifth session:

- A library project without the module whose path is the name of the
  package builds. Its archive and its header take the last part of the
  name of the package, and the header holds the functions of every
  module.
- Two modules of a project that import each other are refused with
  `` cannot find module `com.example.b` `` at the import of the first.
  "Declarations and statements" in `docs/decisions.md` makes the cycle a
  compile error and states no message. This one names a module that is
  there.
- An export constant named as a macro of `<stdint.h>`, as `SIZE_MAX`, is
  written `#define SIZE_MAX ((int64_t)7)`. The header then defines a macro
  of a header it includes again. The entry on reserved names under
  "Build tool and distribution" names a parameter and a field alone,
  which take `_`.
- `lib.instance(Interface)` gives `none` for a class the loader cannot
  build, and no function of `anti.plugin` gives the reason. The runtime
  keeps the message for `load` and `unload` alone.
- A part of an API description that is `null` is a part that is not
  there. A key that is a malformed string is refused at the column of its
  first quote.
- `anti license --from-archive` ends its walk at a member whose size holds
  no digit, and reports an archive without a package header.
- A lock file that is no TOML is passed over without a message.
- `anti run --memory-checks` writes a line of the form of a frame as it
  went in when its module is no file or no program of Anti.
- `anti symbols inventory` finds a program of Linux and one of Windows on
  a Mac, each by the build id in its file.
- The join of COFF objects finds no line table in a `.debug$S` section
  whose table runs past the section. The object after it keeps its debug
  sections.

## Where the task and the code differ

- `./c coverage` ran as `./c coverage part 1/2` and `part 2/2`, since one
  call ends after ten minutes.
- `emit_identity` pins every program of `tests/programs/`, so a new program
  there adds lines to its manifest. The program of T114 stands in the
  matrix for that reason.
- CLAUDE.md asks for a report under two pages, and the task for the
  coverage of each file. The table of 96 files makes this report longer.
- A defect gets a test that the suite expects to fail, and a line of
  `tests/known-failures.txt` names a test that fails in every tree. The
  leak of `--license-text` shows under AddressSanitizer alone. The case
  `antic_message_license_text_with_a_nul_byte` therefore runs in the host
  tree, where it holds the message, and the two sanitizer trees register
  no such case. The defect has no row and no id. A question for Eddie:
  whether the list should take a line for one tree, or such a case should
  leave the suite.

- The third session ran `./c coverage` in two parts as well, at its start
  and at its end.
- `anti symbols inventory` writes its lines of what is missing to the
  standard output, and ends with 0. The cases of the table hold them in
  `OUT`, and `check` ends with 1 for the same lines.

- The task counts an error of the back end without a position in the
  source as a defect. `antic: a size expression divides by zero on
  linux-x86_64` is one, and so is the message of `error_layout_shift`,
  which the suite held before. The entry on the size of an aggregate
  under "Libraries and runtime" in `docs/decisions.md` has the back
  end refuse with the name and the target. `error_layout_divide`
  therefore holds the message, and no id stands for it.
- T121 shows at run time on macos-x86_64 and on Linux x86_64 alone, and
  the Mac runs the first under Rosetta. Its test links for
  windows-x86_64, which every host of the suite does.
- T122 shows on linux-x86_64 as well. Its test dumps for macos-arm64
  alone, since one dump holds the defect.
- The fourth session ran `./c coverage` in two parts at its start and at
  its end.

- The fifth session ran `./c coverage` in two parts at its start and at
  its end.
- `anti_build_message_memory_checks_lines_that_are_no_frames` is
  registered on a Mac alone. Windows keeps the symbolizer of
  AddressSanitizer, and the Linux VM skips `--memory-checks` for x86_64
  under emulation.
- No document states that one mistake gets one message. The row of T035
  holds it as the rule, and T123 follows that row.

## Counts and checks of the second session

- 4515 tests on the host before and 4557 after, 369 expected failures
  before and 372 after.
- Host suite: 4557 of 4557 passed, 372 of them expected failures, two
  skipped, in 517 s, in `build/drive/logs/c5-suite-host.log`.
- ASan and UBSan: 4555 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2250` and `-I 2251,4555`. Every part passed. `asan` took 259 s,
  364 s and 291 s, and `ubsan` 329 s, 379 s and 233 s. The second and the
  third part run the setups of their fixtures again, so the three counts
  are 1150, 1112 and 2324. The logs are `c5-suite-asan-1.log` to `-3.log`
  and `c5-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c5-before-1.log` and `-2.log`, `c5-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md`, `tests/doc/forms.private.expected.md`
  and this report.

## Counts and checks of the third session

- 4557 tests on the host before and 4575 after, 372 expected failures
  before and 376 after.
- Host suite: 4575 of 4575 passed, 376 of them expected failures, two
  skipped, in 496 s, in `build/drive/logs/c6-suite-host.log`.
- ASan and UBSan: 4573 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2250` and `-I 2251,4573`. Every part passed. `asan` took 263 s,
  376 s and 292 s, and `ubsan` 248 s, 352 s and 229 s. The counts of the
  three parts are 1150, 1112 and 2342, since the second and the third run
  the setups of their fixtures again. The logs are `c6-suite-asan-1.log`
  to `-3.log` and `c6-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c6-before-1.log` and `-2.log`, `c6-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.
- `unit_modules` with the sweep took 6.6 s in the host suite, 46 s under
  ASan and 107 s under UBSan. Before it took 3.2 s, 10 s and 54 s. The
  first part of each sanitizer suite took no longer for it, since the
  longest tests of that part set its time. One value per byte in place of
  two halves what the sweep adds. The value 0 reached 12 regions of
  `antl_verify.c` that the turned bit did not.

## Counts and checks of the fourth session

- 4575 tests on the host before and 4596 after, 376 expected failures
  before and 378 after.
- Host suite: 4596 of 4596 passed, 378 of them expected failures, two
  skipped, in 528 s, in `build/drive/logs/c7-suite-host.log`.
- ASan and UBSan: 4594 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2250` and `-I 2251,4594`. Every part passed. `asan` took 314 s,
  410 s and 293 s, and `ubsan` 259 s, 390 s and 232 s. The counts of the
  three parts are 1150, 1112 and 2363. The logs are `c7-suite-asan-1.log`
  to `-3.log` and `c7-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c7-before-1.log` and `-2.log`, `c7-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.

## Counts and checks of the fifth session

- 4596 tests on the host before and 4626 after, 378 expected failures
  before and 379 after.
- Host suite: 4626 of 4626 passed, 379 of them expected failures, two
  skipped, in 502 s, in `build/drive/logs/c8-suite-host.log`.
- ASan and UBSan: 4624 tests each, in three parts, `ctest -I 1,1150`,
  `-I 1151,2250` and `-I 2251,4624`. Every part passed. `asan` took 267 s,
  377 s and 302 s, and `ubsan` 254 s, 377 s and 232 s. The counts of the
  three parts are 1150, 1112 and 2393. The logs are `c8-suite-asan-1.log`
  to `-3.log` and `c8-suite-ubsan-1.log` to `-3.log`.
- Coverage: `c8-before-1.log` and `-2.log`, `c8-after-1.log` and `-2.log`.
- Every tree built with zero warnings. `emit_identity` and
  `link_identity_macos-arm64` pass, and no expected file of either changed.
- The docs-style checker reports nothing on `CLAUDE.md`,
  `docs/audit/tests-findings.md` and this report.

## Proof

`git log --oneline -3` after the push of the tests and of the counts of
the fifth session, before the commit of this report:

```text
5f2b9022 State the counts of the fifth session on the rest of antic in CLAUDE.md
48a920c1 Bind one header for Linux and for both targets of Windows
13db037a Find a program of Linux and one of Windows in the symbols inventory
```

`git rev-parse HEAD origin/main` after that push:

```text
5f2b902293bfd4723cffc49f4c5c544023c55085
5f2b902293bfd4723cffc49f4c5c544023c55085
```

`git status --short` printed this report alone then. The reply of the
session holds the three outputs after the last push, since the hash of
this commit is not known inside it.

## Left for the next session

The scripts stand in `build/cov8/`, copies of those of `build/cov7/`:
`missed.py`, `ctx.py`, `hit.py`, `left.py`, `p.sh`, `pm.sh`, `pc.sh`,
`mk.sh` and `unit.sh`. `hprobe.sh` builds a static library and names the
regions of the header writer it reached, and `proj.sh` writes a starter
project for `pc.sh`. `build/cov8/after/` holds the list of each file after
this session, and `build/drive/logs/c8-after-missed/` a copy. The counts
below are unreached regions after it, as `c8-after-summary.txt` counts
them.

Lowering, read in the first session and still open:

- `lower_place.c` 517 and `lower_expr.c` 2125: a struct of `anti.lang`
  and an `extern fn` that a library file brought.
- `lower_class.c` 207 and 362 and `lower_desc.c` 1549: a singleton of a
  library, and a table entry that neither the class nor the interface
  fills.
- `lower_hook.c` 45, 77, 93, 129, 142 and 165.
- `lower_desc.c` 519, 746, 826 to 830, 929 to 934, 967, 1039, 1092 and
  1341: a second request for a global of a class, and `--no-reflect`.
- `lower_expr.c` 304, 378, 592, 993, 1191, 1772, 1803, 1870, 1948, 2095,
  2097, 2116 and 2246. `lower_stmt.c` 1197 and 1494.
- `lower_eq.c` 100, 139 and 355, `lower_hash.c` 345, 346, 393 and 458 to
  462, `lower_error.c` 116, 127 and 178, `lower_sync.c` 215 and 217.
- `lower_simd.c` 126, 543 and 545, `ir.c` 607, 608 and 611 and
  `ir_print.c` 234 and 236.

The drivers and the header:

- `driver.c` 50. Open: 437, 596, 720, 733, 752, 982, 1126 and 1347. From
  1440 to 1636 a library file as the input of `anti doc` and of `anti bind
  --header` whose own import is damaged or holds no such module. A case
  of a table of `anti` runs no antic, so it writes no library file.
- `driver_index.c` 4, `driver_library.c` 14, `driver_search.c` 12, `main.c` 5, `modpath.c` 1, `userdirs.c` 7.
  Open in `driver_search.c`: 160 and 195 to 214.
- `driver_link.c` 56: 306, 324 to 329, 387, 411 and 458 were read and not
  probed. The rest stands in "Regions no input reaches".
- `header.c` 29: 37, 113, 141, 323 to 351, 533, 708, 771, 1019, 1171,
  1407 and 1629. 1171 is a synchronized export class with a `construct`.
  `attempt` of the library `views` takes a function that may fail and
  did not reach 141. `docpage.c` 14.
- `doc.c` 19, `check.c` 18, `main.c` 17, `test.c` 20, `project.c` 3. What is left of
  `check.c`, `main.c`, `test.c` and `project.c` stands behind a file the
  system refuses to write. `doc.c` was read again in no session.

The library file:

- `antl_io.c` 3, `antl_read.c` 20, `antl_read_ir.c` 31, `antl_tree.c` 41, `antl_verify.c` 19, `antl_write.c` 21.
  The sweep changes one byte, so a refusal that needs two fields to
  disagree stays open. `antl_read.c` 113 to 133, 159, 460 and 499,
  `antl_read_ir.c` 193, 244, 387, 739 to 761 and 914 to 1188, and
  `antl_tree.c` 200 to 220, 376 to 397, 436, 533 to 570, 1113, 1121 and
  1272. A library with an interface and a `may fail` function in the
  sweep reaches ten more regions of `antl_read_ir.c` and costs 5 s.
  `antl_write.c` was not read.

The whole program, the IR and the back end:

- `whole.c` 37: 54, 77, 147, 182, 303, 334 and 417 are guards. 320 and
  322 need two symbolic operations that differ in their second operand
  alone. 548 needs a global with a relocation of a function. 569 to 581
  need a symbolic operation as the offset of an address, which a unit
  test of `tests/unit/test_whole.c` can build.
- `whole_tables.c` 39: 244 to 513 are guards of a signature and of a
  descriptor that a library file brought. 1454 is a class that provides
  an interface whose version records a library file lacks. 787, 862,
  1111, 1360 and 1593 were not probed.
- `layout.c` 18: 421, a size expression of an operation that does not
  fold. 713 and 722, a constant of no value and an `f32` in the
  aggregate of a global. 23, 71, 186, 241, 611 and 762 were not probed.
- `llvm_emit.c` 86: most are tests for `e->failed`. Open beside them
  are a name that needs quotes at 189 to 220 and a pointer constant at
  276. So is a flag apart from its operation at 1716 and 1776. The last
  is an extra argument of a callee that is not variadic, at 2850.
- `optimize.c` 17: 189 and 250 are the `default` of two switches. 689,
  765 and 1572 are a block without an instruction. 913 to 928 are the
  kinds of an offset that `same_operand` compares, and 1266 a use of a
  slot at an offset no field has. A unit test can build each.
- `llvm_debug.c` 4: 42 is a path with a quote, a backslash or a byte
  outside ASCII under `-g`. 22, 25 and 244 were not probed.
- `ir_verify.c` 1, `abi.c` 5, `arith.c` 6, `ast_walk.c` 3: not probed.
  `ast_walk.c` 56 and 153 are an arm of a `switch` and a range after `in`
  inside what a walker visits.
- `coff.c` 27: 1156, a long name of a symbol of an archive member
  without its NUL. The rest stands in "Regions no input reaches".
- `linker.c` 3, `platform.c` 46 of antic, `text.c` 6, `notice.c` 1.

`src/anti/`:

- `build.c` 26: 384, a manifest without a version, which the manifest
  refuses first. 537 to 554, the closure of the libraries of a dev build
  that fails. 672 and 677, a release build whose driver or map fails.
  894, a framework link that fails.
- `deps.c` 33: 380 to 383, a path dependency whose library file names
  dependencies of its own. 629, a dependency of an index whose URL is
  refused. 1126 to 1134, two graphs that differ in a module or a digest.
  194 to 261, 399, 433, 456, 464, 664 and 1058 were not probed.
- `syms.c` 23: 77, 86, 182, 236, 539, 584, 672, 677, 774 to 790 and 840
  to 906. The ELF twin from 933 to 1001 needs the trace of a program of
  Linux and its debug link.
- `symmap.c` 20: a notice or a list of sources whose last line has no
  newline, at 213 to 230, 266 to 293 and 346. 54, 116, 163, 194, 375 and
  398 were not probed.
- `memreport.c` 2: 95, a universal file without the processor that the
  frame names.
- `bindclang.c` 41: a typedef that names itself at 433, 461, 514, 539 to
  553, 622 to 667, 801, 893, 940, 960, 1053, 1124, 1182, 1231, 1273 to
  1282, 1439 and 1512.
- `bindexpr.c` 12: 53, 345, 377, 530, 537, 542, 634 and 688.
  `bindwrite.c` 35: 105 to 145, 202, 245, 330, 425, 447 and 553 to 658.
  `bind.c` 7, `bindtype.c` 24 and `bindmodel.c` 2 were not read.
- `bindapi.c` 14: 138 to 162, the tag of an enum and a pointer to a
  type that does not parse.
- `fmt.c` 19: 122, 385, 905, 1124, 1154, 1189, 1227, 1272, 1293, 1394,
  1479 and 1615. 1884 and 1904 stand behind a file the system refuses.
- `repo.c` 19: 180, the path of Windows in a `file://` URL. 237 to 270
  need a server. 386 to 402 are the four checks of `repo_module`.
- `sdk.c` 13, `units.c` 10, `jsontree.c` 10, `licensing.c` 3, `files.c` 21, `cursor.c` 1, `platform.c` 1, `zip.c` 3:
  see "Regions no input reaches". `units.c` 55, 113 and 121 were not
  probed.
