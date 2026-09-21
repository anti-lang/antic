# The run of the first release

Anti 0.1.0 is out. `./r` made it on the development Mac on 2026-09-21,
in five calls. `docs/reports/2026-09-21-release-0.1.0.md` is the report
the script wrote. This one holds what the run hit and what changed.

The release is <https://github.com/anti-lang/antic/releases/tag/v0.1.0>,
on `e139761`, with the six packages, the six symbols archives and
`SHA256SUMS`. anti-lang.com serves the two installers, the downloads page
of 0.1.0, `SHA256SUMS.sig` and the public key. The install from the site
compiled a program for the Mac and linked one for the three targets whose
sysroot it carries.

## What stopped the run, and what came of it

1. **The signed tag.** Step 7 ran `git tag -s`, and gpg holds no key of
   `eddie@anti-lang.com`. The run stopped after steps 1 to 6 had passed.
   The tag is `git tag -a` now, as in `anti-lang/llvm-tools`, and
   `SHA256SUMS.sig` stays the one signature of a release. The test
   `release_key` refuses a `tag -s`.
2. **The runner matrix.** Step 8 ran the six hosted runners for the first
   time and every one of them failed. Eddie decided the matrix is no step
   of a release: steps 2 and 5 cover the Mac and both VMs, and hosted
   minutes are limited. `./r` leaves step 8 out and `./r --matrix` runs
   it. The test `release_dry_run` refuses a run that names the workflow
   without the flag.
3. **The preflight of a rerun.** With the tag in place, the preflight
   refused the version as published, and the run could not reach step 9.
   The stamp of step 7 now tells the tag of this run from one that is out.
   It is read in the state of the commit that is checked out.
4. **The 403 of the signature.** Step 9 rsynced `SHA256SUMS.sig` into a
   directory that `mkdir -p` had made without the setgid bit. The file
   took the group of the user, and the server could not read it. The group
   on triton was fixed by hand for 0.1.0. Step 9 sets the directory above
   before it makes the directory of the version, and the test
   `release_site` holds that order.

## The test of the fix on the server

The first fix set the bit on both directories after `mkdir -p` made them,
and it does not work. Four arms were run against triton with a version
`0.1.0a`, each with the rsync of step 9 and a fetch over HTTPS.

| arm | what was run | group of the file | answer |
|---|---|---|---|
| a | `mkdir -p` alone, the old form | eddie | 403 |
| b | `mkdir -p`, then the bit on both | eddie | 403 |
| c | the bit on the directory above, then the directory of the version | www-data | 200 |
| d | the form of arm b under the `anti` directory fixed by hand | www-data | 200 |
| e | the text of `tools/release.sh`, on a chain that did not exist | www-data | 200 |

Arm b is the fix as first committed. `chmod g+s` on a directory that
exists sets no group. The directory of the version kept the group of the
user, and so did the file below it. Arm d passes only because the
`anti` directory was repaired by hand, which is the state of the server
today and not something the script establishes. Arm e is the form that
stands now. Every test directory was removed afterwards, and the
signature of 0.1.0 still answers 200.

## The first tag

Step 7 ran twice. The first tag named `ae2b13b` and was deleted with its
release on Eddie's word, because that commit still ran the matrix as part
of a release. The tag that stands names `e139761`.

## What the packages are of

Steps 2 to 6 ran once, on `7255ce0`, and their output was kept across the
four commits that followed. The state was pointed at each new commit by
hand rather than built again. The four commits changed `tools/release.sh`,
its tests and three documents, and no package carries any of them. The
suites, the packages, the symbols and both VMs are therefore of the code
that `e139761` holds.

## Open

- The matrix run `35546384487` failed on all six runners: 111 tests on
  macos-x86_64, 173 on macos-arm64, 105 on each Linux runner and 3 on each
  Windows runner. The same commit passed 494 tests on the Mac and the two
  VMs. Nothing was looked at beyond the counts. A failure that large reads
  more like the setup of the workflow than like the compiler. It is worth
  an hour with `--matrix` before the next release.
- The full suite and the two sanitizer suites did not run before the four
  commits of this session. Eddie ruled them out while the release was
  underway. `release_key`, `release_site`, `release_dry_run` and the
  docs-style checker ran on each.
- `CLAUDE.md` still says 493 tests. The Mac ran 494 in step 2, and
  `release_site` makes 495. No count was measured after it was added.
