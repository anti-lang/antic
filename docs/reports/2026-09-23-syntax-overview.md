# The syntax overview against the built language

`docs/anti-syntax-overview.md` now matches what is built, and the test
`overview_examples` compiles every `anti` block of it through the front end
of antic.

## What changed

1. `tests/run_overview_examples.cmake` and the test `overview_examples`. A
   block splits into items, which stand at module level, and statements,
   which go into the body of one function. A hidden block in an HTML comment
   before an example declares the names it uses. A block that opens with
   `anti not-built` is skipped. The first run failed 32 of 36 blocks.
2. The examples the test found broken:
   - A `switch` arm names one value, and an enum value names its type,
     `Kind.Circle`. The overview had `Square, Rect =>`.
   - `return parse(f);` in a `may fail` function is a bare failing call.
     It is now `return try parse(f);`.
   - `?.` on a field that is no pointer is refused, so `node?.next?.value`
     became `node?.next?.next`.
   - `Error` and `SourceLocation` are named `lang.Error` and
     `lang.SourceLocation`, as every test program names them.
   - The reflection example used `fields(d)` and `get(obj, f)`, which
     do not exist. It now uses `describe`, `field_count`, `field`,
     `get(object, d, index)`, `set`, `call(object, index, args)` and `new`.
   - A trace handler fills all nine hooks, since `TraceHandler` declares
     them abstract.
   - `parallel` takes a slice, an `extern fn` takes `?*byte`, and `...`
     placeholders in a class body and a worker became code.
   - Labels, named arguments, `undefined`, `show`, `unreachable`, the
     target `switch`, wire formats and script mode stand in blocks marked
     `anti not-built`, and each status line names the feature.
3. The prose. Atomic operations lost `xor`, which the checker never had.
   `join_all`, `?fn`, doc comments and the built-in types are named. The
   reserved words follow `src/lexer.c`: `in` is contextual, `join_all` is a
   keyword, and `show`, `unreachable`, `undefined` and `embed` wait. New
   sections name the built standard library modules and the native-library
   modules that wait, and say that generics and closures have no syntax yet,
   so no example of either stands in the overview.
4. `docs/anti-object-model.md`. `try` was "allowed only in a function that
   returns `?*Error`", the form before `may fail`. The `anti.reflect` line
   gave the signatures of before the type ids. Both now say what is built.

## What failed and how it was fixed

- The docs-style checker read `<!--` as an exclamation mark and the code
  inside the HTML comments as prose. The skip mark became the fence tag
  `anti not-built`. The hidden context sits in a fenced block inside the
  comment, and the first line of the comment carries `docs-style:ignore`.
- A wrapper that may fail warned "may fail and never does" on a `try`
  block. The wrapper now may fail only for a forwarding `try` or a `fail`.

## Found and not changed

- The parser takes the members of a class body in one order: fields, then
  constants, static fields and functions. A field after a `static`, a
  `const` or a `fn` gives "expected `fn`". "Class declaration" in
  `docs/anti-object-model.md` says the order is free by grammar. That is
  work on the parser. The overview example puts the field first, which both
  readings accept.
- `docs/anti-language-additions-4.md` is a draft that no session applies
  yet, so the overview leaves it out.

No `[provisional]` entry was added.

## Gates

Logs in `build/drive/logs/`: `build.log`, `test.log`, `asan-build.log`,
`asan-test.log`, `ubsan-build.log`, `ubsan-test.log`, `overview.log`.

- Build: zero warnings on the host and in both sanitizer builds.
- Host: `100% tests passed out of 783`.
- ASan: `100% tests passed out of 782`.
- UBSan: `100% tests passed out of 782`.
- `check_docs.py` on `docs/anti-syntax-overview.md`,
  `docs/anti-object-model.md`, `CLAUDE.md` and this report: no finding.

State after the push of the work, before this report's commit:

```text
$ git log --oneline -3
bcd0e29 Word the comments of the overview test and count it in the state
560f5b2 Bring the syntax overview in line with the built language
d141a0e Compile every example of the syntax overview
$ git status --short
?? docs/c-guidelines.md
?? drive-additions-4.sh
?? drive-audit.sh
?? drive-prep.sh
$ git rev-parse HEAD origin/main
bcd0e291da6ecd8f37ff4803c5be87c696a59ebd
bcd0e291da6ecd8f37ff4803c5be87c696a59ebd
```

The four untracked files belong to the driver of the session and stay
untracked.
