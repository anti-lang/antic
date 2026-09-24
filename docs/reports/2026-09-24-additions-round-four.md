# Round four of the additions, applied

Documents only. Nothing was built.

## What changed

- `docs/anti-language-additions.md` holds the eight sections of round four
  before "Messages", with their entries in the contents: regular expressions,
  bytes, the language hooks, iteration, anonymous functions and closures,
  concurrent classes, nested types, and errors, warnings and checks. The
  "What to change" list is applied: `catch none` under "Failing functions",
  `re` in the prefix table, the contextual words under "Keywords", and the
  compiler's own check of pattern literals in place of the `anti check` rule.
  The date line names round four, and "Timing" says none of it is work until
  Eddie names it. The Mutex bullet points at "Locks", and the classes of a
  wire format stay at module level with the reason.
- `docs/decisions.md` rewrites the two reversed entries with the date and the
  reason: regular expressions are part of the language, and a class body may
  declare a `struct`, `enum` or `class`. The string literals, the string
  prefixes and the pattern check of `anti check` follow.
- `docs/anti-object-model.md`: nested types and the two thread-safe class
  forms under "Class declaration", the language hooks under "Operators" with
  `[]` no longer refused, atomic locals, the worker exception, `catch none`,
  and "Not in the language" without nested classes and closures.
- `docs/tooling-addendum.md`: the pattern check of `anti check` is replaced.
  One line of its block-form rules now reads "more than one line", which the
  docs-style checker asked for.
- `docs/anti-syntax-overview.md`: new sections for anonymous functions and
  closures, concurrent classes, warnings and safety checks, regular
  expressions and bytes, and paragraphs for iteration, `catch none`, nested
  types and the hooks. Every round-four feature stands in a "Not built yet"
  line, and every new example is an `anti not-built` block.
- `docs/anti-language-additions-4.md` is deleted.

No `[provisional]` entry was added. Nothing failed.

## Gates

Logs are in `build/drive/logs/`: `r4-host-build.log`, `r4-host-test.log`,
`r4-asan-*.log`, `r4-ubsan-*.log` and `style-all.log`.

- Host build: no warnings. Host suite: 100% tests passed out of 841.
- ASan: 100% tests passed out of 840. UBSan: 100% tests passed out of 840.
- The docs-style checker: 0 errors and 0 warnings on every touched file.

State before this report's commit:

```text
$ git log --oneline -3
9209567 Fold round four of the additions into the specifications
e9f9097 Report the second merge of the fix lanes
1325be0 Take the test counts of the merged step 20 and 21 lanes
$ git status --short
$ git rev-parse HEAD origin/main
92095670e47a873f3db86e4854e02b1da07cc3e5
e9f909719615ffd0379f447dbdb42ba61d27f233
```

The report commit and the push follow. The reply of the session gives the
state after them.
