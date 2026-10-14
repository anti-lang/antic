# Solid: the baseline of the VMs

The step ran the whole suite on anti-linux and anti-windows at the head of
`main`, `a896e30d`, and sorted every failure. The logs lie in
`build/drive/logs/solid-vm-baseline/`: `linux-1.log` to `linux-3.log` and
`win-1.log` to `win-3.log` at the start, `linux-head-*.log` and
`win-head-*.log` at the end.

## The failures

anti-linux failed 17 tests and anti-windows 42, the ones the step
`fix-ownership` found. The task sorts a failure as an environment of the VM
or a finding. A third kind came up: a test that assumed the Mac. Each such
test now checks the documented behaviour on every host, as strictly as
before.

| Test | Host | Sort | Closed by |
|---|---|---|---|
| `anti_bind_message_clang_*`, 11 cases | both | environment: no clang on PATH | `52eab988` |
| `anti_option_bind_define_name`, `_value`, `_shim`, `anti_option_bind_target` | both | environment: no clang on PATH | `52eab988` |
| `anti_bind_message_clang_type_does_not_parse` | both | test: a block type, which clang reads for Apple alone | `36fd2f45` |
| `antic_option_bundle_static` | both | test: the archive form of Mach-O, and `libanti_rt.a` | `46caccfa`, `1ce28da6` |
| `guide_examples` | linux | finding T130 | `host=linux` |
| `anti_build_message_library_digest` | windows | environment: `file(WRITE)` writes CRLF | `ce4f5333` |
| `anti_check_message_*`, 5 cases | windows | environment: CRLF | `ce4f5333` |
| `anti_message_license_*`, 4 cases | windows | environment: CRLF | `ce4f5333` |
| `anti_option_check_root_library`, `_source` | windows | environment: CRLF | `ce4f5333` |
| `anti_symbols_message_*`, 3 cases | windows | environment: CRLF | `ce4f5333` |
| `variant_wide_tag` | windows | environment: CRLF | `ce4f5333` |
| `anti_test_flow`, `_values`, `_ownership`, `_errors`, `_threads`, `_classes` | windows | finding T132 | `host=windows` |
| `antic_option_output_program` | windows | test: the order of the names without `.exe` | `c9d935b6` |
| `antic_option_soname_linux` | windows | finding T131 | `host=windows` |
| `anti_bind_huge_enum` | windows | T117 does not show there, so the listed test passed | `ad7292bd`, `host=macos,linux` |
| `guide_examples` | windows | test: one output for a size the MSVC rule changes | `7c6794bf` |

- No tool was missing from either VM, so `docs/vm-setup.md` gains no
  setup. The tests found the pinned clang the way `run_anti_bind.cmake`
  already did, through `tests/pinned_clang.cmake`.
- `write_bytes` of `tests/program_output.cmake` writes the files of a case
  as bytes on every host. `docs/notes/hosts-and-harness.md` says why
  `file(CONFIGURE)` does not do.
- The guide's run of Linux stopped at the first crash. With `read` and
  `write` renamed in a copy, the whole guide passed there
  (`linux-guide-probe3.log`), so T130 holds both examples.

Two tests timed out once and passed alone: `matrix_values_aggregates_dev`
on Windows (`win-3-lasttest.log`) and `anti_memory_checks` on Linux
(`linux-head-memchecks.log`). Both ran while the Mac did other work, the
other VM among it. The dev build of `values_aggregates` took 26 s alone on
anti-windows then and 4 s later. Neither is a defect.

## Findings

New, each open and left for the step of its area. The freeze and the task
leave the defects as they are.

- T130, severe, Linux. An `export fn` named `read` or `write` takes the
  symbol of musl in a static program, so `io.println` calls it.
- T131, major, Windows. `--soname` runs `ln -sf` of the host, which is the
  `ln` of Git there and writes a copy. It is also a tool of the host.
- T132, minor, Windows. anti leaves its standard output in text mode, so
  the last line of `anti test` ends in CRLF.

T117 keeps its row and is now listed `host=macos,linux`.

## The list

`tests/known-failures.txt` held 321 lines, 308 of them tests, before the
step, and holds 336 lines, 316 of them tests, after it. A line may end
with `host=` and systems. `tests/CMakeLists.txt` marks such a test on those
hosts alone, and `known_failures` checks the field and the marks of the
tree for its host. `known_failures_runner` holds nine new cases. The hosts
expect 308 failures on macOS, 309 on Linux and 314 on Windows.

## Provisional entries

Under "What the solid run decided" in `docs/decisions.md`:

- `host=` takes more than one system, with a `,` between two, since T117
  shows on two hosts. The task named one host per line.
- A program of a document whose output differs on Windows by a rule takes
  a second block, `text output windows`.

## Gates

- No warning of our code in the builds of `host`, `asan` and `ubsan` on
  the Mac, or in those of the two VMs.
- `host`: 4953 tests, every one passes, 308 of them as expected failures
  and 2 skipped (`mimalloc_environment`). `gate-host-1.log`,
  `gate-host-2.log`.
- `asan` and `ubsan`: 4951 tests each in three parts, every one passes
  with the same skips. `gate-asan-*.log`, `gate-ubsan-*.log`.
- anti-linux at `7c6794bf`, with `run_antic_options.cmake` of `1ce28da6`:
  4875 tests, every one passes, 309 as expected failures, 5 skipped.
  `anti_memory_checks` passed on its run alone.
- anti-windows at `7c6794bf` with the same file: 4853 tests, every one
  passes, 314 as expected failures. `guide_examples` and `package_keys`
  ran alone, in 116 s and 460 s (`win-head-guide.log`,
  `win-head-package.log`).
- `emit_identity` and `link_identity_macos-arm64` pass unchanged. The step
  changed no code of antic, anti or the runtime.
- The docs-style checker reports nothing on every `.md` file the step
  touched.

## State

The commands gave, after the push of the step and before the commit of
this report:

```text
$ git log --oneline -3
4b4c02dc Record the runs of both VMs at 1ce28da6
1ce28da6 Name the runtime of the host in the case bundle_static
7c6794bf Give the example bits of the guide its output on Windows
$ git status --short
?? docs/reports/2026-10-14-solid-vm-baseline.md
$ git rev-parse HEAD origin/main
4b4c02dc2421b25863372f383dd1d8d1b946f183
4b4c02dc2421b25863372f383dd1d8d1b946f183
```
