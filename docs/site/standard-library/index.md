---
title: "The standard library"
description: "Console and file I/O, string formatting, regular expressions, graphics, audio and networking modules."
date: 2026-09-13T22:32:00+02:00
lastmod: 2026-09-13T22:32:00+02:00
draft: true
weight: 260
---

## First modules

The directory `std/` holds the modules of the standard library under the reserved root `anti`, under the 0BSD licence of `std/LICENSE`. The build writes each module as a library file into `std/` of the runtime archive, as the package `anti` with the version of antic. Every module of a package lies under the package root, and `anti.io`, `anti.text` and `anti.license` lie under `anti`. The notice of a program that imports one of them therefore names the package `anti`, which the test `program_licenses_std` checks. A program imports them without `-I`, because antic searches `std/` of the runtime archive after the roots of the command line.

| Module | Functions | C side |
|---|---|---|
| `anti.io` | `print`, `println`, `eprint`, `eprintln`, `exit` | `rt/io.c` |
| `anti.text` | `equal`, `from_c`, `byte_count`, `char_count`, `slice`, `find_byte`, `parse_int`, `Align`, `Builder` | `rt/text.c` |
| `anti.license` | `text` | `rt/license.c` |
| `anti.fs` | `open`, `read`, `write`, `size`, `close`, `list`, `remove`, `rename`, `File`, `Mode` | `rt/fs.c` |

The module `anti.io` writes through the C streams `stdout` and `stderr`, so its output and the output of `printf` in one program keep their order. The function `text.from_c` returns a `str` that points into the C string, because Anti builds no `str` from a pointer and a length. The function `license.text` returns the lines of the notice `anti_licenses` between its markers, without the build id. In a static library for C with a bundled runtime it returns an empty text, because an archive carries no notice, as chapter 25 describes. An `f"..."` calls the `append` functions of `text.Builder` and its `take`, whose text the program frees with `free(s.ptr)`. `f"..."(from)` ends with `take_in(from)` instead, whose text comes from the `anti.mem.Allocator` `from` and goes back through it. `anti.text` imports `anti.mem` for that parameter. Every function of `anti.fs` may fail with the `SystemError` of the system, and `list` gives the names of a directory in one block that the program frees with `free(names.ptr)`. Every failing function of the standard library is written with `may fail`, which the test `std_may_fail_only` checks. Beyond `anti.text` and `anti.fs` these are `toml.Document.read`, `log.FileSink.new`, `args.Parser.parse`, `reflect.set`, `reflect.call` and `json.unquote`. Each module has `//!` documentation and a `///` comment on every `pub` item, and the tests `std_io`, `std_text`, `std_license` and `std_fs` run one program for each.

The module `anti.simd` declares nothing. `select`, `any` and `all` take a mask of any `simd struct`, which no function of Anti can, so the compiler knows the three by name there. A module that calls one imports `anti.simd`, as one that writes `here` imports `anti.lang`.

The module `anti.trace` holds the handlers of `anti.lang.TraceHandler` that the hooks of `anti.lang.Object` reach: `LeakTracker`, `Profiler`, `CallLogger`, `ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`. The function `trace.start` reads the `trace` key of the runtime configuration and installs what it names, one of `leaks`, `profile`, `calls`, `errors`, `threads` and `writes`, or a list of them separated by commas, which gives a `Composite`. The program makes that call, as it calls `rt.configure`, because a program that does not import the module links none of the handlers. A handler allocates no object inside a hook, since an `alloc` under `created` would call `created` again, so every table it keeps is a field of the handler with a size at compile time. The module's `tests` block holds seventeen tests, which the test `std_tests_trace` runs in dev mode and in release mode.

The module `anti.runtime` holds `configure`, which names the configuration file of the program. The specification writes that call as `rt.configure(path)`, so a program imports the module as `import anti.runtime as rt;`. The path `anti.rt` is the C runtime, which no compilation may define, and holds no module of Anti.

Six modules hold a standard interface, an abstract class a program injects with `inject name: *Interface`. Each ships a default implementation and a default provider, so an `inject` field of one links with no entry in the manifest. They are `anti.log.Logger` with `SinkLogger`, `anti.time.Clock` with `SystemClock`, `anti.random.Source` with `SharedRandom`, `anti.fs.FileSystem` with `SystemFileSystem`, `anti.mem.Allocator` with `LibcAllocator` and `anti.config.Config` with `FileConfig`. The default provider is a static function `default` of the interface, which the `[inject]` table of the manifest overrides. The module `anti.config` reads the settings of the program from one TOML file that `config.read` names, and the `[runtime]` table of `--anti.conf` stays the runtime's own.

The modules `anti.net`, `anti.regex`, `anti.raylib` and `anti.miniaudio` wait for the native libraries of the runtime archive.
