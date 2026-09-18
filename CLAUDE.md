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
- All tests pass on the host before every commit.
- The two sanitizer builds run before every push, whatever the work is.
  `cmake --preset asan` and `cmake --preset ubsan`, each with the full suite.
  UndefinedBehaviorSanitizer found a real defect on its first run here, a `bool`
  field read as 64 that the ordinary build passed over.
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
- Every session ends with a report in `docs/reports/<date>-<subject>.md`.

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
2. The whole-program pass over the IR of every module. It is the first
   compiler work. Four things are passes over it: release-mode
   devirtualisation, the class registry, the singleton check and the used-slot
   bitmaps that plugins need. Build the pass once, then those.
3. The class registry, and with it `anti.reflect`'s `call`, `new` and `Value`,
   and `Object.deserialize`.
4. The `[module]` thresholds of `anti.log`. They stay. Every log call passes
   its own module path as a compile-time constant. The check is then one
   comparison against a table read at start. `log.named("http")` may exist
   beside them for a logger per subsystem, not instead of them.
5. `f"..."` interpolation, which is compiler work over `anti.text`.
6. The native libraries in `libs/`, which nothing builds yet.
7. Inline atomic instruction sequences, which are runtime calls today.

Then `docs/anti-language-additions.md` in the order its "Timing" section gives:
nullable pointers, dev-mode checks, debug information, and tests and fixtures
before the first public release. After it, the wrapping and saturating operators
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
- `std/` holds `anti.io`, `anti.text`, `anti.license`, `anti.error`, `anti.time`,
  `anti.os`, `anti.reflect`, `anti.random`, `anti.collection`, `anti.toml`,
  `anti.args`, `anti.json` and `anti.log`.
- 404 ctest tests pass on the development Mac and none is skipped. The ASan and
  the UBSan builds run 403 each, without the `no_paths` test, which needs a
  build that no sanitizer wrote paths into.
- Anti 0.1.0 installs with one command, and all six packages are published under
  `downloads/resources/anti/0.1.0/` of anti-lang.com. See `docs/distribution.md`.
- `.github/workflows/test.yml` runs a five-runner matrix on `workflow_dispatch`
  only. It has never run.
- The `anti` tool is not written. `tools/scripts/format_anti.py` stands in for
  `anti fmt`.
