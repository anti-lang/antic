# Pattern literals, Regex and anti.regex

The step built "Patterns", "Failures, by where a pattern comes from" and "Thread
safety" of "Regular expressions" in `docs/anti-language-additions.md`, over the PCRE2
of `src/native/`. It goes as far as those parts reach without the methods of `str`.

## What was done

- `re"..."` lexes as a raw pattern literal with hash delimiters, of type `Regex`, a
  struct of `anti.lang` that the compiler declares.
- antic links PCRE2, compiled from the pinned source for its host. A malformed literal
  is an error at the byte PCRE2 names. Nested repeats over text that overlaps fail the
  safety check `exponential-pattern`, which `unchecked` overrules.
- Each module compiles its literals once, before `main`, in a constructor that calls
  `anti_rt_regex_literal`. The runtime checks the processor first.
- `Regex.compile(text)` calls `compile` of the new module `anti.regex`, which fails with
  `BadPattern` and declares `Error`, `TooExpensive` and `MissingGroup`.
- Text mode is UTF without `PCRE2_UCP`: `\d`, `\w` and `\s` are ASCII, and `(*UCP)`
  changes them. The inline flags are PCRE2's own.
- The glue `src/rt/regex.c` and `src/rt/patterns.c` is `lib/<target>/libanti_rt_regex.a`
  beside `libpcre2-8.a`. A program or library for C that holds `anti.regex` links both. A
  program below the default level is refused with `anti.regex is built for x86-64-v3, this
  program targets v1`.
- `anti check` lost its class of patterns, and the packer compiles PCRE2 into `antic` and
  `anti`. The library format is 58.
- Tests: `programs/regex_compile.anti`, `programs/pattern_literals.anti` in release and
  dev mode, and the listings `pattern_malformed`, `pattern_exponential`,
  `pattern_import` and `pattern_level`.

## What failed and how it was fixed

- The first full run failed 18 tests, logged in `build/drive/logs/ctest1.log`. The
  library format rose, so `tests/modules/scale.antl.hex` and the constants of
  `tests/unit/test_modules.c` took version 58. `anti fmt` rewrapped `regex.anti`.
  `anti_check` counted 23 modules and no line of patterns.
- `link_identity_macos-arm64` failed because the glue sat in `libanti_rt.a`, which the
  build id digests. The permission layer refused a new digest for the pin. The glue moved
  into a library of its own, and the pin passes unchanged.
- The same move fixed the three `clib_bundle` tests. A bundled runtime had joined the
  glue and left PCRE2 unresolved.
- `package_keys` failed because the packer compiled `src/rt/regex.c` without `pcre2.h`.
  `src/native/pcre2-files.cmake` now holds the list the packer compiles.
- The programs of patterns failed under Rosetta, where `--cpu v1` is refused by the
  decided rule. They run on the host target alone, as a DESIGN comment of
  `tests/CMakeLists.txt` records.
- `emit_identity` lacked the two new programs. `-DWRITE=yes` on the Mac added twelve
  lines and changed no existing one.

Logs: `build/drive/logs/ctest_final.log`, `asan_test2.log`, `ubsan_test2.log`,
`build_final.log`.

## Provisional entries added

All stand under "Regular expressions" in `docs/decisions.md`: `Regex` as a struct of
`anti.lang`, the import of `anti.regex`, `Regex.compile` as a call of `compile`, the
fields of `BadPattern`, the rule of `exponential-pattern`, no constant pattern, the
constructor `patterns.start`, the library `anti_rt_regex`, PCRE2 compiled into antic,
no freeing of a `Regex`, and the programs of patterns off Rosetta.

## Questions for Eddie

- `anti_licenses` names no PCRE2, and `docs/distribution.md` says a program that imports
  `anti.regex` owes the attribution there. The line needs the version of PCRE2, which
  `tools/pcre2-pin` alone may spell. Where should antic read it?
- The link line of `antic --lib static` names no PCRE2. A plugin with a pattern calls the
  glue of its host. Should a plugin link the glue itself?
- The docs-style checker still reports findings on lines this step did not write, in
  `CMakeLists.txt`, `tests/CMakeLists.txt` and eight C files. It reads a CMake comment as
  a Markdown heading and the code after it as its sentence. `tests/CMakeLists.txt` went
  from 245 to 248 errors with the new comments of its tests. Every new file, every
  Markdown file and every C comment of the step passes, and no C file gained a finding.

## Proof

The three suites at `c208bc7`, the last commit that changed code:

```text
host:  100% tests passed out of 959
asan:  100% tests passed out of 958
ubsan: 100% tests passed out of 958
```

The state after the push of the report, before this proof was added:

```text
$ git log --oneline -3
8cfabc1 Report the pattern literals, Regex and anti.regex
c208bc7 Fold the PCRE2 comment of the build and record the state of patterns
16da9c2 Pin the assembly of the pattern programs and format their tests
$ git status --short
(no output)
$ git rev-parse HEAD origin/main
8cfabc14ed4764119114d5cb278b7a97a07f8d06
8cfabc14ed4764119114d5cb278b7a97a07f8d06
```
