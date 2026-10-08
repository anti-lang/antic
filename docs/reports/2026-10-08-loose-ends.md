# Loose ends

The four loose ends of `docs/reports/2026-10-08-no-ci.md`, as Eddie decided them on
2026-10-08.

## Commits

| Commit | Change |
|---|---|
| `d6b8d473` | `./r --dry-run` on a version that is a tag of the checkout or of origin prints `warning:` with the tag, runs every other check, prints the plan and exits with 0. It repeats the warning on its last line. A real `./r` still refuses that version in step 1 |
| `95096d2b` | `memory_checks_list_macos-x86_64` and `std_builder_room_memory_checks_macos-x86_64` run under Rosetta at `v1` |
| `c723898f` | `docs/vm-setup.md` names `ubuntu-26.04.1-live-server-arm64.iso` |
| `7bac6614` | CLAUDE.md names the skip of `sysroot_build_tools` and the counts of the three suites |

## The dry run on a tag

`release_dry_run` now runs `./r --dry-run` and `./r` once with the tag in the checkout
and once with the tag on origin alone. The dry run has to end with 0, warn with the tag
and print the plan of steps 6 to 10. The real run has to refuse in step 1 and reach no
step 2. The first dry run of an untagged version has to print no warning. The test failed
on the old script first. The entry under "The release script" in `docs/decisions.md`,
`docs/work-order-release-script.md`, `docs/distribution.md` and CLAUDE.md say so.
`./r --dry-run` itself did not run on the checkout. It runs the three suites, packs six
hosts and installs on both VMs, which this session did not need.

## Memory checks under Rosetta

The two tests stand in the block of `tests/CMakeLists.txt` that runs the programs of the
emulated target, under `macos-x86_64` alone. `run_memory_checks_list.cmake` takes the
options of antic in `OPTIONS`. Under Rosetta the report symbolizes as on macos-arm64:
`anti.rt.main` reads, and `List<int>.push` freed the room. `memory_checks` has no test
for macos-x86_64. One of its cases compiles a pattern literal, and `anti.regex` is built
for x86-64-v3 alone, which antic refuses at `v1`. The comment of the block says so.

## Gates

| Suite | Result | Time |
|---|---|---:|
| Mac host | 1689 of 1689, `sysroot_build_tools` skipped | 231 s |
| Mac ASan | 1688 of 1688, the same skip | 463 s |
| Mac UBSan | 1688 of 1688, the same skip | 325 s |
| anti-linux host, fresh export of `95096d2b` | 1619 of 1619, six skipped | 153 s |
| anti-windows host, fresh export of `95096d2b`, `-j4` | 1607 of 1607, twelve skipped | 825 s |

The Mac builds print no warning. The VM builds print only those of raylib and mimalloc,
which build under the flags of their own projects. The two new tests add under 4 s to
the ASan suite. The docs-style checker reports nothing on every `.md` file touched.

## Questions for Eddie

1. anti-linux has 6 cores, and `docs/vm-setup.md` says to give the machine 4. The line
   stands as it was, since it is advice for a new machine and not a fact of the image.
   Should it name 6?
