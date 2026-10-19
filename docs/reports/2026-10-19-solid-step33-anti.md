# Step 33, the minor findings of anti

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. This session took `anti`, the rows of
the minor tables whose places lie in `src/anti/`, and the comment on
`WRAP_COLUMNS` that step 32 left. The logs stand in
`build/drive/logs/step33-anti/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Rule 12, `main.c:198` | `a08542a5` | Nine functions freed what they held before each return. `read_stream` restored the length of a text after a failed read and kept its memory. |
| Rule 14, `syms.c:343` | `4cf611bf`, `f24740e9` | Five paths that a configuration or a report names went through `files_read`, so a FIFO was waited on. `configuration_fifo` and `report_fifo` of `unit_syms` timed out before. |
| Rule 14, `zip.c:673` | `193a5863` | `zip_unpack` and `json_read` added 0 to a null pointer. |
| Rule 17, `zip.c:43` | `290a02bc` | The fields of the zip format stood in `unsigned long` and `unsigned`. |
| Rule 26, `syms.c:837` | `4784a822` | Two copies of the unescape called `malloc`. The case `name` of `files_oom` ended through antic's alloc before. |
| Warnings, `bindapi.c:153` | `c436621b` | `(void)is_union;` had no reason. |
| Status 3, `build.c:333` | `24a470d2` | The status of a dev object without main was a number at seven places. |
| The prefix of a message, `bindclang.c:101` | `88749523`, `f68908c8`, `c054115c` | anti spelled `anti: ` at 161 calls and antic `antic: ` at 92. The runtime wrote out of memory in three forms. `say_anti` and `say_antic` of `src/antic/say.c` and `anti_rt_fail_memory` hold the forms, and the case `snapshot` of `rt_oom` lacked the prefix before. |
| Names spelled twice, `main.c:488` | `8e1a536b`, `64222dbb`, `a1ab4403`, `73a1537e` | The build directory, the entries of a symbols archive, the directory of a deliverable, `anti/test` and the macOS targets were literals at more than one place. |
| `strstr` on JSON, `sdk.c:41` | `0b25d29c` | A nested `Version` answered for the SDK. `sdk_export_version_of_the_top_object` failed before. |
| Two `struct unit`, `syms.c:409` | `615f05a0` | The records of `syms.c` took the name of the one of `units.h`. |
| Three helpers, `repo.h:20` | `8fe30b39` | The grammar of a name stood in `repo.c`, the reader of a library header in `deps.c`, and `default_runtime` wrapped one call. |
| Nine comments, `build.c:487` | `0513b451` | Seven comments differed from the code. `root_module` went with `2ca6d800`, and `found` of `resolve_from_index` with `9bdd67cc`. |
| `WRAP_COLUMNS` of `fmt.c` | `b534cf0c` | The comment named `anti html` and `anti tex`, which are not built. |

`8bd056e6` writes the commits into the summary. The notes of the
finding at `doc.md:53` went with `1d078cf6`, and the eight headers with
`342ab0f3`.

## Kept as documented, and questions for Eddie

- The work directories of `anti check`, `anti doc` and `anti test` stay
  under `build/` when `[layout]` names another build directory, since the
  entries on `anti doc` in `docs/decisions.md` give `build/doc` and
  `build/doc-work`. Question: should they follow `[layout]`?
- The refusal of a redirect in `repo.c` keeps "127.0.0.1 or localhost",
  which the entry on redirects quotes. The comment of `repo.h` names
  `[::1]` too.

## Messages that changed

The runtime gives every failure of memory as `anti: out of memory`
followed by its text. The lines of `lock.c` and `snapshot.c` gain the
prefix, and the five `anti: no memory for` lines of `sync.c` and
`threads.c` read `anti: out of memory for`. No document quoted them.
Every message of anti and antic keeps its bytes.

## The tests that cannot fail

The pinned clang 23 follows N3322, which defines NULL + 0, so UBSan
reports neither sum of `193a5863`. `empty_entries` of `unit_zip` and
`json_messages` of `unit_bind` take both paths and passed before the fix.
The refactors of rule 12, the comments, the names and the moves change
no behaviour, and the suites hold them.

## The freeze, provisional entries, known failures

No new language feature, option or command. `say_anti`, `say_antic`,
`anti_rt_fail_memory` and `symmap_readable_name` are C helpers of the
tools and the runtime, and no item of a module. No `[provisional]` entry
was added. The entry on the interface of `anti` lists `say.h` among the
shared helpers now, as `anti_interface` does. No new finding.
`tests/known-failures.txt` has 29 lines before and after, 10 tests and 8
of them on the Mac.

## Identity

- `emit_identity` passes unchanged.
- `link_identity_macos-arm64` changes by `c054115c`. With the `src/rt/`
  before it the old digest passes, `li-old.log`. `12669590` writes the new
  one.

## Gates

- The three builds report no warnings: `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5141 tests, `-I 1,2000` 333 s and `-I 2001,5141` 277 s. All 2000
  and 3162 pass, a part counting the fixtures it pulls in. The 8 expected
  failures of the Mac fail as listed, and the two `mimalloc_environment`
  tests skip: `host-1.log` and `host-2.log`.
- ASan, 5139 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5139`,
  285 s, 367 s and 366 s. All 1150, 1113 and 2910 pass with the same
  skips: `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 258 s, 262 s and 296 s. All 1150, 1113 and 2910
  pass: `ubsan-1.log` to `ubsan-3.log`.
- The docs-style checker reports nothing on each `.md` file the session
  touched: `docs1.log` to `docs4.log`.

The first host run passed the 120 s limit of the tool, which moved it to
the background. The session waited for it in the foreground before going
on.

## Not run on the target

- `290a02bc`, the widths of the zip fields where `unsigned long` holds 32
  bits. `unit_zip` and the tests of `anti symbols` on anti-windows.
- `4cf611bf`, `files_read_file` of the paths of a configuration through
  the Windows branch of `platform_open_file`. `unit_syms` on anti-windows,
  whose FIFO cases run against missing paths there.
- `f24740e9`, `report_fifo` of `unit_syms` on Linux, and the line Windows
  writes itself. `unit_syms_messages` on both VMs.
- `c054115c`, the abort of `anti_rt_fail_memory` on Windows, status 3.
  `rt_oom` on anti-windows.

## State at the push

```text
$ git log --oneline -3
12669590 Write the digest of return42 for macos-arm64 again
8bd056e6 Record the anti minor findings of step 33 in the summary
c054115c Give every failure of memory of the runtime one form
$ git status --short
$ git rev-parse HEAD origin/main
12669590982c497408aa7b8f5d0cdc38a26e27b5
12669590982c497408aa7b8f5d0cdc38a26e27b5
```

## Left for the next session

The other areas of step 33, one per session, in the order of the
summary: the build and the tools, and the tests. Each takes the rows of
the minor tables in `docs/audit/summary.md` whose places lie in its area
and have no commit yet. The session of the tests takes the chapters of
the book in the comments of `tests/CMakeLists.txt`.

CONTINUE
