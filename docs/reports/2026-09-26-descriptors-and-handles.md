# Descriptors of variants and optional values, handle fields, array equality

The step builds Eddie's answers to the questions of
`docs/reports/2026-09-26-bare-calls-and-inline-fields.md`. Each answer is
recorded in `docs/decisions.md` and `docs/anti-language-additions.md`. R15 is
settled without its tag, and item 25 has left "First sessions" in
`CLAUDE.md`.

## What was done

- `952df12`: `a == b` on two arrays of one type compares element by element,
  and an array meets `eq` of a constraint when its elements do. Test:
  `programs/eq_arrays.anti`.
- `4d4e8f9`: a variant has a descriptor with its tag and each case's fields,
  and a `?T` of a value has one of its own. `==`, hash, `serialize`,
  `deserialize` and `reflect` handle a field of either. `serialize` writes
  `{"Circle":{"r":2}}` and a `?T` as its value or `null`. Test:
  `std/serialize_variants.anti` saves a class with a variant field and two
  `?T` fields and reads each back equal, with the same hash.
- `c4e64e5`: locks stay out of `==` and hash. A channel and an `own fn`
  compare by identity, and a Regex by its pattern text and mode. A Match gets
  no default `==`. Test: `programs/eq_handles.anti` and
  `errors/equality.anti`.

## What failed and how it was fixed

- A shared helper for field records emitted the descriptor global before the
  name literal. The unit test that pins the IR of two modules caught the new
  order, and the helper now looks the descriptor up after the name.
- PCRE2 keeps no copy of a pattern's text. The handle of a compiled pattern is
  now `struct anti_pattern`, holding the code, the text, its length and its
  mode. `regex_compile` and `pattern_literals` passed the handle to PCRE2
  themselves and crashed until they read the code from its first word.
- A tuple field of a class has the id of a struct and no descriptor. The
  runtime called it unequal until it was passed over again, as it was before.
- A `?Match` field would have compared by its flag alone. The refusal of a
  Match looks through a `?T`, an array and a case of a variant.
- `eq_handles` holds `anti.regex`, so it runs on the host target alone, as the
  other programs of patterns do.

## Proof

- Host: 1144 of 1144 passed at `c4e64e5`, in `build/scratch/suite13.log`.
- ASan: 1143 of 1143 passed at `c4e64e5`, in `build/scratch/asan-suite.log`.
- UBSan: 1143 of 1143 passed at `c4e64e5`, in `build/scratch/ubsan-suite.log`.
- The pins that changed are `records_type_ids`, the digest of `return42` and
  the manifest of `emit_identity` for the programs that hold such fields.
- The docs-style checker reports nothing on every file the step touched.

## Questions for Eddie

- A tuple field of a class is passed over by the runtime, since a tuple has no
  descriptor. A struct's default compares a tuple part. Should a tuple get a
  descriptor too?
- `serialize` still writes an array field as `null`. Now that the record
  carries the count, should it write the elements?
