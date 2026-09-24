# Second merge of the fix lanes

This step brings the lanes of fix steps 20 and 21 together on `main`. It
fixes no finding of its own. The first merge of the day is
`docs/reports/2026-09-24-merge-lanes.md`.

## Decisions folded

The 24 entries of `docs/decisions-mback.md`, `-mfront.md`, `-mrt.md` and
`-mtool.md` now stand in `docs/decisions.md`, each with its wording and its
`[provisional]` tag. A wrapped entry became one line, as every entry of
`docs/decisions.md` is. Each went to the end of its section, and the four
files are deleted, in commit `e2c16bf`.

| Section | Entries |
|---|---|
| Libraries and runtime | `antl_write` refuses a count past 32 bits, a NUL inside a string of a library file, the atomic start flag of `init.c` |
| Build tool and distribution | the COFF join past 4 GiB, the `files_` prefix, the allocations of `anti`, `files_base_name`, the limits of `zip_write` |
| Compiler behaviour | the checked allocator of the back end, `attributes.h`, the three dots of `ir_vformat`, the cut messages of the front end, `token_kind_name`, variadic aggregates in the verifier, the limits of `mach_add` |
| Runtime configuration | a NUL inside a value or a path |
| Plugins | the cut reason of a failed load |
| The symbols command | an absolute path by the rules of the host |
| The bind command | literals out of range, array lengths past `INT64_MAX`, cut JSON messages |
| Names and shared code of the runtime | the table entry helpers of `object.c`, the failure routine of `assert.c` |
| Files of lowering | the `bool` result of `lower_module` |

No file outside `docs/reports/` named the four lane files.

## State and format version

The "State" section of `CLAUDE.md` now gives 841 host tests and 840 per
sanitizer build, in commit `1325be0`. The format version stays 53, which is
`ANTL_VERSION` in `src/antic/antl.h` and the value both documents give.

## Identity manifests

`emit_identity` and `link_identity_macos-arm64` pass on the merged tree, so
nothing was regenerated. Since the first merge, `fd06ca1`, the changed
lines of `tests/emit-identity/` and `tests/link-identity/` each have a fix
behind them:

- Added emit lines: `long_float` and `long_names`, six targets each, for the
  new tests of M24, M25 and M33 in `57421ca`. No existing emit line changed.
- The macOS link digest of `return42` moved once per runtime change, in
  order: `d0b418e`, `1e55926`, `52b8b8e`, `c2ad2b3`, `5cd2c9b`, `eeda9bc`,
  `e449a88`, `390ccd1`, `7157485`, `1443500`, `4aa7381`, `4154122`,
  `4050280`, `df93f23` and `a809d71`. Each commit replaces the digest that
  the one before it wrote.

## Gates

- Build: zero compiler warnings on host, asan and ubsan, in
  `build/drive/logs/merge2-{host,asan,ubsan}-build.log`.
- Host: 100% tests passed out of 841, `build/drive/logs/merge2-host-ctest.log`.
- ASan: 100% tests passed out of 840, `build/drive/logs/merge2-asan-ctest.log`.
- UBSan: 100% tests passed out of 840, `build/drive/logs/merge2-ubsan-ctest.log`.
- Docs style: nothing on `docs/decisions.md`, `CLAUDE.md` and this report.

## Findings and provisional decisions

No finding was taken in this step, and no `[provisional]` decision was made.

## Proof

Before this report was committed:

```console
$ git log --oneline -3
1325be0 Take the test counts of the merged step 20 and 21 lanes
e2c16bf Fold the decisions of the step 20 lanes into docs/decisions.md
578b949 Report fix step 21 for lower.c
$ git status --short
$ git rev-parse HEAD origin/main
1325be0c2c4c7b66a26e95d05018f612dd63355a
578b949a9b7cde0550675b48891f69429950c948
```

`origin/main` stood at `578b949` until the push that carries this report.
