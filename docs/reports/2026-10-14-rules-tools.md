# The rules of the commands of anti

The step built R155 to R189 of "Rules of 2026-10-13" in `docs/decisions.md`
in one session, on 2026-10-14. No rule of the step waits for Eddie.

## The rules

| Rule | Kind | Tests | Commit |
|---|---|---|---|
| R155 | Change | `anti_help_and_a_word`, the 38 cases `anti_option_<command>_<option>_without_a_value` | `6906cdac`, `aad81fab` |
| R156 | Change | `anti_build_message_dist_is_a_file`, `anti_message_sdk_import_sysroot_is_a_file` | `37733e51` |
| R157 | Change | `anti_build_message_soname_without_lib`, `_bundle_runtime_without_lib` | `7aba623c` |
| R158 | Change | `anti_build_message_cpu_level_of_all_targets` | `05d7a4c6` |
| R159 | Change | the six cases `anti_build_message_run_<option>`, `anti_build_message_run_without_the_host` | `aad81fab` |
| R160 | Change | `anti_build_message_run_a_library`, `anti_check_message_no_source` | `aad81fab` |
| R161 | Keep | `anti_option_check_root_library`, `anti_option_check_root_source` | before |
| R162 | Keep | `anti_build_message_repository_refuses`, `anti_bind_message_clang_refuses_the_header`, `anti_message_sdk_import_no_bundle` | before |
| R163 | Change | `anti_message_sdk_export_no_tar`, `anti_bind_message_clang_missing` | `65698af4` |
| R164 | Change | `anti_build_message_dependency_name`, `_dependency_name_of_a_path` | `ab9c33dc` |
| R165 | Change | `anti_build_message_path_cycle`, `_path_cycle_nine_deep` | `40c7af36` |
| R166 | Change | `anti_build_message_path_with_a_control_byte`, `_lock_path_with_a_double_quote` | `83745040` |
| R167 | Change | `anti_build_message_lock_that_is_no_toml`, `_lock_of_another_version`, `_lock_of_another_repository` | `51c64ea7` |
| R168 | Change | `anti_build_message_library_without_its_root`, `anti_option_library_root` | `2ca6d800` |
| R169 | Keep | `anti_build_message_memory_checks_lines_that_are_no_frames` | before |
| R170 | Change | `anti_bind_message_api_type_does_not_parse`, `anti_bind_api_types`, `anti_bind_api_callbacks` | `0010d4f5` |
| R171 | Change | `anti_bind_api_parts`, `anti_bind_message_api_field_without_a_type` | `0010d4f5` |
| R172 | Change | `anti_bind_message_api_field_without_a_type`, `_without_a_name`, `_that_is_no_object` | `0010d4f5` |
| R173 | Keep | `anti_bind_api_types` | before |
| R174 | Change | `docs/notes/bind.md` | `6b4f9bf1` |
| R175 | Keep | `anti_bind_forms` | before |
| R176 | Keep | `anti_bind_forms`, `anti_bind_functions` | before |
| R177 | Keep | `anti_bind_api_parts` | before |
| R178 | Change | `anti_doc_forms_html`, `anti_doc_forms_private`, `anti_doc`, `antl_scale`, `antl_generic`, `unit_modules` | `91a742af` |
| R179 | Change | `anti_check_message_block_fence_never_closed`, `_module_fence_never_closed`, `anti_doc_message_fence_never_closed`, `_module_fence_never_closed`, `anti_doc_prose_html` | `4ce05714` |
| R180 | Change | `anti_doc_prose_dev` | `4ce05714` |
| R181 | Keep | `fmt_canonical`, over `tests/matrix/types_arrays.anti` | before |
| R182 | Change | `unit_fmt` | `521a65bd` |
| R183 | Change | `anti_symbols_message_archive_map_alone`, `_archive_map_alone_of_macos`, `anti_build` | `a1548643` |
| R184 | Change | `anti_symbols_message_archive_index_key_outside_the_modules` | `7b480670` |
| R185 | Keep | `anti_symbols_message_check_build_id_with_another_letter` | before |
| R186 | Keep | `anti_option_symbols_target` | before |
| R187 | Change | `anti_message_license_from_archive_member_without_a_size` | `adbdc97d` |
| R188 | Change | `anti_message_license_runtime_without_its_text` | `f91e46fe` |
| R189 | Change | `anti_message_license_from_notice_without_a_last_newline` | `68953f4c` |

Each Change test failed before its code changed, in the `tools-r<rule>-red.log`
files of `build/drive/logs/`. R174 is a change of the note alone.

## What changed

- R155. `lacks_value` of `src/anti/main.c` reads a table of the options that
  take a value, per command.
- R156. `files_make_dirs_or_say` names the path that is a file.
- R163. `platform_set_tool` gives the process functions the name of the tool.
- R165. The context of a path dependency keeps the chain of the projects
  above it, by their resolved directories.
- R178. `types_item_spelled` reads the names an item spells, the interface
  keeps them per item, and the library file carries them. Its format is
  version 85.
- R183. The map of a Mach-O program names the address of its text on a line
  `# text`.

"What the rules run added" in `docs/decisions.md` holds the readings of
the step, each `[provisional]`. They cover R155, R156, R159, R160, R164,
R165, R167, R168, R170 to R172, R178, R179, R183, R184 and R187.

## Findings

The step closed no finding and opened none. R189 names a defect, which no
row of `docs/audit/tests-findings.md` held. `tests/known-failures.txt`
holds 374 tests, as before.

## `emit_identity` and `link_identity`

Both passed unchanged in the three suites. No rule of the step touches
the code antic emits or links.

## Where the task and the code differed

- R164. A package name has the form an index takes, which allows one
  segment. The refusal in `src/anti/repo.c` is reached by no input since,
  and `tests/diagnostics-open.txt` says so.
- R166. A double quote in a path still reaches the refusal of the lock file,
  which `anti_build_message_lock_path_with_a_double_quote` holds. Windows
  takes no such name, so the case runs on the other hosts.
- R170. A parameter without its type had the message of a type that does
  not parse. It now says `a parameter names no type`.
- R172. `api_parts.json` lost the struct `Half`, since a description with it
  is refused. Three message cases hold a struct that lost a field.
- R178. An enum without a base reads `: c_int` on its page, the base that
  "Enums" of the overview gives it. The library file cannot tell an absent
  base from one the item names.
- R187. The rule names a size without a digit. A member header that fails in
  another way keeps `carries no package header`, as the nine malformed
  archives of `license_from_archive` hold.

## Counts and checks

- Tests: 4924 before and 4947 after on the host. The sanitizer trees hold
  4945.
- Host suite: every test passed, 374 of them expected failures, and the two
  `mimalloc_environment` tests skip. It ran as `-I 1,2000` in 379 s and
  `-I 2001,4947` in 250 s, in `tools-host-suite-a.log` and
  `tools-host-suite-b.log`.
- ASan: `-I 1,1150`, `-I 1151,2250` and `-I 2251,4945` in 266 s, 369 s and
  351 s, all passed, in `tools-asan-suite-a.log` to `tools-asan-suite-c.log`.
  The parts count 1150, 1112 and 2714 with the setup tests of fixtures.
- UBSan: the same parts in 241 s, 352 s and 269 s, all passed, in
  `tools-ubsan-suite-a.log` to `tools-ubsan-suite-c.log`.
- The three trees built with zero warnings, in
  `tools-host-build-final.log`, `tools-asan-build.log` and
  `tools-ubsan-build.log`.
- The docs-style checker reports `0 error(s), 0 warning(s)` on every `.md`
  file the session touched, in `tools-docs-final.log`.

## Not done

- Neither VM ran the suite. The paths of Windows, the map of an ELF program
  and the glibc and dyld messages ran on the Mac alone.

## Proof

`git log --oneline -3` after the push of the step:

```text
9b3fe0f3 Record the step of the rules of the commands of anti
68953f4c End the last line of a notice before the lines of its sources, by R189
f91e46fe Refuse the plain license of a runtime archive without the text of the runtime, by R188
```

`git status --short` printed nothing, and `git rev-parse HEAD origin/main`
gave `9b3fe0f30bab7055c4a5aeb0a03df4a1367b4065` twice.
