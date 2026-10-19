# Step 33, the minor findings of the build and the tools

Step 33 of "Fix steps" in `docs/audit/summary.md` takes the minor
findings one area per session. This session took the build and the
tools, the rows of the minor tables that `docs/audit/build-and-tools.md`
opens. The logs stand in `build/drive/logs/step33-build/`.

## Findings closed

| Finding | Commit | Cause |
|---|---|---|
| Three guards, `run_warning_set.cmake:14` | `417951fb`, `50d4b909` | `warning_set` skipped `src/native/CMakeLists.txt`, `rt_names` read no weak definition and `repo_layout` passed a file in `src/` and the output of a tool. The three `_runner` tests failed before. |
| A tracked `.pyc` | `417951fb` | The hole of `repo_layout` let it in. |
| The path from `CLAUDE.md`, `banned-terms.md:6` | `01ef5a37` | No file stated the rules for a comment. `tools/docs-style/rules.md` does. |
| Stale comments, `CMakeLists.txt:136` | `f320553b` | Fourteen comments described the book, three commands, a host runtime of CMake and other things gone. |
| Recipes that name modules, `sqlite.cmake:1` | `f6ec7c46` | The headers named modules that are not built. |
| DESIGN without a reason, `run_asserts.cmake:47` | `9eb41b3c` | Three comments stated the rule they test under the word. |
| The nm of the host, `run_clib.cmake:391` | `fedc97ad` | `clib_exports` ran nm. `pinned_compiler` now refuses a binary tool of the machine, and refused it before. |
| The Microsoft toolchain, `release.sh:688` | `b1382389` | The xcrun check could not fire, `llvm-sums` exists nowhere, and two checks gave no reason. |
| A dozen readers of a pin, `upstream-sources.cmake:21` | `a5ebed49`, `9ff64d43` | Each user read a pin its own way. `tools/pins.cmake` is the reader, and `pin_readers` refused fifteen reads before. |
| Versions outside their pins, `sysroot-pins:4` | `b0b29c3c`, `e69fd379` | The comments of `src/native/warnings.cmake`, musl in three rows and glibc 2.35 in `check-libc.cmake`. `native_pin_self`, `runtime_sources` and `linux_libc` failed before. |
| The six targets in twenty places, `pack-anti.cmake:56` | `2424adbd` | `tools/cpu-levels.cmake` reads the targets from the default rows. `target_lists` refused 21 lists before. |
| The host worked out many ways, `pinned-compiler.cmake:37` | `ea4ee07f` | Three derivations, four forms of a Windows test and ten names of the runtime library. Two runners read `CMAKE_EXECUTABLE_SUFFIX`, which `cmake -P` leaves empty. `host_names` refused 52 lines before. |
| Helpers in another file, `fetch-release.cmake:25` | `a5ebed49`, `ea4ee07f`, `0e9ed53b` | The host and the pins moved to files of their own, five names took `antic_`, and `run_sources.cmake` is `run_upstream_sources.cmake`. |
| Small copies, `release.sh:975` | `b5a6a6e3`, `ad6be2c5`, `2424adbd` | Three openssl checks, two reads of `llvm-version`, a DESIGN twice, `--ld-path`, five questions for the resource directory and a second public key. `release_key` refuses the second key, and refused it before. |
| Registrations of `pcre2.cmake:97` | `7bce0c61` | Twelve blocks of the recipes. `antic_native_tests` writes them. |
| Guards of `ANTIC_LLVM_AR` | `30148f7c` | Four tests that cannot be false, left of the finding of `driver.h:23`. |
| Two presets, `CMakePresets.json:23` | `f6e270fc` | Fixed before this step: the `asan` description names what it builds, and `ubsan` names both sanitizers. |

`445f56ac` writes the commits into the summary.

## Findings left

- `vcvarsall.bat` of `tools/release.sh` waits for decision 16.
- Step 10 of `tools/release.sh` beside `tools/check-offline.cmake`. Only
  a release can show a merged step 10, so it stays.
- The three ways to move a download stay. The release script and
  `docs/vm-setup.md` give `ANTIC_CLANG_DIR` and `ANTIC_LLVM_DIR`, and a
  DESIGN keeps `antic_shared_path` for a sanitizer tree. `ef5d53b8` names
  all three in `tools/deps-dir.cmake`.
- Loops over two or four targets stay. Each names a subset with a
  reason. A runner tests a target variable with `MATCHES "^windows-"`,
  which is one form of a question about a target.
- The order of `tests/CMakeLists.txt` and its raw registrations belong to
  the session of the tests, which `docs/audit/tests.md` also opens.

## Questions for Eddie

- The `ubsan` presets run AddressSanitizer without
  `ASAN_OPTIONS=detect_leaks=1` and without `-g`, which `asan` has. That is
  a build configuration, so it stays as it is.
- `link_line` of `src/antic/linker.c` prints `cl main.c` for a Windows
  library, and the C of the tests compiles with the pinned clang. Should
  the line name another compiler?
- Should `ANTI_DEPS_DIR` replace `antic_shared_path` and the two cache
  paths?

## Identity, messages, freeze

- `emit_identity` passes. `2424adbd` writes its lines in the order of
  `tools/cpu-levels`, so the manifest holds the same 224 digests in
  another order. Every `link_identity_<target>` passes unchanged.
- The scripts of `tools/` say `tools/<pin> has no row <KEY>` for a
  missing row. No message of antic, anti or the runtime changed.
- No new feature, option or command. No `[provisional]` entry. No new
  finding. `tests/known-failures.txt` has 29 lines before and after.

## Gates

- The three builds report no warnings: `build-host.log`,
  `build-asan.log` and `build-ubsan.log`.
- Host, 5147 tests, `-I 1,2000` 325 s and `-I 2001,5147` 279 s. All 2000
  and 3168 pass, a part counting the fixtures it pulls in. The 8 expected
  failures of the Mac fail as listed, and the two `mimalloc_environment`
  tests skip: `host-1.log` and `host-2.log`.
- ASan, 5145 tests, `-I 1,1150`, `-I 1151,2250` and `-I 2251,5145`,
  280 s, 372 s and 396 s. All 1150, 1113 and 2916 pass with the same
  skips: `asan-1.log` to `asan-3.log`.
- UBSan, the same parts, 258 s, 285 s and 299 s. All pass:
  `ubsan-1.log` to `ubsan-3.log`.
- The docs-style checker reports nothing on each `.md` file the session
  touched: `docs1.log` to `docs7.log`.

Two ctest runs of a subset passed the 120 s limit of the tool, which
moved them to the background. The session waited for each in the
foreground before going on.

## Not run on the target

- `ea4ee07f`, `ANTIC_HOST_TARGET` from `antic_machine_host` at the
  configure of a Linux and a Windows host. `antic_host_target` on both VMs.
- `ea4ee07f`, the suffix of opt and lld-link on Windows.
  `runtime_hosts` and the `clib_*` tests on anti-windows.
- `ea4ee07f`, the relative links of `get-sysroot.cmake` on Windows.
  `sysroot_links` on anti-windows.
- `fedc97ad`, the exports of a shared object through `--dynamic-syms`.
  `clib_exports` on anti-linux.
- `2424adbd`, the list of targets of `tools/check-offline.cmake` on a VM.
  Step 5 of `./r`.
- `e69fd379`, the names of the musl package and source in a fresh
  download. `tools/get-sysroot.cmake` into an empty directory, on any host.

## State at the push

```text
$ git log --oneline -3
50d4b909 Take the triples of rt_names_runner from antic_target_triple
445f56ac Record the build and tools minor findings of step 33 in the summary
30148f7c Drop four tests of ANTIC_LLVM_AR that cannot be false
$ git status --short
$ git rev-parse HEAD origin/main
50d4b90903f1c92f7f51eadd0ccefdbe1d342148
50d4b90903f1c92f7f51eadd0ccefdbe1d342148
```

## Left for the next session

The last area of step 33: the tests. It takes the rows of the minor
tables whose places lie in `tests/` and have no commit yet. Among them
are the order and the raw registrations of `tests/CMakeLists.txt`.

CONTINUE
