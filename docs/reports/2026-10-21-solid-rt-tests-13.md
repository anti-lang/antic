# Step rt-tests, thirteenth session

The session went on from the list of `2026-10-21-solid-rt-tests-12.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/` and `src/anti/` that a program or a command
reaches, and recorded one new finding. Logs are under
`build/drive/logs/`, named `rt13-*`. The list of the regions not reached
is `rt12-end-unreached.txt` at the start and `rt13-end-unreached.txt` at
the end. The probes are in `build/rt13/`.

## Coverage per directory

Regions reached. The start is `rt12-end-summary.txt`, measured at
`507682b3`, whose sources equal those of `c579c916`. The end is
`ed0df4cc`, `rt13-end-summary.txt`, from `./c coverage part 1/5` to
`5/5`, `rt13-end-1.log` to `-5.log`, every part passed.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 95.53% of 9252 | 95.64% of 9252 | 414, 403 | 95.91% | 96.00% |
| `src/antic/` | 98.11% of 55587 | 98.11% of 55587 | 1053, 1053 | 98.15% | 98.15% |
| `src/anti/` | 98.29% of 11663 | 98.35% of 11663 | 199, 193 | 98.33% | 98.39% |

## The tests

- runtime, `tests/trace/outside.anti`, `trace_outside`: a `StackTrace`
  built from frames of its own, two outside every module, one at
  address 0. `text` writes each as its address and `-`, and `symbolize`
  names no function for either. It reaches `module_number`, the frame
  line outside a module and the arms of a frame without a module in
  `trace.c`, `e1f84541` and `dd6ff223`.
- anti, `tests/fmt/messages.cmake`: `no_source_hidden`, a source whose
  name starts with a dot, which the walk of `files.c` skips,
  `784fb77e`.
- anti, `unit_fmt`: a direct import list that neither `;` nor `as`
  follows is no list, the last arm of `import_list_at`, `c5385dc5`.
- anti, `symbols_resolve_elf` with the runner
  `tests/runners/run_symbols_resolve_elf.cmake`: the tracer built for
  linux-arm64, its archive packed again with the debug link alone. The
  ELF arm of `syms_resolve_binary` names `main` and its line, `ed0df4cc`.
- anti, `fmt_comment_before_operator`: T174, below, `fbe7ea63`.

## Findings

The step closed none. One is new, with its test.

- T174, minor: `anti fmt` writes `return x /* one */ + 1;` as
  `x /* one */ +1`. `ends_value` of `src/anti/fmt.c` reads the piece
  written last, which is the comment, so `prefix_operator` takes the `+`
  for a prefix. "The spaces" in `docs/notes/fmt.md` reads a prefix from
  the token before it. The canonical source stays out of
  `fmt_canonical` while the finding is open, and
  `tests/runners/run_fmt_canonical.cmake` says so. Left for the step of
  `anti fmt`.

The same probe wrote `f(/* a */ 1)` as `f( /* a */ 1)`. No document
says where a comment after `(` stands, so it is no finding.

`tests/known-failures.txt` held 121 lines and 102 entries, 100 expected
to fail on the Mac. It holds 122 lines and 103 entries, 101 expected to
fail on the Mac. No feature, option or command, and no `[provisional]`
entry.

## Regions nothing reaches

- `src/rt/plugin.c` 1248 to 1250: `interfaces.<n>` of 64 bits fits 32
  bytes. 1259: the count of the index is that of all its pairs. That
  is more than the interfaces of one entry, so the loop meets a missing
  field first.
- `src/antic/lower_expr.c` 1040 to 1042: `lower_flag_operation` writes
  `IR_NEG_FL` for a unary itself and passes `unary` false alone.
- `src/antic/sema_pattern.c` 247, the `s`: `matches` is the one method
  whose counts agree, and it takes one argument.
- `src/antic/antl_tree.c` 218 to 226: the checker refuses a format
  specification with a byte past ASCII as an unknown format before a
  library file is written.
- `src/antic/coff.c` 700 to 733 and 782: an object past 4 GiB, a string
  table past `LONG_NAME_LIMIT` and a section of more than 65535
  relocations.
- `src/antic/driver.c` 1558 to 1587: `anti doc` runs the front end before
  `driver_interface`, so a source that does not parse or fails the
  checker is refused first, `rtt/sc/doc1`.
- `src/anti/doc.c` 480: `doc_body` returns for an empty doc first.

## Not run on the target

None. Every change runs on the Mac. `trace_outside` and
`symbols_resolve_elf` run in the suite of the VMs as well once new ones
exist.

## Gates

- No warning of our code: `rt13-hb4.log`, `rt13-hb6.log`,
  `rt13-asan-build.log` and `rt13-ubsan-build.log`.
- Host, 5698 tests, `-I 1,2000` and `-I 2001,5698`: 2000 and 3724 passed,
  in 234 s and 281 s. Part 2 runs 26 fixtures of part 1 again. 101
  expected failures fail as listed, and the two `mimalloc_environment`
  tests skip: `rt13-host-1.log` and `rt13-host-2.log`.
- ASan, 5696 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5696`: 1151, 1105, 1576, 906 and 998 passed in 215 s, 182 s,
  133 s, 446 s and 356 s, `rt13-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1105, 1576, 906 and 998 passed in 147 s,
  149 s, 138 s, 172 s and 298 s, `rt13-ubsan-1.log` to `-5.log`.
- `emit_identity` and `link_identity_macos-arm64` pass unchanged.
- The docs-style checker reports nothing on this report, on
  `docs/audit/tests-findings.md` and on `docs/notes/source-tree.md`.

## State at the push

```text
$ git log --oneline -3
ed0df4cc Test the resolve of a Linux trace through its debug link
c5385dc5 Test a direct import list that no semicolon follows
fbe7ea63 Test a binary operator after an ordinary comment
$ git status --short
$ git rev-parse HEAD origin/main
ed0df4cc4b46b17693a7659a70c867effad4b7d9
ed0df4cc4b46b17693a7659a70c867effad4b7d9
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1053 places. A library of generic functions with tuples,
  inherited fields, `size_of(T)`, format specifications and `alloc(T, n)`
  reached none, `build/rt13/m1`. `llvm_emit_call.c` 416 to 429: a
  program of atomic pointer, `i32` and `u8` fields reaches no
  conversion of `reinterpret`, and the cause is not found yet,
  `build/rt13/t/at.anti`.
- `src/anti/`, 193 places: `bindclang.c`, `bindwrite.c`, `repo.c` and
  `deps.c`. The diamond of `run_anti_build_resolve.cmake` reaches no
  loop of `graph_add`, `require` or `package_need`.
- `src/rt/`, 403 places: `trace.c`, mostly ELF and PE, `plugin.c`,
  `object.c` and `registry.c`.
