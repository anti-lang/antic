# Tuple descriptors, array fields in JSON, and regex below the default level

The step builds Eddie's two answers to the questions of
`docs/reports/2026-09-26-descriptors-and-handles.md`, recorded in
`docs/decisions.md` and `docs/anti-language-additions.md`. It also answers
why `anti.regex` is refused on the macos-x86_64 run of the suite.

## What was done

- `0f8a333`: a tuple has the type id `Tuple` and a descriptor with one record
  per part. A tuple field of a class compares, hashes, serializes, reads back
  and reflects as a tuple part of a struct does. `serialize` writes a tuple as
  a JSON array of its parts. It writes an array field as a JSON array of its
  elements, through every level in order. `Object.deserialize` fails when a
  count differs. Tests: `std/serialize_tuples.anti`, the updated
  `std/serialize_null.anti` and `records_type_ids`.

## The refusal of `anti.regex` on macos-x86_64

The refusal is on purpose, and nothing about it changed.

- PCRE2 is present. `lib/macos-x86_64/libpcre2-8.a` and `libanti_rt_regex.a`
  stand in the runtime archive. `eq_handles` builds and links for
  macos-x86_64 at the default level. Run under Rosetta, the start-up check
  then refuses it: `this program needs a processor with AVX2`.
- The suite's macos-x86_64 programs compile with `--cpu v1`, since Rosetta has
  no AVX2. `docs/decisions.md` records that under "CPU levels", the entry that
  begins "The suite's macos-x86_64 programs".
- The native libraries stay at the default level of each target. A program
  below it that imports one is refused at link with `anti.regex is built for
  x86-64-v3, this program targets v1`. The settled entry that begins "The
  native libraries stay at the default level" gives the reason: a native
  library is large and is built once. The refusal states the limit rather
  than link instructions the machine cannot run.
- The refusal stands in `native_inputs` of `src/antic/driver.c`, under its
  DESIGN comment, and `errors/pattern_level.anti` holds it. The pattern
  programs and `eq_handles` leave the macos-x86_64 run by the filter under the
  DESIGN comment of `rosetta_sources` in `tests/CMakeLists.txt`.

Regular expressions therefore work on every target at its default level. The
suite cannot run such a program for macos-x86_64 on this Mac. A real
macos-x86_64 machine with AVX2, or the release runner `macos-15-intel`, can.

## Proof

- Host: 1145 of 1145 passed at `0f8a333`, in `build/scratch/suite14.log`.
- ASan: 1144 of 1144 passed at `0f8a333`, in `build/scratch/asan-suite.log`.
- UBSan: 1144 of 1144 passed at `0f8a333`, in `build/scratch/ubsan-suite.log`.
- The digest of `return42` follows the new records. So does the manifest of
  `emit_identity` for `for_patterns` and `owning_values`, which hold tuple
  fields.
- The docs-style checker reports nothing on every file the step touched.

## Questions for Eddie

- An array of more than one level is written as one flat JSON array. The
  record carries the count of all elements and not the length of each level.
  Should the record carry each length so the text nests?
