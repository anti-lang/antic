# The skeleton of the developer's guide

The step `guide-plan` of the documentation driver, on 2026-10-11, on the
branch `docs`. It builds the skeleton of `docs/site/developers-guide/` and
the test `guide_examples` that checks every page of it. An earlier attempt
of the step wrote the runner and the pages and stopped before its report.
This attempt read both against the task, changed what follows and ran the
gates again.

## What the step wrote

- `tests/runners/run_document_examples.cmake`, the runner of the syntax overview
  under a new name. It reads a list of documents and directories, so one
  runner serves `docs/anti-syntax-overview.md` and the guide. The overview
  keeps its test name and its counts.
- The test `guide_examples` in `tests/CMakeLists.txt`, over every page of
  `docs/site/developers-guide/`, with opt, `llvm-mc` and `llvm-objdump` of
  the pinned tools.
- `docs/site/developers-guide/index.md` and the 21 chapter pages, in the
  front matter form of `docs/site/build-tool/index.md`, with `draft: true`
  and a weight of ten times the chapter number. The pages hold 87 feature
  sections, each with the five part headings and one sentence per part.

## What this attempt changed

- A release `llvm` excerpt read `<output>.ll`, the text before opt. The
  task defines the excerpt as the module llc receives. The runner now
  prints `<output>.bc` with `opt -S` of the pinned tools and reads that.
- A `refused` block went through the front end alone. The whole-program
  pass refuses programs too, an interface without a provider among them.
  A block the front end accepts now goes through a release build.
- Sentences of the skeleton stated results nobody had read out of the
  code: sizes in bytes, checks that release mode drops, counts of calls.
  They now name the subject alone. Three of them were wrong, and the
  section on older documents lists them.

## The examples it added

Chapter 2 holds every mark once, under "Reading the excerpts". The program
`hello` has its `text output` block. A `refused` block names the error
"expected `int`, found `str`". An `llvm` excerpt shows `hello.twice` of the
dev build with its overflow check, and a second one `hello.main` of the
release build after opt. An `asm` excerpt shows the end of `hello.main` of
the release build. All three excerpts are for linux-x86_64.

## The proof of the marks

`build/scratch-guide/prove_marks.py` runs the runner over 19 copies of
chapter 2 and two pages of its own, each in `build/scratch-guide/proof/`.
Each log is `build/drive/logs/docs-guide-plan-proof-<name>.log`. The
intact copy passes, and so does a page whose block the whole-program pass
refuses. The other 19 fail with the message the table names.

| Copy | What the runner says |
|---|---|
| `output`, `output_extra` | the standard output differs |
| `error_text`, `later_pass_text` | fails with another error than the one named |
| `refused_compiles` | is marked `refused` and compiles |
| `refused_no_error` | names no error |
| `llvm_dev_line`, `llvm_dev_order` | the line does not stand where the excerpt puts it |
| `llvm_release_line`, `llvm_release_before_opt` | the same, in the module after opt |
| `asm_line`, `asm_adjacent` | the same, in the disassembly |
| `too_long` | has 21 lines, and an excerpt has at most 20 |
| `no_compile` | an example does not compile |
| `unknown_block`, `name_twice`, `name_not_first` | the name of the block |
| `bad_header`, `bad_target` | the header of the excerpt, and the target |

`llvm_release_before_opt` names `@hello.twice`, which stands in the text
before opt and not in the module after it.

## Answers from the code

- `--keep-llvm` keeps `<output>.ll`, the text before opt, in both modes.
  Dev mode runs no opt, so llc reads that text. Release mode keeps the
  module after `default<O3>` as `<output>.bc`, which llc reads. Under the
  default `--lto full` a release program writes no `.bc`. The source is
  `driver_compile_llvm` of `src/antic/driver.c` and `llvm_run` of
  `src/antic/llvm_run.c`, and the header of the runner records it.
- `-S` takes the path of `--lto none`, by `lto_mode` of `driver.c`. A
  release excerpt therefore shows the module of the block before any link.
- A whole program keeps every function internal, by `linkage` of
  `src/antic/llvm_emit.c`. A release excerpt shows a function only when
  `main` reaches it and opt has not inlined every call of it.
- `llvm-objdump -d` prints no relocations, so a call in an `asm` excerpt
  shows no name of its callee. The `llvm` excerpt names it.
- The test `raw_output` refuses a captured output of a tool outside its
  list. The runner calls opt as `tests/runners/run_direct_calls.cmake` does.
- Chapters 1, 2, 18, 20 and 21 describe tools and not features, so their
  sections carry one sentence and no five parts. Chapter 19 has one
  feature section, on the two blocks.

## What was wrong in older documents

- The usage text of `--keep-llvm` in `src/antic/main.c` and the comment in
  `src/antic/driver.h` say the option keeps `.ll` and `.bc`. Dev mode
  keeps no `.bc`, and neither does a release program under `--lto full`.
- `CLAUDE.md` says a dev build marks a copy of a generic link-once. The
  emitter writes `weak_odr`. The first skeleton named `linkonce_odr`.
- `CLAUDE.md` counts 1701 tests on the Mac and 1700 under each sanitizer.
  With `guide_examples` the counts are 1702 and 1701. The step left
  `CLAUDE.md` as it is, since its task names no change to it.
- The first skeleton listed `undefined`, which the overview marks as not
  built, and said a `str` has no terminator, against the overview.

## Open points for Eddie

- `check-web-content` of the web-content skill reports 174 findings. All
  stand on the part headings "What you know", "How antic builds it" and
  "What it costs", which are no noun phrases. The task fixes them.
- The pages stand beside an `index.md`, which Hugo reads as a leaf bundle.
  The chapter list carries no links, and `index.md` has the weight 1.
- With `-j14` the ASan suite ran in 496 s, the UBSan suite in 448 s and
  the host suite in 360 s. The audit ran in the main checkout meanwhile.

## Gates

The host suite passed 1702 of 1702 tests, two of them skipped as on every
Mac. The ASan and the UBSan suite passed 1701 of 1701 each. The docs-style
checker reports nothing on the 22 pages and on this report. `git grep`
finds no marker of unfinished text in the pages. This is the state before
the commit of this report:

```text
$ git log --oneline -3
e1d285ed Run opt in the document runner without a captured output
93da9bd2 Keep the skeleton of the guide to what each part will hold
6c09e147 Read the module after opt for a release llvm excerpt
$ git status --short
$ git rev-parse HEAD origin/docs
e1d285ed0f4dd66acbc5d0c6de6210a7832757ed
e1d285ed0f4dd66acbc5d0c6de6210a7832757ed
```
