# Step fuzz-fix: the findings of the fuzzer

The step fixed the three findings the step fuzz recorded, all of grade
major, in the order of their ids. It removed the workaround of each from
the generator and ran the generator again. Logs are under
`build/drive/logs/`, named `fx-*`.

## Findings closed

- T180, commit `3929c842`. The constant folding of
  `src/antic/sema_const.c` applied R029 and R031 to the operation of the
  flags form, which traps in no build. The checker now names the
  operation of the flags form it checks, and the folding leaves it alone.
  The new matrix program `values_flags_form_of_a_constant_negation` holds
  unary `-` of the least value, which the same cause refused.
- T181, commit `9350a152`. `check_snapshot` of `src/antic/sema_closure.c`
  read the declared type `?int` of the name an `if let` binds. It now
  takes the type the block proves for a name its block never assigns, as
  the closure reads it. The new refusal
  `closures_snapshot_of_an_if_let_pointer` keeps a bound pointer refused.
- T182, commit `1f4de38c`. The condition of `do { } while cond` had no
  end, so a statement that starts with `(` or `*` on the next line
  continued it. An operator, a link, `as` or `is` that stands first on a
  line now ends the condition outside its brackets. `anti fmt` had the
  same gap: a condition over two lines gave the statement after it the
  indent of a continuation, and it now follows the parser. Three new
  matrix programs hold a `*p = n;` after the loop, a condition over two
  lines and the refusal of a condition continued by `&&`. The developers'
  guide, chapter 4, and the overview state the rule.

Commit `5e1f6448` formats the programs of the first two fixes. Commit
`9c4c62af` writes the three commits into the rows of
`docs/audit/tests-findings.md`.

## The generator

Commit `679d68d2` names the generated functions `fun0` and on. The names
`f0` and on reached `f16`, a keyword, so every program above size 16 was
refused. That was the one find of the step fuzz-campaign, a fault of the
generator and no defect of antic.

| Seeds | Size | Log | Time | Finds |
|---|---|---|---|---|
| 30001 to 30150, the finds of fuzz-campaign | 40 | `fx-run-30001-s40.log` | 228 s | none |
| 1, 5 and 3122, the seeds of T180 to T182 | 6 | `fx-run-seed-*.log` | | none |
| 40001 to 40500 | 6 | `fx-run-40001.log` | 363 s | none |
| 40501 to 41000 | 6 | `fx-run-40501.log` | 366 s | none |

The generator of commit `679d68d2` wrote each of them. It steers around no
finding now, and seeds 1 to 40 of it hold 37 snapshots that read an `if
let` binding and 6 flags forms of unary `-`. Nothing is left: the step
recorded no new finding.

## Counts

`tests/known-failures.txt` held 142 lines before and 133 after, 123
entries before and 114 after. The Mac expects 121 tests to fail before
and 112 after, since two entries are `host=windows`.

## `[provisional]` entries

Under "What the solid run decided" in `docs/decisions.md`:

- The operation of the flags form is no constant expression of R029 and
  R031. A shift keeps R032.
- The condition of `do { } while cond` ends at a line break, outside its
  brackets, and goes on over a line that ends with an operator. `anti
  fmt` reads it the same way.

## Gates

- No warning of our code: `fx-gate-host-build.log`, a build with `--clean-first`,
  `fx-gate-asan-build.log` and `fx-gate-ubsan-build.log`. The six
  warnings of the host log are those of mimalloc under its own flags.
- Host, 5811 tests, `-I 1,2000`, `2001,3900` and `3901,5811`: 2000, 1927
  and 1917 passed in 378 s, 177 s and 274 s, `fx-gate-host-1.log` to
  `-3.log`. The later parts run the fixtures of the first again. The 112
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip.
- ASan, 5809 tests, `-I 1,1150`, `1151,2250`, `2251,4000`, `4001,4900`
  and `4901,5809`: 1151, 1106, 1777, 906 and 909 passed in 240 s, 187 s,
  212 s, 458 s and 357 s, `fx-gate-asan-1.log` to `-5.log`.
- UBSan, 5809 tests, the same parts: 1151, 1106, 1777, 906 and 909 passed
  in 242 s, 169 s, 164 s, 185 s and 304 s, `fx-gate-ubsan-1.log` to
  `-5.log`.
- `emit_identity`, `emit_identity_texts` and `link_identity_macos-arm64`
  pass unchanged.
- The docs-style checker reports nothing on the `.md` files of the step.

## Not run on the target

Nothing. The changes are in the checker, the parser, `anti fmt` and the
generator, and none has a path of its own on Linux or Windows.

## State at the push

```text
$ git log --oneline -3
9c4c62af Record the commits of T180 to T182 and the campaign of fuzz-fix
679d68d2 Name the generated functions so that no size hits a keyword
1f4de38c End the condition of a do loop at its line
$ git status --short
?? docs/reports/2026-10-22-solid-fuzz-fix.md
$ git rev-parse HEAD origin/main
9c4c62afaefc393223325d05eb17b75f596641cc
9c4c62afaefc393223325d05eb17b75f596641cc
```

The one untracked file is this report, whose commit follows the push.
