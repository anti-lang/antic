# Fix step 40, the minor findings of the runtime

Step 40 of "Fix steps" in `docs/audit/summary.md`, the session for the
runtime: the rows of the minor table whose places lie in `src/rt/` or
`src/native/`. The detail is in `docs/audit/rt.md` and
`docs/audit/input-readers.md`. The finding on `-isystem` stands at
`CMakeLists.txt:124` and concerns the runtime's `regex.c`, so this session
took it. Logs are in `build/drive/logs/s40r-*`.

Status: green and pushed.

## Findings

| Rule or question | Result |
|---|---|
| Q3, header list of the glue, `src/native/pcre2.cmake:71` | Fixed in `4980ac22`. The glue, every object of `anti_rt` and the licence stub write a dependency file with `-MMD`, read through `DEPFILE`. Touching `rt.h` rebuilds the glue (`s40r-dep-touch.log`). |
| Q3, layout of `Match`, `src/rt/patterns.c:126` | Fixed before this step in `d1515881`. `struct anti_match` stands in `regex.h`, and `types.c` names it there. |
| Q5, a version compared two ways, `src/rt/plugin.c:129` | Fixed in `2eb62ac3`. `manifest_read` refuses a `package.version` that `deps_version_valid` refuses. `manifest_refusals` read `1.0.0.1` and `1.0-beta` before (`s40r-red-version.log`). |
| 1, extensions, `src/rt/lock.c:63` | Fixed by step 34 in `8a79aab0`. |
| 1, `-isystem`, `CMakeLists.txt:124` | Fixed in `4980ac22`. `antic_core` takes the PCRE2 include directory as `SYSTEM`. |
| 3, `src/rt/atomic.c:60` | Fixed in `e83e945b`. A byte goes in and out by its bits. The MSVC branch, forced in, compiles with clang for windows-x86_64 (`s40r-msvc-x86_64.log`). For windows-arm64 clang refuses the `__int8` of the old load, which MSVC takes. |
| 3, `src/rt/start.c:213` | Fixed in `085b3978`. `exit_status` keeps the low 32 bits by defined arithmetic. `conf_exit_status` pins it, and passed before as well, since clang gives the same bits. |
| 11, `src/rt/regex.h:29` | Fixed in `a8bc6477`. |
| 13, `src/rt/text.c:133` | Left. See below. |
| 14, `src/rt/conf.c:700` | Fixed in `4ea305d3`. Both walks stop at the 257th file of one configuration. `conf_include_depth` failed before (`s40r-red-fanout.log`). |
| 16, `src/rt/plugin.c:791` | Fixed in `375ee78e`, with the trace line of `syms.c:1418`. |
| 18 | Left for step 41, which takes the splits, as the other step 40 sessions did. |
| 20, `src/rt/object.c:218` | Fixed in `d59fd2ee`. |
| 25, `src/rt/object.c:65` | Not a defect. Rule 25 of `docs/c-guidelines.md` now names the exception: a C function that implements an item of an Anti module carries its mangled name, as `anti_lang_Object_*` does. |
| 26, growing buffers, `src/rt/patterns.c:606` | Fixed in `eeac32fc`. `anti_rt_reserve` of the new `src/rt/grow.h` holds the doubling and the overflow check, and each caller keeps its own answer to a failure. The unit test did not compile before (`s40r-red-grow.log`). |

The one runtime defect that no rule names, `[injections]` in
`rt.configure`, was fixed by step 31.

## Left: rule 13

The finding asks that `anti_rt_builder_append` end the program through the
failure routine when memory runs out. "Standard library phase" in
`docs/decisions.md` decides the other way, and the entry is not
provisional: "A count of bytes that the size of a text builder cannot hold
is treated as memory that ran out. The builder keeps what it holds, and a
pad or an append of that count writes nothing." `builder_bounds` of
`tests/unit/test_rt_bounds.c` pins it, and `Builder.take_in` follows it.
The question for Eddie: should the builder stop the program through
`anti_rt_fail_abort` when memory runs out, as `anti_rt_text_copy` does,
or keep the decision?

## Provisional entries added

- `anti` refuses a manifest whose `[package] version` is not one to three
  parts of digits.
- One configuration reads at most 256 files, in the runtime and in
  `anti symbols`.
- A status of `main` past 32 bits keeps its low 32 bits on every system.

## Gates

The build has no warnings in the three trees (`s40r-final-build.log`,
`s40r-asan-build.log`, `s40r-ubsan-build.log`). The pinned digest of
`return42` for macos-arm64 changed with `start.c` and was written in
`d921dd47`. The docs-style checker reports nothing on `docs/decisions.md`
and this report.

| Suite | Passed | Log |
|---|---|---|
| host | 1382 of 1382 | `build/drive/logs/s40r-final-host.log` |
| asan | 1381 of 1381 | `build/drive/logs/s40r-asan.log` |
| ubsan | 1381 of 1381 | `build/drive/logs/s40r-ubsan.log` |

## State

Taken after the push of the code and before the commit of this report.

```console
$ git log --oneline -3
d921dd47 Write the link digest of return42 after the runtime changes of step 40
d59fd2ee Make the variant case lookup of object.c static
e83e945b Pass the bytes of the Windows atomics by their bits
$ git status --short
$ git rev-parse HEAD origin/main
d921dd47ef750d2d08856a6a96bdf2e81f874134
d921dd47ef750d2d08856a6a96bdf2e81f874134
```
