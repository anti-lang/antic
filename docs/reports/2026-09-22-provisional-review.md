# Provisional entries for review

`docs/decisions.md` holds 3 `[provisional]` entries at the commit of this report. The review of 2026-09-22 answered the other 168 of the 171 it started with.

| Group | Entries | Answered | Still provisional |
|---|---|---|---|
| A-review | 38 | 35 | 3: R2, R10 and R23 |
| A-settled | 108 | 108 | 0 |
| B, internal | 23 | 23 | 0 |
| Resolved before the split | 2 | 2 | 0 |

The two resolved before the split are the rule for the error a handler binds, corrected in `22f2927`, and the refusal of `f"..."`, removed in `0a81962`. The numbers R, S and B are those of the second version of this report, and no longer stand in `docs/decisions.md`.

## Still provisional

**R2**, line 62, "Core language". Open until item 22 of the first sessions confirms that `for i in 0..10 by -3` gives `9 6 3 0`, or fixes the code that gives `7 4 1`.

> `by -k` walks the values of `by k` in reverse, and it starts at the largest of them rather than at the high bound. Reason: the work order gives both a rule and a procedure. The rule is that the two forms walk the same values, and the procedure starts at `hi` and decrements. The two agree only when `k` divides the span, and `for i in 0..10 by -3` gives `7 4 1` under the procedure and `9 6 3 0` under the rule. The rule is what a program relies on, so it wins. Lowering computes the first value as `low + ((high - low - 1) / k) * k`, one division by a constant before the loop.

**R10**, line 107, "Object model". Deferred until `anti.mem` and the form of `Object.deserialize` with an `Allocator` exist.

> `Object.deserialize` gives a `str` field bytes of its own on the heap, which nothing frees. Until `anti.mem` exists they come from libc. The form with an `Allocator` replaces this one. `Object.deserialize` then takes an `Allocator`, and every string and every owned sub-object it creates comes from it. The caller frees that memory at once when the object's life ends. It is the first real use of injected allocation. Reason: `own` is refused on a `str`, so no `destruct` could free them, and the input text belongs to the caller. Deferred: it waits for `anti.mem` and the form with an `Allocator`.

**R23**, line 321, "Standard library phase". Deferred until the code renames `anti.rt.Object` to `anti.lang.Object`.

> The compiler and the runtime still name the root class `anti.rt.Object`, with the symbols `anti_rt_Object_*`, and `Job` stays under `anti.rt` too. The documents name both under `anti.lang`, and the rename in the code follows later. Reason: the step that made `anti.lang` moved `Error` alone, and the rename changes the runtime and the symbol of every descriptor. Deferred: it waits for the rename of `anti.rt.Object` to `anti.lang.Object` in the code.

## How the others were answered

- A-review, 35 entries: the tags came off in `765dd01`. R14 lost a false reason in `75ad1fe` first.
- A-settled and B, 131 entries: 127 tags came off together, after this report was last generated. The other four came off in `93688c5`, when `may fail` became the one failing form: the entries on `undo`, on `may fail` and an `extern fn`, and the two tests `std_may_fail_only` and `tests_may_fail_only`, which were reworded to that rule.
- S2, the entry on `Error.new`, lost its tag with A-settled and is superseded rather than settled. Item 21 of the first sessions keys failing on `may fail` alone and drops the entry with the `makes_error` exemption.

## Checks left for a code session

The first sessions of `CLAUDE.md` carry the code that the answers need, each with a test.

- Item 20 deletes the error a handler binds on every exit of the handler.
- Item 21 keys failing on `may fail` alone and drops `makes_error` and S2.
- Item 22 checks `by -k`, which decides R2.
- Item 23 checks that a `may fail` function returning a tuple takes one out pointer.
- Item 24 checks that the default `serialize` still writes `null` where `Object.deserialize` keeps a default.
