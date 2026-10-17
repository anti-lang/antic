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
and a commit that builds a feature changes them. A decision that changes the
language goes into the specifications and the overview in the same commit as
its entry in `docs/decisions.md`.

The choices inside a compiler pass are in `docs/notes/`.
`docs/notes/value-rules.md` holds the pinned outputs a change rewrites, the
leak-check pattern and the traps of the value rules. The design pages of the
runtime archive, the build tool and the standard library are in `docs/site/`.
The test machines are in `docs/vm-setup.md`, and the traps of the hosts, the
sanitizers, Windows and CMake scripts in `docs/notes/hosts-and-harness.md`. Do
not re-open a settled decision without asking Eddie.

## Rules

- No AI attribution anywhere: no Co-Authored-By lines, no session links, no
  "generated with" notes in commits, files or pull requests.
- Commit messages: a short subject line, a body that states what changed and
  why. No emoji, no trailers.
- Every tool a build runs comes from the pinned release of
  `anti-lang/llvm-tools` under `build/deps/`, on every host: the Mac,
  anti-linux and anti-windows alike. clang compiles, lld links, llvm-mc
  assembles and llvm-ar archives. Never a compiler, linker, assembler or
  archiver of the host: no system gcc or clang, no Apple ld, no GNU ld, no
  MSVC. Each target's runtime is the cross build against its pinned
  sysroot, musl for Linux, the host's own target included. The configure
  step refuses a tree that would run a tool of the host, and the test
  `pinned_tools` checks it. See "Scope and toolchain" in
  `docs/decisions.md`.
- Warnings are errors in all of our own code. That is the compiler, the
  tools, the runtime and the native glue code. It is also the tests and
  anything the packer or the release script builds. A part of our code built without them is a gap to
  close. The set stands in `tools/warnings.cmake` alone, and the test
  `warning_set` refuses a flag spelled anywhere else.
- A warning is fixed by changing the code so it is correct. No pragma, no
  `-Wno-` flag, no cast or `(void)` whose only purpose is to hide it. A
  warning that is genuinely wrong is silenced at that one line, with a
  comment giving the reason.
- Third-party sources in `src/native/` build with the warning flags of their
  own projects, from `src/native/warnings.cmake`, and are never patched to
  silence a warning. Our glue code around them stays under our flags. See
  "Warnings" in `docs/c-guidelines.md`.
- A commit that changes only files under `docs/`, or `CLAUDE.md`, `README.md`
  or `CHANGELOG.md`, runs the docs-style checker over its `.md` files and
  nothing else. That covers reports and notes. Prose is prose wherever it
  sits. A push made only of such commits needs no suite.
- A commit that changes anything else runs the full suite on the host first.
  A push that includes such a commit runs both sanitizer suites first, once
  per push and not once per commit. `cmake --preset asan` and `cmake --preset
  ubsan`, each with the full suite. Build and test the asan tree with
  `cmake --build --preset asan` and `ctest --preset asan`, whose presets set
  `ASAN_OPTIONS=detect_leaks=1`. UndefinedBehaviorSanitizer found a real
  defect on its first run here, a `bool` field read as 64 that the ordinary
  build passed over.
- Each of the three suites finishes in under 9 minutes with `ctest -j14`,
  the number of cores of the development Mac. Measured on 2026-10-12 in the
  step `values` of the test run: `host` 409 s, `asan` 535 s and `ubsan` 479 s.
  `package_keys` alone takes 408 s on the host, 499 s under ASan and 479 s
  under UBSan, and `guide_examples` 517 s under ASan. They set the time of
  all three. macOS checks every new executable in turn, about 0.3 s each,
  and the two tests wait in that queue, so a step that adds programs adds
  to both. The step `types` measured `host` 414 s, `asan` 569 s and `ubsan`
  472 s on the same day, while other programs of the Mac ran. Under ASan
  the times of all tests add up to 14 times the time of the suite, so
  every second of a test costs the suite a fourteenth. The 135 tests of
  `types` take 100 to 144 s there, 7 to 10 s of the suite. The ASan suite
  stands above its 9 minutes until tests get faster.
  `docs/reports/2026-10-12-tests-types.md` names where. The step
  `ownership` measured `host` 417 s. It ran each sanitizer suite in two
  parts, `ctest -I 1,1300` and `ctest -I 1301,2697`: `asan` 501 s and
  152 s, `ubsan` 448 s and 121 s. Both stand above 9 minutes as one run.
  The step `errors` measured `host` 418 s, and the same two parts up to
  test 2865: `asan` 526 s and 181 s, `ubsan` 462 s and 146 s.
  The step `threads` measured `host` 413 s, and the two parts up to test
  3028: `asan` 523 s and 201 s, `ubsan` 463 s and 125 s.
  The step `classes` measured `host` 425 s. It ran each sanitizer suite as
  `ctest -I 1,1000` and `ctest -I 1001,3234`: `asan` 474 s and 376 s,
  `ubsan` 437 s and 237 s.
  The step `modules` measured `host` 451 s, and the same two parts up to
  test 3507: `asan` 532 s and 396 s, `ubsan` 477 s and 266 s.
  The step of the checker's messages measured `host` 439 s, and the same
  two parts up to test 3533: `asan` 463 s and 397 s, `ubsan` 429 s and
  254 s.
  The step of the other messages measured `host` 435 s. It ran each
  sanitizer suite as `ctest -I 1,1000`, `ctest -I 1001,2000` and
  `ctest -I 2001,3654`: `asan` 251 s, 367 s and 215 s, `ubsan` 213 s,
  347 s and 172 s.
  The step of the options measured `host` 437 s. It ran each sanitizer
  suite as `ctest -I 1,1100`, `ctest -I 1101,2100` and
  `ctest -I 2101,3793`: `asan` 250 s, 361 s and 221 s, `ubsan` 241 s,
  346 s and 172 s.
  The step of the coverage of the front end measured `host` 441 s. It ran
  each sanitizer suite as `ctest -I 1,1100`, `ctest -I 1101,2100` and
  `ctest -I 2101,4126`: `asan` 273 s, 406 s and 243 s, `ubsan` 253 s,
  394 s and 196 s.
  The second session of that step measured `host` 438 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2200` and
  `ctest -I 2201,4382`: `asan` 267 s, 359 s and 265 s, `ubsan` 236 s,
  344 s and 201 s.
  The third session of that step measured `host` 451 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2200` and
  `ctest -I 2201,4455`: `asan` 276 s, 360 s and 283 s, `ubsan` 248 s,
  345 s and 209 s.
  The first session of the step of the coverage of the rest of antic
  measured `host` 482 s. It ran each sanitizer suite as `ctest -I 1,1150`,
  `ctest -I 1151,2200` and `ctest -I 2201,4514`: `asan` 271 s, 358 s and
  296 s, `ubsan` 249 s, 344 s and 222 s.
  The second session of that step measured `host` 517 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4555`: `asan` 259 s, 364 s and 291 s, `ubsan` 329 s,
  379 s and 233 s.
  The third session of that step measured `host` 496 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4573`: `asan` 263 s, 376 s and 292 s, `ubsan` 248 s,
  352 s and 229 s.
  The fourth session of that step measured `host` 528 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4594`: `asan` 314 s, 410 s and 293 s, `ubsan` 259 s,
  390 s and 232 s.
  The fifth session of that step measured `host` 502 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4624`: `asan` 267 s, 377 s and 302 s, `ubsan` 254 s,
  377 s and 232 s.
  The sixth session of that step measured `host` 525 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4648`: `asan` 275 s, 369 s and 316 s, `ubsan` 257 s,
  357 s and 242 s.
  The step of the summary measured `host` in two parts, `ctest -I 1,2000`
  and `ctest -I 2001,4650`: 396 s and 216 s. It ran each sanitizer suite
  in the three parts of the session before: `asan` 288 s, 406 s and
  313 s, `ubsan` 245 s, 364 s and 237 s.
  The step `record` of the rules of 2026-10-13 measured `host` as
  `ctest -I 1,2000` and `ctest -I 2001,4671`: 384 s and 228 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4669`: `asan` 301 s, 388 s and 318 s, `ubsan` 245 s,
  349 s and 236 s.
  The step of the rules of flow and of errors measured `host` as
  `ctest -I 1,2000` and `ctest -I 2001,4708`: 377 s and 228 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4706`: `asan` 266 s, 371 s and 321 s, `ubsan` 248 s,
  355 s and 243 s.
  The first session of the step of the rules of values and of constants
  measured `host` as `ctest -I 1,2000` and `ctest -I 2001,4758`: 378 s and
  233 s. It ran each sanitizer suite as `ctest -I 1,1150`,
  `ctest -I 1151,2250` and `ctest -I 2251,4756`: `asan` 266 s, 368 s and
  322 s, `ubsan` 246 s, 347 s and 240 s.
  The second session of that step measured `host` as `ctest -I 1,2000`
  and `ctest -I 2001,4774`: 377 s and 234 s. It ran each sanitizer suite
  as `ctest -I 1,1150`, `ctest -I 1151,2250` and `ctest -I 2251,4772`:
  `asan` 260 s, 371 s and 324 s, `ubsan` 245 s, 347 s and 247 s.
  The step of the rules of types and of generics measured `host` as
  `ctest -I 1,2000` and `ctest -I 2001,4825`: 374 s and 239 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4823`: `asan` 266 s, 365 s and 339 s, `ubsan` 242 s,
  346 s and 257 s.
  The step of the rules of ownership measured `host` as `ctest -I 1,2000`
  and `ctest -I 2001,4838`: 373 s and 239 s. It ran each sanitizer suite
  as `ctest -I 1,1150`, `ctest -I 1151,2250` and `ctest -I 2251,4836`:
  `asan` 266 s, 365 s and 340 s, `ubsan` 244 s, 346 s and 256 s.
  The step of the rules of threads measured `host` as `ctest -I 1,2000`
  and `ctest -I 2001,4862`: 437 s and 374 s. It ran each sanitizer suite
  as `ctest -I 1,1150`, `ctest -I 1151,2250` and `ctest -I 2251,4860`:
  `asan` 361 s, 399 s and 458 s, `ubsan` 336 s, 358 s and 369 s.
  The step of the rules of classes measured `host` as `ctest -I 1,2000`
  and `ctest -I 2001,4878`: 389 s and 246 s. It ran each sanitizer suite
  as `ctest -I 1,1150`, `ctest -I 1151,2250` and `ctest -I 2251,4876`:
  `asan` 285 s, 381 s and 338 s, `ubsan` 255 s, 367 s and 271 s.
  The step of the rules of modules and of C measured `host` as
  `ctest -I 1,2000` and `ctest -I 2001,4910`: 395 s and 246 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4908`: `asan` 281 s, 382 s and 352 s, `ubsan` 253 s,
  376 s and 278 s.
  The step of the messages and the options of antic measured `host` as
  `ctest -I 1,2000` and `ctest -I 2001,4924`: 380 s and 137 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4922`: `asan` 275 s, 381 s and 349 s, `ubsan` 256 s,
  360 s and 271 s.
  The step of the rules of the commands of anti measured `host` as
  `ctest -I 1,2000` and `ctest -I 2001,4947`: 379 s and 250 s. It ran each
  sanitizer suite as `ctest -I 1,1150`, `ctest -I 1151,2250` and
  `ctest -I 2251,4945`: `asan` 266 s, 369 s and 351 s, `ubsan` 241 s,
  352 s and 269 s.
  A suite past 9 minutes gets
  faster tests, never fewer: see
  "Repository layout" in `docs/decisions.md`.
- Every comment and every `.md` file follows the docs-style rules. The checker
  reads `.md` files alone. Run `python3 tools/docs-style/check_docs.py <files>`
  on the `.md` files before committing, read its exit status without a pipe,
  and fix every finding. The configure step installs `tools/hooks/pre-commit`,
  which runs the checker on the staged `.md` files and refuses a commit with a
  finding. Comments in source and CMake files follow the rules
  for comments without the checker. The tool output under `docs/audit/data/`
  stays as the tools wrote it. The copy under `tools/` is pinned on purpose, so
  the rules do not change when the skill it came from syncs.
- No CI. Anti runs no GitHub workflow, and the test `no_workflows` refuses
  any file under `.github/workflows/`. Every suite runs on the development
  Mac and on the two VMs of `docs/vm-setup.md`.
- One commit per logical change. Push to `origin/main` directly, no pull
  requests, after every completed step.
- The feature freeze of 2026-10-14, Eddie's decision, holds until Eddie
  lifts it. antic, anti, the runtime and the standard library get no new
  language feature and no new module or public item of the library. They
  get no new option and no new command either. A fix may add one only
  when the defect it fixes cannot be fixed without it. The commit and the
  report then name the defect and say why. The ten cases of "Rules of
  2026-10-13" that wait for Eddie stay as they are: R009, R017, R037,
  R051, R054, R066, R074, R083, R100 and R134. So does the question of
  R081, the finding T127. No step changes them or their tests. See
  "Feature freeze of 2026-10-14" in `docs/decisions.md`.
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

## Distribution

The target stands under "Binary distribution" in `docs/decisions.md`.

- Building Anti takes the pinned toolchain under `build/deps/`, clang included, on
  every host. Using Anti takes the pinned LLVM tools that travel inside every
  package. No C compiler ships, and no user needs one.
- Six packages, one per platform, each complete on its own. Each holds antic and
  anti with the pinned LLVM tools beside them in `bin/`, and the standard library.
  It also holds the runtime of
  all six targets with its bitcode of full LTO, and the native libraries and their
  headers. mimalloc, the C allocator of every program of musl, stands beside the
  runtime of each Linux level. The sysroots of every target, the licence texts and the record of their upstream sources complete it. Nothing is downloaded at install time or later. One package builds for all
  six targets with no network. Apple's frameworks are the one thing a package lacks.
- antic and anti use only what is in the package. The two exceptions are
  `antic --linker platform` and `anti bind --clang`, which a user asks for by name
  to work with the user's own C world. Building programs never needs either.
- The installers download the package, `SHA256SUMS` and `SHA256SUMS.sig`, check
  the signature and the digest, unpack, run `antic --version` and `anti --version`
  of the package and put the two on the path. They run no CMake and lay out no
  sysroot, and the package carries no script or pin for them.
- A change that makes a package depend on something outside it, or on a tool of the
  host, is refused like a warning.

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
./c            # Debug build in build/host
./c release    # Release build in build/release
./c test       # the Debug build, then its suite
./c coverage   # the coverage build, its suite and the coverage report
```

`./c` is a link to `tools/compile.sh`. It configures a tree on its first
build and takes `asan`, `ubsan`, `coverage`, `test` and `fresh` as well. The long form is
`cmake --preset host && cmake --build build/host -j8 && ctest --test-dir
build/host -j8`.

`CMakePresets.json` carries five configurations, each in `build/<preset>`:
`host`, the Debug build, `release`, the sanitizer configurations `asan`
and `ubsan`, and `coverage`. The suite runs in every tree. The three builds
a step of a driver proves are `host`, `asan` and `ubsan`.

`./c coverage` measures what the suite reaches. The preset `coverage` is a
Debug build in which the pinned clang compiles antic, anti and the C unit
tests with `-fprofile-instr-generate -fcoverage-mapping`. Every test of
that tree runs with `LLVM_PROFILE_FILE` set, so its suite leaves raw
profiles in `build/coverage/profiles/`. llvm-profdata merges them, and
llvm-cov writes `build/coverage/summary.txt` and the annotated sources in
`build/coverage/html/`. The summary holds one row per file of `src/antic/`,
`src/anti/` and `src/rt/`, with its lines, functions, regions and branches.
Of `src/rt/` it holds the files that antic, anti and the unit tests
compile. The runtime inside an Anti program is not instrumented.

- `./c coverage part 1/3` runs one part of the suite per call, for a
  session whose command ends after ten minutes. The first part removes the
  profiles of the run before, and the last writes the report from the
  profiles of every part.
- `./c coverage report` writes the report again from the profiles on disk.
- The measurement is no gate. A step runs it when its task says so.
- `tools/coverage.cmake` holds the flags and the directory of the profiles,
  and `tools/coverage-report.cmake` writes the report.
- The measurement was built and run on the development Mac alone.
  `docs/reports/2026-10-12-tests-coverage-baseline.md` holds the first one.

The pinned downloads lie in `build/deps/`, one copy for all three trees. A
release adds the LLVM tools of the other five hosts in
`build/deps/llvm-tools/<host>` for the packer. The
pinned clang in `build/deps/clang` compiles everything, and the configure step
installs it and the pinned LLVM tools in `build/deps/llvm/bin` with
`tools/get-clang.cmake` and `tools/get-llvm.cmake`. It then installs the
sysroots in `build/deps/sysroot` with `tools/get-sysroot.cmake`, and the
pinned raylib source in `build/deps/raylib` with `antic_get_source` of
`src/native/get-source.cmake`, so `./c` builds a fresh checkout.
`tools/clang-pin`, `tools/llvm-pin`, `tools/sysroot-pins` and
`tools/raylib-pin` hold the versions and the digests. clang and the LLVM tools
come from the releases of `anti-lang/llvm-tools`, and openssl checks their
signature. Windows configures with `-G Ninja`. `-DANTIC_SYSTEM_COMPILER=ON` is
for a reader's build and never for a release.

## Layout

| Path | Contents |
|---|---|
| `src/antic/` | The compiler |
| `src/anti/` | The `anti` tool |
| `src/rt/` | anti_rt sources, starting with `src/rt/start.c`, 0BSD |
| `src/std/` | The standard library in Anti, 0BSD |
| `src/native/` | The recipes that build the third-party C libraries of the runtime archive |
| `tests/` | Test programs, expected outputs, the runner |
| `tests/matrix/` | The matrix of constructs and contexts, by decision 28 of "Decisions of 2026-10-11" |
| `tools/` | Helper scripts, the pins, the installers and the docs-style checker |
| `tools/keys/` | `release.pem`, the public key that checks the LLVM tools, and the ignored `private/` |
| `docs/` | The specifications, the decisions and the reports |
| `LICENSES/` | Licence texts of bundled components |
| `build/` | Every build tree, the downloads in `deps/` and the driver state in `drive/`. Never committed |

## Repository layout

- The top level holds exactly `build/`, `docs/`, `LICENSES/`, `src/`, `tests/`
  and `tools/`, plus the root files `CLAUDE.md`, `README.md`, `CHANGELOG.md`,
  `LICENSE`, `CMakeLists.txt`, `CMakePresets.json` and `.gitignore`.
- Files that tools require in the root, the release link `r` and the build
  link `c`: `.gitattributes`, `.editorconfig`, a tracked
  `.claude/settings.json` if there is one, `r` and `c`.
- `src/` holds exactly `antic/`, `anti/`, `rt/`, `std/` and `native/`.
- `docs/` holds exactly the directories `audit/`, `notes/`, `reports/` and
  `site/`. `docs/audit/` holds the reports of the code audit and their
  machine data.
- Every new file goes into the directory its kind already has: compiler code
  in `src/antic/`, tool code in `src/anti/`, runtime C in `src/rt/`, Anti
  library code in `src/std/`, native library recipes in `src/native/`, a test
  in the `tests/` subdirectory of its kind, a report in `docs/reports/`, a
  note in `docs/notes/`, an audit report or its data in `docs/audit/`.
- No new directory at the top level, or directly under `src/`, `tests/` or
  `docs/`, without Eddie's decision. A session that needs one stops and
  reports BLOCKED with the reason.
- Scratch work, temporary files and generated output go under `build/` only,
  never in the source tree. Nothing under `build/` is committed.

The test `repo_layout` reads `git ls-files` and holds these lists and the
directories of `tests/` and `docs/`. Adding to any list is Eddie's decision.

## Tests

A new construct, diagnostic or option comes with its test, in the commit that
builds it. A construct is tested in every context it can stand in. A
diagnostic is tested by a program that gets it, and an option by a run that
passes it.

Each test fails for one reason. A program prints what it checks, and a
refusal holds the one message it expects. A test that needs a long program
tests two things and becomes two tests.

Where a test goes:

- `tests/matrix/`: a construct in a context, as a program or as a refusal.
  The next list holds its rules.
- `tests/programs/`: a program whose subject is one feature. Each runs in
  release mode as `program_<name>`, and a list of `tests/CMakeLists.txt`
  names the ones that run in another mode as well.
- `tests/std/`: a program whose subject is a module of the standard library,
  `std_<name>`.
- `tests/errors/`: a refusal that needs options of its own, `error_<name>`.
  Its case stands in the list of `tests/CMakeLists.txt`, and the test
  `errors_registered` refuses a program without one.
- `tests/traps/`: a check of dev mode or of `--memory-checks` that stops a
  program, `check_<name>`.
- `tests/dump/`: what antic prints for a stage, `dump_<stage>_<subject>`.
- `tests/modules/`: a program of more than one module, in release mode and
  in dev mode. A refusal at the boundary of a module is a program
  `tests/errors/modules_<name>.anti`, whose case in `tests/CMakeLists.txt`
  names the tree of its library files.
- `tests/abi/` and `tests/clib/`: Anti against C, as the caller and as the
  library. `program_abi_ctypes` and `clib_ctypes` hold every C type.
- `tests/trace/`, `tests/conf/`, `tests/inject/` and `tests/plugin/`: stack
  traces, the runtime configuration, injection and plugins. Their output
  holds addresses or paths, so pattern files hold it.
- `tests/anti-build/`, `tests/anti-test/`, `tests/anti-symbols/`,
  `tests/check/`, `tests/fmt/`, `tests/doc/` and `tests/bind/`: the commands
  of `anti`.
- `messages.cmake` in each of those seven directories: one case per message
  of the command, `anti_<command>_message_<case>`. A case writes its files,
  runs anti and holds the whole standard error. `tests/anti_messages.cmake`
  holds the cases of `anti new`, `anti license` and `anti sdk`, which have
  no directory. `tests/antic_messages.cmake` holds the messages of the
  driver of antic that stand at no position of a source, as
  `antic_message_<case>`, through the same runner. The name of a case registers its test, and the header of
  `tests/run_anti_messages.cmake` states the form.
- `tests/unit/`: the C unit tests, `unit_<file>` for `test_<file>.c`.
  `unit_<file>_messages` compares the standard error of a group with
  `tests/unit/<file>.err`, for `zip` and `syms`.
- `tests/opt/`, `tests/emit-identity/`, `tests/link-identity/`, `tests/raw/`
  and `tests/framework/`: the IR around the optimizer, the pinned bytes of
  the output, the raw-bytes rule and a framework of Apple.
- `tests/run_antic_options.cmake` and `tests/run_anti_options.cmake`: an
  option beside the run without it, `antic_option_<case>` and
  `anti_option_<case>`. A case runs the tool twice and names what differs,
  so it fails for an option the tool takes and ignores. The header of
  each runner lists its cases.
- `tests/run_usage.cmake`: the usage, and a command line a tool refuses
  with it, `antic_usage_<case>` and `anti_usage_<case>`.
- `tests/bench/`: the benchmarks, outside the suite.

The matrix, by decision 28 of "Decisions of 2026-10-11":

- `<name>.anti` with `<name>.expected` is a program. It runs as
  `matrix_<name>_dev`, `matrix_<name>_release` and
  `matrix_<name>_memory_checks`. Each compares the whole output with the one
  expected file, so a program whose output differs between the three fails.
- `<name>.anti` with `<name>.err` is a refusal, `matrix_<name>_refused`.
  antic runs the whole build in `tests/matrix/`, exits with 1 and prints the
  bytes of the `.err` file.
- The file name registers the tests, and no list of `tests/CMakeLists.txt`
  names a matrix file. A name holds small letters, digits and `_`.
- The expected file starts with the line `exit N`, as in `tests/programs/`,
  and `<name>.args` holds the arguments of a program.
- The dev build links the dev object of every module of the standard library
  that the program imports. The configure step reads them from the `import`
  lines at the start of a line.
- The runner stops antic and the program after 60 seconds each.
- The test `matrix_files` refuses a file that registers no test.
- A program may hold one construct in every context of its row, one
  function and one line of output per context. Every program is an
  executable the suite builds three times, and `flow_<construct>` of the
  area flow shows the form. A program may hold more than one construct
  as well, as `values_aggregates` does, where a line of output names its
  construct and its place. antic 0.2.0 takes 21 s for 150 such functions
  in one program, so a program stays below about 80. A cell that shows a
  defect gets a program of its own, so the list of the known failures
  names that cell alone.
- No build of the matrix keeps a `tests` or a `fixtures` block, and none
  reads a library file. `anti_test_flow` runs a module of
  `tests/anti-test/` through `tests/run_anti_test_module.cmake` for the
  first, and `flow_modules` holds a library of `tests/modules/` for the
  second.

The known failures, by decision 30 of "Decisions of 2026-10-11":

- A test that shows a defect stays in the suite, written as the rule says
  the program must behave. No test is weakened to pass.
- The defect gets the next free id and one row in
  `docs/audit/tests-findings.md`, whose header describes the row.
- The test gets one line in `tests/known-failures.txt`: its name, the id and
  one sentence. A second test of the same defect names the same id.
- A defect that shows on some hosts alone ends its line with
  `host=linux`, `host=windows` or `host=macos,linux`. The suite of every
  other host expects the test to pass.
- `tests/CMakeLists.txt` sets `WILL_FAIL` on every listed test. A listed
  test that passes fails the suite, so a fix removes the line. It also
  writes `fixed` into the row.
- A listed test passes for any failure. Read its output before listing it,
  with `ctest --test-dir build/host -V -R '^<test>$'`. It must fail for the
  reason its finding names.
- ctest counts a test that its own time limit stops, or whose command
  crashes, as failed whatever the list says. A listed test therefore runs
  through a `cmake -P` runner, which reports both as an ordinary failure.
- The list takes effect when the tree is configured, which a build does
  after a change of it.
- The test `known_failures` compares the list with the table and with the
  tree. `tests/known_failures.cmake` reads the list for both.

The two guards and the lists of the matrix:

- `diagnostics_covered` reads every message of `src/antic/` and of
  `src/anti/` and fails for one that no expected output under `tests/`
  holds. `options_covered` reads the options of antic and the commands and
  options of anti and fails for one that no test passes.
- `tests/diagnostics-open.txt` and `tests/options-open.txt` list what had no
  test on 2026-10-12. A line leaves its list in the commit that adds the
  test. A new message or option comes with its test and never with a line.
  `tests/options-open.txt` holds no line since 2026-10-12.
- A message that stays in `tests/diagnostics-open.txt` for good has the
  lines `# Stays: ...` before it, which give the reason. One reason is
  that no program and no input a test can write reaches it. The other is
  that antic gives it as the second message of one mistake alone, and a
  finding holds that. Every line of the list has such a reason since
  2026-10-12.
- Each guard fails for a line whose message or option a test covers, and for
  a line that names nothing of the sources.
- The diagnostics guard reads the functions its runner names. A new function
  that hands a format on to `diagnostics_add` goes into `TEXTS_ANTIC` of
  `tests/run_diagnostics_covered.cmake`.
- `docs/notes/test-matrix.md` holds the constructs, the contexts, the kinds
  of value and the matrix, with each cell open or held by a source.

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
15. Done. `f"..."` and `rf"..."` with their format specifications, as
    calls of `anti.text.Builder`. `docs/reports/2026-09-21-interpolation.md`
    reports it, and `docs/notes/interpolation.md` holds its choices.
16. Done. The native libraries in `src/native/`. PCRE2, SQLite, Mbed TLS,
    miniaudio and raylib build for all six targets, as "Libraries and runtime"
    in `docs/decisions.md` records, and their headers stand in
    `include/<library>/` of the runtime archive. The build of Mbed TLS carries
    no `lib/cacert.pem`. `anti.regex` binds PCRE2, and no module binds the
    other four. `docs/reports/2026-10-09-dist-headers.md` reports the headers.
17. Done. The LLVM text writes every atomic operation as an LLVM atomic
    instruction, and the runtime keeps its functions for C.
    `docs/reports/2026-10-07-inline-atomics.md` reports it.
18. Done. The one manifest of a release, and the installers that read its
    signature. `docs/reports/2026-09-20-one-manifest.md` reports both.
19. Done. Every Windows link passes `/DEBUG` with `/PDBALTPATH:%_PDB%`, the
    packer leaves the PDB of each Windows program in `build/dist/symbols/`,
    and step 4 puts it in the symbols archive beside the map of the sections.
    `tools/check-pdb.cmake` reads the CodeView record of an executable
    against the GUID of its PDB, in step 4 and in the test `pdb_guid`.
20. Done. The error a `catch e` binds is deleted on every exit of its
    handler, and `programs/catch_exits.anti` takes each exit under a leak
    check. `fail e` hands it on as `return e` does.
21. Done. `is_failing` keys on the `may fail` marking alone, and a
    function that returns `*Error` or `?*Error` without it is ordinary.
    `programs/error_values.anti` and `errors/failing.anti` check both
    sides, and `makes_error` is gone.
22. Done. `for i in 0..10 by -3` gives `9 6 3 0`, and lowering follows
    the rule already. `programs/by_reverse.anti` pins it with constant
    bounds and bounds read at run time.
23. Done. A `may fail` function that returns a tuple takes one out
    pointer. `ir_tuple_out` pins the signature in the IR, and
    `clib_tuples` pins it in the header and calls it from C.
24. Done. A field that `serialize` cannot write keeps its default, and a
    pointer, a function pointer or an empty `own` slice written as `null`
    comes back as that value. The entry under "Object model" says so, and
    `std/serialize_null.anti` takes every kind through both. A bitfield
    of a class body goes through its unit, which
    `programs/class_bitfields.anti` checks.
    `docs/reports/2026-09-22-first-sessions-20-to-24.md` reports 20 to 24.
26. Done. Plugins on Linux and Windows, and the Linux link mode against
    glibc. A module names a library of the glibc sysroot with
    `link linux "X11";`. A Linux program that reaches one, or that can
    load a plugin, links dynamically against glibc 2.35. A Windows host
    links with an import library its plugins resolve against.
    `docs/reports/2026-09-24-glibc-and-plugins.md` reports it.

The gap to "Binary distribution" in `docs/decisions.md`, one item per difference, in
the order they are best built. `docs/reports/2026-09-27-distribution-target.md` holds
the same list.

27. Done. The LLVM tools of the host travel in `bin/` of every package beside
    antic and anti, with `llvm-version`, and the installers download none.
    The packer takes the tools of another host from
    `build/deps/llvm-tools/<host>`, which `tools/get-llvm.cmake -DHOST=<host>`
    lays out and step 1 of `./r` fills.
    `docs/reports/2026-10-09-dist-tools.md` reports it.
28. Done. antic and anti take their tools, their libraries and their
    sysroots from the package alone. A tool that `bin/` of the archive
    lacks is refused by its path, a Windows link without its sysroot is
    refused, every lld-link runs with `/lldignoreenv` and the sysroot as
    its Visual Studio and Windows SDK, and no message names
    `tools/get-sysroot.cmake`. `own_tools` and `package_keys` check
    it. `docs/reports/2026-10-09-dist-own-tools.md` reports it.
29. Done. The two glibc sysroots, with their X11 and OpenGL packages and the glibc
    runtime of both Linux targets, travel in every package. A program of `link
    linux`, raylib or a plugin host then links for Linux from any host.
    `docs/reports/2026-10-09-dist-glibc.md` reports it.
30. Done. `licenses/sources.txt` of the runtime archive and of every package
    records the name, the version and the URL of the exact upstream source
    package of every pinned component, written by
    `tools/upstream-sources.cmake` from the pins, and `anti license --from`
    prints the line of a component after its text.
    `docs/reports/2026-10-09-dist-sources.md` reports it.
31. Done. The headers of the native libraries stand in `include/<library>/` of
    the runtime archive and of every package, and `anti bind --clang` of a
    package binds `include/raylib/raylib.h` as the tree binds the pinned
    source. `docs/reports/2026-10-09-dist-headers.md` reports it.
32. Done. The Windows sysroot of mingw-w64: the headers and the import
    libraries of both Windows targets, written from the `.def` files of the
    mingw-w64 project, travel in the runtime archive and in every package.
    A program links `ucrtbase.dll`, `ntdll.dll`, `kernel32.dll` and the
    builtins, and nothing of Microsoft. The runtime defines the static part
    of the C runtime, the entry point among it, and the C of every Windows
    target compiles for the gnu triple against the mingw-w64 headers.
    `docs/reports/2026-10-10-dist-mingw.md` reports it.
33. Done. The tool that fetched the CRT and SDK of Microsoft is gone. Its
    pins left `tools/sysroot-pins`, and `ANTI_MICROSOFT` left
    `tools/release.sh` and the tests of the installers. Step 10 of `./r`
    refuses an install that lacks the sysroot of a target. The test
    `no_sdk_fetcher` refuses the name of the tool outside
    `docs/decisions.md` and `docs/reports/`, so this item does not spell
    it, and neither does the path of its report: the report of 2026-10-10
    in `docs/reports/`, named after the step that follows `mingw`.
34. Done. The installers download the package alone and use `| bash`. Each
    checks the signature and the digest, unpacks, runs `antic --version` and
    `anti --version` of the package as the check of the install and puts the
    two on the path. Neither runs CMake, lays out a sysroot or reads a file
    of the package, which carries no `tools/`. The downloads page shows the
    manual install with the same checks as commands.
    `docs/reports/2026-10-10-dist-installers.md` reports it.
35. Done. `--bundle-runtime` on Mach-O writes an archive of the library
    object, the runtime members and a marker member, and Apple's `ld -r`
    left the code. Two bundled libraries in one program are the duplicate
    symbol `anti_rt_bundle`. `docs/reports/2026-10-10-dist-bundle-macho.md`
    reports it.
36. Done. Step 5 of `./r` installs the package on each VM from the staging
    area and turns the network of the VM off. `tools/check-offline.cmake`
    then builds and runs a raylib program and a plugin host for the target of
    the VM and links a hello program for each of the other five. The network
    comes back for the suite. `docs/vm-setup.md` holds the commands that turn
    the network of each VM off and on.
    `docs/reports/2026-10-10-dist-offline.md` reports it.
37. Done. The other four forms of `anti license` of `docs/distribution.md`:
    the plain form over `licenses/` of the package, `--project`, `--project
    --notice` and `--from-archive`. `anti build` writes `NOTICE.txt` from
    `anti.lock` and the imported bundled modules, as `--project --notice`
    writes it. `docs/reports/2026-10-10-dist-license-forms.md` reports it.
38. Done. The release check of 0.2.0. The package of each host, packed
    from the tree, installed on its machine from the staging area, and
    the check of step 10 of `./r` run by hand with the network off on the
    Mac and both VMs. `tools/version` holds 0.2.0 and `CHANGELOG.md` its
    entry. `docs/reports/2026-10-10-dist-release-check.md` reports it.

Then `docs/anti-language-additions.md` in the order its "Timing" section gives:
nullable pointers, the dev-mode checks, the lines of `-g`, tests and fixtures
and the CPU levels are built. The variables of `-g` come before the first public
release. After it, the wrapping and saturating operators with `Flags`, then
sum types, then locking and channels, all three built. Then injection, hooks
and tracing, plugins and runtime configuration, which belong together. All
four are built. Then generics and closures. Closures are built, `snapshot fn`
included. Round five, generics and collections, stands in the same document.
Its first nine parts, the syntax of generics, the constraints, the
compiled copies, the generics of library files and of C, what can be
generic, the other features with generics, the optional values, ownership
at a call and lending, are built. So are the language part of walking a
collection, hashing and order, direct imports, the parts every collection
shares in `anti.collection`, `Shared<T>` of `anti.mem`, the collections of
`anti.collection`, the thread-safe collections and `--memory-checks`. See
`docs/notes/collections.md`.

`docs/work-order-completion.md` is the work order those items come from, with
its book steps removed. `docs/reports/2026-09-20-object-model-completion.md`
reports what it finished.

## State

- The compiler lexes, parses and type-checks Anti across modules. Its IR holds
  no sizes: the back end lays out types per target (`src/antic/layout.c`) and folds
  symbolic values. The back end translates the IR into LLVM IR text, and opt
  and llc of the pinned release write the object for all six targets. lld
  links every target against the sysroots of the runtime archive. A release
  build links the program and the runtime as bitcode through full LTO, and
  `--lto none` links objects. An atomic operation is an LLVM atomic
  instruction. Both work orders of LLVM are complete. See
  `docs/notes/llvm.md`.
- The object model is implemented: classes, interfaces as inline sub-objects
  with thunks, four visibility levels, `construct` and `destruct`, operators,
  singletons, the error forms and reflection over descriptors. A class names
  its base in its header, `class Circle inherits Shape { }`.
- `worker fn`, `parallel` and `dispatch` compile and run on all six targets.
- A pass over the whole program's IR runs after lowering. In dev mode it runs
  for the module that links. It holds release devirtualisation, the class
  registry, the singleton check and the used-slot bitmaps.
- `src/std/` holds `anti.lang`, `anti.io`, `anti.text`, `anti.license`,
  `anti.error`, `anti.time`, `anti.os`, `anti.fs`, `anti.reflect`,
  `anti.random`, `anti.collection`, `anti.toml`, `anti.config`, `anti.args`,
  `anti.json`, `anti.log`, `anti.debug`, `anti.mem`, `anti.runtime`,
  `anti.simd`, `anti.trace`, `anti.plugin` and `anti.regex`, and the
  collection modules under `anti.collection`.
  `anti.lang` is the root and imports nothing. It holds `Error`,
  `NoneDereference`, `SourceLocation` and `StackTrace`, and `anti.error`
  holds `SystemError`, `on_fatal` and `check`. The compiler declares
  `Object`, `Job`, `Flags` and `FieldDescriptor` in `anti.lang` itself, and
  the runtime defines the root's functions and descriptor as
  `anti_lang_Object_*`.
- Hooks and tracing are built. `anti.lang.Object` declares nine hooks with
  empty bodies, which take the nine table entries after its seven functions:
  `created`, `destroyed`, `copied`, `dispatched` and `joined` in every build,
  `enter`, `leave` and `failed` under tracing and `changed` under
  `--trace writes`. `anti.lang.TraceHandler` declares the same nine with the
  object after `self`, and `anti.lang.Trace.install(h)` stores one handler. A
  site calls the handler and then dispatches the object's own hook, reversed
  on `leave`. `trace` is a contextual word before `class` and before `fn` in a
  class body, `--trace` and `--no-trace` decide instead of the mode,
  `--trace <pattern>` reaches a class that did not ask, and `--no-hooks` drops
  every site and is refused for a plugin and a program that loads one.
  `anti.trace` ships `LeakTracker`, `Profiler`, `CallLogger`,
  `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`, and
  `trace.start` installs the one the runtime key `trace` names. See "Hooks
  and tracing" in `docs/decisions.md`, `docs/notes/hooks.md` and
  `docs/notes/trace-handlers.md`.
- 4947 ctest tests pass on the development Mac, and two of them, the
  `mimalloc_environment` tests, skip there, since they need a Linux host.
  374 of them are the expected failures of `tests/known-failures.txt`.
  The ASan and the UBSan builds run 4945 each with the same skips, without
  the `no_paths` test, which needs a build that no sanitizer wrote paths
  into, and without `antic_message_license_text_with_a_nul_byte`, which
  shows a leak of antic under AddressSanitizer. `overview_examples` compiles every
  `anti` block of `docs/anti-syntax-overview.md` through the front end.
- The rules of flow and of errors of 2026-10-13 are built, R001 to R025 of
  "Rules of 2026-10-13" in `docs/decisions.md`, apart from R009 and R017,
  which wait for Eddie. A `switch` whose every arm and `else` end the path
  ends the path, and so does a `while true` without a `break` of its own.
  The handler of a `try` block takes `yield;`, and a `try` in the block
  forwards to it. An `undo` that cannot run is the warning
  `undo-never-runs` or refused. A `construct` that fails tears down what
  was built before it. `docs/reports/2026-10-13-rules-flow-errors.md`
  reports the step.
- The rules of values and of constants are built, R026 to R047 apart from
  R037, which waits for Eddie. A constant that leaves the range of its
  type is refused: a narrowing `as`, a conversion to `char`, `+`, `-`, `*`
  and unary `-`, a shift by a count below zero and an index below zero on
  a fixed array. A dev build stops the least value divided by -1, unary
  `-` of it and `<<%` by a count below zero. `.len` of a constant `str` is
  a constant, `operator fn eq` on a slice is refused, `=` copies no
  concurrent class, and no constant holds a Mutex. The default `[v; n]`
  of a field of a class takes a length from `size_of` or from a constant
  parameter of the class: the constant holds `v` once, and a loop writes
  the elements. A copy of a generic class takes a default that names a
  constant parameter with its argument in place. R034 keeps the operator
  on a union. `docs/reports/2026-10-14-rules-values.md` and
  `docs/reports/2026-10-14-rules-values-2.md` report the two sessions.
- The rules of types and of generics are built, R048 to R069 apart from
  R051, R054 and R066, which wait for Eddie. A constant that does not fit
  the width of a bitfield is refused, a dev build stops any other value
  that does not fit, and `+|=` clamps at the width. `alloc(T, n)` of a
  type that `align(N)` raises above 16 takes aligned memory from
  `anti_rt_alloc_aligned`, on macOS and Linux. `as` to an enum takes an
  integer alone, `as` to the type of the value does nothing, and `as`
  with a `?` apart from it is refused. A `char` lane of a simd struct is
  refused. `--front-end` compiles the copies of the generics, an
  `operator fn` whose type parameter no operand names is refused, and a
  generic call names the parameter whose shape an argument misses.
  `docs/reports/2026-10-14-rules-types.md` reports the step.
- The rules of ownership are built, R070 to R085 apart from R074 and R083,
  which wait for Eddie, and R081, which the expected failure T127 holds.
  `delete(c)` tears down every value the channel still holds, the oldest
  first, through `anti_rt_chan_held` of the runtime. An array literal
  moves an owning local, as a tuple literal does. A use after a move into
  a place without a name gives the line of the move. An `own` field of
  type `?fn(...)` takes `none`, as its default and with `=`, which frees
  the snapshot it held. `docs/reports/2026-10-14-rules-ownership.md`
  reports the step and asks about R081.
- The rules of threads are built, R086 to R099. A worker of another module
  stands after the arrow as `m.f`, a `dispatch` and a `parallel` fill in
  the defaults of the worker, and a count of chunks below zero is refused
  as a constant and stops a dev build. A snapshot names no atomic local. A
  pointer to a guarded field is used in the `sync` block that took it
  alone. A `sync` on a Mutex its thread holds is refused in an anonymous
  function inside the block too, and stops a dev build otherwise. A public
  function of a thread-safe class stores no pointer into its fields
  outside the object. `docs/reports/2026-10-14-rules-threads.md` reports
  the step.
- The rules of classes are built, R101 to R120. R100 waits for Eddie.
  An `operator fn eq` gives a `bool` and takes no class
  with a Mutex by value. The `construct` of a singleton takes no
  parameters, no class inherits a singleton, `dup` and `*` of one are
  refused, and a singleton declares no `get`. A base of another module is
  torn down when a `construct` below it fails. A class that replaces
  `destroyed` has a teardown, so the hook runs at the end of every value.
  An abstract function takes `self`.
  `docs/reports/2026-10-14-rules-classes.md` reports the step.
- The rules of modules and of C are built, R121 to R135. R134 waits for
  Eddie. Two modules that import each other are refused with the cycle
  named. An `extern fn` takes C types alone, a function of the runtime
  apart, and one C function has one signature in a program. `c_long` and
  `c_ulong` are variadic arguments. `export` takes no `operator fn` and no
  name of the three C headers a header includes. `--lib static` of a
  module that exports nothing gives `nothing-exported`, and three options
  beside the wrong kind of library are refused. The load of a plugin that
  lacks a module names it.
  `docs/reports/2026-10-14-rules-modules.md` reports the step.
- The rules of the messages and the options of antic are built, R136 to
  R154 and R190 to R197. A message names `uint`, `i64`, `u8` and `f64`
  in a file that writes them. A call of an enum value, `destroy` of a
  value and a private function of a class of a library get messages of
  their own. A boundary or a lookahead after a repeat passes the check
  `exponential-pattern`. The usage lists every option, `--dev
  --no-asserts` drops the text of an assertion, and the module path comes
  from normalised paths. `--soname` beside `--lib static` is refused, a
  library of macOS has the install name `@rpath/<file name>`, and the link
  of a Windows library is reproducible.
  `docs/reports/2026-10-14-rules-messages.md` reports the step.
- The rules of the commands of anti are built, R155 to R189. `anti --help`
  with a second word prints the usage, and an option without its value
  gets one line. `anti run` takes the options of its usage alone and
  refuses a library project, and `--target all --cpu` checks every target
  first. The manifest refuses a dependency that is no package name and a
  path with a control byte, a cycle of path dependencies is refused at any
  depth, and a lock file that is no TOML is an error. `anti bind` gives one
  warning per type of a description and refuses a struct that lost a
  field. `anti doc` writes a type as its item spells it, which the library
  file carries, and warns of a fence that nothing closes. `anti symbols
  resolve` answers a frame through the map alone.
  `docs/reports/2026-10-14-rules-tools.md` reports the step.
- The wrapping operators `+% -% *% <<%`, the saturating operators `+| -| *|`,
  `mul_high` and the flags form `let (result, flags) = e;` are built, and so are
  the compound assignments `+%=` and `+|=` of each. A
  carry in, `a + b + f.carry`, is `adc` on x86_64 and `adcs` on ARM64, and
  the function computes the flags it reads alone. See "Wrapping and
  saturating operators and `Flags`" in `docs/decisions.md` and
  `docs/notes/flags.md`.
- Sum types are built. `variant Shape { Circle { r: f32 }, Empty }` is a
  struct of a tag and a union of its cases with C layout.
  `Shape.Circle { r: 2.0 }` and `Shape.Empty` are literals, and `switch`
  binds a copy of a case's fields with `Circle c =>` and covers every case
  without `else`.
  `if let`, `v is Shape.Circle` and `v.tag` read the tag, a variant crosses
  a module, and the header writes the enum of its tags. See "Sum types" in
  `docs/decisions.md` and `docs/notes/variants.md`.
- The default `==` is built. A struct, a class value, a tuple, a variant,
  a `?T` and an array compare part by part, a field by its kind, and agree
  with the default hash. `<` has no default, so `Ordered` needs `operator
  fn lt`. Module-level `operator fn` items share a name across the types
  of their first parameters, declared as `eq:Point`, and a bare call picks
  by its first argument. `union` names a function after `fn` and `.`. See
  "Generics and collections" in `docs/decisions.md` and `docs/notes/hashing.md`.
- Variants, `?T`, tuples and arrays of more than one level have runtime
  descriptors. `equals`, `hash`, `serialize`, `deserialize` and `reflect`
  handle a field of each, an array field serializes as nested JSON arrays,
  a channel and an `own fn` compare by identity and a Regex by its text.
  The handle of a compiled pattern is `struct anti_pattern` of
  `src/rt/regex.h`.
- Locking and channels are built. `Mutex.new()` makes an `anti.lang.Mutex`,
  `sync m { }` holds it for a block and unlocks it on every exit, and a
  nested `sync` on the same place in one function is refused. `chan int(16)`
  makes a channel, `send` and `recv` block, `recv` gives `?*T` or `none`
  once the channel is closed and empty, and `select` takes from whichever
  channel is ready. `src/rt/sync.c` holds the channels and `src/rt/lock.c`
  the Mutex. The warning on a field written inside `sync` and read outside it
  is not built. See "Locking and channels" in `docs/decisions.md` and
  `docs/notes/locking.md`.
- Concurrent classes are built. A Mutex is one word of the program's memory,
  a futex word, an `os_unfair_lock` or an `SRWLOCK`, and cannot be copied or
  assigned. `synchronized class` runs every function with `self` that is not
  private under a hidden lock that its thread takes again without waiting,
  and `sync obj { }` holds it. `concurrent class` has every field guarded by
  a Mutex, atomic or fixed, and the safety check `unguarded-field` refuses the
  rest unless `unchecked` covers it. A public function of either gives out no
  pointer into the fields, `let n: atomic int = 0;` is an atomic local, and a
  worker takes a pointer to a thread-safe object. A dev build reports two
  locks taken in opposite orders. `sync a, b { }` takes two hidden locks in
  the order of their addresses, and the default `==` and hash of a
  synchronized class run under them. A concurrent class has no default `==`
  or hash. `equals` of a class whose module gives it `operator fn eq` calls
  it, and an operator at module level reaches the private members of the
  classes of its module. See "Concurrent classes" in
  `docs/decisions.md` and `docs/notes/concurrent-classes.md`.
- `anti.mem.Allocator` is built, with `alloc(size, align)` and `free(p)`, the
  default `LibcAllocator` over `src/rt/mem.c` and `ArenaAllocator` over blocks of
  another allocator. `alloc` and `free` name a function of a class and follow
  `.`. See "`anti.mem`" in `docs/decisions.md`.
  `Object.deserialize(input, from)` takes the object, its strings and its
  owned objects from the `Allocator` `from`. `destroy(p, from)` and
  `delete(p, from)` run the destruct chain and give the owned memory back
  to `from`, and `delete` the object as well. `text.Builder.take_in(from)`
  gives a text in memory of one, and `f"..."` takes none. The runtime
  calls `alloc` and `free` as table entries 8 and 9.
- Simd structs are built. `simd struct Vec4 { x: f32, y: f32, z: f32, w: f32 }`
  declares a vector whose fields are its lanes. The element-wise operators,
  the masks of the comparisons, `simd.select`, `simd.any` and `simd.all` of
  `anti.simd` and the built-ins `splat`, `load`, `store`, `shuffle`, `sum`,
  `min`, `max` and `dot` compile to the instructions of the native width:
  an `f32x8` is one `vaddps` at x86-64-v3 and two adds everywhere else. A
  simd struct of 16 bytes is the vector type of C in the header and passes
  in a vector register, and one above the cap of 256 bytes is an array and a
  loop. See "Simd structs" in `docs/decisions.md` and `docs/notes/simd.md`.
- The runtime configuration is built. `--anti.conf=<path>`, `ANTI_CONF` and
  `rt.configure(path)` of `anti.runtime` name the TOML file, which holds
  `include`, `[runtime]` and `[injections]`. The includes run first and in
  order, so the including file wins per key. Every key of `[runtime]` is an
  option of the same name, `--anti.inspect` prints the effective value of
  each with the layer it came from, and `--anti.help` lists the options.
  Precedence per key is the command line, the file, the build. The pool
  reads `threads` and `anti.log` reads `logger`, so `ANTI_THREADS` and
  `ANTI_LOGGER` are gone, and `backtrace = true` turns the frames of an
  error on in a release build. `rt.get(key)` gives the effective value of a
  key. A line of `[injections]` that names no injectable interface of the
  program is a startup error, and a file of `rt.configure` that holds the
  table is refused, since injections are fixed at start. See "Runtime
  configuration" in `docs/decisions.md` and `docs/notes/runtime-conf.md`.
- Injection is built. `inject log: *Logger` and `inject final alloc: *Allocator`
  mark a field of a class that the provider of its interface fills before
  `construct` runs. `anti test` reads the `[inject]` and `[inject.test]` tables
  of `anti.toml` and passes each provider to antic as `--inject
  Interface=Provider`. One slot per interface holds the provider, and every
  site calls through it. The link refuses an interface with no provider, a
  provider that is no function of the program or is no such interface, and a
  cycle through the providers. `anti_rt_injectable` names the interfaces of a
  program, so `--anti.inject` reports what it may replace and refuses an
  `inject final` field. The run-time replacement waits for plugins. See
  "Injection" in `docs/decisions.md` and `docs/notes/injection.md`.
- Plugins are built. `provides Interface as Class;` at module level says
  what a library offers, and `antic --lib shared --no-runtime` writes one:
  the object of its own module alone, linked without the runtime and bound
  against the host that loads it. `plugin.load(path)` of `anti.plugin` gives
  a `Library`, `lib.instance(Interface)` builds the class it provides,
  `lib.supports(Interface, "f")` reads its functions, and `lib.unload()`
  refuses while an object of the library is alive. antic writes
  `anti-plugins.toml` beside a library, and `"plugin:path"` and `"discover"`
  of the manifest take a provider from one. `[injections]` and
  `--anti.inject` replace a provider at start, and `--closed` builds a
  program a plugin cannot bind against. A program loads a library on all
  three systems. On Linux a host links against glibc, and a Windows
  plugin links against the import library `<program>.lib` of its host.
  See "Plugins" in `docs/decisions.md` and `docs/notes/plugins.md`.
- Interface versioning is built. Every class descriptor carries the version
  of the package that declared the class, `--package-version` and `0.0.0`
  without one. An abstract class carries the chain of its structural hashes,
  one per prefix of its table, and the floor a `compatible 1.1;` line names.
  A plugin records the chain, the fields, the size and the version of each
  interface it was built against. The load compares the two chains at the
  length of the shorter, refuses a field added between the versions and
  refuses a slot the program's calls reach and the library lacks.
  `anti_rt_slots` keeps the slots of the calls and a flag for
  `reflect.call`, and a slot only reflection may reach is filled with a
  stub. See "Versions" in `docs/decisions.md` and `docs/notes/versions.md`.
- The six standard interfaces are built, each an abstract class with a
  default implementation and a default provider: `anti.log.Logger` with
  `SinkLogger`, `anti.time.Clock` with `SystemClock`, `anti.random.Source`
  with `SharedRandom`, `anti.fs.FileSystem` with `SystemFileSystem`,
  `anti.mem.Allocator` with `LibcAllocator` and `anti.config.Config` with
  `FileConfig`. The default provider is a static `default` of the interface,
  which the manifest overrides, so an `inject` field of one needs no entry.
  See "Standard interfaces" in `docs/decisions.md`.
- `antic -g` writes the line of every statement and an entry per function,
  and the link then keeps the debug sections. lldb and gdb stop by file and
  line and print a backtrace of Anti function names. A copy of a generic is
  `app.List<int>.push` there, although its symbol escapes the brackets. Variables are the next step. See `docs/notes/llvm.md`.
- Error origins and stack traces are built. The first `fail` of an error
  writes `at` and, when backtraces are on, `frames`. `e.text()` names the
  position, the causes and the trace. `StackTrace` has `capture`, `frames`,
  `text` and `symbolize`. Backtraces are on in dev mode, and
  `--anti.backtrace` turns them on in release. See `docs/notes/traces.md`.
  `here` and default parameter values are built.
- `may fail` and `fail` are built. A function declared `may fail` returns
  `?*Error` and writes its result through an out pointer. `fail` leaves on the
  error channel, with an error or with a text, `try` forwards inside another
  `may fail` function, and `undo` runs on the fail path. The header writes the
  ABI and the `.antl` records the flag. Every module of the standard library
  follows the form: `text.parse_int`, the three user directories of anti.os,
  every function of anti.fs, `toml.Document.read`, `log.FileSink.new`,
  `args.Parser.parse`, `reflect.set`, `reflect.call` and `json.unquote` may
  fail. The tests `std_may_fail_only` and `tests_may_fail_only` refuse a
  function of `src/std/` or `tests/` written by hand as `-> ?*Error`, apart
  from the tests of that form. A function fails by its `may fail`
  marking alone, and one that returns an error without it is ordinary.
  `fn(A) -> R may fail` is a type, and a call through it takes a handler.
  A `construct` that can fail is written
  `may fail`, a derived one calls `self.super.construct(args)` as its
  first statement, and C makes an object with `anti_<Class>_construct`.
- Anonymous functions, closures and `snapshot fn` are built.
  `fn(m) { }` takes its types from the parameter it is passed to, and a
  closure captures the locals it names by reference through a context in the
  frame. A parameter of function type is two words, the code and a context,
  unless it is `keep`, and a plain field, a global, a result and a plain
  `keep` parameter hold one C function pointer, so a closure never reaches
  them. `concurrent`
  marks a parameter that goes on to `parallel` or `dispatch`, and a closure
  there changes no captured variable whose type is not thread-safe. See
  "Anonymous functions and closures" in `docs/decisions.md` and
  `docs/notes/closures.md`. `snapshot fn` copies what it captures.
  At an `own` field or a `keep own` parameter its snapshot lives on the
  heap as `own fn`, two words that the owner frees and `dup` copies.
- Nested types are built. A class body declares a `struct`, an `enum` or a
  `class`, named as written in the class and as `PeopleList.Node` in the
  symbols, and `PeopleList_Node` in the C header. A public signature of the
  class that names one is refused. See "Nested types" in `docs/decisions.md`
  and `docs/notes/nested-types.md`.
- Tuples are built. `(int, str)` is an anonymous struct with C layout, `(a, b)`
  builds one, `t.0` reads an element, and `let (a, b) = e;` and
  `for i, x in items` are the two forms that take one apart. The header writes
  one struct per distinct tuple of an exported signature, and the `.antl`
  carries the elements.
- Every warning carries a stable name at the end of its message.
  `allow(name, "reason")` silences a warning and `unchecked(name, "reason")`
  overrules a safety check, before a statement, last in a header, after a
  field's type or at the top of the file. A clause that silences nothing is
  `unused-allow` or `unused-unchecked`. A release build refuses every
  warning, `--warnings-as-errors` gives a dev build the same, and `anti check`
  passes it. `catch none` counts a failure as `none`. See "Errors, warnings
  and checks" in `docs/decisions.md` and `docs/notes/warnings.md`.
- Pattern literals are built. `re"..."` is raw and has type `Regex`, a
  struct of `anti.lang` that the compiler declares. antic links PCRE2 and
  checks every literal: a malformed one is an error at the byte PCRE2
  names, and nested repeats over text that overlaps fail the safety check
  `exponential-pattern`. Each module compiles its literals once before
  `main`, in a constructor. `Regex.compile(text)` calls `compile` of
  `anti.regex`, which fails with `BadPattern`. A program that holds
  `anti.regex` links `anti_rt_regex` and `libpcre2-8.a` of `lib/<target>/`.
  `s.matches(r)`, `s.find_all(r)`, `s.replace(r, with)` and `s.split(r)` are
  calls of `anti.regex` with `limit` in both directions. A literal at the call
  cannot fail and stops at the match limit, and any other pattern may fail.
  `ByteRegex` and the same methods of `[]byte` are built, a literal takes its
  mode from where it stands, and a match of bytes is a `ByteMatch`. `patch`
  takes `into`, `at` and `limit` by position until named arguments are built,
  and `to_bytes` and `to_text` are calls of `anti.text`.
  `Match` is a struct of `anti.lang` and `?Match` its `?T`, a match stands
  as a condition, `if let` binds one, and the groups of a literal are fields. See
  "Regular expressions" in `docs/decisions.md` and `docs/notes/patterns.md`.
- `f"..."` and `rf"..."` are built. Each text and each `{expr}` is a call
  on an `anti.text.Builder`, the format specification after a colon gives
  the arguments of the call, and the text is memory of its own that the
  program frees with `free(s.ptr)`. See "Interpolation" in
  `docs/decisions.md`.
- `f16` is built: sixteen bits in a field, an array, a slice or a variable.
  A read gives an `f32`, a write takes `as f16`, and no operator takes one.
  ARM64 and x86-64-v3 convert in one instruction, and `v1` and `v2` call the
  runtime. See "`f16`" in `docs/decisions.md`.
- `-> never` is built. A call of such a function ends its path, the IR ends
  its block with `IR_UNREACHABLE`, and the LLVM text writes `noreturn cold`.
  `Error.fatal` is `final` and returns `never`. See "Facts that end a path"
  in `docs/work-order-llvm-optimization.md`.
- `*T` never holds `none` and `?*T` may, and a function value follows the same
  rule with `?fn(...)`. Narrowing is per block and follows `&&` and `||`.
  `let m = p else { }` and `p catch` bind the checked value, and every pointer
  of an `extern fn` is `?*T`. A failing function returns `?*Error`.
- `?T` of any type is built. A `?T` of a value is the value and one flag
  byte, a struct with C layout, and follows every rule of `?*T`: `none`,
  the test and the narrowing, `if let`, `let ... else`, `??`, `?.` and the
  guards of `catch`. `?*T` and `?fn(...)` stay one pointer, and a `?T` of a
  type parameter whose argument is a pointer is that pointer. The header
  writes `struct anti_opt_<T>` with `value` and `has`, and `?Match` is a
  `?T` of `Match`. See "Optional values" in `docs/decisions.md`.
- The language hooks are built. `operator fn` takes `iter`, `next`, `value`,
  `index` and `set_index` beside the operators, `for x in e` walks a
  collection and an iterator, `e[i]` and `e[i] = v` call `index` and
  `set_index`, and every iterator has `to_slice`. See "Language hooks and
  iteration" in `docs/decisions.md` and `docs/notes/iteration.md`.
  `for x in e` gives a copy and refuses a change that reaches the copy
  alone. `for x in &e` lends each element for one turn, over a slice, an
  array and an iterator whose `value` gives a `lent *T`. A dev build traps
  when a collection changed while a loop walks it, through
  `anti.lang.Changes` and `anti.lang.Watch`. See "Generics and
  collections" in `docs/decisions.md`.
- The syntax of generics is built. Type parameters in `<>` on functions,
  structs, classes, variants, interfaces and the functions of a class
  body, `N: int`, type arguments in every type, the rule of C# in an
  expression and `>>` closing two lists. `constraint` and `type` are
  items. The checker checks a body against its constraints and each use
  where it stands, and infers the type arguments of a call. Hooks,
  interfaces, `+`, named sets and `Number` of `anti.lang` are
  constraints, and a parameter without them is stored, copied, passed on
  and measured alone. Every use with concrete arguments compiles a copy of
  its own, `max<int>` and `List<int>.push`, a clone of the checked tree
  with the arguments in place. A dev build marks a copy link-once, and a
  release build merges copies whose code is identical. A library file
  carries the checked tree of every generic, and a module that uses one
  makes its copies from it under the path of the generic's module.
  `export type` writes a copy into the C header, and `anti doc` shows
  generics. A function of a class with type parameters of its own, a type
  nested in a generic class, generic synchronized and concurrent classes and
  generic workers compile for the module's own source, and a library file
  carries neither of the first two yet. See "Generics and collections" in
  `docs/decisions.md` and `docs/notes/generics.md`.
- Hashing and order are built. `operator fn hash(self) -> u64` is a hook,
  `x.hash()` gives a `u64` on every type, a struct, a tuple, a variant, a
  `?T`, an array and a slice take the default over their parts in order,
  and a class takes the `hash` of `Object` and replaces it with `concrete
  fn hash`. `lower_hash.c` writes the default, and `src/rt/hash.h` holds
  the constants antic and the runtime share. `anti.lang` ships
  `constraint Ordered = eq + lt;`, and the runtime chooses the seed of the
  hashing collections at start, `lang.hash_seed()` gives it and
  `--anti.hash_seed` fixes it. See "Generics and collections" in
  `docs/decisions.md` and `docs/notes/hashing.md`.
- Direct imports are built. `import anti.text.{Builder, equal};` makes the
  listed items of a module visible without its name, and the module stays
  reachable by its name. A listed name that clashes with a name of the file
  is refused, naming both, and `anti fmt` sorts the list. See "Generics and
  collections" in `docs/decisions.md` and `docs/notes/direct-imports.md`.
- The collections of round five are built, each module a file under
  `src/std/anti/collection/`: `List<T>`, `Deque<T>`, `Ring<T, N>` and
  `Grid<T>`, `Map<K, V>` and `HashMap<K, V>`, `Set<T>`, `HashSet<T>` and
  `BitSet`, `SortedMap<K, V>` and `SortedSet<T>`, `Pool<T>` with `Handle<T>`,
  `Tree<T>` and `PriorityQueue<T>`. The thread-safe ones are `SyncList<T>`,
  `SyncMap<K, V>`, `SyncSet<T>` and `SyncPool<T>` of
  `anti.collection.synchronized` and `ConcurrentMap<K, V>` and
  `SpscRing<T, N>` of `anti.collection.concurrent`. Each has the operations of
  its family and those of `Collection<T>`, and takes an optional allocator when
  it is made. `dup` copies every collection, and `=` between two collections is
  refused. `deserialize` of a collection is not built. See "Generics and
  collections" in `docs/decisions.md`.
- `--memory-checks` is built for antic, `anti build`, `anti run` and `anti
  test`. The back end calls the checks of AddressSanitizer at every load and
  store, the program links its runtime from the runtime archive, the runtime
  marks the blocks it keeps until exit, and `anti run` and `anti test` put the
  report through Anti's symbolizer. `memory_checks_list` finds a pointer into
  a `List` kept across a push that grows it. windows-arm64 refuses the option,
  and windows-x86_64 reports no leaks.
- Of the small things, `switch` on a `str` is built, a chain of calls of
  `anti.text.equal`, with `x in lo..hi`, `p ?? q`, `p?.x` and `p?.f(args)`.
  See "Small things" in `docs/decisions.md`.
- Anti 0.2.0 installs with one command. The six packages, the six symbols archives and
  `SHA256SUMS` are assets of the GitHub release of the tag, which `tools/release-base`
  names. anti-lang.com serves text alone: the two installers, the two
  uninstallers, the downloads page, `SHA256SUMS.sig` of every version and the
  public key, which is the single trust anchor. `tools/site-base` names it. The two halves stand on two hosts on purpose, so
  a forged release needs both. See `docs/distribution.md`.
  An install follows the platform: `~/.local/bin` and `~/.local/share/anti`, or
  `%LOCALAPPDATA%\Programs\anti\bin` and `%LOCALAPPDATA%\anti`. Nothing outside
  the user's profile is written. The packer links macOS against the Apple SDK
  that `tools/macos-sdk-pin` names, never a bare `xcrun`.
- `./r` makes a release, in ten steps from a pushed `main` to the published
  download. The version stands in `tools/version` and its entry in
  `CHANGELOG.md`. `./r --dry-run` runs the first five steps and prints a plan
  for the rest, and on a version that is a tag already it warns where a real
  run refuses. See `docs/work-order-release-script.md`, and its decisions
  under "The release script" in `docs/decisions.md`.
- Anti uses no CI. The Mac runs macos-arm64 and macos-x86_64 under Rosetta,
  anti-linux runs linux-arm64 and linux-x86_64 under `qemu-x86_64`, and
  anti-windows runs windows-arm64 and windows-x86_64 under the x64 emulation
  of Windows. A test of `--memory-checks` for an x86_64 target skips under
  qemu and the emulation of Windows, and runs under Rosetta for every program
  that links at `v1`. The last run on real x86_64 hardware is
  that of 2026-10-08, in `docs/reports/2026-10-08-x86_64-hardware.md`. See
  "Scope and toolchain" in `docs/decisions.md`.
- The `anti` tool holds `new`, `build`, `run`, `sdk export`, `sdk import`,
  `test`, `check`, `fmt`, `doc`, `bind`, `symbols` and `license`, and
  nothing else of `docs/tooling.md`. `anti license` has its five forms, and
  `--project` builds the project to print its notice. `anti build` writes
  `NOTICE.txt` beside a program from `anti.lock` and the bundled modules the
  project imports, and the notice of a program of musl names musl and
  mimalloc.
- `anti symbols inventory`, `check` and `resolve` are built. They read the
  runtime configuration, find the program beside it and the libraries of
  `plugins` and `[injections]`, and match every binary to an archive by its
  build id. See "The symbols command" in `docs/decisions.md` and
  `docs/notes/symbols.md`.
- `anti bind` is built. `anti bind raylib_api.json` and `anti bind --clang
  <header>` write a binding module, a shim for the inline functions and, with
  `--probe`, the ABI probe in C and in Anti. `--clang` runs clang with
  `-Xclang -ast-dump=json` and reads the JSON with the scanner of
  `src/rt/json.c`. `anti bind --header <name>.antl` writes the header of `--lib`.
  `link framework "Name";` and `link linux "Name";` are built, and `anti`
  passes the names to antic. A Linux program that reaches a `link linux`
  line links dynamically against the glibc 2.35 sysroot
  `linux-<cpu>-glibc`, and every other one statically against musl.
  See "The bind command" in `docs/decisions.md` and `docs/notes/bind.md`.
  `anti fmt` writes the canonical form of the formatter rules, and `src/std/` and
  `tests/` stand in it.
- `anti build` is built. It reads `anti.toml`, resolves the dependencies into
  `anti.lock`, fetches every library file from a `file://` or `https://`
  repository into the cache of the user and compiles the project. Dev mode
  writes one object per module, cached by the digest of its input, the compiler
  version, the target and the level, the options of the manifest and the
  digests of its imports, with `-g` on. `--release` compiles the
  whole program in one call and writes `<program>-symbols.zip` beside it.
  `--target`, `--cpu`, `--offline`, `--strip-docs`, `--lto` and
  `--lib static|shared` are built, and a project without `main` is a library project. `anti run`
  builds for the host and runs it, and `anti new` writes a starter project. See
  "The build command" in `docs/decisions.md` and `docs/notes/build.md`.
- `anti check` is built, with its four classes in the order of
  `docs/tooling-addendum.md`: the front end on every source, with
  `--targets all` once per target, the `anti` blocks of the doc comments in
  their two contexts, the doc warnings and the formatting, which compares
  each file with what `anti fmt` writes. The first
  failing class ends the run, and the doc-warning class reports and fails
  nothing. No class checks patterns, since the front end checks every
  pattern literal. `antic --front-end` runs the front end
  alone, `--warn-undocumented` reports a `pub` item without a `///`
  comment, and the checker warns where the name a `catch` binds shadows a
  variable. See "The check command" in `docs/decisions.md` and
  `docs/notes/check.md`.
- `anti doc` is built. It writes one page per module and an index of them,
  plain semantic HTML with eight class names and no styling, or Markdown with
  `--markdown`. User docs come from the public interface alone, so a library
  file is enough and the page built from one equals the page built from the
  source, which the test `anti_doc` checks. `--dev` and `--private` read the
  syntax tree for the private items and the `//#` notes and refuse a library
  file. The library file now carries the parameter names of a function of a
  class body and the `worker` mark, and its format version is 88. See "The doc
  command" in `docs/decisions.md` and `docs/notes/doc.md`.
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
  level of the runtime it linked, and refuses a machine below it. `src/antic/cpu.c`
  holds the table, `tools/cpu-levels` holds it for the build and
  `cpu_levels_pin` compares the two.
