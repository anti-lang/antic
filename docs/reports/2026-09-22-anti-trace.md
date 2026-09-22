# anti.trace and its seven handlers

The module `anti.trace`, the handlers that `docs/anti-language-additions.md`
names under "Hooks and tracing", and the runtime key `trace` that names one.

## What was built

`std/anti/trace.anti` holds `LeakTracker`, `Profiler`, `CallLogger`,
`ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`. Each
inherits `anti.lang.TraceHandler` and fills all nine hooks. Between them
they read all nine: `created`, `copied` and `destroyed` in the tracker,
`dispatched` and `joined` in the thread monitor, `enter` and `leave` in
the profiler and the logger, `failed` in the error monitor and the
logger, and `changed` in the journal.

`trace.start()` reads the `trace` key with `anti_rt_conf_get` and installs
what it names. `trace.install(what)` takes the same text from a program.
The names are `leaks`, `profile`, `calls`, `errors`, `threads` and
`writes`, and a list separated by commas gives a `Composite`.

Two rules run through the module. A handler allocates no object inside a
hook, because an `alloc` under `created` calls `created` again without
end. Every table it keeps is therefore a field with a size at compile
time. A
handler counts with atomic fields, because a hook runs on whichever
thread the object it stands for runs on. The reports are written by the
program and go to standard error.

`docs/notes/trace-handlers.md` holds the choices of the module.

## What the step needed first

Three gaps stood between the module and its `tests` block, each found by
the work and fixed with a failing test first.

A `tests` or `fixtures` block did not reach the private items of its
module, which the specification gives it. `level_allows` of `src/sema.c`
now passes a function of either block through, for a class of the module
being compiled. Commit `cf130c2`.

A literal could not name `anti.lang.FieldDescriptor`. The compiler
declares it as it declares `Flags`, and the struct-literal path knew
`Flags` alone. Commit `b2ea3b2`.

A dev build links one object per module, and `anti test` passed the
object of the module under test alone, so only a module that imported
nothing could carry a `tests` block. `driver_libraries` of
`src/driver.c` gives the library files a compilation needs, and the tool
writes an object of each and links them. Commit `bf79dd4`.

That link then failed on `anti_rt_registry`, `anti_rt_trampolines` and
`anti_rt_backtrace_default`. An object carries every function of its
module, reached or not, so the reader of a table comes into the link
from a function the program never calls. `whole_options` carries a `dev`
flag and the pass writes the three tables in a dev build whatever the
reach says. Commit `31fcedf`. The listing
`tests/dump/main.dev.linux-x86_64.s` holds them now.

## Tests

`std_tests_trace` runs the module's seventeen tests with `anti test`, in
dev mode and in release mode. It refuses a release run that reports
other tests than the dev run. The tests call the hooks of each handler
directly, which is what a hook site does.

`program_trace_handlers` and `program_trace_handlers_traced` drive the
handlers through the real sites, once in release mode, where the five
always-on hooks fire alone, and once under `--trace --trace writes`.

`trace_handlers` matches the reports and the lines of a `CallLogger`
against patterns, because the spans a `Profiler` reports differ from run
to run.

`anti_test` gained `com/example/greeting.anti`, a module whose test
reaches another module, which pins the dev link.

## Gates

The host suite runs 738 tests and passes, the ASan and the UBSan suites
737 each and pass, `build/drive/logs/test.log`,
`build/drive/logs/asan-test.log` and `build/drive/logs/ubsan-test.log`.
The build writes no warning, `build/drive/logs/build.log`. The docs-style
checker reports nothing on every file the step touched. `emit_identity`
needed the six digests of the new program, written on the Mac with
`-DWRITE=yes`.

## The provisional entries

`docs/decisions.md` gained eleven `[provisional]` lines, nine under
"Hooks and tracing" and two under "Tests and fixtures".

The one worth Eddie's eye is the first: the specification says the key
"selects one by name at start", and nothing of Anti runs between the
runtime's own start and `main`. A program that does not import
`anti.trace` links none of the handlers, so no layer below the program
could install one for it. `trace.start()` as the first statement of
`main` is the smallest form that keeps the key in charge, and it is the
form `rt.configure(path)` already takes. A compiler that wrote the call
into a program which links the module would match the words more
closely, at the cost of a call nothing in the source shows.

## Question

Should `trace.start()` stay a call the program makes, or should antic
write it into the entry of a program that links `anti.trace`?
