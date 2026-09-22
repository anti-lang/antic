# Hooks and tracing

The "Hooks and tracing" section of `docs/anti-language-additions.md`, in
five commits. The choices of the passes are in `docs/notes/hooks.md` and
the entries in `docs/decisions.md` under "Hooks and tracing".

## What was built

`anti.lang.Object` declares nine hooks with empty bodies after its seven
functions: `created`, `destroyed`, `copied`, `dispatched`, `joined`,
`enter`, `leave`, `failed` and `changed`. They take the nine table
entries after the root's seven in every class, so `alloc` and `free` of
`anti.mem.Allocator` moved to entries 17 and 18.

`anti.lang.TraceHandler` declares the same nine with the object after
`self`, and `anti.lang.Trace.install(h)` stores one handler in an atomic
word of the runtime. A site calls the handler and then dispatches the
object's own hook, reversed on `leave`.

`trace` is a contextual word before `class` and before `fn` in a class
body. `--trace` and `--no-trace` decide for marked code instead of the
mode, `--trace <pattern>` reaches a class that did not ask,
`--trace writes` adds `changed`, and `--no-hooks` drops every site.

`anti.lang.FieldDescriptor` is a new built-in struct of the compiler,
with the layout of one field record, which `changed` takes.

## What is not built

`anti.trace` with `LeakTracker`, `Profiler`, `CallLogger`,
`ErrorMonitor`, `ThreadMonitor`, `ChangeJournal` and `Composite`, and the
runtime key `trace` that names one. The cost the specification gives, one
load and one compare at a site without a handler, is not met either: a
site is one call of the runtime today. Both stand in the "Built" line of
`docs/anti-syntax-overview.md` and in `docs/decisions.md`.

## What failed and how it was fixed

The first run of a `trace class` whose own `enter` hook is a `pub`
function looped without end: entering `enter` fired `enter`. A hook is
never instrumented now, whatever its signature, and `hook_name` in
`src/lower.c` decides by the name alone.

`anti.lang.TraceHandler` declares nine functions whose names the root
already has, with one parameter more. Keyed by name alone, those bodies
took the entries of the root's nine. A hook site on a handler object
would then have called a two-argument function with one argument. A table
entry is keyed by its name and its parameter count now.

`FieldDescriptor` is a struct of `anti.lang` that the compiler declares.
Written into `anti.lang`'s own library file as a local struct, it came
back as a second type, and `concrete fn changed` of a handler was refused
against a type that printed the same. `src/antl.c` treats it as it treats
`Flags`.

The report of an abstract class that no class fills named `TraceHandler`
on every build of `anti.lang`. It runs for a build that writes a program
now, and not for `-c` or `--lib`.

The table grew by nine entries, so the slots of every test that names one
moved: `tests/unit/test_lower.c`, `tests/unit/test_modules.c`,
`tests/unit/test_optimize.c`, `tests/unit/test_whole.c`,
`tests/std/reflect_call.anti`, `tests/modules/shared/client.anti`, the
two headers of `tests/dump/` and the four `devirt` listings. The library
format rose to version 47 for the `trace` marking of a class, so
`tests/modules/scale.antl.hex` and the bytes in `tests/unit/test_modules.c`
were written again. `tests/emit-identity/programs.sha256` and
`tests/link-identity/return42.macos-arm64.sha256` hold the output of this
Mac again.

## Provisional entries

Twelve, all under "Hooks and tracing" in `docs/decisions.md`: the key of a
table entry, the site as one call of the runtime, the two hooked `join`
entry points, where each of the five always-on hooks fires, `trace`
before a module function, a hook never being instrumented, what a
`--trace` pattern does and does not turn on, what `--trace writes` takes
and from where, the build that decides `enter` and `leave`,
`FieldDescriptor`, the error type of the root's `failed`, and the
narrowing of the abstract-class report.

## The gates

The host suite, both sanitizer suites and the docs-style checker ran on
the pushed tree. The logs are under `build/drive/logs/`:
`final-build.log`, `final-test.log`, `final-asan-test.log` and
`final-ubsan-test.log`.

| Gate | Result |
|---|---|
| Host build | zero warnings |
| Host suite | 733 of 733 |
| `cmake --preset asan` | 732 of 732 |
| `cmake --preset ubsan` | 732 of 732 |
| Docs-style | nothing on every file this step wrote |

The checker reads `CMakeLists.txt` and `tests/CMakeLists.txt` as Markdown,
because of their extension, so every `#` comment is a heading and every
line of code is prose. Those two files carry 227 findings of that kind
before this step, and the lines written here add six more. No other file
this step touched reports anything.

## Proof

```text
$ git log --oneline -3
1011743 Pin the hooks across a module boundary
bbc83d1 Record hooks and tracing in the documents
c8186ae Test every hook and the order of the two calls

$ git status --short

$ git rev-parse HEAD origin/main
1011743db99230816498908f292ac92e5d72cf36
1011743db99230816498908f292ac92e5d72cf36
```

## Questions for Eddie

- The specification gives a hook site one load and one compare without a
  handler. A site is one call of the runtime today, which holds the order
  of the two calls and the compare against the root's empty body. The
  inline form needs the compiler to write the load, the branch and both
  dispatches at every site. Is it worth that, or does the call stand?
- `anti.trace` and its seven handlers are the next step of this section.
  The runtime key `trace` names one at start, which the runtime
  configuration already reads keys for.
- `enter` and `leave` stand in the body of a function, so `--trace` on a
  program reaches no function whose body came from a library file. A
  build of the standard library under `--trace` would be one way to reach
  it. Is that wanted, or does the marking of the author decide alone?
