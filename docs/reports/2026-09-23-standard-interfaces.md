# Standard interfaces

The five standard interfaces that were left are built, each an abstract
class with a default implementation and a default provider.
`anti.mem.Allocator` gains the provider it was missing. An `inject`
field of any of the six links with no entry in a manifest. The rule is
in "Standard interfaces" of `docs/anti-language-additions.md`.

## What was done

- The link resolves a provider from a static function `default` of the
  interface where the build's table names none. `src/whole.c` holds it,
  `docs/notes/injection.md` its choices, and
  `tests/programs/inject_default.anti` an interface whose default is its
  own. The message of an interface with no provider names both ways to
  give one, which `tests/errors/inject_missing.err` carries.
- `anti.log.Logger` is the interface and keeps the name, the level and
  the format. `log(level, message)` is abstract, `trace` to `fatal` are
  bodies over it, and the new `SinkLogger` writes the line and owns the
  `Sink`. `Logger.default` gives the logger of the program.
- `anti.time.Clock` has `now`, `wall` and `sleep` abstract and `since`
  as a body, with `SystemClock` as the default.
- `anti.random.Source` has `next` abstract and `below`, `between`,
  `boolean` and `fraction` as bodies. `Random` gives the draw, and
  `SharedRandom` is the default, seeded from the wall clock and locked
  per draw.
- `anti.fs.FileSystem` works in whole files, so a fake holds the bytes
  of each path. `anti.fs` gains `read_file` and `write_file`, and
  `SystemFileSystem` is the default over them.
- `anti.config` is new. `Config` has `has` and `text_of` abstract and
  `int_of` and `bool_of` as bodies, and `FileConfig` reads the settings
  of the program from the TOML file that `config.read` names.
  `toml.Document.open` and `text.from_bytes` carry the file in.
- `tests/std/interfaces.anti` injects all six into one class with no
  manifest and asks every default. `tests/inject/standard` lays a fake
  over each under `anti test`, in dev and in release.

## What failed and how it was fixed

- A singleton refuses a write to a plain field outside `construct`, so
  the document of `FileConfig` is `atomic`, as the logger of `Log` is.
  The swap deletes the document before it.
- `emit_identity` failed for six programs, because the standard library
  they import changed. The manifest was written again on the Mac with
  `-DWRITE=yes`.

## The entries added to `docs/decisions.md`

One section, "Standard interfaces", with eight `[provisional]` entries:
the static `default` as the default provider, the shape and the default
of each of the five interfaces, and `Document.open` with
`text.from_bytes`.

## Gates

- `cmake --build build`: no warning. `build/drive/logs/build.log`.
- The host suite: 756 of 756. `build/drive/logs/test.log`.
- `cmake --preset asan`: 755 of 755. `build/drive/logs/asan-test.log`.
- `cmake --preset ubsan`: 755 of 755. `build/drive/logs/ubsan-test.log`.
- `python3 tools/docs-style/check_docs.py` on every file touched: no
  finding.

```text
$ git log --oneline -3
a594e76 Name the fallback of a provider for what it is
1d529d1 Test the standard interfaces and record their decisions
57dfd5d Build the five standard interfaces with their defaults

$ git status --short

$ git rev-parse HEAD origin/main
a594e76f98e38532623149431b0ce8b0d622e690
a594e76f98e38532623149431b0ce8b0d622e690
```

## Questions for Eddie

- `anti.random.Source` is the name the specification gives, and
  `docs/anti-syntax-overview.md` said `Random`. The overview now says
  `Source`, and `Random` stays the class that gives the draw.
- `SharedRandom` seeds itself from the wall clock, so the default source
  draws a new sequence per run and takes a mutex per draw. A program
  that wants one sequence again makes its own `Random`.
- `anti.config.Config` reads the settings of the program from a TOML
  file that `config.read` names. The specification names the interface
  and nothing else about it.
