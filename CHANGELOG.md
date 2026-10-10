# Changelog

One entry per released version, the newest first. The release script reads
the entry of `tools/version` and refuses a release without one. The body of
the entry becomes the text of the GitHub release.

## 0.2.0 - 2026-10-10

The second release of Anti. Every package is complete on its own, and a
user builds programs for all six targets from any one of them with the
network off.

- The back end translates the IR into LLVM IR text, and opt and llc of
  the pinned release write the object for all six targets. A release
  build links the program and the runtime as bitcode through full LTO,
  and `--lto none` links objects. An atomic operation is an LLVM atomic
  instruction, `-> never` ends a path, and `--profile-generate` and
  `--profile-use` build with the profile of a run.
- `may fail` and `fail`. A function declared `may fail` returns its error
  on a channel of its own, `try` forwards it, `undo` runs on the fail
  path and `catch` binds it. The first `fail` records the origin of an
  error, and a dev build adds the stack trace. `here` and default
  parameter values.
- `?T` of any type, tuples, nested types, sum types with `variant` and
  `switch`, `f16`, `fallthrough`, `switch` on a `str`, `x in lo..hi`,
  `p ?? q` and `p?.x`.
- `f"..."` and `rf"..."` with their format specifications, the string
  prefixes, `re"..."` pattern literals over PCRE2, and the methods of
  `str` and `[]byte` that take one.
- The wrapping operators `+% -% *% <<%`, the saturating operators
  `+| -| *|`, `mul_high` and the flags form `let (result, flags) = e;`.
- Generics: type parameters on functions, structs, classes, variants and
  interfaces, constraints, one compiled copy per use, and generics in
  library files and in the C header. With them hashing and order, the
  default `==`, the hooks `iter`, `next`, `index` and `set_index`,
  `for x in &e`, direct imports, and ownership at a call with lending.
- Anonymous functions, closures and `snapshot fn`.
- Locking and channels: `Mutex`, `sync m { }`, `chan T(n)`, `send`,
  `recv` and `select`. `synchronized class` and `concurrent class`, with
  the safety check of unguarded fields.
- Simd structs, whose operators compile to the vector instructions of
  the native width.
- Hooks and tracing, injection, the six standard interfaces, plugins
  with `provides` and `anti.plugin`, interface versioning, and the
  runtime configuration of `--anti.conf`.
- Every warning carries a stable name, `allow` and `unchecked` silence
  one with a reason, and a release build refuses every warning.
- The collections of `anti.collection`: `List`, `Deque`, `Ring`, `Grid`,
  `Map`, `HashMap`, `Set`, `HashSet`, `BitSet`, `SortedMap`,
  `SortedSet`, `Pool`, `Tree` and `PriorityQueue`, and their thread-safe
  forms. `--memory-checks` calls the checks of AddressSanitizer at every
  load and store.
- The standard library gains `anti.lang` as its root, `anti.fs`,
  `anti.config`, `anti.debug`, `anti.mem` with `Allocator`,
  `anti.runtime`, `anti.simd`, `anti.trace`, `anti.plugin` and
  `anti.regex`.
- `anti` holds `new`, `build`, `run`, `check`, `fmt`, `doc`, `bind`,
  `symbols` and `license` beside `sdk export`, `sdk import` and `test`.
  `anti build` reads `anti.toml`, resolves the dependencies into
  `anti.lock` and writes `NOTICE.txt` beside a program.
- The native libraries PCRE2, SQLite, Mbed TLS, miniaudio and raylib
  build for all six targets, and their headers stand in `include/` of
  the package. mimalloc is the allocator of every program of musl.
- Every package holds the pinned LLVM tools of its host in `bin/` and
  the runtime of all six targets with its bitcode. With them stand the
  native libraries, the sysroots of all six targets and
  `licenses/sources.txt`. The Windows
  sysroot is mingw-w64, and a Windows program links against no CRT or
  SDK library of Microsoft. The installers download the package, check
  its signature and its digest, unpack it and run nothing else. Step 5
  of a release checks an install on each VM with the network off.
- Every tool a build runs comes from the pinned release of
  `anti-lang/llvm-tools`, on every host. A Mac links with the pinned
  ld64.lld against the pinned Apple SDK. Anti runs no CI.

## 0.1.0 - 2026-09-20

The first release of Anti.

- The compiler lexes, parses and type-checks Anti across modules, and its
  IR holds no sizes. The back end lays out types per target and writes
  assembly for the six targets, which llvm-mc assembles and lld links.
- The object model: classes, interfaces as inline sub-objects with thunks,
  four visibility levels, `construct` and `destruct`, operators, singletons,
  the error forms and reflection over descriptors.
- `worker fn`, `parallel` and `dispatch` on all six targets.
- The standard library holds `anti.io`, `anti.text`, `anti.license`,
  `anti.error`, `anti.time`, `anti.os`, `anti.reflect`, `anti.random`,
  `anti.collection`, `anti.toml`, `anti.args`, `anti.json` and `anti.log`.
- `antic -g` writes the line of every statement, and lldb and gdb stop by
  file and line and print a backtrace of Anti function names.
- The processor levels of `--cpu`, with one runtime per level in the
  archive and a check at the start of every program.
- `anti` holds `sdk export`, `sdk import` and `test`.
- Six packages, one per host, with the installers of the two shells. The
  packages, the symbols archives and `SHA256SUMS` are assets of the GitHub
  release of the tag. anti-lang.com serves the installers, the downloads
  page, `SHA256SUMS.sig` and the public key, and no binary. A forged
  release needs both hosts.
