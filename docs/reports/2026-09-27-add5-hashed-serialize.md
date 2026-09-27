# The hashed maps and sets, the order of serialize

The second session, `docs/reports/2026-09-27-add5-hashed.md`, stopped on one
point: `serialize` of a `HashMap` could not sort a key of a struct, a class or
a variant that has `lt`. Eddie decided that `serialize` sorts the entries by
the bytes of the serialized text of each key, not by `lt`. This session builds
that and finishes the step.

## What was done

- `serialize` of `HashMap` writes the text of every key and value into one
  builder, sorts the entries by the bytes of the key text and writes the texts
  in that order. `serialize` of `HashSet` does the same with its elements.
  `9`, `10` and `-3` now stand as `-3`, `10`, `9`.
- Where two keys write the same text, the text of the value decides, so the
  output never depends on the slots. This is `[provisional]`.
- The sort by the kind of the key is gone: `ordered`, `before`, `sorted` and
  the fifteen type ids they read. `kind_of` stays for the id of `str`, which
  chooses between a JSON object and an array of pairs.
- `std_hash_map` checks numbers, structs with and without `lt`, and seven keys
  that differ in a pointer alone and write the same text. `std_hash_set`
  checks numbers. The two expected lines of `int` and `u64` keys changed to
  the order of the text.
- `docs/decisions-hashed.md` records the decision under "Built", the tie rule
  as provisional, and moves the blocked point to "Resolved on main".

## What failed and how it was fixed

- `[int; 6]` is not the array syntax of Anti. The test writes `[6]int`.
- `fmt_canonical` and `anti_check` found four comments wrapped short or long.
  `anti fmt` rewrapped them.
- The expected line of the scores had `{"value":1}` before `{"value":10}`. The
  byte `0` comes before `}`, so the program was right and the line was wrong.

## Provisional decisions

- Two keys of a `HashMap` that write the same text are ordered by the text of
  their values.

## Proof

- Host: 1170 of 1170, `build/drive/logs/h3-suite-host.log`.
- ASan: 1169 of 1169, `build/drive/logs/h3-suite-asan.log`.
- UBSan: 1169 of 1169, `build/drive/logs/h3-suite-ubsan.log`.
- No warning in the three build logs.
- The docs-style checker reports nothing on the two `.md` files touched.
