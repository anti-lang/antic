# The syntax overview updated after its section plan

Date: 2026-10-12. Branch `docs`, on `9b8cf68b`. The plan is "Section plan"
of `docs/reports/2026-10-12-docs-overview-gaps.md`. This step changed
`docs/anti-syntax-overview.md` and wrote this report. It changed no code
and no test.

## State of the gates

- `overview_examples` passes: 73 blocks compiled, 5 programs run, 9 skipped
  as not built. Before the step it held 55, 1 and 9.
- The docs-style checker reports nothing on the overview and on this report.
- `git grep` finds none of the three marker words of the gates in either.
- The host suite does not pass. `ctest --preset host -j14` ran 3508 tests in
  406 s: 3507 passed and `fmt_canonical` failed.

`fmt_canonical` names 52 sources under `tests/` that are not in the form of
`anti fmt`. The commits `17d39076` and `c1272025` of the modules area added
them, and this branch holds both. `origin/main` holds `0b7e207c`, "Write the
sources of the modules area in the form of anti fmt", which this branch does
not hold. Its 52 `.anti` files are the 52 the test names. Their copies from
`origin/main` pass `anti fmt --check` with the `anti` of this tree. `src/`
is the same on both. The step merges nothing, so the failure stands until
the driver merges `origin/main`.

## What the step wrote

39 commits, one per section and two for the head of the page. 35 sections
changed, two are new, and three moved into one of the new ones.

- New: "Runtime configuration" after "Plugins", with the three places that
  name the file, the six keys of `src/rt/conf.c`, and the output of
  `--anti.inspect`.
- New: "Features not built" before "Reserved words". It holds compile-time
  targets, wire formats and script mode as three subsections.
- Tables of the language: the doc comments, the format specification, the
  conversions of `as` and the names the checker knows.
- Tables of the library and the tools: the operations of every collection,
  the members of the standard classes, and every option of `anti` and of
  `antic`.
- The rules of decisions 17 and 19 to 27 stand in their sections as rules.
  Each section names in its "Not built yet" sentence what antic lacks.
- "Reserved words": the keywords, the type names and the contextual words
  equal the tables of `src/antic/lexer.c`, 100 words, and stay as they were.

The task names a section "Later". The rewrite of 2026-10-11 took it out, and
"Features not built" has its role.

## Examples added

Four programs with their output: a format specification of each kind, a
class with its base, a trace handler that counts objects, and `rt.get`.
The other new blocks compile through the front end:

- the doc comments, the constants of every kind, the conversions with
  `is`, `as` and `as?`, and the flags form
- a `switch` on a `str` and on an integer, `t.1.0`, a class below `Error`
  with `catch none` and `Error.wrap`
- `align(64)`, the members of an enum body, `use` with a generic interface,
  the type alias
- `transient` with `dup` and a move, a chain of `?.`, an operator per struct
  with the index hooks, `swap` and `compare_swap`
- `sync a, b`, `compatible`, `supports` and `unload`, `setup` and `teardown`
- `export` on a class, an enum and a variant, with a variadic call
- a function whose index a dev build stops at, with the message it prints

Two `sh` blocks hold commands that were run here: five of `anti` on a
starter project, and two that choose a processor level.

## Where the overview and the code disagreed

The code won in each case. The first six are those of the gaps report.

- `a += b` on a type with `operator fn add` is refused. The page said the
  compound assignments derive.
- `?.` gives a pointer or a function value. The page listed it for every
  `?T`.
- An integer literal never becomes a float.
- `anti fmt` leaves a one-line item on its line.
- No global exists that a closure could be stored in.
- A tuple of four elements compiles. The sentence on it read as a rule.
- `lib.instance(Interface)` gives a `?*Interface`. The page listed it among
  the functions that may fail.
- `nanoseconds` to `between` of `anti.time` are functions of the module. The
  page wrote them as members of `Duration`.
- `Document.open` and `FileConfig.read` may fail, and the page lacked both.
- The example of the patterns dropped the slice of `to_slice()` and the
  text of `replace`, beside the four frees the gaps report names.

## Wrong in an older document

- `docs/reports/2026-10-12-docs-overview-gaps.md` lists `--sdk`,
  `--show-sdk-path` and `--show-sdk-version` as options of `antic`. They are
  arguments that `src/antic/driver_link.c` passes to `xcrun`.
- The same report leaves ten sections as they are. Three of them changed
  for an example: "Hooks and tracing", "Processor levels" and "Regular
  expressions".

## Defects without a finding

- `anti test` fails on a module that defines `main` and imports a module.
  The link reports `undefined symbol: _anti.io.println`. The starter project
  of `anti new` is such a module, so the example of "Tools" leaves the
  command out.
- The gaps report names one more: a qualified body cannot name a generic
  interface.

## Questions answered from the code alone

- Does `p as *T` stop a release build on a mismatch? Yes. `checked_cast` of
  `src/antic/lower_expr.c` calls the runtime in every mode.
- What does `rt.configure` do with a file it cannot read? It ends the
  program with status 70 and a message, probe.
- Does an arm of a `switch` take two values? No, probe.
- Does `return T(args) catch fatal;` parse? No, the parser expects `;`
  after the call, probe. The example binds the value with `let` first.
- What does `add_child` of a `Tree` give? A `?Handle`, probe.
- Is the closing section named "Later"? No. The plan names it, and the plan
  is the document the task follows.

## Questions for Eddie

- Does a step whose one failure is fixed on `origin/main` count as done once
  the driver has merged?
