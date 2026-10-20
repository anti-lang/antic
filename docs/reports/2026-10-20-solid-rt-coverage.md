# Step rt-coverage: the runtime inside the programs of the suite

`./c coverage` measured `src/rt/` only where antic, anti and the unit
tests compile it, so a test that runs a program added nothing to it.
Commit `fb53310a` measures the runtime inside every program of the suite
as well. The archive of every other tree is unchanged.

## What changed

- A coverage tree compiles the runtime of the targets of the host's
  system, `macos-arm64` and `macos-x86_64` on the Mac, with
  `-fprofile-instr-generate -fcoverage-mapping`: its objects and its
  bitcode, at every level, and the glue of `anti.regex` in
  `src/native/pcre2.cmake`. `antic_coverage_runtime` of
  `tools/coverage.cmake` gives the flags.
- The library and its bitcode archives take the members of the profile
  runtime of the pinned clang, which writes the counts at exit. antic
  names that runtime in a link of `--profile-generate` alone, so the
  members travel inside `libanti_rt.a` and no option of antic changes.
  The profile runtime of macOS is one universal file, which llvm-ar does
  not read. `.incbin` of llvm-mc and `ld.lld --oformat binary` cut the
  slice of the processor out, since the pinned release holds no
  llvm-lipo. `file(DOWNLOAD)` of a file URL did it first, and
  `downloads_once` refused it.
- The report names the runtime objects of the host at its default level
  and the objects of the glue as binaries of llvm-cov.
- The profiles no longer come from `LLVM_PROFILE_FILE`, which ctest set
  for every test. Each compile names its pattern: the `%9m` pool of
  before for antic, anti and the unit tests, and `program-%p-%1m` for the
  runtime inside a program. The first run with the variable failed
  `trap_place_sorted`: two programs that link the same runtime functions
  in another order share a `%m` signature, and the second wrote three
  errors of the profile runtime to its standard error and no profile.
  This replaces the DESIGN comment of `antic_coverage_finish` that chose
  the variable, and the new comment gives the reason. Eddie may want to
  review it.
- A coverage tree disables nine tests that read the bytes of the
  runtime and of antic as a package ships them: `atomic_load` of the
  three ARM64 levels, `address_tables` of both macOS targets,
  `link_identity_macos-arm64`, `function_alignment_macos-arm64`,
  `no_paths` and `rt_names`. The counters, the profile members and the
  compiled pattern change what each reads. Every other tree runs them.
- `CLAUDE.md`, the header of `tools/compile.sh` and the row of
  `tools/coverage.cmake` in `docs/notes/source-tree.md` say so.

## Coverage of `src/rt/`

Before is `7a859aa3`, the head before the step, with the tree measured
again: `build/drive/logs/rtcov-before-1.log` to `-4.log`, 264 s, 147 s,
115 s and 282 s, and `rtcov-before-summary.txt`. After is `fb53310a`:
`rtcov-final2-1.log` to `-4.log`, 259 s, 152 s, 119 s and 290 s, and
`rtcov-after-summary.txt`. Every part passed both times. A dash marks a
file that no program of the report compiled before.

| File | Lines | Lines before | Lines after | Regions | Regions before | Regions after |
|---|---|---|---|---|---|---|
| `src/rt/assert.c` | 61 | 42.62% | 52.46% | 32 | 56.25% | 59.38% |
| `src/rt/atomic.c` | 52 | 46.15% | 53.85% | 96 | 28.12% | 36.46% |
| `src/rt/backtrace.c` | 8 | - | 100.00% | 4 | - | 100.00% |
| `src/rt/call.c` | 32 | - | 90.62% | 34 | - | 94.12% |
| `src/rt/cast.c` | 8 | 0.00% | 0.00% | 2 | 0.00% | 0.00% |
| `src/rt/check.c` | 36 | - | 0.00% | 12 | - | 0.00% |
| `src/rt/conf.c` | 503 | 42.74% | 91.65% | 397 | 41.56% | 92.44% |
| `src/rt/cpu.c` | 51 | 76.47% | 96.08% | 43 | 83.72% | 95.35% |
| `src/rt/cpu_level.h` | 18 | 100.00% | 100.00% | 8 | 100.00% | 100.00% |
| `src/rt/digest.c` | 94 | 100.00% | 100.00% | 54 | 100.00% | 100.00% |
| `src/rt/errno.c` | 2 | - | 100.00% | 2 | - | 100.00% |
| `src/rt/f16.c` | 6 | 100.00% | 100.00% | 2 | 100.00% | 100.00% |
| `src/rt/f16.h` | 66 | 86.36% | 86.36% | 45 | 100.00% | 100.00% |
| `src/rt/fail.h` | 6 | 100.00% | 100.00% | 9 | 100.00% | 100.00% |
| `src/rt/fs.c` | 172 | 41.28% | 84.30% | 169 | 46.75% | 79.88% |
| `src/rt/grow.h` | 22 | 90.91% | 90.91% | 26 | 76.92% | 76.92% |
| `src/rt/hash.c` | 32 | 25.00% | 100.00% | 22 | 13.64% | 100.00% |
| `src/rt/hash.h` | 8 | 100.00% | 100.00% | 3 | 100.00% | 100.00% |
| `src/rt/hooks.c` | 125 | - | 96.80% | 77 | - | 97.40% |
| `src/rt/init.c` | 11 | - | 100.00% | 5 | - | 100.00% |
| `src/rt/inject.c` | 161 | 0.00% | 89.44% | 133 | 0.00% | 87.97% |
| `src/rt/io.c` | 7 | - | 100.00% | 7 | - | 100.00% |
| `src/rt/json.c` | 306 | 84.64% | 97.06% | 371 | 86.25% | 97.57% |
| `src/rt/license.c` | 26 | - | 100.00% | 30 | - | 83.33% |
| `src/rt/loaded.c` | 57 | 42.11% | 91.23% | 42 | 28.57% | 90.48% |
| `src/rt/lock.c` | 221 | - | 93.67% | 157 | - | 96.18% |
| `src/rt/mem.c` | 41 | 0.00% | 87.80% | 58 | 0.00% | 93.10% |
| `src/rt/object.c` | 779 | 2.44% | 77.02% | 681 | 2.35% | 82.67% |
| `src/rt/patterns.c` | 604 | - | 81.79% | 595 | - | 88.24% |
| `src/rt/platform.h` | 14 | 92.86% | 92.86% | 11 | 100.00% | 100.00% |
| `src/rt/platform_posix.c` | 396 | 81.06% | 85.35% | 280 | 82.86% | 85.36% |
| `src/rt/plugin.c` | 978 | 72.29% | 89.98% | 948 | 70.78% | 89.45% |
| `src/rt/reflect.c` | 175 | - | 98.29% | 214 | - | 91.59% |
| `src/rt/regex.c` | 497 | 91.75% | 92.76% | 562 | 89.50% | 90.04% |
| `src/rt/registry.c` | 649 | - | 88.14% | 712 | - | 89.47% |
| `src/rt/signal.c` | 31 | 70.97% | 80.65% | 22 | 81.82% | 86.36% |
| `src/rt/snapshot.c` | 35 | 0.00% | 94.29% | 34 | 0.00% | 94.12% |
| `src/rt/start.c` | 68 | - | 97.06% | 37 | - | 94.59% |
| `src/rt/symbols.c` | 903 | 92.69% | 92.69% | 926 | 94.06% | 94.06% |
| `src/rt/sync.c` | 160 | - | 86.88% | 91 | - | 89.01% |
| `src/rt/text.c` | 612 | 80.23% | 97.55% | 587 | 83.65% | 97.79% |
| `src/rt/threads.c` | 259 | - | 91.89% | 178 | - | 94.38% |
| `src/rt/toml.c` | 494 | 86.03% | 86.03% | 495 | 91.31% | 91.31% |
| `src/rt/trace.c` | 369 | - | 71.82% | 390 | - | 70.77% |
| `src/rt/utf.c` | 214 | 100.00% | 100.00% | 216 | 100.00% | 100.00% |

| `src/rt/` | Before | Not reached | Reached | After | Not reached | Reached |
|---|---|---|---|---|---|---|
| Lines | 6617 | 2237 | 66.19% | 9369 | 1104 | 88.22% |
| Regions | 6274 | 1992 | 68.25% | 8819 | 938 | 89.36% |
| Branches | 3829 | 1452 | 62.08% | 5127 | 1027 | 79.97% |
| Functions | 450 | 136 | 69.78% | 652 | 34 | 94.79% |

The total of all three directories went from 94.98% to 96.45% of the
lines and from 94.93% to 96.49% of the regions. `src/antic/` and
`src/anti/` did not change.

## Limits of the measurement

- A program writes its profile at exit. One that ends through a trap,
  an abort or a signal writes none, so the paths to such an end read as
  unreached although tests take them: `check.c`, `cast.c`, and
  `anti_rt_fail_abort` and `anti_rt_assert_failed` of `assert.c`.
- llvm-cov keeps one record per function and file, the first one it
  reads. Three runtime functions compile otherwise for the unit tests,
  where `ANTI_DEV_CPU` stands, or for antic: `anti_rt_cpu_level` and
  `machine_level` of `cpu.c` and `anti_rt_runtime_version` of
  `license.c`. Their counts in programs do not reach the report.
- The summary counts a region in a macro once per expansion. Its 938
  regions not reached are 780 places of the source, which the list
  below gives.
- `build/coverage/profiles` holds 2506 raw profiles, 96 MB, after the
  suite.

## Regions the suite does not reach

The 780 places of `src/rt/`, by file and function, with the lines of
each. `build/drive/logs/rtcov-unreached.txt` holds the same list.

- `src/rt/assert.c`, 13 regions: `anti_rt_fail_abort` 30-37,
  `anti_rt_fail_memory` 40-49, `anti_rt_start_alloc` 67,
  `anti_rt_assert_failed` 99-114.
- `src/rt/atomic.c`, 16 regions: `anti_rt_atomic_compare_swap` 23-26 and
  94-96, `anti_rt_atomic_load` 32-34, `anti_rt_atomic_store` 41-43,
  `anti_rt_atomic_swap` 49-51, `anti_rt_atomic_add` 57-59,
  `anti_rt_atomic_sub` 65-67, `anti_rt_atomic_and` 70-75,
  `anti_rt_atomic_or` 78-83.
- `src/rt/call.c`, 2 regions: `anti_rt_reflect_call` 26-28 and 44.
- `src/rt/cast.c`, 2 regions: `anti_rt_cast_failed` 12-15,
  `anti_rt_table_unset` 24-27.
- `src/rt/check.c`, 12 regions: `anti_rt_check_failed` 14-40,
  `anti_rt_walk_changed` 49-58.
- `src/rt/conf.c`, 20 regions: `format_text` 129-131 and 134-136,
  `seed_of` 209-211, `set_key` 234-236, `anti_rt_conf_option` 317-319,
  322-324, 333-338 and 346-348, `resolve` 365-367, `digits_only` 446-448,
  `element_of` 466-468, `join_elements` 495-497 and 501-503, `read_key`
  534-536 and 551-557, `read_file` 635-637, `environment_path` 667-669,
  `help` 704, `anti_rt_conf_configure` 746-748.
- `src/rt/cpu.c`, 2 regions: `anti_rt_cpu_level_message` 74,
  `anti_rt_cpu_level` 172.
- `src/rt/fs.c`, 34 regions: `native_path` 42-45, `anti_rt_fs_open` 63-65,
  `anti_rt_fs_size` 84-86 and 88-90, `anti_rt_fs_read` 113-115, 117-119
  and 122-129, `add_name` 154-157, `finish` 187-191, `anti_rt_fs_remove`
  244-246, `anti_rt_fs_rename` 257.
- `src/rt/grow.h`, 3 regions: `anti_rt_reserve` 42 and 45-47.
- `src/rt/hooks.c`, 2 regions: `anti_rt_hook_failed` 166-168,
  `anti_rt_hook_changed` 185-187.
- `src/rt/inject.c`, 14 regions: `anti_rt_inject_line` 124-136 and
  139-141, `anti_rt_inject_inspect` 215, 225-227 and 231-237.
- `src/rt/json.c`, 9 regions: `read_string` 134-136, `anti_rt_json_member`
  358-360, 377-379 and 382-384.
- `src/rt/license.c`, 4 regions: `anti_rt_license_text` 32,
  `anti_rt_runtime_version` 43 and 47-48.
- `src/rt/loaded.c`, 4 regions: `anti_rt_plugin_at` 55,
  `anti_rt_plugin_registry` 71-73, `anti_rt_plugin_image` 83-86.
- `src/rt/lock.c`, 6 regions: `orders_grow` 96-98, `order_take` 138-140
  and 148-150, `order_give` 181-187, `held_already` 231-234.
- `src/rt/mem.c`, 4 regions: `anti_rt_out_of_memory` 84-86, `anti_rt_grow`
  92 and 94-96.
- `src/rt/object.c`, 108 regions: `checked_object` 64-69,
  `anti_lang_Object_type_name` 78-79, `anti_rt_type_size` 105-119,
  `anti_rt_pattern_same` 202-207, `anti_rt_pattern_hash` 218-220,
  `variant_case` 233-235 and 242, `element_descriptor` 265, `element_size`
  279, `anti_rt_array_shape` 295 and 299-301, `anti_lang_Object_equals`
  320-325, `anti_lang_Object_hash` 335-337 and 340, `anti_rt_element_size`
  445, `put_value` 599-601, 610-613, 629-632 and 646-649, `serialize_into`
  682-685 and 691-694, `anti_rt_type_arg` 730-732 and 734-736,
  `array_length` 749-753, `serialize_object` 770-773,
  `anti_rt_element_serialize` 784-787 and 793-808, `show_object` 823-826,
  `show_row` 834-844, `show_value` 863-922, `anti_rt_element_text`
  928-931, `anti_lang_Object_destruct` 938-940, `anti_lang_Object_created`
  947-949, `anti_lang_Object_destroyed` 952-954,
  `anti_lang_Object_dispatched` 964-966, `anti_lang_Object_joined`
  969-971, `anti_lang_Object_enter` 974-977, `anti_lang_Object_leave`
  980-983, `anti_lang_Object_failed` 987-991, `anti_lang_Object_changed`
  995-998, `anti_rt_dup` 1057-1059, `anti_rt_copy_buffer` 1139-1141.
- `src/rt/patterns.c`, 56 regions: `literal` 56-61,
  `anti_rt_regex_message` 98-100, 106-108 and 113-117, `out_of_memory`
  148-150, `failure` 162-163 and 166-169, `match_data` 177-179,
  `crlf_newline` 199-206, `next_character` 215-218 and 220-222, `slice`
  284-286, `anti_rt_regex_current` 402, `anti_rt_regex_test` 445,
  `named_group` 482-484 and 488-490, `search_again` 510-514,
  `anti_rt_regex_group` 528-531, `grow_append` 588-590,
  `anti_rt_regex_replace` 645-647 and 657-660, `anti_rt_regex_patch`
  701-706 and 714-716, `anti_rt_bytes_patch` 786-788, 794 and 797,
  `anti_rt_regex_stop_limit` 840-852, `anti_rt_regex_stop_group` 859-868,
  `anti_rt_regex_stop_name` 874-879, `anti_rt_regex_stop_template`
  887-895, `anti_rt_regex_stop_into` 900-908,
  `anti_rt_regex_stop_group_bytes` 945-947.
- `src/rt/platform_posix.c`, 41 regions: `anti_rt_monitor_new` 97-102,
  104-106, 108-110 and 113-121, `anti_rt_thread_start` 247-249 and
  251-254, `anti_rt_processors` 263, `anti_rt_getenv` 277-279,
  `anti_rt_file_seek` 317, `anti_rt_directory_list` 357,
  `anti_rt_library_error` 393, `anti_rt_trace_walk` 449-451, `copy_path`
  476-479, `anti_rt_signal_route` 665-667 and 669-676, `anti_rt_sleep`
  730-732 and 735-737, `utf8_list` 748-750 and 755-761,
  `anti_rt_process_arguments` 772, `anti_rt_aligned_alloc` 820-822,
  `feature` 877-879, `anti_rt_arm64_level` 889-894.
- `src/rt/plugin.c`, 80 regions: `fitted` 43-49, `path_of` 81-83,
  `version_part` 152-155, `version_below` 180-182, `function_at` 216-217,
  `checked` 228, 233-237, 258 and 260-262, `stub` 285-300, `stubs_of`
  333-335, `build` 377-380, `claim` 481-485, `reach` 553 and 556-559,
  `element_width` 574-581, `array_width` 601-603, 606-608, 612-614 and
  618-620, `width_of` 636-638, 643-645 and 652, `descriptor_sound`
  713-715, `damage_of` 824, `missing_symbol` 858-860, `file_bytes`
  877-879, 884-886 and 889-891, `missing_module` 928-930,
  `anti_rt_plugin_load` 976-980, 998-1001 and 1003-1006,
  `anti_rt_plugin_unload` 1158, `index_lists` 1242-1244 and 1253,
  `discover_in` 1273-1275, `discover` 1347, `anti_rt_plugin_provider`
  1391-1400.
- `src/rt/reflect.c`, 18 regions: `anti_rt_reflect_class_name` 19-20,
  `anti_rt_reflect_parent` 27, `anti_rt_reflect_field_count` 32,
  `anti_rt_reflect_field_name` 52-53, `anti_rt_reflect_field_offset` 62,
  `anti_rt_reflect_field_type` 70, `anti_rt_reflect_field_owned` 78,
  `anti_rt_reflect_function_count` 83, `function_at` 90-92,
  `anti_rt_reflect_function_name` 102-103, `anti_rt_reflect_function_slot`
  112, `anti_rt_reflect_function_params` 120, `value_kind` 137-148,
  `anti_rt_reflect_get` 159.
- `src/rt/regex.c`, 38 regions: `keep` 41-47, `put` 98-104, 106-109 and
  113-116, `is_alnum` 145, `class_escape_end` 212-239, `member_end` 249,
  `walk_class` 346-349 and 378, `anti_rt_regex_compile_bytes` 498-502,
  `anti_rt_regex_message_into` 537-539 and 549,
  `anti_rt_regex_group_number` 586-593.
- `src/rt/registry.c`, 75 regions: `find_in` 36 and 38-41, `registry_find`
  73, 86, 89-91, 93-95, 101-107 and 113, `lend` 204-206, 209-211 and
  214-216, `room_for_object` 231-233, `type_of` 338-340, `field_named`
  363, `read_char` 465-467, `read_elements` 492-494, 498-500, 505-507,
  509-511, 516-518 and 520-522, `read_owned` 537-539 and 544-546,
  `read_parts` 561-563 and 568-570, `read_array` 610-612, `read_value`
  665-667, 694-696, 701-703, 707-709, 767, 772-774, 778, 780-782, 788 and
  794-796, `fill` 836-838, 841-843 and 856-858, `read_object` 905-908,
  `anti_lang_Object_deserialize` 924-926.
- `src/rt/signal.c`, 3 regions: `deliver` 31-33 and 37-39,
  `anti_rt_on_signal` 48-50.
- `src/rt/snapshot.c`, 2 regions: `anti_rt_snapshot_new` 22-24,
  `anti_rt_snapshot_text` 38.
- `src/rt/start.c`, 2 regions: `runtime_options` 43, `slice_of` 88-90.
- `src/rt/symbols.c`, 55 regions: `anti_rt_coff_demangle` 23-25,
  `string_at` 198-201, `read_form` 249, 251-253, 257-280 and 282-283,
  `file_name` 302-304, 318-320, 329, 333-335 and 343, `skip_file_table`
  388-390 and 405-407, `read_line_header` 426-428, 434-437 and 453-455,
  `run_line_program` 507 and 523-525, `unit_line` 562-564, `dwarf_line`
  583-585, `elf_find` 677-679, `better` 741, `anti_rt_macho_functions`
  1056-1058, `macho_sections_visit` 1190-1193, `anti_rt_macho_object_line`
  1284 and 1290.
- `src/rt/sync.c`, 10 regions: `fatal` 21-23, `channel_of` 72-74,
  `anti_rt_chan_new` 84-88, 92-95 and 97-99, `anti_rt_chan_send` 132-135,
  `anti_rt_chan_held` 188-190, `anti_rt_chan_delete` 204-206,
  `anti_rt_select` 256.
- `src/rt/text.c`, 9 regions: `anti_rt_text_kept` 58-60,
  `anti_rt_text_copy` 94 and 96-98, `reserve` 159-161,
  `anti_rt_builder_take` 213-215, `compare` 422, `anti_rt_builder_float`
  600-602, `halve_by` 698-700, `anti_rt_read_float` 864-867.
- `src/rt/threads.c`, 10 regions: `start_locked` 208-210,
  `anti_rt_parallel` 244-248, 250-255 and 269-275, `anti_rt_dispatch`
  315-317, 322-324 and 329-331.
- `src/rt/toml.c`, 34 regions: `keep` 112-114, `add` 130-133, 136-139 and
  149-156, `grow_names` 195-197 and 200-204, `index_pair` 235-237,
  `repeats` 268-270, `read_header` 302-305 and 330-333, `read_key`
  373-375, `read_inline_table` 441-444, 450-453, 457-460, 468-470 and
  473-475, `read_array` 505-508, 510-513, 518-520, 523-525 and 530-532,
  `read_pair` 559-562, `anti_rt_toml_read` 584-586, `anti_rt_toml_count`
  610, `text_of` 617-618, `anti_rt_toml_key` 624-626, `anti_rt_toml_value`
  632-634, `anti_rt_toml_find` 643-645, `anti_rt_toml_line` 657-659.
- `src/rt/trace.c`, 92 regions: `text_of` 34-35, `text_part` 43-44,
  `function_name` 106, 109-111 and 114-115, `notice_id` 129-131 and
  135-137, `load` 165, 176-178 and 183, `image_slide` 203-205, `elf_id`
  267-283, `module_id` 289-291, 295-298 and 300, `anti_rt_trace_frame`
  314-316, `symbolize_macho` 338-340, `symbolize_elf` 363-381,
  `symbolize_pe` 388-404, `anti_rt_trace_symbolize` 418-420 and 425-430,
  `add` 449-451 and 453-457, `add_line` 468-472, `module_number` 485-487,
  489-491 and 502, `anti_rt_trace_text` 522-524, 533-535, 542-544, 550-553
  and 559-561.

## Findings

The step closed no finding and recorded none. The runtime under the
measurement showed no defect of Anti: every test that passes in the host
tree passes in the coverage tree. `tests/known-failures.txt` holds 14
lines before and after, 12 of them expected to fail on the Mac. No
`[provisional]` entry, and no new feature, option or command.

## Gates

- The three builds report no warnings of our code:
  `rtcov-host-build.log`, `rtcov-asan-build.log` and
  `rtcov-ubsan-build.log`.
- Host, 5194 tests, `-I 1,2000` 358 s and `-I 2001,5194` 284 s. All
  2000 and 3220 pass, a part counting the fixtures it pulls in. The 12
  expected failures of the Mac fail as listed, and the two
  `mimalloc_environment` tests skip: `rtcov-host-1.log` and
  `rtcov-host-2.log`.
- ASan, 5192 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5192`. All
  1150, 1119 and 2950 pass, in 247 s, 167 s and 562 s:
  `rtcov-asan-1.log` to `-3.log`. The last part stands above 9 minutes.
- UBSan, the same parts. All 1150, 1119 and 2950 pass, in 244 s, 142 s
  and 382 s: `rtcov-ubsan-1.log` to `-3.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged: the
  runtime of a tree without `ANTIC_COVERAGE` builds from the same
  commands as before.
- The docs-style checker reports nothing of this step on `CLAUDE.md`,
  `docs/notes/source-tree.md` and this report: `rtcov-docs1.log` and
  `rtcov-docs2.log`. Its one warning, line 511 of the notes, is older.

## Not run on the target

- A coverage tree of a Linux host instruments the runtime of the four
  Linux targets and takes the members of the thin `libclang_rt.profile.a`
  of each. `./c coverage` on a Linux machine would show it.
- A coverage tree of a Windows host does the same with `clang_rt.profile.lib`
  of both Windows targets, members `.obj`. `./c coverage` on a Windows
  machine would show it.

## State at the push

```text
$ git log --oneline -3
fb53310a Measure the runtime inside the programs of the coverage suite
7a859aa3 Report the end of step 34
fc945d8f Close rule 18 of step 34 in the summary
$ git status --short
$ git rev-parse HEAD origin/main
fb53310ad08babbaae3202d7e463f038d6575a08
fb53310ad08babbaae3202d7e463f038d6575a08
```

This report is written after that push, and its commit follows it.
