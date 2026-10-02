# Closure of the second audit

This document accounts for every finding of `docs/audit/summary.md`: S01 to
S34, M01 to M62, each finding of the minor table and the defects no rule
names. Each row gives the state and the commit that fixed it, or the reason
it stands. The code at each place was read again at `b6ceb345`, after fix
step 41, and a row says fixed only where the code now holds the fix. The
closure session fixed what that reading found open, in the commits the rows
name from `27450261` on. The fix steps and their tests are reported in
`docs/reports/`, one report per step.

States:

- Fixed: the code at the place no longer has the defect.
- Not a defect: the code is right, and the row says why.
- Left: the defect stands, and the row says what it waits for.

## Severe

| ID | Place | State | Commit or reason |
|---|---|---|---|
| S01 | `antl_tree.c:1258` | Fixed | `09ad158e`. `antl_verify_tree` of `antl_verify.c` checks every tree after reading: `otherwise_at`, case numbers, `enum_value`, parameter and copy counts, reference 0, the depth and a node seen twice. |
| S02 | `sema_expr.c:2258` | Fixed | `387bfc7f`. `fixed_layout`, now in `sema_simd.c`, refuses the unit break and the word of a Mutex. |
| S03 | `layout.c:96` | Fixed | `79143b24`. `simd_shape` of `antl_verify.c` refuses a lane of no bytes in both tables. |
| S04 | `sema_checker.h:67` | Fixed | `cb4794dc`. `sema_enter` and `sema_leave` are the only writers of `c->ctx`. |
| S05 | `parser.c:3272` | Fixed | `925c2402`. `parser_descend` in nested types, the depth of the angle scan, `LEX_FORMAT_DEPTH_MAX` in the lexer and the reach of a chain. |
| S06 | `types.c:1412` | Fixed | `fe4d293e` and `4afe0a3e`. One walk bounds the nesting of values, and `sema_chain_prepare` takes chains of types apart. |
| S07 | `sema.c:2749` | Fixed | `1653e19d`. Each copy of a generic is checked for cycles and bases when it is filled. |
| S08 | `parser.c:981` | Fixed | `925c2402`. The inner parser of a placeholder gets an `origin` of its own. |
| S09 | `sema_call.c:3022` | Fixed | `387bfc7f`. The base of a type member is typed. |
| S10 | `sema.c:2356` | Fixed | `387bfc7f`. An enum base that is no integer type is refused. |
| S11 | `sema_generic.c:220` | Fixed | `423243e3`. A nested type of a refused generic class is skipped. |
| S12 | `sema_expr.c:37` | Fixed | `387bfc7f`. `sema_is_place` refuses a bound function. |
| S13 | `antl_tree.c:1155` | Fixed | `805c7e27`. The lane count is 32 bits and must equal the field count of the struct. |
| S14 | `sema_pattern.c:294` | Fixed | `5e57cc77`. The calls of `anti.regex` are held against the signatures of the library. |
| S15 | `ast_dump.c:430` | Fixed | `705c8d60`. |
| S16 | `ast_dump.c:52` | Fixed | `705c8d60`. |
| S17 | `optimize.c:1072` | Fixed | `7dbcf4f2`, with `programs/forward_own_base.anti`. |
| S18 | `optimize.c:1217` | Fixed | `8a2d0025`, with `programs/slot_two_addresses.anti`. |
| S19 | `whole.c:240` | Fixed | `c8a39456`. Table calls stay indirect where a library can load. |
| S20 | `lower_stmt.c:743` | Fixed | `1cf4e6b2`, with `programs/by_ends.anti`. |
| S21 | `regalloc.c:993` | Fixed | `2b340927`. `frame_limit` on both targets, each sum checked. |
| S22 | `ir_print.c:484` | Fixed | `1737305c`. `ir_name_append` writes a name without a module, in the printer and the verifier. |
| S23 | `layout.c:882` | Fixed | `3df402f7`. `antl_verify_const` refuses a scalar of `IR_AGG`. |
| S24 | `arm64.c:1063` | Fixed | `79143b24`. Both tables meet the simd shape, and the IR verifier refuses an operation above the vector cap. |
| S25 | `patterns.c:753` | Fixed | `b0238ab9`. `fits_span` checks the offset and length without a sum. |
| S26 | `patterns.c:547` | Fixed | `b0238ab9`. `has_group` bounds the group by the pattern's count. |
| S27 | `trace.c:367` | Fixed | `225a1fa5`. `symbolize` takes the image from `anti_rt_module_at`. |
| S28 | `plugin.c:597` | Fixed | `61fb797a`, pinned by `3c362b12`. The object is counted under the lock. |
| S29 | `hooks.c:79` | Fixed | `4203cb1a`. antic refuses `--no-hooks` where a library loads, as Eddie decided. |
| S30 | `src/anti/files.c:38` | Fixed | `1fdea311`. The helpers end the program when memory runs out. |
| S31 | `src/anti/bindclang.c:1165` | Fixed | `59e60f85`. Lines count in `int64_t`, and `marker_line` bounds a marker. |
| S32 | `src/anti/bindclang.c:794` | Fixed | `59e60f85`. The sum is unsigned. |
| S33 | `plugin.c:616` | Fixed | `4ad4d480`. `descriptor_sound` checks each list of a plugin class. |
| S34 | `src/anti/repo.c:248` | Fixed | `eeab25a6`. A stamp below 0 is refused. |

## Major

| ID | Place | State | Commit or reason |
|---|---|---|---|
| M01 | `collection.anti:708` | Fixed | `3db10218`, by Eddie's decision. |
| M02 | `src/anti/doc.c:1440` | Fixed | `61880479`. `anti` reaches antic through `driver.h`, `antic.h` and the helpers, which `anti_interface` checks. |
| M03 | `ir.h:477` | Fixed | `42cec95f`. `alloc.c` owns the heap memory of antic. |
| M04 | `lower_desc.c:205` | Fixed | `86b2db30`. `rt_abi.h` lists the functions and records once. |
| M05 | `lower_stmt.c:1110` | Fixed | `8d87c400`. One teardown gated by `sema_needs_teardown`, which counts an array of `own fn`. |
| M06 | `whole.c:2390` | Fixed | `86b2db30`. Every item is named by its constant of `rt_abi.h`. |
| M07 | `header.c:158` | Fixed | `38754c6c`. |
| M08 | `map.anti:57` | Fixed | `7021df72` and `476ed15a`. |
| M09 | `log.anti:476` | Fixed | `6d48b5e9`. `anti.log` reads and writes through `anti.fs`. |
| M10 | `sema_checker.h:4` | Fixed | `ec635c18`, `af9601ae`, `84d09a25` and `f1f9eb76`. |
| M11 | `lower.c:1` | Fixed | `207f554b`. |
| M12 | `sema_export.c:531` | Fixed | `e484bdb9`, `11c00537`, `4e6f623b`, `0f7f4469`, `0203529a` and `3c6f4c0a`. The walks of the copy pass, the dump and the tree writer build every field of a node and are no analyses, by the entry of `e1aa5a2d`. |
| M13 | `lower_eq.c:306` | Fixed | `7524e759`. `lower_part_of` is the one rule. |
| M14 | `lower.c:2197` | Fixed | `8d87c400`. `lower_prepare_object` prepares every new object. |
| M15 | `arm64.c:680` | Fixed | `0c455b5b`. `append_add_offset` owns the large offset. |
| M16 | `whole.c:617` | Fixed | `73879aa7`. `ir_reach` decides what a program reaches. |
| M17 | `driver.c:2780` | Fixed | `e9dfc107`. `driver_link_inputs_of` fills every link. |
| M18 | `layout.c:424` | Fixed | `c12842dd`. `ir_fold_int` folds for both. |
| M19 | `header.c:937` | Fixed | `38754c6c`. `sema_table_of` orders both tables. |
| M20 | `antl_tree.c:280` | Fixed | `7dad887f` and `32288fc6`. |
| M21 | `driver.c:2664` | Fixed | `e9dfc107`. antic reads the index with `src/rt/toml.c`. |
| M22 | `registry.c:386` | Fixed | `33746be3`. No `strtoll` is left in `registry.c`. |
| M23 | `src/anti/build.c:177` | Fixed | `bc180d2a`. `unit_options` carries the package name. |
| M24 | `src/anti/symmap.c:146` | Fixed | `ef84b323`. `symmap_notice` is the one reader of a binary's id. |
| M25 | `src/anti/syms.c:312` | Fixed | `4cd5c40f`, as Eddie decided. |
| M26 | `src/anti/bind.c:44` | Fixed | `e0dbe4e1`. |
| M27 | `collection.anti:600` | Fixed | `b65ca450`. |
| M28 | `concurrent.anti:630` | Fixed | `1b3310a3`. |
| M29 | `json.anti:146` | Fixed | `7eee766c`. The runtime is the one reader of JSON strings. |
| M30 | `src/rt/platform.h:20` | Fixed | `0543578c`, `22e41fee`, `f8d68677`, `d74728af`, `dd38e02a` and `8a79aab0`. `run_platform_layer` checks it. |
| M31 | `selfpath.c:100` | Fixed | `0543578c` and `22e41fee`. |
| M32 | `src/rt/errno.c:52` | Fixed | `d93ab8e3`. `FormatMessageW` and UTF-8. |
| M33 | `antl_tree.c:688` | Fixed | `9ad39344`. |
| M34 | `sema_generic.c:1092` | Fixed | `4c999dac`. |
| M35 | `sema_call.c:2605` | Fixed | `e80d998b`. |
| M36 | `sema_call.c:445` | Fixed | `f137060b` and `1653e19d`. |
| M37 | `parser.c:88` | Fixed | `925c2402`. The angle scan keeps its own index. |
| M38 | `registry.c:409` | Fixed | `33746be3`. `anti_rt_text_invalid` at the member. |
| M39 | `src/anti/fmt.c:457` | Fixed | `a9a679b7`. |
| M40 | `src/anti/doc.c:917` | Fixed | `591466a9`. |
| M41 | `src/anti/zip.c:232` | Fixed | `6d269eee`. |
| M42 | `src/anti/syms.c:1113` | Fixed | `6d7206f9`. |
| M43 | `tests/unit/test_modules.c:1417` | Fixed | Trees `09ad158e`, `9ad39344` and `18c2f183`, parser `925c2402`, deserialize `33746be3`, plugin lists `4ad4d480`, index `eb1ffce1`, configuration `acfddab2`, Mach-O `33a57ca6`, template `b0238ab9`, NUL `2226f104`, splitter `ff110b85`, zip `6d269eee`, COFF `fbc3b964`. |
| M44 | `sema_pattern.c:335` | Fixed | `5e57cc77`. |
| M45 | `lower_expr.c:388` | Fixed | `b5c02466`. |
| M46 | `src/anti/fmt.c:1491` | Fixed | `a9a679b7`. |
| M47 | `plugin.c:420` | Fixed | `61fb797a`. |
| M48 | `lower.c:2832` | Fixed | `8d87c400`. |
| M49 | `lower_stmt.c:1748` | Fixed | `0fb3204b` and `25afea23`. |
| M50 | `arm64.c:1977` | Fixed | `0c455b5b`. |
| M51 | `header.c:716` | Fixed | `38754c6c`. |
| M52 | `header.c:1259` | Fixed | `cc7fdb61`. |
| M53 | `regalloc.c:420` | Fixed | `efb12d0a`. |
| M54 | `tests/CMakeLists.txt:1743` | Fixed | `b52fe83c`. No test sets `PASS_REGULAR_EXPRESSION`. |
| M55 | `owning_values.anti:24` | Fixed | `f8206f4b`. |
| M56 | `link_linux.anti:1` | Fixed | `de779914`. |
| M57 | `sema.c:3028` | Fixed | `760c6829`. |
| M58 | `plugin.c:559` | Fixed | `4ad4d480`. |
| M59 | `run_modules.cmake:71` | Fixed | `057e2d02`. |
| M60 | `run_pcre2_pin.cmake:46` | Fixed | `36605b3f`. |
| M61 | `run_table.cmake:19` | Fixed | `6c8da2de`. |
| M62 | `run_plugin.cmake:25` | Fixed | `6c8da2de`. |

## Minor

One row per finding of the minor table, by its rule and place. Paths without
a directory are in `src/antic/`.

| Rule | Place | State | Commit or reason |
|---|---|---|---|
| Q1 | `src/anti/syms.c:1251` | Fixed | `3f028a0d`, for `syms.c`, `test.c`, `bind.c` and `build.c`. |
| Q1 | `src/anti/files.c` | Not a defect | Two `[provisional]` entries keep the allocation helpers in `files.c` under `files_`. |
| Q1 | `json.anti:59` | Fixed | `64481158`. `regex.Growing` is no defect: the DESIGN at `src/rt/patterns.c` keeps `anti.text` out of a program of patterns. |
| Q1 | `log.anti:28` | Fixed | `30b82c77`, through `rt.get`. |
| Q2 | `notice.h:6` | Fixed | `61880479`. |
| Q2 | `mach.c:9` | Fixed | `0a262269`, `3d29ada8` and `cc7dbecd`. The layout in `select_module` and the archive reader of `coff.c` are no defect, by the DESIGN at `select.c` and the primitives the joiner shares. |
| Q3 | `whole.c:281` | Fixed | `f49282a6`. |
| Q3 | `driver.c:114` | Fixed | `3943d07e`. |
| Q3 | `src/anti/syms.c:32` | Fixed | `fa5a8dc5`. |
| Q3 | `selfpath.c:119` | Fixed | `0543578c`. `files_base_name` taking both separators is no defect, by its `[provisional]` entry. |
| Q3 | `sema_stmt.c:27` | Fixed | `ec635c18` and `af9601ae`. |
| Q3 | `src/native/pcre2.cmake:71` | Fixed | `4980ac22`. |
| Q3 | `src/rt/patterns.c:126` | Fixed | `d1515881`. |
| Q3 | `map.anti:1595` | Fixed | `565fe1cf`. |
| Q3 | `map.anti:1398` | Fixed | `edc892e6`. |
| Q3 | `synchronized.anti:29` | Left | `96942a9e` holds `Stamp` once. `Changed` and `Snapshot` stay where entries of `docs/decisions.md` name their modules, one of them not provisional. Moving them is Eddie's decision. |
| Q4 | `sema_checker.h:376` | Fixed | `d875bace`, the test `header_sections`. |
| Q4 | `antl.c:3631` | Fixed | `7dad887f`. |
| Q5 | `src/rt/plugin.c:129` | Fixed | `2eb62ac3`. |
| Q5 | `collection.anti:453` | Fixed | `ebe86b3c`: the matching loops and `SpscRing`. |
| Q5 | `synchronized.anti:413` | Fixed | `5ff63780`. |
| Q5 | `collection.anti:299` | Fixed | `5d0eefd9`. |
| Q5 | `config.anti:32` | Fixed | `b2238417`. |
| IR | `lower.c:1534` | Fixed | `b64223cf`, for the pattern slot and the plugin holder. `layout_data` is no defect: the back end lays out a valued global for its one target, as `layout_resolve` folds the instructions in place, by the DESIGN at `select.c` and `ir.h:333`. |
| Names | `text.anti:141` | Left | Size and presence. Which of `len`, `count`, `has` and `given` should stand is Eddie's decision. |
| Names | `args.anti:104` | Left | Whether `args.text_of` and `args.at` give `?str` is Eddie's decision. `json.member` and `reflect.get` are no defect, by entries that are not provisional. |
| Names | `log.anti:57` | Not a defect | A sink is an abstract class a `Logger` keeps, and the logging entry holds a pointer. |
| Names | `sorted.anti:1306` | Fixed | `30a1997d`. |
| Names | `regex.anti:980` | Not a defect | `str` has no `patch`, so `patch_bytes` names its operand. |
| 1 | `diagnostic.h:60` | Fixed | `78add679`. |
| 1 | `src/anti/bindmodel.h:143` | Fixed | `86744666`. |
| 1 | `src/rt/lock.c:63` | Fixed | `8a79aab0`. |
| 1 | `CMakeLists.txt:124` | Fixed | `4980ac22`. |
| 3 | `antl_tree.c:247` | Fixed | `7dad887f` and `9ad39344`. |
| 3 | `x86_64.c:688` | Fixed | `7f017478`. |
| 3 | `src/rt/atomic.c:60` | Fixed | `e83e945b`. |
| 3 | `src/rt/start.c:213` | Fixed | `085b3978`. |
| 4 | `tests/unit/test_utf.c:11` | Fixed | `fbc477e4`, with the test `one_allocator`. |
| 5 | `arena.c:31` | Fixed | `42cec95f`. |
| 5 | `ir.c:644` | Fixed | `b64223cf`. |
| 5 | `src/anti/jsontree.c:82` | Fixed | `1eea6716`. |
| 6 | `ast_dump.c:443` | Fixed | `cc02e10d`. |
| 6 | `regalloc.c:664` | Fixed | `422a5555`. |
| 9 | `diagnostic.c:31` | Fixed | `db054102`, `78add679`, `1b05f98b` and `7529fa33`. |
| 9 | `lower.c:450` | Fixed | `b64223cf`, `ddc299c0` and `e8b73311`. |
| 11 | `antl_io.h` | Fixed | `7dad887f` and `21270a2d`. |
| 11 | `src/rt/regex.h:29` | Fixed | `a8bc6477`. |
| 12 | `src/anti/check.c:474` | Fixed | `7869e53e`. |
| 13 | `src/rt/text.c:133` | Not a defect | The builder writes nothing on a count it cannot hold, by an entry of "Standard library phase" that is not provisional. Whether it should stop the program is a question for Eddie. |
| 14 | `antl.c:3518` | Fixed | `1b05f98b`, `30136c85` and steps 2 and 3. |
| 14 | `optimize.c:1361` | Fixed | `93ce7de7` and `73879aa7`. |
| 14 | `src/anti/bindwrite.c:263` | Fixed | `dcbcb75e`. |
| 14 | `src/rt/conf.c:700` | Fixed | `4ea305d3`. |
| 16 | `sema_call.c:3392` | Fixed | `ec4bd9fd`. |
| 16 | `ir.c:398` | Fixed | `8422bcb7`. |
| 16 | `src/rt/plugin.c:791` | Fixed | `375ee78e`. |
| 18 | 163 functions | Fixed | Step 41 split the functions its reports judged: `ae9a37e0`, `2235fefa`, `44325e3a`, `fb3414cd`, `177c24e3` and `25c67580`. The closure passed the parameter lists the reports judged clearer as a struct: `e31b2bea`, `12d4eea1` and `df183509`. The rest follow the shape of their problem or were judged from their counts alone, which rule 18 leaves standing. |
| 19 | 9 files | Fixed | `c05d3054`, `20623c5e`, `3a2c3467` and `f1f9eb76`. No file of `src/` passes 3000 lines. `519dacc5` split `whole.c` as the back-end report judged. |
| 20 | `sema.c:2223` | Fixed | `ec635c18`. `db054102` exports `sema_shared_name` again for its second file. |
| 20 | `memcheck.h:43` | Fixed | `261ff475`. |
| 20 | `src/rt/object.c:218` | Fixed | `d59fd2ee`. |
| 23 | `sema.c:85` | Fixed | `3d66c3f8`. |
| 24 | `sema.c:1927` | Fixed | `46cf326b` and `27450261`. Five casts stand with their reason at the line: three in the tree walk that reads and writes through one table, and two where a const view becomes a type the substitution hands out. |
| 24 | `memcheck.h:55` | Fixed | `46a522f2`. The cast of `lower_place.c` stands with its reason at the line. |
| 24 | `src/anti/test.h:21` | Fixed | `57105b7f`. |
| 25 | `sema.h:271` | Fixed | `37dea845`. |
| 25 | `src/rt/object.c:65` | Not a defect | The exception Eddie added to rule 25. |
| 25 | `src/anti/jsontree.h:48` | Not a defect | The `[provisional]` entry on the prefixes of `anti` keeps `json_`. |
| 26 | `lower_eq.c:375` | Fixed | `ee314c78`, and steps 17, 18 and 23. |
| 26 | `lower.c:1542` | Fixed | `42cec95f`. |
| 26 | `sema_expr.c:1683` | Fixed | `e572114c`, `b0f45b48` and `0f46bc9b`. |
| 26 | `optimize.c:94` | Fixed | `a09ca5e1`, `ee314c78`, `ddc299c0` and `44c19c0e`. |
| 26 | `lower.c:3188` | Fixed | `0356cefa` and `217ab190`. |
| 26 | `src/rt/patterns.c:606` | Fixed | `eeac32fc`. |
| 26 | `src/anti/deps.c:457` | Fixed | `840aba91`. |
| 27 | `sema_expr.c:1095` | Fixed | `db054102` and `cc02e10d`. |
| 27 | `lower_lowerer.h:4` | Fixed | `2d5636c0` and `217ab190`. |
| 27 | `src/anti/doc.c:780` | Fixed | `664a7bc6`. |
| Warnings | `arm64.c:406` | Fixed | `0356cefa`. |
| None | `debug.c:440` | Fixed | `f3a1413d`. |
| Tests | `tests/CMakeLists.txt:1313` | Fixed | `07249add`. |
| Tests | `tests/run_checks.cmake:96` | Fixed | `ca3bf832`. |
| Tests | `tests/CMakeLists.txt:479` | Fixed | `ca3bf832` and `eaaa26f9`. |
| Tests | `tests/unit/test_sema.c:28` | Fixed | `fbc477e4`, `704a7bb9` and `c8845ef6`. |
| Tests | `tests/unit/test_lower.c:547` | Fixed | `c8845ef6`. |
| Tests | `tests/CMakeLists.txt:320` | Left | `2dcb38e6` fixed what needs no new directory. The projects of the `anti` commands and the scripts flat in `tests/` need a new directory under `tests/`, which is Eddie's decision. |
| Tests | `tests/CMakeLists.txt:3678` | Fixed | `eaaa26f9` and `2dcb38e6`. |
| Tests | `main.c:265` | Fixed | `dfe5d620`. The stub of a slot that `reflect.call` alone reaches has no test: nothing in Anti reaches it, a question for Eddie. |
| Tests | `tests/run_start.cmake:1` | Fixed | `8de724a1`. |
| Tests | `tests/CMakeLists.txt:1305` | Fixed | `07249add`. |

## Defects no rule names

| Report | Place | State | Commit or reason |
|---|---|---|---|
| front-end | `sema_const.c:527`, text constants | Fixed | `2117cca4`. |
| front-end | `sema_const.c:694`, struct literal constant, S7 of the first audit | Fixed | `97426525`. |
| front-end | `sema.c:2475`, enum value past its base | Fixed | `82e5112e`. |
| front-end | `sema_stmt.c:2515`, `switch` arms | Fixed | `a7ab4085`. |
| front-end | `sema_export.c:613`, doc names of a library class | Fixed | `2d775a15`. |
| front-end | doc comment in `f"{ }"` | Fixed | `6f7a6978`. |
| front-end | `lexer.c:918`, raw NUL in `'...'` | Fixed | `46f68f87`. |
| rt | `rt.configure` and `[injections]` | Fixed | `9383cd04`, as Eddie decided. |
| anti-tool | `manifest.c:92`, `manifest_inject_read` | Fixed | `984be114`. |
| anti-tool | resolver keeps every requirement | Fixed | `338e83fd`. |
| anti-tool | `deps.c:649`, `resolve_from_repo` | Fixed | `09ab37dd` and `1662bc49`. |
| anti-tool | `fmt.c:1896`, `fmt_run` | Fixed | `8d078d5e` and `8e40cdae`. |

## What stands

Four rows are left, each for a decision of Eddie's: `Changed` and
`Snapshot`, the names of size and presence, the `?str` of `args`, and the
directories of `tests/`. Two questions stand beside rows that are fixed or
no defect: the builder that runs out of memory and the reflection stub of a
plugin slot.
