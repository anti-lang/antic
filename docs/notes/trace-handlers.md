# The stock trace handlers

The choices of `src/std/anti/trace.anti`, the seven handlers of
`anti.lang.TraceHandler` and the `trace` key that names one. The rules
are in "Hooks and tracing" of `docs/anti-language-additions.md`, the
decisions under "Hooks and tracing" of `docs/decisions.md` and the sites
of the hooks themselves in `docs/notes/hooks.md`.

## What a handler may do

A hook runs wherever the object it stands for runs. A site is one call
of the runtime that loads the installed handler and calls it. Two rules
follow for every handler here.

A handler allocates no object inside a hook. `alloc` of a class fires
`created`, which reaches the installed handler, which allocates again.
Every table a handler keeps is therefore a field of the handler, with a
size at compile time. The sizes are `PROFILE_SLOTS`, `JOURNAL_SLOTS`,
`MESSAGE_BYTES` and `COMPOSITE_SLOTS`, each a `pub const` of the
module.

A handler counts with atomic fields. `created` fires on the thread that
made the object and `joined` on the thread that joined the job. Two
hooks therefore run at once in a program with workers.

The reports are written by the program and never by a hook. `report`
allocates nothing either, because `eprint_int` writes its digits into a
local array and hands `text.from_c` the address of the first one.

## The seven

| Handler | Hooks it reads | What it keeps |
|---|---|---|
| `LeakTracker` | `created`, `copied`, `destroyed` | three counts |
| `Profiler` | `enter`, `leave` | a slot per function name |
| `CallLogger` | `enter`, `leave`, `failed` | the depth and the lines |
| `ErrorMonitor` | `failed` | the last code, message and function |
| `ThreadMonitor` | `dispatched`, `joined` | three counts |
| `ChangeJournal` | `changed` | the last writes, newest first |
| `Composite` | all nine | the handlers it owns |

`LeakTracker` counts `copied` as one object made. `dup` runs no
`construct`, so it fires `copied` alone, and the object it makes is one
more object to free. `tests/programs/hooks.expected` shows that order.

`Profiler` keeps a depth per slot rather than a stack. `enter` reads the
clock where the depth is 0 and `leave` adds the span where it falls back
to 0, so a function that calls itself is one span per outermost call.
The names come from the compiler as constants in read-only data, so a
slot keeps the `str` it was given.

`ErrorMonitor` copies the message into a byte array of its own. The
error a `catch` binds is deleted when the handler exits, so a `str` into
it points at freed memory once the hook has returned. The code and the
name of the function are values and constants and need no copy.

`ChangeJournal` keeps two names per entry, the class of the object and
the field. Both stand in read-only data: the field record comes from the
class descriptor, and `reflect.name` reads the class name from the same
descriptor. The object itself is never kept, because the program may
delete it before the journal is read.

`Composite` owns what it holds. `install` builds the handlers it adds,
so one `delete` frees the group. It reaches them in the order they were
added, and on `leave` in the other order. That is the rule the hook
site already follows between the handler and the object's own hook.

## The key

`start` reads the `trace` key with `anti_rt_conf_get` and hands it to
`install`. `install` takes one name, or names separated by commas, and
gives the handler that `Trace.install` now holds. The names are `leaks`,
`profile`, `calls`, `errors`, `threads` and `writes`.

The program makes the call, as it calls `rt.configure`. Nothing of Anti
runs between the runtime's start and `main`. A program that does not
import `anti.trace` links none of the handlers, so no other layer could
install one for it.

## The tests

The `tests` block holds seventeen tests and calls the hooks of each
handler directly, which is what a hook site does. A block sees every
private item of its module, so a test reads `Profiler.inside` and
`CallLogger.depth` where the accessors would hide the answer. The
fixtures give an `anti.lang.Error` for the object of a hook and a
`FieldDescriptor` for `changed`.

`tests/trace/handler_reports.anti` pins the reports and the lines of a
`CallLogger`, against patterns, because the spans a `Profiler` reports
differ from run to run.

`tests/programs/trace_handlers.anti` drives the handlers through the
real sites instead. It runs once in release mode, where the five
always-on hooks fire alone. It runs again under `--trace --trace
writes`, where the call hooks and the writes fire as well. The two expected files are the same
program with two sets of numbers.
