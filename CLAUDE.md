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
cmake --preset host && cmake --build build/host -j8 && ctest --test-dir build/host -j8
```

`CMakePresets.json` carries three configurations, each in `build/<preset>`:
`host`, and the sanitizer configurations `asan` and `ubsan`. All three run the
full suite.

The pinned downloads lie in `build/deps/`, one copy for all three trees. The
pinned clang in `build/deps/clang` compiles everything, and the configure step
installs it and the pinned LLVM tools in `build/deps/llvm/bin` with
`tools/get-clang.cmake` and `tools/get-llvm.cmake`. The build also needs the
sysroots in `build/deps/sysroot`, which `tools/get-sysroot.cmake` installs
after `tools/get-clang.cmake`, and the pinned raylib source in
`build/deps/raylib`, which `tools/get-raylib.cmake` installs.
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
| `tools/` | Helper scripts, the pins, the installers and the docs-style checker |
| `tools/keys/` | `release.pem`, the public key that checks the LLVM tools, and the ignored `private/` |
| `docs/` | The specifications, the decisions and the reports |
| `LICENSES/` | Licence texts of bundled components |
| `build/` | Every build tree, the downloads in `deps/` and the driver state in `drive/`. Never committed |

## Repository layout

- The top level holds exactly `build/`, `docs/`, `LICENSES/`, `src/`, `tests/`
  and `tools/`, plus the root files `CLAUDE.md`, `README.md`, `CHANGELOG.md`,
  `LICENSE`, `CMakeLists.txt`, `CMakePresets.json` and `.gitignore`.
- Files that tools require in the root, and the release link r: `.github/`,
  `.gitattributes`, `.editorconfig`, a tracked `.claude/settings.json` if
  there is one, and `r`.
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
16. The native libraries in `src/native/`. PCRE2, SQLite, Mbed TLS, miniaudio
    and raylib build for all six targets, as "Libraries and runtime" in
    `docs/decisions.md` records. Their headers and `lib/cacert.pem` are not yet
    part of the runtime archive. `anti.regex` binds PCRE2, and no module binds
    the other four.
17. Inline atomic instruction sequences, which are runtime calls today.
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
25. A variant gets a descriptor with its tag and each case's fields, so
    `serialize`, `deserialize` and `reflect` handle it. It replaces the
    entry under "Sum types" in `docs/decisions.md` that gives a variant
    none.
26. Done. Plugins on Linux and Windows, and the Linux link mode against
    glibc. A module names a library of the glibc sysroot with
    `link linux "X11";`. A Linux program that reaches one, or that can
    load a plugin, links dynamically against glibc 2.35. A Windows host
    links with an import library its plugins resolve against.
    `docs/reports/2026-09-24-glibc-and-plugins.md` reports it.

Then `docs/anti-language-additions.md` in the order its "Timing" section gives:
nullable pointers, the dev-mode checks, the lines of `-g`, tests and fixtures
and the CPU levels are built. The variables of `-g` come before the first public
release. After it, the wrapping and saturating operators with `Flags`, then
sum types, then locking and channels, all three built. Then injection, hooks
and tracing, plugins and runtime configuration, which belong together. All
four are built. Then generics and closures. Closures are built, `snapshot fn`
included. Round five, generics and collections, stands in the same document.
Its first four parts, the syntax of generics, the constraints, the
compiled copies and the generics of library files and of C, are built, and
none of the rest.

`docs/work-order-completion.md` is the work order those items come from, with
its book steps removed. `docs/reports/2026-09-20-object-model-completion.md`
reports what it finished.

## State

- The compiler lexes, parses and type-checks Anti across modules. Its IR holds
  no sizes: the back end lays out types per target (`src/antic/layout.c`) and folds
  symbolic values. Both back ends cover all integer and float operations.
  The emitter writes assembly for all six targets, llvm-mc assembles it, and lld
  links every target against the sysroots of the runtime archive.
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
  `anti.simd`, `anti.trace`, `anti.plugin` and `anti.regex`.
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
  every site. `anti.trace` ships `LeakTracker`, `Profiler`, `CallLogger`,
  `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`, and
  `trace.start` installs the one the runtime key `trace` names. See "Hooks
  and tracing" in `docs/decisions.md`, `docs/notes/hooks.md` and
  `docs/notes/trace-handlers.md`.
- 1006 ctest tests pass on the development Mac and none is skipped. The ASan
  and the UBSan builds run 1005 each, without the `no_paths` test, which needs a
  build that no sanitizer wrote paths into. `overview_examples` compiles every
  `anti` block of `docs/anti-syntax-overview.md` through the front end.
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
  locks taken in opposite orders. See "Concurrent classes" in
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
  error on in a release build. A line of `[injections]` is a startup error
  while no program carries an injectable interface. See "Runtime
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
  `app.List<int>.push` there, although its symbol escapes the brackets. Variables are the next step. See `docs/notes/debug.md`.
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
  `Match` and `?Match` are structs of `anti.lang`, a match stands as a
  condition, `if let` binds one, and the groups of a literal are fields. See
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
- `*T` never holds `none` and `?*T` may, and a function value follows the same
  rule with `?fn(...)`. Narrowing is per block and follows `&&` and `||`.
  `let m = p else { }` and `p catch` bind the checked value, and every pointer
  of an `extern fn` is `?*T`. A failing function returns `?*Error`.
- The language hooks are built. `operator fn` takes `iter`, `next`, `value`,
  `index` and `set_index` beside the operators, `for x in e` walks a
  collection and an iterator, `e[i]` and `e[i] = v` call `index` and
  `set_index`, and every iterator has `to_slice`. See "Language hooks and
  iteration" in `docs/decisions.md` and `docs/notes/iteration.md`.
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
  generics. See "Generics and collections" in `docs/decisions.md` and
  `docs/notes/generics.md`.
- Of the small things, `switch` on a `str` is built, a chain of calls of
  `anti.text.equal`, with `x in lo..hi`, `p ?? q`, `p?.x` and `p?.f(args)`.
  See "Small things" in `docs/decisions.md`.
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
- The `anti` tool holds `new`, `build`, `run`, `sdk export`, `sdk import`,
  `test`, `check`, `fmt`, `doc`, `bind` and `symbols`, and nothing else of
  `docs/tooling.md`.
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
  version, the target and the level, with `-g` on. `--release` compiles the
  whole program in one call and writes `<program>-symbols.zip` beside it.
  `--target`, `--cpu`, `--offline`, `--strip-docs` and `--lib static|shared`
  are built, and a project without `main` is a library project. `anti run`
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
  class body and the `worker` mark, and its format version is 62. See "The doc
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
