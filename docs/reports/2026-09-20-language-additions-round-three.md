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

## Three readings, all answered by Eddie in this session

1. The errors entry of `docs/decisions.md` held the old sentence, "A function
   that can fail returns `*Error`, `none` on success, and writes its results
   through out pointers", in a planning-era entry that also said `anti.rt.Error`
   and `rt.check`. It predates `may fail`, `anti.lang` and the `none` rename.
   Eddie said to correct it. The entry now says the standard library uses
   `may fail` and points at "Failing functions" in the additions document for
   the rule. It keeps the out-pointer convention as the ABI and names
   `anti.error.Error` and `error.check`. `error.check` is what the standard
   library ships, in `std/anti/error.anti`, and what `tests/std/error.anti`
   calls.
2. `fn construct(self, args...) -> ?*Error` keeps the hand-written spelling in
   both specifications and in the overview's examples. Eddie confirmed it
   becomes `fn construct(self, args...) may fail` when the standard library's
   signatures are rewritten, in that same session, with both specifications and
   the overview changing with it. Until then the hand-written spelling stands.
3. Two judgement calls, both confirmed. The additions document's
   nullable-pointers section never carried the out-pointer sentence, so the
   addendum's instruction to replace it there had nothing to act on. A one-line
   pointer to "Failing functions" was added at the end of that section instead.
   `for i, x in &slice`, which binds a pointer, was in the small things
   and is not in the addendum's tuples wording. It is carried into the tuples
   section as destructuring of an `(int, *T)`, so nothing was lost.
