# Round three of the language additions, applied

Docs only. `docs/anti-language-additions-3.md` is folded into
`docs/anti-language-additions.md` and deleted. Nothing in it is built, and every
section that entered the syntax overview says so.

## What changed

`docs/anti-language-additions.md` gained the eight sections the addendum names:
failing functions with `may fail`, tuples, error origin and stack traces, source
locations with `here`, the symbols tooling, the CPU levels, the simd structs and
the round-three small items. They sit by subject: the four error and location
sections after the nullable pointers, the CPU levels and the simd structs after
`Flags`, the symbols tooling after the debug information, the small items after
the small things.

The four line changes. The timing takes the CPU levels and `none` before the
first release and the `may fail` group after it. The `Flags` pair is a tuple
destructuring, and the sentence that said there is no pair type is gone. The
`for i, x` special form left the small things for the tuples section. The
keywords took `fail`, `here`, `fallthrough`, `may fail`, `simd` and the string
prefixes `rf` and `x`. The eleven messages joined the messages section.

`docs/anti-object-model.md`: `Error` gained `pub at: SourceLocation` and
`own frames: ?*StackTrace`, `e.text()` now names the origin and the trace, and
the failing-function sentence refers to "Failing functions" instead of spelling
out the out-pointer convention. `null` and `anti.error.NullPointer` were already
`none` and `anti.error.NoneDereference`, so that part of the addendum was
already in place.

`docs/anti-syntax-overview.md`: new sections for tuples, simd structs and source
locations, a rewritten errors section on `may fail` and `fail`, the literal
prefixes `rf` and `x`, `fallthrough` in the statements, `f16` and tuples in the
types, and the CPU baselines and the symbols archives under checks and
debugging. The reserved words carry the new keywords, `may fail`, `simd` and the
prefix list.

The docs-style checker passes on all three files. No code changed, so no suite
ran, which the rule for a docs-only commit allows.

## One reading to confirm

The addendum's tuples section says the two special forms are "removed from the
additions document and described here instead". `for i, x in &slice`, which
binds a pointer, was in the small things and is not in the addendum's wording.
It is carried into the tuples section as destructuring of an `(int, *T)`, so
nothing was lost. Say if that is wrong.

## Questions

1. `docs/decisions.md` line 305 still holds the old sentence, "A function that
   can fail returns `*Error`, `none` on success, and writes its results through
   out pointers", inside a planning-era entry that also says `anti.rt.Error` and
   `rt.check`. The addendum says to replace that sentence "wherever it appears",
   but the session was scoped to the additions document, the object model and
   the syntax overview, and decisions.md is the first authority. It was left
   alone. Should that entry be corrected, or does the header line, which says
   the object model and the additions document are not repeated there, already
   settle it?
2. `fn construct(self, args...) -> ?*Error` keeps the hand-written spelling in
   both specifications and in the overview's examples. The addendum leaves the
   rewrite of the standard library's signatures to a later session and says the
   hand-written form stays legal, so nothing was changed. Confirm that
   `construct` becomes `may fail` in that rewrite rather than staying as it is.
3. The additions document's nullable-pointers section never carried the
   out-pointer sentence, so the addendum's instruction to replace it there had
   nothing to act on. A one-line pointer to "Failing functions" was added at the
   end of that section instead.
