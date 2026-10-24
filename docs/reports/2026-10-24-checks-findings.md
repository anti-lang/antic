# The open findings before the compile-time checks

The step fixed the open findings of `docs/audit/tests-findings.md` that the
solid run left, severe first. Logs are under `build/drive/logs/checks-findings/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| T139 | `1f44581a`, `09355daa`, `153a97ce` | anti.fs, TOML, patterns, traces, the loader, the registry, a monitor and the start of a thread turned memory that ran out into an error or less. |
| T142 | `cfe9dd89` | The switch to LLVM dropped the check of a frame of 2^31 bytes. |
| T145 | `35defd9f` | `load` and `unload` kept the loader's buffer as the message. |
| T150 | `53c366f5` | One object of `alloc` came from malloc whatever `align(N)` said. |
| T158 | `e7b396ca` | `show_value` took an array field's bytes from a size of 0. |
| T166 | `2deb8e3a` | A search handed a missing pattern to PCRE2. |
| T175 | `b3970973` | `--no-reflect` dropped the field lists of values as well. |
| T141 | `6deae4a8` | `memcheck_function` passed over the bitfield operations. |
| T148 | `7b37b27f` | A constant `none` of a `?T` holding a class was read as an aggregate. |
| T149 | `aa7e3d19` | A default naming a later class was evaluated before that class had fields. |
| T151 | `c3a19837` | An unread licence text left an empty notice and status 0. |
| T152 | `46e29969` | A failed read kept its partial bytes and printed an unwritten buffer. |
| T167 | `b34a9eef` | The C result was cut at the first parenthesis of the type. |
| T168 | `902e12bd` | `super` replaced the field of the base on the walk up. |
| T169, T179 | `92563d1b` | Library files carry dependencies without their paths. |
| T170 | `bd59575b` | A fixed base without an Anti type fell back to `c_int`. |
| T171 | `bdcfdd84` | `const_as` folded numbers and chars alone. |
| T172 | `5e3741ca` | An operator with an owner and parameters of its own was not copied. |
| T176 | `dc9a4f28` | The literal took the left operand's type before inference. |
| T146 | `0a9be770` | The link line lacked the regex glue and PCRE2. |
| T153 | `6982765a` | Callers used the silent `files_make_dirs`. |
| T154 | `9e67384e` | `macho_lines` went on past an unread object. |
| T155 | `3c05ba35` | `copy_stubs` passed over an entry of unreadable kind. |
| T156 | `7c24eebd` | `files_copy` did not say which file failed. |
| T159 | `c14a832b`, `f26419e9` | An alias depth above 32 counted as a cycle. |
| T160 | `3a842189` | A bool in a cast of a copy's length stayed unfolded. |
| T161 | `0e51022d` | A hook of a refused signature counted as well formed. |
| T162 | `9bfccf59` | `unify` did not read the element of a channel. |
| T163 | `118a59b1` | `operator_spelling` gave `>=` to every missing operator. |
| T164 | `85e9628d` | An infinity was written as `inf`, no JSON. |
| T165 | `f217242e` | An enum body did not drop `internal` after its message. |
| T173 | `0dbcd7fe` | `tree_item` named an operator by its item. |
| T174 | `58121ea3` | `ends_value` read the comment written last. |
| T177 | `c0ae8057`, `0d653ee8` | The name of a function type left out `own`. |
| T178 | `e631038c` | A brace inside the header's brackets was taken for the body. |

`f26419e9` drops a bound of T159 that never held, and `0d653ee8` leaves the
`own` of T177 out of a page after the colon. `76d32792` routes the stop of
the tools through `alloc.c`, and `153a97ce` corrects two cases of T139.

## Findings left

T136 and T138 show on Windows alone. The step fixed both in the code, `fad02b0d`
and `08ca2f7d`, and their lines keep `host=windows`.

## Tests whose expectation was wrong

- `faults_regex` bytes, map and message, `faults_registry` objects, blocks and
  fields: the expected files lacked what the program prints before it stops.
- `faults_trace_copy` four cases, `plugin_versions_fault_stubs_memory`,
  `faults_threads.thread_memory` and `faults_os.reader_memory` held the old
  fallback where decision 4 stops the program.
- `anti_build_message_fault_object_directory_not_made` named a directory no
  build makes. It is `fault_library_file_directory_not_made` now.
- The path fault cases of `anti build` count the open of a project's manifest.
- Four test programs that `anti fmt` wrote in the old form of T178.

## Identity

`emit_identity` is unchanged. `link_identity_macos-arm64` changed twice, by
T139 in `22aeb408` and by T164 in `e73ba268`.

## Counts

`tests/known-failures.txt`: 133 lines and 114 entries before, 21 lines and 2
entries after, both `host=windows`. No new finding was recorded.

| Suite | Tests | Passed | Skipped | Expected failures | Logs |
|---|---|---|---|---|---|
| host | 6362 | 6360 | 2 | 0 | `host-final-1.log` to `-3.log` |
| asan | 6360 | 6358 | 2 | 0 | `asan-1.log` to `-4.log` |
| ubsan | 6360 | 6358 | 2 | 0 | `ubsan-1.log` to `-5.log` |

## Provisional entries added

- `--no-reflect` drops the field lists of classes alone (T175).
- `serialize` writes an infinity as `1e999` and `-1e999` (T164).

The non-provisional entries on the loader's message (T145) and on frames of
2^31 bytes (T142) say what the fixes do now.

## Not run on the target

- T136, `plugin_trace` on Windows: the `.def` of a plugin exports `anti_licenses`.
- T138, `no_llvm_cov_packed` on Windows: the runner reads a wrapped message as one line.
- `platform_windows.c`: a monitor and the start of a thread stop on lost
  memory, `program_faults_threads_fault_thread_memory`.
- T146 on Windows and Linux: `clib_regex_link_line` with `cl` and `.lib`
  names, and a GNU linker that reads the glue before the runtime.
- T150 on Windows: `anti_rt_malloc_aligned` there ignores the alignment, so
  `matrix_types_alloc_of_an_object_aligned_above_16` would fail, as R050
  already leaves the Windows targets at the alignment of malloc.

## Questions

1. `serialize` keeps the text `nan` for a NaN, which is no JSON.
2. ASan part 4 took 563 s, near the 10 minutes of a call. The next step
   should split it.

## State at the push of the code

```text
$ git log --oneline -3
2c445674 Bring CLAUDE.md up to date after the answers and the open findings
feb9d5f3 Record the commits of the findings the step of open findings fixed
e73ba268 Write the digest of return42 for macos-arm64 again after T164
$ git status --short
$ git rev-parse HEAD origin/main
2c445674590d87e6f486a979646d50dd83b90377
2c445674590d87e6f486a979646d50dd83b90377
```
