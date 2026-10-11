# The read-through of the developer's guide

The last step of the documentation driver for the guide, on 2026-10-11, on
the branch `docs`. It read `docs/site/developers-guide/` from the index to
chapter 21 and fixed what a reader would stumble on. It changed no code
under `src/` and no file under `tests/`. The task text gives the step no
id, and `guide-final` names the report of chapters 19 to 21, so this report
takes the name `guide-read`.

## How the guide was read

The task asks for three readings, one each as a programmer from Java, from
C and from C++. This session read every line of the 22 pages once, in
order, and asked at each section what each of the three readers would
stumble on. It did not read the 25,700 lines three times. Four scripts
under `build/scratch-guide/` covered what a second and a third reading
look for.

- `structure.py` checks each feature section for its five parts in order
  and for an excerpt. It checks that "Where it lives:" ends its part and
  that "Pitfalls" names all three languages.
- `links.py` checks that every link names a page and a heading of it.
- A script lists the first use of 54 terms before the chapter that
  explains them, with the sentence around it.
- `long.py` lists every sentence above the limit of `check-web-content`.

## What changed

- A term used before its chapter now has one sentence or a pointer at its
  first use. Chapter 1 says what a library file, a processor level, LTO
  and the symbols archive are. Chapter 2 names musl as the C library.
  Chapter 3 explains `f"..."` with `free(line.ptr)`, which its second
  program uses four sections before "Text and bytes". Chapter 4 explains
  `may fail`, `try` and `catch` for the section on `defer`, and what an
  `extern fn` is. Chapter 18 says what a `//#` note is.
- Chapter 1 gains one paragraph and one example on modules, `pub` and the
  two other forms of `import`, since every chapter imports.
- "Flags and carry" was a sixth heading in a section of five parts in
  chapter 3. Its text stays under "The feature".
- "Plugins" of chapter 15 was the one feature section without an excerpt.
  It now shows the call that `lib.instance(log.Logger)` becomes.
- Two pairs of sections said the same thing. Chapter 4 and chapter 8 both
  showed the IR of a `switch` on a variant, and chapter 8 keeps it.
  Chapter 4 and chapter 16 both described the `Watch` of an iterator, and
  chapter 16 keeps it.
- The table of dev checks in chapter 6 read as the whole list and held
  six. One paragraph names the table of chapter 21 that holds every check.
- 174 references to another chapter were plain text, and the chapter list
  of the index had no link. Each is a `relref` link now, in the form
  chapter 21 uses. `links.py` finds 308 links and none broken.
- 63 prose sentences above 26 words are split. Announced counts, openings
  that repeat a heading and two uses of "stands as" are gone.
- The weights were 10 times the chapter number already, the index has 1,
  and every page keeps `draft: true`.

## Examples added

- Chapter 1: a block with `import anti.io.{println};` and
  `import anti.text as t;`.
- Chapter 15: the host block is named `host`, and an `ir` excerpt of the
  dev build shows the descriptor of the interface as the argument.

## What the guide does not cover

The index holds this list under "Subjects outside the guide", with the
section of `docs/anti-syntax-overview.md` for each. The task lists 21
chapters, and none of them is about these subjects.

- Modules, `import` and visibility as a chapter. One paragraph of chapter
  1 stands in.
- Regular expressions: `re"..."`, `Regex`, `ByteRegex` and `patch`.
- Warnings, `allow` and `unchecked` as a subject. The sections that meet
  one explain it there.
- The modules of the standard library that no chapter uses, `anti.json`,
  `anti.args` and `anti.os` among them.
- `target.os`, wire formats and named arguments, which are not built.
  Chapter 20 states the same of script mode.

## What was wrong in antic

- `let x = try e;` on an optional compiles. The checker narrows `x` to the
  value, and lowering writes no exit: on `none` it makes a
  `NoneDereference`, deletes it and goes on. A `?*int` then faults at
  `*x`, and a `?int` reads a stale value. The host example of chapter 15
  used the form on `lib.instance`. It now uses `let ... else` with `fail`,
  and chapter 7 states what `try` does there.

## What was wrong in the guide

- Chapter 3 sent the reader to chapter 8 for arrays. Chapter 12 holds
  them.
- Chapter 3 said the pattern literal "stands with the regular expressions
  of the standard library", and no page of the guide holds them.
- No page named `docs/anti-syntax-overview.md`. The index and two
  chapters now do.

## Answers from the code

- `src/antic/llvm_emit.c` writes no `switch` instruction, in either mode.
- `lib.instance(Interface)` is a dispatch through entry 17 of the table of
  `Library`, `instance_at`, which calls `anti_rt_plugin_instance`.
- `antic --print-targets` prints the `msvc` triples chapter 20 shows.
- The docs-style checker counts two words more for a `relref` link, and
  `check-web-content` counts none.

## Open points for Eddie

- The guide was read once and not three times, as the first section
  states.
- `check-web-content` still reports 210 errors beside the 174 on the
  three part headings the task fixes. 169 are `[length]`. The checker
  joins a sentence that starts with a code span or a link to the one
  before it, and `long.py` finds no prose sentence above 26 words. 28 of
  the 31 `[colon-reveal]` errors are "Where it lives:", which the task
  fixes too.
- The 308 `relref` links name sibling pages. Hugo reads a directory with
  `index.md` as a leaf bundle, the open point of `guide-plan`.

## Gates

The host suite passed 1702 of 1702 tests in 358 s with `ctest --preset
host -j14`, two of them skipped as on every Mac. `guide_examples` passed
in 231 s. The sanitizer suites did not run, since the step changed no file
under `tests/`. The docs-style checker reports nothing on the 22 pages and
on this report. `git grep` finds no marker of unfinished text in the pages.
Its one hit for the letters of such a marker is the file name
`libSystem.tbd`. This is the state before the commit of this report, with
the report itself as the one untracked file:

```text
$ git log --oneline -3
12a77f76 Name the chapter of three forms at their first use in the guide
44b8e864 Link the chapter references of the guide and its index
c4b086d8 Split four sentences of chapter 20 of the guide and drop two counts
$ git status --short
?? docs/reports/2026-10-11-docs-guide-read.md
$ git rev-parse HEAD origin/docs
12a77f76fa82e8f9150a8bd20ccc2635d28365c1
12a77f76fa82e8f9150a8bd20ccc2635d28365c1
```
