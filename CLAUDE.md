# Handover

This repository is the Anti language. It holds `antic`, the `anti` tool, the
runtime, the standard library, the runtime archive with its native libraries,
and everything served from anti-lang.com. It implements every rule in
`docs/anti-object-model.md`, `docs/anti-language-additions.md` and
`docs/decisions.md`. The book lives in `FoundingFuture/book-writing-a-compiler`
and is not a concern here.

## Order of authority

1. `docs/decisions.md`. One entry per settled decision, and the open items at
   the end.
2. `docs/anti-object-model.md` and `docs/anti-language-additions.md`. The two
   specifications. Every rule in them is decided. A rule that is not implemented
   is work, not a question.
3. `docs/tooling.md`, `docs/tooling-addendum.md`, `docs/distribution.md` and
   `docs/libraries-for-c.md`. They agree with `docs/decisions.md`, and a change
   goes into both.

`docs/anti-syntax-overview.md` summarises the two specifications and adds
nothing. A change to either specification changes it in the same commit. Its
"Built" and "Not built yet" lines are the status page of the implementation,
and a commit that builds a feature changes them.

The choices inside a compiler pass are in `docs/notes/`. The design pages of the
runtime archive, the build tool and the standard library are in `docs/site/`.
The test machines are in `docs/vm-setup.md`. Do not re-open a settled decision
without asking Eddie.

## Rules

- No AI attribution anywhere: no Co-Authored-By lines, no session links, no
  "generated with" notes in commits, files or pull requests.
- Commit messages: a short subject line, a body that states what changed and
  why. No emoji, no trailers.
- Warnings are errors. `CMakeLists.txt` sets `-Wall -Wextra -Wpedantic -Werror`
  and `/W4 /WX`. The build produces zero warnings on clang, gcc and MSVC.
- A commit that changes only files under `docs/`, or `CLAUDE.md`, `README.md`
  or `CHANGELOG.md`, runs the docs-style checker and nothing else. That covers
  reports and notes. Prose is prose wherever it sits. A push made only of such
  commits needs no suite.
- A commit that changes anything else runs the full suite on the host first.
  A push that includes such a commit runs both sanitizer suites first, once
  per push and not once per commit. `cmake --preset asan` and `cmake --preset
  ubsan`, each with the full suite. UndefinedBehaviorSanitizer found a real
  defect on its first run here, a `bool` field read as 64 that the ordinary
  build passed over.
- Every comment and every `.md` file follows the docs-style rules. Run
  `python3 tools/docs-style/check_docs.py <files>` before committing, and fix
  every finding. The copy under `tools/` is pinned on purpose, so the rules do
  not change when the skill it came from syncs.
- No workflow runs. Every workflow stays `workflow_dispatch` only, which the
  test `workflows_dispatch_only` checks. Hosted build minutes are limited.
- One commit per logical change. Push to `origin/main` directly, no pull
  requests, after every completed step.
- The gap procedure. Where a specification is silent, take the smallest option
  that keeps every struct C layout and the IR free of sizes. Record one
  `[provisional]` line in `docs/decisions.md` with its reason, and continue.
  Questions go in the report at the end of the session and nowhere else.
- A `[provisional]` entry is a decision made by a session that Eddie has not
  reviewed yet. It is binding until Eddie changes it. Nothing is ever removed,
  disabled or narrowed because it is provisional. Provisional entries are
  reviewed by Eddie in batches, and a review either removes the tag or changes
  the entry. No other action follows from the tag.
- Never two calls that emit code or have side effects in one argument list.
  Bind each to a local first. C leaves the order of the arguments unspecified.
  antic built by clang for Windows emitted another program when a call in
  lower.c passed two of them. The test `emit_identity` compares the output with
  the Mac's.
- Every session ends with a report in `docs/reports/<date>-<subject>.md`.

## Context and budget

- Read what the task needs. Name sections, not documents: "Tables and dispatch" in
  docs/anti-object-model.md, not the whole file. Never reread a file already in the
  context.
- Do not narrate. No summaries of what you are about to do, no restating of the
  task, no lists of what you read. Say what changed and what failed, in one line
  each, and only when a step completes.
- Run the full suite once per commit that needs it, not per edit. While working on
  one test, run that test alone. Run the sanitizer suites once per push that needs
  them.
- Do not print test output into the context. Redirect it to a file and grep for
  failures. Show me at most the failing lines.
- Do not show file contents you did not change. When you edit, show the diff, not
  the file.
- One task per session. When a task ends, write the report and stop. Do not start
  the next task in the same context.
- When context is compacted, first write the current state to
  docs/reports/<date>-state.md: steps done, step in progress, next command. Then
  continue from that file.
- Reports under two pages, as before. The state file under half a page.

## Building

```bash
cmake -S . -B build && cmake --build build -j8 && ctest --test-dir build -j8
```

`CMakePresets.json` carries the two sanitizer configurations: `cmake --preset
asan` and `cmake --preset ubsan`, each with its own build directory. Both run
the full suite.

The pinned clang in `build/clang` compiles everything, and the configure step
installs it and the pinned LLVM tools in `build/llvm/bin` with
`tools/get-clang.cmake` and `tools/get-llvm.cmake`. The build also needs the
sysroots in `build/sysroot`, which `tools/get-sysroot.cmake` installs after
`tools/get-clang.cmake`, and the pinned raylib source in `build/raylib`, which
`tools/get-raylib.cmake` installs. `tools/clang-pin`, `tools/llvm-pin`,
`tools/sysroot-pins` and `tools/raylib-pin` hold the versions and the digests.
clang and the LLVM tools come from the releases of `anti-lang/llvm-tools`, and
openssl checks their signature. Windows configures with `-G Ninja`.
`-DANTIC_SYSTEM_COMPILER=ON` is for a reader's build and never for a release.

## Layout

| Path | Contents |
|---|---|
| `src/` | The compiler |
| `rt/` | anti_rt sources, starting with `rt/start.c`, 0BSD |
| `std/` | The standard library in Anti, 0BSD |
| `libs/` | CMake build of the third-party static libraries in the runtime archive |
| `tests/` | Test programs, expected outputs, the runner |
| `tools/` | Helper scripts, the pins, the installers and the docs-style checker |
| `docs/` | The specifications, the decisions and the reports |
| `keys/` | `release.pem`, the public key that checks the LLVM tools |
| `LICENSES/` | Licence texts of bundled components |

## First sessions, in order

1. Done. The LLVM tools are built for every host in `anti-lang/llvm-tools`,
   and antic downloads the archive of its host. `docs/decisions.md` carries it
   under "Scope and toolchain", and `docs/reports/2026-09-21-llvm-tools.md` of
   that repository reports the first release.
2. Done. The whole-program pass over the IR of every module, with release-mode
   devirtualisation, the class registry, the singleton check and the used-slot
   bitmaps. `docs/reports/2026-09-19-whole-program.md` reports it, and
   `docs/notes/whole-program.md` holds its choices.
3. Done. One descriptor and one set of tables per class in dev and release.
4. Done. `inherits` accepts a qualified `module.Class`.
5. Done. Both VMs ran the suite, `emit_identity` included.
6. Done. `anti.reflect`'s `call` and `Value`, and the bitmap rule.
   `docs/reports/2026-09-19-first-sessions-3-to-6.md` reports 3 to 6.
7. Done. The field record carries a type id. `get` and `set` take and return
   `Value`, and `serialize` and `deserialize` cover every field kind.
8. Done. Tests of `reflect.call` through an interface pointer and of
   `self.super.construct(x)` on a base from another module, in both modes.
9. Done. `reflect.call` follows the error convention, and the decisions hold
   Eddie's answers. `docs/reports/2026-09-19-first-sessions-7-to-9.md`
   reports 7 to 9.
10. Done. `delete(p)` runs `destruct` on `p`, then on every object it owns,
    recursively, then frees the memory of each.
11. Done. One descriptor per struct per program, written by the module that
    declares it, as for classes.
12. Done. The entry on the `str` that `Object.deserialize` reads names the
    form with an `Allocator` that replaces it. The tag stays.
13. Done. Both VMs ran the suite at `03d1064`.
    `docs/reports/2026-09-19-first-sessions-10-to-13.md` reports 10 to 13.
14. The `[module]` thresholds of `anti.log`. They stay. Every log call passes
    its own module path as a compile-time constant. The check is then one
    comparison against a table read at start. `log.named("http")` may exist
    beside them for a logger per subsystem, not instead of them.
15. `f"..."` interpolation, which is compiler work over `anti.text`.
16. The native libraries in `libs/`, which nothing builds yet.
17. Inline atomic instruction sequences, which are runtime calls today.
18. Done. The one manifest of a release, and the installers that read its
    signature. `docs/reports/2026-09-20-one-manifest.md` reports both.
19. Done. Every Windows link passes `/DEBUG` with `/PDBALTPATH:%_PDB%`, the
    packer leaves the PDB of each Windows program in `build/dist/symbols/`,
    and step 4 puts it in the symbols archive beside the map of the sections.
    `tools/check-pdb.cmake` reads the CodeView record of an executable
    against the GUID of its PDB, in step 4 and in the test `pdb_guid`.

Then `docs/anti-language-additions.md` in the order its "Timing" section gives:
nullable pointers, the dev-mode checks, the lines of `-g`, tests and fixtures
and the CPU levels are built. The variables of `-g` come before the first public
release. After it, the wrapping and saturating operators
with `Flags`, then sum types, then locking and channels. Then injection, hooks
and tracing, plugins and runtime configuration, which belong together. Then
generics and closures.

`docs/work-order-completion.md` is the work order those items come from, with
its book steps removed. `docs/reports/2026-09-20-object-model-completion.md`
reports what it finished.

## State

- The compiler lexes, parses and type-checks Anti across modules. Its IR holds
  no sizes: the back end lays out types per target (`src/layout.c`) and folds
  symbolic values. Both back ends cover all integer and float operations.
  The emitter writes assembly for all six targets, llvm-mc assembles it, and lld
  links every target against the sysroots of the runtime archive.
- The object model is implemented: classes, interfaces as inline sub-objects
  with thunks, four visibility levels, `construct` and `destruct`, operators,
  singletons, the error forms and reflection over descriptors.
- `worker fn`, `parallel` and `dispatch` compile and run on all six targets.
- A pass over the whole program's IR runs after lowering. In dev mode it runs
  for the module that links. It holds release devirtualisation, the class
  registry, the singleton check and the used-slot bitmaps.
- `std/` holds `anti.lang`, `anti.io`, `anti.text`, `anti.license`,
  `anti.error`, `anti.time`, `anti.os`, `anti.reflect`, `anti.random`,
  `anti.collection`, `anti.toml`, `anti.args`, `anti.json` and `anti.log`.
  `anti.lang` is the root and imports nothing. It holds `Error` and
  `NoneDereference`, and `anti.error` holds `SystemError`, `on_fatal` and
  `check`.
- 519 ctest tests pass on the development Mac and none is skipped. The ASan and
  the UBSan builds run 518 each, without the `no_paths` test, which needs a
  build that no sanitizer wrote paths into.
- `antic -g` writes the line of every statement, and the link then keeps the
  debug sections. lldb and gdb stop by file and line and print a backtrace of
  Anti function names. Variables are the next step. See `docs/notes/debug.md`.
- `may fail` and `fail` are built. A function declared `may fail` returns
  `?*Error` and writes its result through an out pointer. `fail` leaves on the
  error channel, with an error or with a text, `try` forwards inside another
  `may fail` function, and `undo` runs on the fail path. The header writes the
  ABI and the `.antl` records the flag. anti.lang, anti.error, anti.io and
  anti.text follow the form: `text.parse_int` may fail, and nothing else of
  anti.io and anti.text fails. The other modules of the standard library
  still write it by hand.
- Tuples are built. `(int, str)` is an anonymous struct with C layout, `(a, b)`
  builds one, `t.0` reads an element, and `let (a, b) = e;` and
  `for i, x in items` are the two forms that take one apart. The header writes
  one struct per distinct tuple of an exported signature, and the `.antl`
  carries the elements.
- `*T` never holds `none` and `?*T` may, and a function value follows the same
  rule with `?fn(...)`. Narrowing is per block and follows `&&` and `||`.
  `let m = p else { }` and `p catch` bind the checked value, and every pointer
  of an `extern fn` is `?*T`. A failing function returns `?*Error`.
- Anti 0.1.0 installs with one command. The six packages, the six symbols archives and
  `SHA256SUMS` are assets of the GitHub release of the tag, which `tools/release-base`
  names. anti-lang.com serves text alone: the two installers, the downloads page,
  `SHA256SUMS.sig` of every version and the public key, which is the single trust
  anchor. `tools/site-base` names it. The two halves stand on two hosts on purpose, so
  a forged release needs both. See `docs/distribution.md`.
  An install follows the platform: `~/.local/bin` and `~/.local/share/anti`, or
  `%LOCALAPPDATA%\Programs\anti\bin` and `%LOCALAPPDATA%\anti`. Nothing outside
  the user's profile is written. The packer links macOS against the Apple SDK
  that `tools/macos-sdk-pin` names, never a bare `xcrun`.
- `./r` makes a release, in eleven steps from a pushed `main` to the published
  download. The version stands in `tools/version` and its entry in
  `CHANGELOG.md`. `./r --dry-run` runs the first five steps and prints a plan
  for the rest. See `docs/work-order-release-script.md`, and its decisions
  under "The release script" in `docs/decisions.md`.
- `.github/workflows/test.yml` runs a five-runner matrix on `workflow_dispatch`
  only. It has never run.
- The `anti` tool holds `sdk export`, `sdk import` and `test`, and nothing else
  of `docs/tooling.md`. `tools/scripts/format_anti.py` stands in for `anti fmt`.
- `tests { }` and `fixtures { }` compile under `antic --tests` alone, and
  `anti test` writes the runner, links it and runs it. Every other build drops
  both blocks after parsing.
- The processor levels are built. `--cpu` takes `v1`, `v2` or `v3` on x86_64 and
  `armv8.0`, `armv8.2` or `armv8.5` on ARM64, and each target has a default:
  x86-64-v3 on both x86_64 targets, `armv8.5` on macos-arm64, `armv8.2` on
  windows-arm64 and `armv8.0` on linux-arm64. x86-64-v3 writes the VEX forms of
  the scalar float instructions. The runtime archive holds one anti_rt per
  target and level in `lib/<target>/<level>/`, so a program below the default
  links a runtime of its own level, and its atomics are one instruction at
  `armv8.2` and above. Every program checks the processor at start, against the
  level of the runtime it linked, and refuses a machine below it. `src/cpu.c`
  holds the table, `tools/cpu-levels` holds it for the build and
  `cpu_levels_pin` compares the two.
