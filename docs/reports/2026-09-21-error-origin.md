# Error origins and stack traces

The error-origin step, finished from the working tree that the previous session
left. That session committed `212137e`, default parameter values and `here`, and
`d3373b1`, the build id. It left the rest uncommitted. This session read that
tree, checked it against "Error origin and stack traces" and "Source locations"
of `docs/anti-language-additions.md`, fixed its comments, rewrote the two
identity manifests and committed it in three commits. Nothing was restarted and
nothing of the tree was dropped.

## Commits

- `3c083ce` Capture stack traces in anti.lang.StackTrace. `StackTrace` with
  `capture`, `frames`, `text` and `symbolize`, `RawFrame` and `Frame`,
  `anti.debug.backtrace`, `rt/trace.c` and `rt/symbols.c`, and the tests
  `trace_stack*`, `trace_symbols_*` and `std_backtrace`.
- `f84fed2` Write the origin and the frames of an error at its first fail.
  `Error.at` and `Error.frames`, the lowering of `fail`, `e.text()` with the
  position, the causes and the trace, `anti_rt_backtrace_default` from the
  pass over the whole program, `--anti.backtrace`, and the tests
  `trace_origin*` and `trace_options*`.
- `35d0e7c` Say that license.text leaves out the build id. A line of the site
  page that belonged to `d3373b1`.

The full suite ran on the host before each of the two code commits. It
passed 545 of 545 before the first and 552 of 552 before the second. The
first commit was cut from the tree by hand. Its own emit and link manifests
come from a run on the Mac at that state.

## What the spec sections ask, and where it stands

| Rule | State |
|---|---|
| `at` written by the first `fail`, kept by `try` and a second `fail` | Built, `trace_origin` |
| `frames` captured by the same `fail` when backtraces are on | Built, `trace_origin_dev`, `trace_origin_option` |
| On in dev mode, off in release, `--anti.backtrace` in release | Built, `trace_origin_dev_off`, `trace_options*` |
| `backtrace = true` of the runtime configuration | Not built. The configuration file does not exist yet |
| `e.text()`: `file:line:column: `, the error, the causes, the trace | Built, `trace_origin*`, `std_lang` |
| `Error.new` takes no location | Unchanged, as specified |
| `StackTrace.capture`, `frames`, `text`, `symbolize` | Built, `trace_stack*` |
| `symbolize` names the function in every build, file and line in `-g` | Built, `trace_stack_g`, `trace_symbols_*` |
| `anti.debug.backtrace` | Built, `std_backtrace` |
| `here` as keyword and as a call-site default | Built in `212137e`, `program_defaults`, `program_location` |

`symbolize` against a `-g` build is proven twice. `trace_stack_g` runs a `-g`
program on the Mac and matches `stack_trace.inner stack_trace.anti:14` and the
lines of its callers. `trace_symbols_<target>` links a `-g` program for
linux-x86_64, linux-arm64, macos-x86_64 and macos-arm64 and reads it with the
readers of `rt/symbols.c`, compiled for the host.

## Changes this session made to the tree

- Sixteen findings of the docs-style checker in the comments of `rt/trace.c`,
  `rt/trace.h`, `rt/symbols.c`, `rt/symbols.h`, `rt/backtrace.c`,
  `rt/start.c`, `src/whole.c` and `tests/unit/symbols_probe.c`, and one long
  sentence in `tests/CMakeLists.txt`. The checker reads `CMakeLists.txt` as
  Markdown, so that one was fixed by reading it.
- `tests/emit-identity/programs.sha256` and
  `tests/link-identity/return42.macos-arm64.sha256`, written on the Mac. The
  programs that change are the thirteen that pull in code of `anti.lang`. At
  the first commit only the numbering of its constants moves. `nullable` is one
  of them without a `fail` of its own.
- `CLAUDE.md`, the counts and the line of the state.

## Evidence

```console
$ git log --oneline -3
35d0e7c Say that license.text leaves out the build id
f84fed2 Write the origin and the frames of an error at its first fail
3c083ce Capture stack traces in anti.lang.StackTrace
$ git status --short
$ git rev-parse HEAD origin/main
35d0e7c5f45e5f972e73767192e0c43fc30cb160
35d0e7c5f45e5f972e73767192e0c43fc30cb160
```

The three suites ran at `35d0e7c` after `cmake --fresh` and a build with
`--clean-first`, before the push:

```text
build        100% tests passed out of 552
build-asan   100% tests passed out of 551
build-ubsan  100% tests passed out of 551
```

The sanitizer builds leave out `no_paths` as before. The UBSan preset builds
with `-fno-sanitize-recover=all`, so a finding fails its test.

## Not done

- Windows has not run a trace. `RtlCaptureStackBackTrace` and DbgHelp compile
  in the cross runtime of both Windows targets, and no Windows machine ran them.
  The two VMs of `docs/vm-setup.md` did not run in this session.
- The host link still prints the message of Apple's linker that
  `libLTO.dylib` of the pinned clang is missing, as the earlier reports
  record. It is no warning of the compiler and fails nothing.

## Questions for Eddie

- The eleven `[provisional]` entries under "Error origins and stack traces" in
  `docs/decisions.md` wait for review. The ones with the widest reach are the
  status 70 for an unknown `--anti.` option and the 64 frames a trace keeps.
