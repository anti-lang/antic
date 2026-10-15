# Solid: the findings of the area flow

The step fixed the open findings of `docs/audit/tests-findings.md` whose
test starts with `matrix_flow_`, in one session on 2026-10-15. All seven
are fixed, and `tests/known-failures.txt` holds no line of a test of the
area. None of them was a case the freeze keeps. The logs lie in
`build/drive/logs/solid-flow/`.

## Findings closed

| Finding | Grade | Commit | Cause |
|---|---|---|---|
| T005 | severe | `ec170d89` | The name a handler binds was a temporary that held the error, so a closure took a word of the error object as the address of the name. |
| T001 | major | `e01c6194` | The checker refused `yield` and `undo` inside a `defer` or an `undo` and none of the other exits of decision 19. |
| T003 | major | `e1133217` | An error sent from a deferred statement to the handler of a `try` block walked the scope that held the statement, so lowering ran the `defer` inside itself until its stack ran out. |
| T004 | major | `06ea3773` | A closure read every captured `?T` with its declared type, the name of an `if let` included. |
| T007 | major | `f111e7b0` | The checker writes an `if let` on a `?T` as a block, and the missing-return rule read no block. |
| T006 | minor | `72ebd769` | The parser read the statement of a `defer` whole before it refused a `defer` there. |
| T096 | minor | `bbafd51b` | A use of a hook, a simd struct or a copy whose declaration was refused checked it again and gave a second message. |

- T001: a refused `fail` or `try` still answers the `may fail` of its
  function, so one mistake gives one message. A `break` or a `continue`
  whose loop stands inside the same statement stays allowed, by R001. A
  `try` is refused inside a `defer` even inside a `try` block of the
  same statement, as decision 19 says without exception. The overview,
  `docs/anti-language-additions.md` and chapter 4 of the guide state the
  rule, the guide with a refused example.
- T003: a deferred statement runs against a copy of its scope that holds
  the actions not run yet. The flag of a function exit is restored after
  a walk inside one.
- T004: the walk that decides it reads the block before the checker.
  A name of an inner block that shadows the name therefore counts as an
  assignment. The new test `matrix/flow_if_let_binding_assigned_in_closure`
  pins the refusal where the block assigns the name.
- T007: a block whose last statement ends the path now ends it, which
  chapter 4 of the guide states.
- T006 changes the depth case of `unit_parser` for a chain of `defer`.
  The inner `defer` is refused before anything nests, so both lengths
  now expect the message of the `defer` in place of the depth limit of
  the long one.
- T096 is no test of the area by its row, but two lines of the area
  named it. Its five tests share the cause and all pass. The two
  messages that only those uses gave stay in
  `tests/diagnostics-open.txt`, with the reason that no program reaches
  them.
- `emit_identity` and `link_identity` are unchanged.

## Findings left open

None of the area.

## New findings

None.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- A closure in the block of an `if let` on a `?T` reads the name as the
  value without `none` when nothing in the block assigns the name or
  takes `&` of it. Otherwise the entry on a captured `?*T` holds.

No feature, option, command or public item was added.

## Counts and gates

`tests/known-failures.txt` held 128 lines, 108 of them tests, before the
step. It holds 94 lines, 74 of them tests, after it. 66 of them are
expected to fail on the Mac, and the other 8 name another host.

- The build of all three trees has no warning, `bh2.log`, `ba.log` and
  `bu.log`.
- Host, through `ctest --preset host -j14` in two parts: 1998 and 2986
  pass, `h1.log` and `h2.log`, and two `mimalloc_environment` tests
  skip. The second part repeats 21 tests that are fixtures of its tests.
  The first run of part 1 failed `header_sections` and `fmt_canonical`
  for the step's own new lines, fixed in `95c4d208`.
- ASan: 1150, 1110 and 2732 pass, two skip in the second part,
  `a1.log` to `a3.log`.
- UBSan: 1150, 1110 and 2732 pass, two skip in the second part,
  `u1.log` to `u3.log`.
- No step changed a platform layer, a runtime path, a link, a sysroot or
  a package, so no VM ran.
- The docs-style checker reports nothing on every `.md` file the step
  touched.

## State before this report

The report commits after these commands ran.

```text
$ git log --oneline -3
95c4d208 Put the new declarations under their files and format the new test
6305b043 Record the commits of the findings the step flow fixed
bbafd51b Give a use of a refused declaration no second message
$ git status --short
$ git rev-parse HEAD origin/main
95c4d208b4b6eaefc5e07d3ec46a8ef86c162289
95c4d208b4b6eaefc5e07d3ec46a8ef86c162289
```
