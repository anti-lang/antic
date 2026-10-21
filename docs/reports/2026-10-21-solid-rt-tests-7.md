# Step rt-tests, seventh session

The session went on from the list of `2026-10-21-solid-rt-tests-6.md`,
which stays, as do the reports of the sessions before it. It wrote tests
for regions of `src/rt/`, `src/antic/` and `src/anti/` that a program, an
option or an injected fault reaches. Logs are under `build/drive/logs/`,
named `rt7-*`. The list of the regions not reached is
`rt6-end-unreached.txt` at the start and `rt7-end-unreached.txt` at the
end. The probes are in `build/rt7/`, and `cr.sh` measures one run against
the list of the start.

## Coverage per directory

Regions reached. The start is `5739ba07`, `rt6-end-summary.txt`, whose
tree is that of `318bc115`. The end is `791fcf4b`, `rt7-end-summary.txt`.
Every part of the run passed, `rt7-end-1.log` to `-5.log`.

| Directory | Start | End | Not reached, start and end | Lines at start | Lines at end |
|---|---|---|---|---|---|
| `src/rt/` | 94.65% of 9252 | 95.16% of 9252 | 495, 448 | 94.92% | 95.52% |
| `src/antic/` | 97.98% of 55587 | 97.99% of 55587 | 1122, 1119 | 98.02% | 98.03% |
| `src/anti/` | 98.11% of 11663 | 98.22% of 11663 | 221, 208 | 98.16% | 98.24% |

## The tests

- runtime: `unit_symbols` reads the entry tables of DWARF 5 in every form
  the line reader takes, in 32-bit and 64-bit DWARF. It reads the tables
  the reader refuses, as directories and as files. It reads a unit of a
  version outside 2 to 5 and a header longer than its unit. So it does a
  line range of 0 and a file table of DWARF 4 cut inside an entry. An
  address of four bytes and `fixed_advance_pc` are read. A walk of the
  functions of Mach-O stops where its callback says. A COFF name whose
  length has more digits than the name is refused. That is 47 regions of
  `symbols.c`.
- antic: `matrix_flow_for_iterator_element_address` takes the address of
  the element of a loop over an iterator hook.
  `antic_message_module_path_beside_a_root_that_does_not_exist` names a
  search root under `/` that does not exist.
- anti: `unit_fmt` formats a line of a block comment that holds the star
  of the gutter alone and a doc line indented past 80 bytes. The bind
  cases `clang_va_list_field`, `clang_no_runtime` and
  `clang_pack_pop_of_an_empty_stack`. The build cases
  `fault_soname_link_not_written`, a fault of the link that `--soname`
  writes in `dist/`, and `index_dependency_url`, a dependency of an index
  named by an `ftp://` URL.

`emit_identity` and `link_identity_macos-arm64` pass, and no file of
`tests/emit-identity/` or `tests/link-identity/` changed.

## Findings

The step closed none. One is new, with its test.

- T170, major: `anti bind --clang` binds an enum of the fixed base
  `__int128` as one of `c_int` with wrong values, and binds the function
  that takes it, without a warning. `anti_bind_message_clang_enum_of_128_bits`
  holds it, `13147ff9`. Its test also reaches the four regions of
  `find_constant` for a value past 64 bits.

`tests/known-failures.txt` held 110 lines and 91 entries, 89 expected to
fail on the Mac, and holds 111 lines and 92 entries, 90 on the Mac. No
feature, option or command, and no `[provisional]` entry.

## Regions nothing reaches

- `src/rt/registry.c` 696 to 711: `deserialize` reads a float only after
  `anti_rt_json_valid_number` passed it, and `anti_rt_read_float` reads
  every such number.
- `src/rt/symbols.c` 583: both callers of `dwarf_line` hand it a line
  section they found.
- `src/antic/sema_const.c` 227: the fewest digits that read back as the
  value never end in 0. 236 to 242: a value between -1 and 1 truncates to
  0, which every integer type holds, so no message names one.
- `src/antic/sema_stmt.c` 129 to 131: `sema_implemented_in` finds an
  interface reached through a derived interface first.
- `src/antic/lower_stmt.c` 1069 and 1193: the block of `let ... else`
  leaves, which the checker holds.
- `src/antic/lexer.c` 1570 to 1585 and 1616 to 1617: the parser expects
  punctuation and names alone, and the dump of tokens stops before the
  end token.
- `src/anti/bindwrite.c` 218: `declare_function` cuts the C result at
  its first `(`, so the result never holds one. T167 holds it.
- `src/anti/bindclang.c` 91: a clang whose `--version` names its version
  after the first line reaches it, and the message then reads `clang is
  clang -1`. No document gives the message, so it has no test. The
  question is below.
- `src/rt/trace.c`, `elf_id`, `symbolize_elf`, `symbolize_pe` and the
  cases of ELF and PE: the Mac runs Mach-O alone.

## Questions

- What does `anti bind --clang` say of a clang whose version it cannot
  read? Today it says `clang is clang -1`.

## Not run on the target

- `antic_message_module_path_beside_a_root_that_does_not_exist` on
  Windows, where `/no-such-root-of-anti` is no absolute path. The case
  would show the root passed over there as well.
- `anti_build_message_fault_soname_link_not_written` on Linux and
  Windows: the count of opens may differ there. The case would show the
  message of the link.

## Gates

- No warning of our code: `rt7-hb1.log` to `-hb5.log`,
  `rt7-asan-build.log` and `rt7-ubsan-build.log`.
- Host, 5628 tests, `-I 1,2000` and `-I 2001,5628`: 2000 and 3654
  passed, in 352 s and 311 s. Part 2 runs 26 fixtures of part 1 again.
  90 expected failures fail as listed, and the two
  `mimalloc_environment` tests skip: `rt7-host-1.log` and
  `rt7-host-2.log`.
- ASan, 5626 tests, `-I 1,1150`, `1151,2250`, `2251,3800`, `3801,4700`
  and `4701,5626`: 1151, 1114, 1576, 905 and 928 passed in 238 s, 181 s,
  184 s, 446 s and 346 s, `rt7-asan-1.log` to `-5.log`.
- UBSan, the same parts: 1151, 1114, 1576 and 905 passed in 213 s, 90 s,
  87 s and 168 s. Part 5 failed `release_tags` once, after 50 s,
  `rt7-ubsan-5.log`, whose output the run did not keep. It passed alone
  in 13 s, `rt7-ubsan-rt.log`, and the rerun of part 5 passed all 928 in
  286 s, `rt7-ubsan-5b.log`.
- The docs-style checker reports nothing on the `.md` files the session
  touched.

## State at the push

```text
$ git log --oneline -3
791fcf4b Test a dependency of an index named by a URL of no repository
972962f0 Test a pragma pack pop of an empty stack under anti bind
13147ff9 Record T170: anti bind gives an enum of __int128 the base c_int
$ git status --short
$ git rev-parse HEAD origin/main
791fcf4b299ea5ceb4e023157b6ca0f3a0845a48
791fcf4b299ea5ceb4e023157b6ca0f3a0845a48
```

This report is written after that push, and its commit follows it.

## Left for the next session

- `src/antic/`, 1119 places: `llvm_emit_function.c`, `sema_generic.c`,
  `driver_link.c`, `sema_const.c` and `antl_tree.c` have the most.
- `src/anti/`, 208 places: `bindclang.c`, `bindwrite.c`, `build.c` and
  `deps.c`.
- `src/rt/`, 448 places: `trace.c`, mostly ELF and PE, `plugin.c` and
  `object.c`.
