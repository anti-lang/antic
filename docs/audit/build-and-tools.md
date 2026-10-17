# Build and tools audit

The build and the tooling at `69fc7bee`: `CMakeLists.txt`,
`CMakePresets.json`, the 54 tracked files of `tools/`, the 15 `.cmake`
files of `src/native/`, `tests/CMakeLists.txt` and the 157 `.cmake` scripts
of `tests/`. `src/native/CMakeLists.txt` was read as well, since the
recipes cannot be judged without it. The root files, `tools/` and
`src/native/` were read line by line for the report. Five reviewers read
`tests/CMakeLists.txt` and the runner scripts in full, and each claim of
theirs that stands below was read again at the line it names. A claim that
rests on a kept output of an earlier build says so. Nothing was built and
no suite was run. Each finding was compared with the reports of
`docs/audit/2026-09-23/` and `docs/audit/2026-09-30/`, with
`docs/audit/2026-09-30/closure.md`, and with `docs/audit/structure.md` and
`docs/audit/tool-pass.md` of the third audit.

Three standards apply. Safety and C has no numbered rule for a test
script. Its section "Scope and bar" says that `tests/` "must be correct and
must not hide a failure". A finding on a test that hides one cites that
sentence.

## Counts

| Severity | Findings | New | Returned | Still open |
|---|---|---|---|---|
| Severe | 2 | 2 | 0 | 0 |
| Major | 17 | 16 | 1 | 0 |
| Minor | 20 | 19 | 0 | 1 |

| Severity | Standard | Findings |
|---|---|---|
| Severe | Documentation, a file whose documented behaviour differs | 2 |
| Major | Structure, a part doing the work of another, or a dependency the wrong way | 2 |
| Major | Structure, duplicated logic that has diverged | 4 |
| Major | Structure, a pin in two places with no test between them | 1 |
| Major | Structure, left of a replaced feature, with an effect | 2 |
| Major | Structure, a file whose purpose is not one sentence | 1 |
| Major | Documentation, a header or a comment that differs from the code | 5 |
| Major | Safety and C, the bar of `tests/` | 2 |
| Minor | Structure, one definition spelled in more than one place | 8 |
| Minor | Structure, left of a replaced feature | 2 |
| Minor | Structure, placement, names and one way to run a tool | 3 |
| Minor | Documentation, a stale comment or a header that says less | 6 |
| Minor | Safety and C, the bar of `tests/` | 1 |

The task asks two questions beside the findings. Their answers stand at
the end of the minor findings. One is "The build from CLAUDE.md and the
headers". The other is "One place for the pins, the digests and the
release steps".

## Severe

### The header of the build script promises downloads the configure step does not make

`tools/compile.sh:13`, documentation, a file whose documented behaviour
differs from what the code does, severe, new.

The header says the first build of a tree configures it. That step,
it says, "installs the pinned clang, the LLVM tools, the sysroots and
raylib under build/deps". The configure step installs the first two alone.
`tools/pinned-compiler.cmake:26` runs `get-llvm.cmake` and
`get-clang.cmake`, and no build file runs `tools/get-sysroot.cmake` or
`tools/get-raylib.cmake`. The only callers of the first are four tests
that run a copy of it on stand-in archives.

A first `./c` on a fresh checkout therefore stops at
`src/native/raylib.cmake:10`, "Run tools/get-raylib.cmake". Before that
line it has skipped every runtime without a word: `CMakeLists.txt:279` and
`CMakeLists.txt:530` pass over a target whose sysroot is missing and print
nothing. `tools/release.sh:764` states the true behaviour, "stops at
raylib, which has no fetcher in the configure". `README.md:19` holds the
two commands, and `CLAUDE.md` says under "Building" that `./c` "configures
a tree on its first build" without them. The fix is one of two. The
header says what `README.md` says, or the configure step fetches the
sysroots and raylib as it fetches the five other sources.

### The test of the empty CodeView subsection cannot fail

`tests/runners/run_codeview_empty.cmake:24`, documentation, a file whose documented
behaviour differs from what the code does, and the bar of `tests/`, severe,
new.

The header says the test checks that a module without functions writes no
empty subsection of symbols. The check searches the assembly for the text
`DEBUG_S_SYMBOLS` and fails when it stands there without `S_LPROC32`. Both
were spellings of the native back end. No file of `src/antic/` holds
either text now, and antic writes `-S` through llc, which writes the kind
of a subsection as a number. The reviewer read the kept output of an
earlier build, which holds neither word. Line 26 can then never hold, so
`codeview_empty_windows-x86_64` and `codeview_empty_windows-arm64` pass
whatever antic writes. The fix reads the object with `llvm-readobj
--codeview`, or links the program and refuses the warning of lld-link.

## Major

### A DESIGN comment of the root build file states the opposite of the packer

`CMakeLists.txt:303`, documentation, a DESIGN comment whose statement is no
longer true, major, new.

The comment on the bitcode of the runtime says "tools/pack-anti.cmake
leaves the directory out of the package until Eddie's yes under condition
C3". `tools/pack-anti.cmake:382` records that yes, of 2026-10-07, and
copies `bitcode/full/` into every package at line 388. `CLAUDE.md` says the
same under "Distribution". A reader of the build file is told that a
package carries no bitcode.

### The tools of the runtime archive are built by the test directory

`tests/CMakeLists.txt:1050`, structure, a part doing work that belongs to
another, major, new.

`add_custom_target(llvm_tools ALL ...)` is the only producer of
`runtime/bin`. It runs `tools/check-llvm.cmake` with `DEST`, which copies
the LLVM tools and writes `llvm-version`. antic finds its tools there, and
`tools/pack-anti.cmake:98` copies the directory into the package of the
host. A part of the product is therefore a target of `tests/`. The target
and the test `toolchain_pin` both stand inside `if(ANTIC_LLVM_MC)` at
line 1048, and line 1049 takes the directory of the tools from the path
of llvm-mc. `docs/audit/structure.md` asks that the dead option
`--llvm-mc` go. Whoever removes the variable with it removes the target
and its check together, and a configured tree keeps a stale `runtime/bin`.
The target belongs in the root file beside the copy of the sanitizer
runtimes, keyed on `ANTIC_LLVM_DIR`.

### The raylib source is named by the test directory and fetched by no one

`tests/CMakeLists.txt:1297`, structure, a dependency that runs the wrong
way, major, new.

`ANTIC_RAYLIB_DIR` is declared in `tests/CMakeLists.txt`, the one place of
the CMake build that reads `tools/raylib-pin`. `src/native/raylib.cmake:9`
builds the library from it and `tools/release.sh:286` reads it from the
cache. The recipe works because the root adds `tests` before
`src/native`, at `CMakeLists.txt:688` and `690`. The other five recipes
declare their own directory with `antic_shared_path` and run their own
fetch script at configure time, as `src/native/sqlite.cmake:8` does.
raylib alone has its fetch script in `tools/`, is fetched by hand, and
stops the configure step when it is missing. One recipe that declares the
directory and runs the script would make the six alike.

### Six fetch scripts are one script, and one copy has diverged

`src/native/get-pcre2.cmake:14`, structure, duplicated logic that has
diverged, major, new.

`get-pcre2.cmake`, `get-mbedtls.cmake`, `get-miniaudio.cmake`,
`get-mimalloc.cmake` and `get-sqlite.cmake` differ in the name, the
suffix of the archive, the hash and the file that marks an unpacked tree.
A `diff` of each against the first shows nothing else. `tools/get-raylib.cmake`
is the sixth copy and has drifted. It downloads without the check for an
archive already on disk, unpacks on every run where the five keep an
unpacked tree, and stands in another directory.
`tools/get-sysroot.cmake:63` holds a seventh download with a digest check
under the name `fetch`, and `tools/fetch-release.cmake:82` an eighth under
`download`. The second audit found the same shape in the five pin checks
of the tests, which `tests/runners/run_native_pin.cmake` now holds once. One
function that takes the pin, the archive name and the marker file would
hold the fetch once as well.

### The LLVM tools are listed and checked in two places that disagree

`tools/check-llvm.cmake:16`, structure, duplicated logic that has diverged,
major, new.

`tools/pinned-tools.cmake:46` defines `ANTIC_LLVM_TOOLS`, eight names, and
its comment says `check-llvm.cmake` copies them into the runtime archive.
`check-llvm.cmake` reads no such list. It checks the version of nine names
at line 16 and copies eleven at line 32. `antic_llvm_tool_problems` at
`pinned-tools.cmake:53` is a second version check, which the test
`pinned_tools` alone runs. The two differ. `check-llvm.cmake` never asks
llvm-objdump for its version, and it copies a tool only `if(EXISTS ...)`,
so a missing llvm-objdump leaves a `runtime/bin` without it and no error.
The other check reports a missing tool. The eleven names are spelled again
at `tests/runners/run_package.cmake:107` and `tests/runners/run_release_dry_run.cmake:261`,
and the header of `tools/get-llvm.cmake` lists the eight in prose.
`check-llvm.cmake` is also two things, a check and the copy that makes
`runtime/bin`. One list and one check, with the copy as a step of its own,
would remove the difference.

### The triple and the flags of a target are written in eight places

`src/native/CMakeLists.txt:31`, structure, duplicated logic that has
diverged, and documentation, a comment that differs from the code, major,
new.

The comment of `antic_native_target` says "They are the flags the cross
builds of anti_rt pass for the same target". They are not. The runtime of
a Linux target takes `-mno-outline-atomics` on ARM64 and
`-fno-omit-frame-pointer -momit-leaf-frame-pointer` on both processors, at
`CMakeLists.txt:541` to `556`, each under a DESIGN comment with its reason.
The native libraries take neither. Whether the libraries need them is not
recorded anywhere, so the difference reads as drift.

The same mapping from a target to a triple and its headers stands in:

- `CMakeLists.txt:557` to `588`, for the runtime.
- `src/native/CMakeLists.txt:33` and `src/native/media.cmake:72`.
- `tools/host-compile.cmake:17`, the one table a script can include.
- `tests/CMakeLists.txt:1359`, which puts the headers of clang before those
  of the sysroot, the reverse of the root file.
- `tests/runners/run_linux_libc.cmake:34`, a second `triple_of`.
- `tests/runners/run_macos_sdk.cmake:80` and `tests/runners/run_clib.cmake:96`.
- `tools/get-sysroot.cmake:455`, which spells the two gnu triples that
  `tools/windows-compile.cmake` calls its "One definition".

The Windows half has one definition and three users. The Linux and macOS
halves have none.

### The upload half of the publish script is left of the download area

`tools/publish.cmake:91`, structure, left of a replaced feature, and
documentation, a header that names another purpose, major, new.

The header says "Upload a version directory of the download area". No
release uploads one. The packages are assets of the GitHub release, and
`tools/release.sh:1021` runs the script with `CHECK_ONLY` alone, as the
test `publish_manifest` does. Lines 91 to 123, the `scp` of every file and
the `ssh` that checks them, are reached by no caller. They would also
break a decision. Line 105 sends `SHA256SUMS.sig` to the directory of the
binaries, and `tools/site-base` says the signature "never stands beside
them". `tools/release.sh:87` and `1015` still describe the upload. The
comment at `publish.cmake:114` stands above the wrong call. Line 34 says
the signature is read "with the openssl of the installers", and line 47
uses `dgst -verify` where both installers and `tools/fetch-release.cmake`
use `dgst -binary` and `pkeyutl -verify`. The script is the check of a
release area, and its name and header should say that.

### The release script spells the version of raylib

`tools/release.sh:666`, structure, a pin in two places with no test
between them, major, new.

The scripts of both VMs configure with
`-DANTIC_RAYLIB_DIR=.../raylib/raylib-6.0`, at lines 666 and 767.
`tools/raylib-pin` holds `RAYLIB_VERSION=6.0`. The test `raylib_pin` hands
`tests/runners/run_native_pin.cmake` `src/native/raylib.cmake` alone, and no runner
holds the text `raylib-6`. A move of the pin leaves step 5 of a release
pointing at a directory that no longer exists, and the first run to find
out is a release. The script already reads `tools/site-base` and
`tools/cpu-levels`, and can read the pin the same way.

### The uninstallers name an address the release never publishes

`tools/uninstall.sh:5`, documentation, a header that says how the file is
run and is wrong, major, new.

The header gives `curl -fsSL https://anti-lang.com/uninstall.sh | sh`, and
`tools/uninstall.ps1:4` the same with `irm`. Step 9 of `tools/release.sh`
sends `install.sh` and `install.ps1` at line 1167 and its plan says "would
send nothing else". `tools/site-base` lists what the site serves, and the
uninstallers are not among it. `tools/downloads.html.in`,
`docs/distribution.md` and the pages under `docs/site/` do not name them.
A user of an install has no way to the script its own header describes.
Either step 9 publishes the two files, or the headers name the copy a
user has.

### The policy test reads two list files, and four runners are outside it

`tests/runners/run_script_policies.cmake:11`, documentation, a header that claims
more than the code checks, major, new.

The header says "Every script that runs as `cmake -P` names the minimum
version of CMake." The loop reads the `-P` calls of `CMakeLists.txt` and
`tests/CMakeLists.txt`. The files of `src/native/` register tests with
`-P` too, and four runners they name carry no `cmake_minimum_required`:
`tests/runners/run_native_link.cmake`, `tests/runners/run_glibc_link.cmake`,
`tests/runners/run_native_licenses.cmake` and
`tests/runners/run_raylib_miniaudio_pin.cmake`. The header gives the cost of that,
24 tests failed under CMake 3.31. Line 28 also accepts any line that
starts with the command, with any version. The minor finding on the two
versions below belongs to the same rule.

### Tests pass without running, or do not exist, and the run says nothing

`tests/CMakeLists.txt:691`, Safety and C, the bar of `tests/`, major,
returned.

The second audit reported "Tests drop out or pass without running on some
hosts" at `tests/CMakeLists.txt:1313`, and `07249add` fixed the places it
named. The pattern is back in a runner added after that fix, and it stands
in others the finding did not name.

- `llvm_coff_plugin_<target>` at line 691 sets no
  `SKIP_REGULAR_EXPRESSION`. `tests/runners/run_llvm_coff_plugin.cmake:17` prints
  `SKIP:` and returns when the sysroot is missing, and the test reports a
  pass. The loop above it, at line 684, sets the property.
- `package_keys` exists only when all six sysroots are on disk, at line
  2239, with no `else`. `CLAUDE.md` names it as the check of item 28.
  Line 2373 says of raylib "Fail rather than skip", and the sysroots get
  the opposite.
- `debug_info`, the one test of `-g` under a debugger, exists only when
  `find_program` finds lldb or gdb, at line 3644.
- `tests/runners/run_macos_sdk.cmake:27` turns every failure of
  `tools/macos-sdk.cmake` into `SKIP:`, the refusal of a changed SDK among
  them, which the header lists as a thing the test shows.
- `tests/runners/run_release_dry_run.cmake:97` to `144` turns a fixture that does
  not compile into `SKIP:` with its output discarded. A warning of the
  pinned clang under `-Werror` then skips the whole test of `./r`.

A missing sysroot or debugger is a state of the machine. The suite should
name it, as a skip with its reason or as a failure.

### Five checks cannot tell a failure from a pass

`tests/runners/run_manifest_signature.cmake:155`, Safety and C, the bar of
`tests/`, major, new.

- The case of a wrong signature writes a manifest whose digest is all
  zeros. It passes when the installer fails and its log holds the word
  "signature". An installer that accepts every signature prints
  "SHA256SUMS carries the signature" at `tools/install.sh:153` and then
  fails on the digest at line 261. The case passes for it as well.
  `tests/runners/run_installer_github.cmake:243` has the same form. The case
  should keep the digest right and change the signature, or match the
  message of the refusal.
- `tests/runners/run_probe.cmake:31` and `32` read the exit status of both probes
  into variables that nothing reads. Two programs that die with the same
  output pass.
- `tests/runners/run_fmt_canonical.cmake:38` adds the output of a failed
  `anti fmt --check` to the report, and line 59 fails on a report that is
  not empty. A run that ends by a signal prints nothing and passes.
- `tests/runners/run_llvm_verify.cmake:74` counts a program whose `--dump-llvm`
  fails as skipped whenever `--dump-opt` fails too, a crash included. The
  list of the skipped is compared with nothing.
- `tests/runners/run_anti_build.cmake:139` reads the build id with no check that
  a line was found. With no line, the id is empty, `# build ` matches the
  head of any map at line 143, and line 150 compares two empty texts.
  `tests/runners/run_build_id.cmake:32` reads the same line and demands 64 digits.

### The allocator test passes over a program that calls malloc

`tests/CMakeLists.txt:126`, documentation, a comment that differs from the
code, and the bar of `tests/`, major, new.

The comment says "The programs of tests/ that link antic_core take their
memory from src/antic/alloc.c as well." `core_test_sources` lists four of
them. `antl_rename` at line 2157 and `poke_byte` at line 3350 link
`antic_core` and are not listed. `tests/doc/antl_rename.c:29` calls
`malloc` and line 37 `realloc`. The list should come from the targets that
link the library, not from memory.

### The LLVM program tests are a second runner, and run on a Mac alone

`tests/CMakeLists.txt:554`, structure, left of the two-back-end period,
major, new.

`antic_llvm_program_test` and `tests/runners/run_llvm_program.cmake` date from the
time LLVM was the second back end. The runner repeats
`tests/runners/run_program.cmake`: the expected file beside the source, the
`.args` file and `program_expect`. It adds `--target`, `--dev` and a check
that no `.ll` or `.bc` stays. `llvm_targets` at line 599 is set for a Mac
host alone, and the comment at line 629 says "the step vm adds the
others". That step is closed and added nothing. Line 1108 takes every
`llvm_*.anti` out of the program glob, so `llvm_division`, `llvm_wrap` and
`llvm_flags` run on a Mac and on neither VM. So do `llvm_run`,
`llvm_opt_failed`, `llvm_llc_failed`, `llvm_inline_lines` and the
`llvm_program_abi_*` tests of line 2386, whose comment still says "under
the LLVM back end" and "its native test". The check for a kept `.ll` is an
option of `run_program.cmake`, and the programs belong in the glob of every
host.

### The helpers of the runners exist in many copies with different contracts

`tests/runners/run_dev.cmake:26`, structure, duplicated logic that has diverged,
major, new.

The second audit reported two such families, M61 and M62, and the scripts
it named hold their fix. The same shape stands in others.

- antic must succeed and print nothing. `tests/program_output.cmake:28`
  states the rule and `antic_program` holds it. Seven runners write it
  out again, among them `run_native_link.cmake:23`, `run_pdb_guid.cmake:33`
  and `run_program.cmake:46`. More than twenty call sites check the status
  alone, so a warning of antic or of the linker passes there:
  `run_dev.cmake:26`, `run_modules.cmake:38`, `run_emit_identity.cmake:37`,
  `run_licenses.cmake:25`, `run_link_identity.cmake:30` and
  `run_memory_checks.cmake:116` among them.
- `function(run)` is defined in eight runners with four contracts.
  `run_anti_bind.cmake:48` stops on a status, `run_unwind.cmake:15` on a
  status or any text, `run_plugin.cmake:21` puts antic first, and
  `run_release_dry_run.cmake:72` takes a message first.
- The call of `anti` with a cache of its own is written in ten places
  under four signatures of `run_anti`, from `run_anti_build.cmake:37` to
  `run_anti_memory_checks.cmake:45`. One effect shows:
  `run_anti_build_deps.cmake:153` calls `run_anti` without the `--llvm-ar`
  that its own `build` passes.
- The fake release of the installer tests stands three times, in
  `run_installer_alone.cmake:37`, `run_installer_github.cmake:68` and
  `run_manifest_signature.cmake:29`. Two put the real cmake first on the
  path, and the first a cmake that fails, to show the installer calls
  none.
- A missing sysroot is a failure in `run_address_tables.cmake:51` and a
  skip in `run_cross_link.cmake:25` and `run_dead_code.cmake:35`.

`tests/program_output.cmake` is the place these helpers already have.

### The two largest runners hold many tests under one name

`tests/runners/run_release_dry_run.cmake:1`, structure, a file whose purpose is not
one sentence, major, new.

`run_release_dry_run.cmake` has 915 lines and eight parts. Lines 21 to 43
pin the network commands of the release script against
`docs/vm-setup.md`. Lines 59 to 417 build the fixtures and the clone. Then
come step 5, step 1, the outputs of steps 2 to 4, the signing key, the
refusals of the preflight, and at line 875 a test of
`tools/check-cpu.cmake`. That last part uses neither the clone nor a
stand-in, and it is the only test of the script: `check-cpu` occurs in no
other file of `tests/`. It stands behind five returns that skip the file
on a host that is no Mac. The comment at line 151 says the test
`cpu_archive_levels` covers `check-cpu.cmake`, and that test runs
`tests/runners/run_cpu_archive.cmake` over the runtime.

`tests/runners/run_package.cmake` has 663 lines under the test name
`package_keys`, and its first sentence is about the key. It also checks
the listing, `sources.txt`, `anti bind`, the link for six targets, two
licence forms, the glibc and raylib programs, `anti build --release`,
`tools/check-offline.cmake`, a cross pack and a refusal of the packer. A
failure of any part reads as a failure of the key test. The pack and the
unpack are the shared part, and the rest are tests of their own.

### A runner says it links with the recipe of the packer and holds a copy

`tests/runners/run_linux_libc.cmake:12`, documentation, a header that differs from
the code, major, new.

The header says the test "links a stand-in with that same recipe" as
`tools/pack-anti.cmake`, "so a change to the recipe that reaches the
builder's libc fails here". Line 55 holds a hand copy of the `ld.lld`
line of `tools/pack-anti.cmake:291`, and the compile line is the runner's
own. A change of the packer is not seen. The header also documents
`PACKAGE`, "which a release run has". No caller passes it, and the packer
runs `tools/check-libc.cmake` itself at line 144.

## Minor

### A pin is read by a dozen readers

`tools/upstream-sources.cmake:21`, structure, one way of reading a file,
minor, new.

`antic_read_pins` sets every `KEY=value` line of a pin as a variable. It
is the general reader, and it stands in the file of the sources record.
Beside it each user reads a pin with a loop of its own:
`tools/fetch-release.cmake:55`, `tools/get-raylib.cmake:13`, the five
fetch scripts of `src/native/` at line 14 or 16,
`tools/clang-release.cmake:10`, `tools/libc-versions.cmake:9`,
`tools/macos-sdk.cmake:27`, `tools/pack-anti.cmake:179`,
`src/native/pcre2-source.cmake:22` and `tests/CMakeLists.txt:1294`.
`tools/get-sysroot.cmake:51` to `59` is `antic_read_pins` line for line.
Each fetch script reads its pin twice, once itself and once through
`antic_pin_url`. The tests add `read_pin` with three signatures, in
`run_sources.cmake:55`, `run_native_pin.cmake:36` and
`run_raylib_miniaudio_pin.cmake:10`, and `row` in
`run_release_pins.cmake:21`. They differ in what they refuse: an empty
value, a missing row, or nothing.

### The six targets are listed in more than twenty places

`tools/pack-anti.cmake:56`, structure, one definition spelled in more than
one place, minor, new.

`CMakeLists.txt:272` defines `ANTIC_TARGETS`. `tests/CMakeLists.txt` never
reads it. It spells the six targets at lines 536, 672, 2239, 2662, 2711,
2733, 2759 and 4727, and a pair of them in thirteen more loops. The
scripts spell the list again: `tools/pack-anti.cmake:56`,
`tools/check-offline.cmake:44`, `tools/release.sh:38`,
`tests/runners/run_emit_identity.cmake:22`, `tests/runners/run_llvm_atomics.cmake:24`,
`tests/runners/run_package.cmake:47` and `306`,
`tests/runners/run_release_dry_run.cmake:60` and `tests/runners/run_release_pins.cmake:11`.
The contents agree and the orders differ. `tools/check-cpu.cmake:38` shows
the other way: it takes the targets from the `default` lines of
`tools/cpu-levels`, "rather than against a list written here a second
time".

### The host and its suffixes are worked out in many ways

`tools/pinned-compiler.cmake:37`, structure, one platform layer, minor,
new.

The name of the host is derived three times in the build:
`pinned-compiler.cmake:37` before `project()`, `CMakeLists.txt:95` after
it, and `antic_machine_host` at `tools/fetch-release.cmake:25`.
`tests/CMakeLists.txt` asks for a Windows host in four ways, among them
`WIN32` at line 2113, `ANTIC_HOST_TARGET MATCHES` at line 163 and
`ANTIC_HOST_OS STREQUAL` at line 1918. The runners take the suffix of an
executable from `HOST` in `run_anti_build.cmake:27`, from
`CMAKE_HOST_WIN32` in `run_bench.cmake:50`, and from a `SUFFIX` argument in
`run_plugin.cmake`. `run_runtime_hosts.cmake:15` and `run_clib.cmake:76`
read `CMAKE_EXECUTABLE_SUFFIX`, which a script run with `-P` does not
define. The name of the runtime library per target is decided at
`CMakeLists.txt:360`, in `antic_native_library` of `src/native/`, at
`tests/CMakeLists.txt:750` and `842`, and in six runners.
`run_cpu_check.cmake:17` looks for `libanti_rt.a` alone and would skip a
Windows target.

### Versions stand outside their pins

`tools/sysroot-pins:4`, structure, a pin in more than one place, minor,
new.

- musl stands three times in one pin: `MUSL_VERSION=1.2.6`,
  `MUSL_APK=1.2.6-r2` and the URL of `MUSL_SOURCE_URL`, which takes no
  `@VERSION@` as `MINGW_URL` does. `tests/runners/run_sources.cmake:99` ties the
  first to the third through the record. Nothing compares `MUSL_APK`.
- glibc 2.35 is the default of `MAX_GLIBC` at `tools/check-libc.cmake:24`,
  beside the file names of `tools/sysroot-pins`.
- The comments of `src/native/warnings.cmake` name six versions, at lines
  13, 17, 21, 24, 28 and 39. No pin test lists the file, so a move of a
  pin leaves them.
- `tools/clang-pin:6` and `tools/llvm-pin:7` both hold the tag. The test
  `release_pins` compares them at `tests/runners/run_release_pins.cmake:63`, which
  is the seam held as it should be.

### Fourteen scripts name CMake 3.20, and two of them call a 3.21 command

`tools/get-sysroot.cmake:30`, structure, one definition spelled in more
than one place, minor, new.

The root file and most scripts name 3.21. Fourteen name 3.20: eight of
`tools/`, the five fetch scripts of `src/native/` and
`tests/runners/run_find_order.cmake`. `tools/get-sysroot.cmake` calls
`file(COPY_FILE ...)` eighteen times and `tools/pack-anti.cmake` twice,
and CMake has the command from 3.21 on. The test `script_policies` asks
for the line and compares no version.

### llvm-mc and three dead options run through the build files

`tools/pinned-compiler.cmake:45`, structure, left of the native back end,
minor, new in the third audit. `docs/audit/structure.md` reports the
option `--llvm-mc` and the comments that call llvm-mc the assembler. These
places of the area add to it.

- `tools/pinned-compiler.cmake:45` and `tools/pinned-tools.cmake:7` name
  the assembler among the tools "the runtime, the native libraries and the
  tests run". `antic_pinned_tool_problems` at `pinned-tools.cmake:101`
  refuses a tree whose `ANTIC_LLVM_MC` is not pinned.
- `src/native/CMakeLists.txt:5`, and `-DLLVM_MC` at twelve registrations
  of the recipes.
- `tools/cpu-levels:5` keeps the column "llvm-mc -mattr= value", which
  `--print-cpu-levels` alone reads.
- `tests/runners/run_program.cmake:30` stops every program test when the file of
  llvm-mc is missing, a tool antic ignores.
- `tests/CMakeLists.txt` holds nine guards that cannot be false, since
  `pinned-compiler.cmake:47` sets both variables with `FORCE`:
  `if(ANTIC_LLVM_MC)` at 1048, 3510, 3548, 3644 and 4415, and
  `if(ANTIC_LLVM_AR)` at 4295, 4303, 4332 and 4364.
- `tools/pinned-compiler.cmake:51` says the tests run opt and llc "until
  antic runs them itself". antic runs them. `tests/CMakeLists.txt` still
  passes `--opt` and `--llc` at lines 184, 2533, 2537, 2545, 2549 and
  2558, where the sibling tests pass `--runtime`.
- `tests/runners/run_unicode_paths.cmake:7` says antic "runs llvm-mc".

`tests/runners/run_asm.cmake` and `tests/runners/run_unwind.cmake` do assemble with
llvm-mc, so the tool has two users left.

### Checks and words of the Microsoft toolchain remain

`tools/release.sh:688`, structure, left of a replaced feature, minor, new.

- `tools/release.sh:688` sets `VC` to `vcvarsall.bat` of the Build Tools,
  and line 761 calls it before the suite of the Windows VM. The comment at
  line 682 says "The build there needs the environment of vcvarsall.bat"
  and gives no reason. `docs/vm-setup.md:158` gives it: the call puts the
  Ninja of Visual Studio on the path. The whole MSVC environment is loaded
  for one build tool, and a Ninja installed alone would end the last use
  of the Build Tools.
- `tests/runners/run_windows_sysroot.cmake:182` refuses the directories `crt` and
  `sdk` in a sysroot, the layout of Microsoft's CRT and SDK, which
  `tools/get-sysroot.cmake` can no longer write. No comment says why.
- `tests/runners/run_release_pins.cmake:83` refuses the name `llvm-sums`, a file
  that exists nowhere.
- `tests/runners/run_clib.cmake:43` sets `DRIVER` to `cl`, since
  `src/antic/linker.c:1169` prints `cl main.c` in the link line of a
  Windows library. The DESIGN comment at line 58 of the runner says such a
  program is built with the pinned clang.
- `tests/runners/run_package.cmake:32` refuses a bare `xcrun` in the packer.
  `tools/pack-anti.cmake` holds no `xcrun` at all, since the call moved to
  `tools/macos-sdk.cmake:53`. The check cannot fire, and the DESIGN
  comment above it describes a rule the file no longer checks.

### Comments describe what the build no longer does

`CMakeLists.txt:136`, documentation, a stale comment, minor, new.

- `CMakeLists.txt:136`, a DESIGN comment: anti's "commands are sdk export
  and sdk import", then "and test". anti has twelve commands.
- `CMakeLists.txt:62`: "One warning set for clang, gcc and MSVC", so that
  "the code in the book compiles without noise". The book left the
  repository.
- `CMakeLists.txt:289`: "The SDK that a cross compile for macOS reads".
  The runtime compiles against the sysroot of Zig, and
  `pinned-compiler.cmake:104` sets `CMAKE_OSX_SYSROOT` to the pinned SDK.
  The `xcrun` below it runs in a reader's build alone.
- `tools/pack-anti.cmake:7`: "the pinned clang of build/clang". It is
  `build/deps/clang`.
- `tools/sysroot-pins:1`: "tools/get-sysroot.sh". The script is
  `get-sysroot.cmake`.
- `tools/release.sh:1091`: two comments stand above `write_page`, and the
  first describes an "index of the download area, which the site's build
  reads". Step 9 says the site has no build.
- `tools/install.ps1:192`, a DESIGN comment: "A release checks a package
  that way on each VM". `tools/release.sh:690` says the VM installs "not a
  tree under ANTI_HOME".
- `tools/macos-sdk.cmake:1` and `tools/macos-sdk-pin:1` call it the SDK "a
  release links" against. Every build on a Mac links against it, through
  `pinned-compiler.cmake:94`, and both media libraries compile against it.
- `tests/CMakeLists.txt:1045`: the archive "carries llvm-mc, lld and
  llvm-ar". It carries eleven files.
- `tests/CMakeLists.txt` names chapters of the book in fourteen comments,
  line 4462 among them, and `tests/runners/run_clib.cmake:9` and `181` name
  chapter 25.
- `tests/runners/run_release_key.cmake:1`: the key checks the signature "of the
  LLVM tools". The installers check the release of Anti.
- `tests/runners/run_script_policies.cmake:6` gives its reason through "A GitHub
  runner", and `tests/runners/run_float_digits.cmake:26` describes a host runtime
  "built by CMake" with members named `text.c.o`. Neither exists.
- `tests/runners/run_find_order.cmake:20` calls `add_subdirectory(tests)` the last
  line of the root file, and `tests/runners/run_front_end.cmake:29` says a run
  needs "no runtime archive", above a check that it fails without one.

### The native recipes name modules that do not exist

`src/native/sqlite.cmake:1`, documentation, a header whose sentence names
what is not built, minor, new.

`sqlite.cmake` opens with "the reference driver of anti.db",
`raylib.cmake` with "the library of anti.raylib", `miniaudio.cmake` with
"the audio library of anti.miniaudio", and `media.cmake:5` speaks of "a
program that imports anti.raylib or anti.miniaudio". `src/std/anti/` holds
none of the three, and `CLAUDE.md` says "no module binds the other four".
`mbedtls.cmake:1` adds "the HTTPS client of anti", and
`src/anti/repo.c:231` says anti fetches with curl. Each header should say
what the library is for today, a library of the runtime archive that no
module binds yet.

### Headers of runners say less than the file does

`tests/program_output.cmake:1`, documentation, a header that says less,
minor, new.

- `tests/program_output.cmake` opens with a DESIGN comment on the raw
  bytes rule. No sentence says that it is the include file of the runners
  or names its four functions.
- A header that leaves out a part of the work: `run_anti_build.cmake`,
  `run_anti_build_resolve.cmake`, `run_build_id.cmake`,
  `run_installer_options.cmake`, `run_macos_sysroot.cmake`,
  `run_plugin.cmake`, `run_raw_output.cmake`, `run_release_key.cmake`,
  `run_release_pins.cmake` and `run_windows_sysroot.cmake`.
- A variable the header lists and the code never reads: `OBJDUMP` at
  `run_anti_build.cmake:4`, and `LLVM_MC` and `RUNTIME` at
  `run_internal.cmake:5`.
- A statement the code contradicts: `run_modules.cmake:20` says
  `LIB_OPTIONS` are the options "for the library modules", and the dev
  compile at line 49 does not pass them. `run_dev.cmake:4` says "the same
  assembly" and compares LLVM text. `run_cross_link.cmake:1` names one
  source where `SOURCE` is an argument.
- Thirteen comments cite a finding as `M03` or `S45` with no audit named.
  `tests/runners/run_anti_build_inputs.cmake:12` cites `docs/audit/summary.md`,
  which does not exist.

### DESIGN comments that give no reason

`tests/runners/run_asserts.cmake:47`, documentation, a DESIGN comment that gives
no reason, minor, new.

"DESIGN: the decision belongs to the build that compiles the program"
states the rule the block tests. The same sentence opens
`tests/runners/run_checks.cmake:40`. `tests/CMakeLists.txt:1683` says what
`anti.os` does and what the test does. Each is a plain comment under the
wrong word. The other DESIGN comments of the area give a reason, and the
ones whose statement no longer holds stand in the findings above.

### The downloads can be moved in three ways

`CMakeLists.txt:42`, structure, one concern in one place, minor, new.

`tools/deps-dir.cmake:5` says it "is the one place the build names that
directory", and `ANTI_DEPS_DIR` moves all of it.
`antic_shared_path` at `CMakeLists.txt:42` is a second way: a path that is
not on disk is taken from the cache of `build/host`. It serves the
sysroots, raylib and the five native sources. `ANTIC_LLVM_DIR` and
`ANTIC_CLANG_DIR` at `tools/pinned-compiler.cmake:17` are a third, plain
cache paths that take neither. The macro is defined in the root file and
called from `src/native/` and `tests/`. `ANTI_DEPS_DIR` and
`ANTIC_DEPS_DIR` differ by one letter and mean the option and its result.

### Helpers stand in the file of another purpose

`tools/fetch-release.cmake:25`, structure, placement and names, minor,
new.

- `antic_machine_host` names the host of the machine. It stands in the
  fetch script, and `tools/pack-anti.cmake:92` includes the fetcher to
  call it.
- `antic_read_pins` and `antic_pin_url` serve every fetch script and stand
  in `tools/upstream-sources.cmake`, the file of the sources record.
- `tools/sources.cmake` lists the C sources, `tools/upstream-sources.cmake`
  writes `sources.txt`, and `tests/runners/run_sources.cmake` tests the second.
  Three names stand for two things.
- `no_outline` at `CMakeLists.txt:541` also holds the two frame pointer
  flags from line 554 on.
- The prefix of a build name is `ANTIC_` or `antic_` in most places and
  `ANTI_` or `anti_` in others: `ANTI_HOOKS_DIR`, `ANTI_GIT`,
  `ANTI_STD_COLLECTION_DIR`, `anti_cross_runtime`,
  `anti_std_collection_modules`.

### Small copies inside the tools

`tools/release.sh:975`, structure, a small duplication, minor, new.

- `tools/release.sh` writes the two `openssl` lines that verify a
  signature three times, at lines 975, 1006 and 1220.
- Step 10, `verify` at line 1257, builds a hello program for the host and
  links one for the other five targets. `tools/check-offline.cmake:173`
  does the same and reads the format of each file with llvm-readobj. The
  two have the same aim and the second checks more.
- `tools/pack-anti.cmake` reads `tools/llvm-version` at lines 71 and 94,
  repeats one DESIGN comment at lines 138 and 331, and spells
  `--ld-path=` at line 226 beside `antic_pinned_link_options`.
- `tools/cpu-levels` has four readers: `CMakeLists.txt:184`,
  `tools/pack-anti.cmake:194`, `tools/check-cpu.cmake:31` and
  `tests/runners/run_cpu_archive.cmake`.
- The resource directory of clang is asked for at `CMakeLists.txt:526`
  and `601`, at `src/native/CMakeLists.txt:27` and at
  `tests/CMakeLists.txt:1357` and `1366`.
- `tools/keys/public/release.pem` holds the same bytes as
  `tools/keys/release.pem`. Every script reads the second, and no file of
  the area names the first.

### Registrations are written out where a helper would serve

`src/native/pcre2.cmake:97`, structure, a small duplication, minor, new.

The second audit asked for helpers at `tests/CMakeLists.txt:479`, and
`antic_program_test`, `antic_dump_test` and `antic_modules_test` hold the
runners it named. The recipes of `src/native/` were outside that finding.
Each of the five registers a run test and a link test with the same seven
arguments, and `raylib.cmake` does it twice: twelve blocks.
`tests/CMakeLists.txt` keeps 22 raw sites over five other runners,
`run_asm.cmake` at 4737, `run_warnings.cmake` at 2505,
`run_trace.cmake` at 3937, `run_cpu_level.cmake` at 724 and
`run_clib.cmake` at 4311. Twelve more compile a library file as a fixture
with one block, from line 3111 on.

### The test build file follows its history, and the runners lie flat

`tests/CMakeLists.txt:1`, structure, one kind per directory, minor, still
open.

The second audit reported that "The 108 scripts lie flat in `tests/`".
The closure leaves it, since a new directory under `tests/` is Eddie's
decision. The scripts are 157 now. The file that registers them has 4961
lines in 44 sections, in the order the features were written. Error tests
stand in six places, dumps in five, ABI tests in four and memory checks
in three. `llvm_targets` is set at line 599 and read at 2386, and a helper
is defined at line 3528. `docs/audit/structure.md` reports that the
opening comment names two kinds of test. The file can split into include
files beside the runners with no new directory: helpers, runtime
programs, programs and ABI, diagnostics, dumps, links, modules, the anti
tool, pins and release, benchmarks.

### Two presets describe themselves wrongly

`CMakePresets.json:23`, documentation, a description that differs from the
build, minor, new.

The `asan` preset is described as "Debug build of antic and its runtime
under AddressSanitizer". The runtime libraries are cross builds with the
flags written at `CMakeLists.txt:375`, and `CMAKE_C_FLAGS` never reaches
them. The `ubsan` preset runs AddressSanitizer too, by its own
`-fsanitize=address,undefined`. Its build and test presets set no
`ASAN_OPTIONS=detect_leaks=1`, which the `asan` presets set, and it has no
`-g`.

### The docs-style rules are not in the repository, and a compiled file is

`tools/docs-style/banned-terms.md:6`, documentation, the path from
CLAUDE.md, minor, new.

`CLAUDE.md` says every comment follows the docs-style rules, and
`docs/decisions.md:1273` says the comments of CMake files follow "the
docs-style rules for comments" without the checker. No file of the
repository states those rules. `tools/docs-style/` holds the checker and
the list of banned terms, and that list names `scripts/check_docs.py`, a
path of the skill it was copied from. A reader learns the rules for a
comment from the source of the checker. The directory also holds
`__pycache__/check_docs.cpython-314.pyc`, which `docs/audit/structure.md`
reports. The test `repo_layout` reads nothing below `tools/`, so it does
not refuse the file.

### Three guards have holes

`tests/runners/run_warning_set.cmake:14`, Safety and C, the bar of `tests/`, minor,
new.

- `warning_set` globs `src/native/*.cmake` and not
  `src/native/CMakeLists.txt`, so a warning flag written there passes.
  `docs/c-guidelines.md` says the test "refuses a warning flag spelled
  anywhere else".
- `repo_layout` at `tests/runners/run_repo_layout.cmake:80` checks a directory
  under `src/`, `tests/` and `docs/` and needs two slashes. A file placed
  directly in `src/` passes, and so does anything under `tools/` and
  `LICENSES/`.
- `rt_names` at `tests/runners/run_rt_names.cmake:70` reads a line that holds
  ` g `. A weak definition on ELF or Mach-O prints ` w ` and is not read,
  while the COFF branch above it handles weak names.

No tracked file slips through any of the three today.

### A test runs the nm of the host

`tests/runners/run_clib.cmake:391`, structure, one way to run a tool, minor, new.

The case `exports` runs `nm -gU` on a Mac and `nm -D --defined-only` on
Linux, at lines 391 and 399. The pinned release carries no nm.
`tools/pinned-tools.cmake:13` says of nm and its kind that "no rule of
this build runs them", and the test `pinned_compiler` refuses a host
compiler in a runner. The Windows branch at line 393 uses llvm-readobj of
the runtime archive, and the runner already takes `LLVM_OBJDUMP`.

### The build from CLAUDE.md and the headers

A reader gets from `CLAUDE.md` to a first build only through `README.md`.
"Building" names `./c` and says the configure step installs clang and the
LLVM tools. The sysroots and raylib are named as things "the build also
needs", with their scripts and no command. The severe finding on
`tools/compile.sh` is where that path breaks.

Below that, the headers carry the reader well. Every script of `tools/`
that runs with `-P` opens with what it does and its command line. So do
the scripts of `tests/`, in the form "Run with cmake -P and these values".
The exceptions are in the findings above. The include
files of `tools/` say who includes them. What a reader cannot find is the
map: which scripts the configure step runs, which a person runs once, and
which the release runs. `docs/audit/structure.md` reports that no document
lists the files of `tools/`. A table of that kind in `CLAUDE.md`, of the
size of the one in the last section here, would close it.

### One place for the pins, the digests and the release steps

The pins are in one place each, with the exceptions of the finding on
versions. Every pin is a file of `tools/` with a comment that says where
its digest comes from. The digests of clang and the LLVM tools are checked
three ways in `tools/fetch-release.cmake`, against the pin, the manifest
of the release and its signature. The digest of the Apple SDK is checked
in `tools/macos-sdk.cmake`, and every source archive in its fetch script.
The seams between a pin and a second spelling are held by tests where one
exists: `release_pins`, `cpu_levels_pin`, `installer_github`,
`release_key` and the six `<name>_pin` tests. The seams without a test are
`raylib-6.0` in the release script, `MUSL_APK`, glibc 2.35 in
`check-libc.cmake` and the versions in the comments of
`src/native/warnings.cmake`.

The release steps are in one place, `tools/release.sh`, numbered as
`docs/work-order-release-script.md` numbers them, with one function per
step. What stands outside it is the upload half of `tools/publish.cmake`,
which no step runs.

## The sentence of each file

The sentence is the purpose as read from the code. The last column says
whether the header of the file gives it. "Says it" means the header gives
the purpose and, for a script, how it is run. A pin has no code, and its
sentence is what the pin holds.

### The root

| File | Purpose | Header |
|---|---|---|
| `CMakeLists.txt` | Builds antic, anti, the runtime of every target and level and the standard library, and lays out the runtime archive. | None. The file opens with one comment on the pinned compiler. |
| `CMakePresets.json` | Names the four build trees `host`, `release`, `asan` and `ubsan`. | Two descriptions differ, see the finding on the presets. |

### The scripts of tools

| File | Purpose | Header |
|---|---|---|
| `tools/check-cpu.cmake` | Refuses a package whose runtime levels are not those of `tools/cpu-levels`. | Says it. |
| `tools/check-libc.cmake` | Refuses a Linux program that needs a glibc above 2.35. | Says it. |
| `tools/check-llvm.cmake` | Checks the version of the LLVM tools in a directory and, with `DEST`, copies them into the runtime archive. | Says it. Two purposes, see the finding on the tools. |
| `tools/check-offline.cmake` | Builds a raylib program, a plugin host and five cross links from an install alone. | Says it. |
| `tools/check-pdb.cmake` | Checks the CodeView record of a Windows executable against the GUID of its PDB. | Says it. |
| `tools/clang-release.cmake` | Gives the version, the tag and the release page of the pinned clang to the build of anti. | Says it. |
| `tools/compile.sh` | Configures, builds and tests one of the four presets. | Says something else: the downloads of the first build. |
| `tools/deps-dir.cmake` | Names the directory of the pinned downloads. | Says it. |
| `tools/docs-style/check_docs.py` | Checks `.md` files against the docs-style rules. | Says it. |
| `tools/docs-style/banned-terms.md` | Lists the banned terms for a reader. | Names `scripts/check_docs.py`. |
| `tools/docs-style/__pycache__/check_docs.cpython-314.pyc` | A compiled copy of the checker, which should not be tracked. | None. |
| `tools/downloads.html.in` | The template of the downloads page that step 9 fills. | Says it. |
| `tools/fetch-release.cmake` | Downloads one archive of the LLVM release and checks it against the pin, the manifest and the signature. | Says it, and also holds `antic_machine_host`. |
| `tools/get-clang.cmake` | Installs the pinned clang of the host. | Says it. |
| `tools/get-llvm.cmake` | Installs the pinned LLVM tools of the host, or lays out those of another host for the packer. | Says it. Calls llvm-mc the assembler. |
| `tools/get-raylib.cmake` | Downloads and unpacks the pinned raylib source. | Says it, and not who runs it. |
| `tools/get-sysroot.cmake` | Installs the sysroot of each named target and the licences of its components. | Says it. |
| `tools/git-hooks.cmake` | Points `core.hooksPath` of the checkout at `tools/hooks`. | Says it. |
| `tools/hooks/pre-commit` | Refuses a commit whose staged `.md` files fail the checker. | Says it. |
| `tools/host-compile.cmake` | Gives the options of the pinned clang that compile antic or anti for one host. | Says it. |
| `tools/install.sh` | Installs the package of a macOS or Linux host after checking the signature and the digest. | Says it. |
| `tools/install.ps1` | The same for a Windows host. | Says it. One DESIGN comment is stale. |
| `tools/libc-versions.cmake` | Gives the versions of musl and mimalloc to the build of antic. | Says it. |
| `tools/macos-sdk.cmake` | Finds the pinned Apple SDK and checks the digest of its `libSystem.tbd`. | Says "a release", and every Mac build uses it. |
| `tools/pack-anti.cmake` | Compiles antic and anti for each named host and writes its package and the manifest. | Says it. Names `build/clang`, and does not list the bitcode. |
| `tools/pinned-compiler.cmake` | Fetches the pinned tools and sets the compiler, the archiver and the linker of a build. | Says it. |
| `tools/pinned-tools.cmake` | Holds the list of the LLVM tools and the checks that refuse a tool of the host. | Says it. |
| `tools/publish.cmake` | Checks that a release directory holds what its signed manifest names and nothing else. | Says something else: an upload. |
| `tools/release.sh` | Makes a release in ten steps, from the preflight to the report. | Says it. |
| `tools/sources.cmake` | Lists the C sources of antic and anti for the build and the packer. | Says it. |
| `tools/std-modules.cmake` | Orders the collection modules of the standard library by their imports. | Says it. |
| `tools/uninstall.sh` | Removes an install that carries the marker, with its executables and its profile line. | Names an address that is not published. |
| `tools/uninstall.ps1` | The same for Windows. | Names an address that is not published. |
| `tools/upstream-sources.cmake` | Reads pins, builds the URL of a pinned source and writes `licenses/sources.txt`. | Says it. Three jobs under one name. |
| `tools/warnings.cmake` | Holds the warning set of our own C. | Says it. |
| `tools/windows-compile.cmake` | Holds the triples, the compile options and the link options of a Windows target. | Says it. |

### The pins and data of tools

| File | Holds | Header |
|---|---|---|
| `tools/clang-pin` | The release, the asset name and six digests of the pinned clang. | Says it. |
| `tools/llvm-pin` | The same for the LLVM tools. | Says it. |
| `tools/llvm-version` | The LLVM version, one line. | A data file with no comment. |
| `tools/version` | The version of Anti, one line. | A data file with no comment. |
| `tools/cpu-levels` | The processor levels and the default level of each target. | Says it. One column is of llvm-mc. |
| `tools/sysroot-pins` | The pins of musl, the glibc packages and mingw-w64. | Names `get-sysroot.sh`. |
| `tools/zig-stubs-pin` | The pin of the Zig release and of the APSL text. | Says it. |
| `tools/macos-sdk-pin` | The version, the name and the digest of the Apple SDK. | Says "a release", as the script does. |
| `tools/raylib-pin` | The pin of raylib and the miniaudio version it bundles. | Says it. |
| `tools/pcre2-pin` | The pin of PCRE2. | Says it. |
| `tools/sqlite-pin` | The pin of SQLite. | Says it. |
| `tools/mbedtls-pin` | The pin of Mbed TLS. | Says it. |
| `tools/miniaudio-pin` | The pin of miniaudio. | Says it. |
| `tools/mimalloc-pin` | The pin of mimalloc. | Says it. |
| `tools/release-base` | The addresses of the GitHub release. | Says it. |
| `tools/site-base` | The addresses of the site, the signature and the key. | Says it. |
| `tools/keys/release.pem` | The public key of the release. | A key file. |
| `tools/keys/public/release.pem` | A second copy of the public key, which no script of the area reads. | A key file. |

### The recipes of src/native

| File | Purpose | Header |
|---|---|---|
| `src/native/get-pcre2.cmake` | Downloads and unpacks the pinned PCRE2 source. | Says it. |
| `src/native/get-sqlite.cmake` | The same for the SQLite amalgamation. | Says it. |
| `src/native/get-mbedtls.cmake` | The same for Mbed TLS. | Says it. |
| `src/native/get-miniaudio.cmake` | The same for miniaudio. | Says it. |
| `src/native/get-mimalloc.cmake` | The same for mimalloc. | Says it. |
| `src/native/warnings.cmake` | Holds the warning flags of each third-party library. | Says it. |
| `src/native/pcre2-files.cmake` | Lists the sources and the definitions of PCRE2 for the build and the packer. | Says it. |
| `src/native/pcre2-source.cmake` | Fetches PCRE2, writes its generated files and builds the copy antic links. | Says it. |
| `src/native/pcre2.cmake` | Builds PCRE2 and the regex glue of the runtime for every target, with their tests. | Says it. |
| `src/native/sqlite.cmake` | Builds SQLite for every target, with its tests. | Names `anti.db`, which does not exist. |
| `src/native/mbedtls.cmake` | Builds Mbed TLS for every target, with its tests. | Names `anti.net` and a client anti does not have. |
| `src/native/media.cmake` | Holds the targets, the flags and the glibc link test of miniaudio and raylib. | Says it. Names two modules that do not exist. |
| `src/native/miniaudio.cmake` | Builds miniaudio for every media target, with its tests. | Names `anti.miniaudio`. |
| `src/native/raylib.cmake` | Builds raylib for every media target, with its tests and the audio link test. | Names `anti.raylib`. |
| `src/native/mimalloc.cmake` | Builds mimalloc for both musl targets at every level. | Says it. |
| `src/native/CMakeLists.txt` | Holds the helpers of the recipes and includes them in order. | Says it. One comment on the flags is wrong. |

### The test build file and its include

| File | Purpose | Header |
|---|---|---|
| `tests/CMakeLists.txt` | Registers every test of the suite, builds the C programs of the tests, and builds `runtime/bin`. | Says less: two kinds of test. |
| `tests/program_output.cmake` | Holds the four functions that run a program with its output in files and check status and bytes. | Opens with a DESIGN comment and no purpose. |

### The runners from address_tables to deps_dir

| File | Purpose | Header |
|---|---|---|
| `tests/runners/run_address_tables.cmake` | Checks that an object and the runtime archives of a target carry the address-significance table. | Says it. |
| `tests/runners/run_alloc_oom.cmake` | Runs the allocation helpers out of memory and expects status 70. | Says it. |
| `tests/runners/run_anti_bind.cmake` | Runs one of ten cases of `anti bind`. | Says it. |
| `tests/runners/run_anti_build.cmake` | Drives `anti build`, `anti run` and `anti new` over a project of two modules. | Says less, and lists `OBJDUMP`, which is not read. |
| `tests/runners/run_anti_build_deps.cmake` | Drives `anti build` over a path dependency and a `file://` repository. | Says it. |
| `tests/runners/run_anti_build_framework.cmake` | Builds a project that names a framework and checks the program loads it. | Says it. |
| `tests/runners/run_anti_build_inputs.cmake` | Feeds `anti build` a malformed index and lock file. | Says it. Cites a summary that does not exist. |
| `tests/runners/run_anti_build_rerun.cmake` | Runs and rebuilds a program in turns on a Windows host. | Says it. |
| `tests/runners/run_anti_build_resolve.cmake` | Checks the resolver and the search over more than one repository. | Says less: the first part alone. |
| `tests/runners/run_anti_build_running.cmake` | Builds a changed program while the old one runs. | Says it. |
| `tests/runners/run_anti_check.cmake` | Drives `anti check` over fixture projects and the standard library. | Says it. |
| `tests/runners/run_anti_doc.cmake` | Drives `anti doc` over a fixture in its forms. | Says it. |
| `tests/runners/run_anti_fmt.cmake` | Checks the canonical form of one file and `--check`. | Says it. |
| `tests/runners/run_anti_fmt_write.cmake` | Checks that `anti fmt` leaves a source whole when a write fails. | Says it. |
| `tests/runners/run_anti_interface.cmake` | Checks by its includes that anti reaches antic through the interface alone. | Says it. |
| `tests/runners/run_anti_internal.cmake` | Runs check, build, test and doc over a project with an internal item. | Says it. |
| `tests/runners/run_anti_memory_checks.cmake` | Drives `anti run` and `anti test` with `--memory-checks`. | Says it. |
| `tests/runners/run_anti_memory_dll.cmake` | Cross builds for windows-x86_64 with memory checks and looks for the DLL. | Says it. |
| `tests/runners/run_anti_sdk.cmake` | Checks `anti sdk export` and `anti sdk import`. | Says it. |
| `tests/runners/run_anti_symbols.cmake` | Drives `anti symbols inventory`, `check` and `resolve`. | Says it. |
| `tests/runners/run_anti_symbols_input.cmake` | Checks that `anti symbols resolve` refuses numbers beyond 64 bits. | Says it. |
| `tests/runners/run_anti_test.cmake` | Drives `anti test` over fixture modules. | Says it. |
| `tests/runners/run_antic_stack.cmake` | Compiles the deepest accepted expression on a small stack. | Says it. |
| `tests/runners/run_antl.cmake` | Compares the bytes of a library file with a hex listing. | Says it. |
| `tests/runners/run_antl_verify.cmake` | Changes one byte of a library file and expects the refusal of the verifier. | Says it. |
| `tests/runners/run_asm.cmake` | Writes assembly with `-S` and assembles it with llvm-mc. | Says it. |
| `tests/runners/run_asserts.cmake` | Checks `assert` in each build mode and through a library file. | Says it. |
| `tests/runners/run_atomic_load.cmake` | Checks the instruction of the atomic load in an ARM64 runtime. | Says it. |
| `tests/runners/run_bench.cmake` | Builds and times the benchmark programs and their C twins. | Says it. Names `--backend,native`. |
| `tests/runners/run_build_id.cmake` | Checks that links of one program share a build id. | Says less. |
| `tests/runners/run_check.cmake` | Builds a check program in both modes and expects the dev build to stop. | Says it. |
| `tests/runners/run_checks.cmake` | Checks `--checks`, `--no-checks` and a library whose checks follow the build. | Says it. |
| `tests/runners/run_classlib.cmake` | Compiles a library with a class and a client of it. | Says it. |
| `tests/runners/run_clib.cmake` | Runs one of twenty cases of libraries for C. | Says `CROSS` is for Windows, and macOS passes it too. |
| `tests/runners/run_codeview_empty.cmake` | Meant to check for an empty CodeView subsection. | Says what the code cannot check. |
| `tests/runners/run_conf_include_depth.cmake` | Checks that the runtime and `anti symbols` stop at the same include depth. | Says it. |
| `tests/runners/run_cpu_archive.cmake` | Checks that each target of the archive has a runtime per level. | Says it. |
| `tests/runners/run_cpu_check.cmake` | Runs a program above the level of the machine and expects the refusal. | Says it. |
| `tests/runners/run_cpu_level.cmake` | Checks the dev assembly of one level for patterns. | Says it. |
| `tests/runners/run_cpu_pin.cmake` | Compares `antic --print-cpu-levels` with `tools/cpu-levels`. | Says it. |
| `tests/runners/run_cpu_runtime.cmake` | Checks the instructions of one runtime library. | Says it. |
| `tests/runners/run_cross_link.cmake` | Links a program for a target and checks the format of the file. | Names one source, and takes any. |
| `tests/runners/run_dead_code.cmake` | Checks which functions a link keeps and drops. | Says it. |
| `tests/runners/run_debug.cmake` | Checks the line table of `-g` and three debugger sessions. | Says it. |
| `tests/runners/run_deps_dir.cmake` | Configures a copy of the tree with `ANTI_DEPS_DIR` and checks nothing is downloaded. | Says it. |

### The runners from dev to manifest_signature

| File | Purpose | Header |
|---|---|---|
| `tests/runners/run_dev.cmake` | Builds a program in dev mode from sources and from library files and compares both. | Says "assembly", and compares LLVM text. |
| `tests/runners/run_direct_calls.cmake` | Refuses an indirect call in one function of a release build. | Says it. |
| `tests/runners/run_distinct_addresses.cmake` | Checks that distinct functions keep distinct addresses under a link mode. | Says it. |
| `tests/runners/run_dump.cmake` | Compares the output of antic with an expected file. | Says it. |
| `tests/runners/run_emit_identity.cmake` | Compares the digests of the LLVM text of every program with those of the Mac. | Says it. |
| `tests/runners/run_errors_registered.cmake` | Checks that every error program is named by a test. | Says it. |
| `tests/runners/run_expect.cmake` | Runs a command and checks its status and output. | Says it. |
| `tests/runners/run_expect_self.cmake` | Tests `run_expect.cmake` and refuses `PASS_REGULAR_EXPRESSION`. | Says it. |
| `tests/runners/run_files_oom.cmake` | Runs the file helpers out of memory and expects status 70. | Says it. |
| `tests/runners/run_find_order.cmake` | Refuses a variable read above the `find_program` that sets it. | Says it. One comment is stale. |
| `tests/runners/run_float_digits.cmake` | Checks that three runtime members name no printf or scanf function. | Says it. One comment is stale. |
| `tests/runners/run_fmt_canonical.cmake` | Runs `anti fmt --check` over the sources of the tree. | Says it. |
| `tests/runners/run_framework.cmake` | Links a program with a framework, or expects the refusal without an SDK. | Says it. |
| `tests/runners/run_front_end.cmake` | Runs `antic --front-end` on a good and a bad program. | Says it. One comment contradicts a check. |
| `tests/runners/run_function_alignment.cmake` | Checks the 16-byte alignment of ARM64 functions. | Says it. |
| `tests/runners/run_glibc_link.cmake` | Links a C probe of a native library against the glibc sysroot. | Says it. No `cmake_minimum_required`. |
| `tests/runners/run_glibc_sysroot.cmake` | Checks the glibc pins and runs the sysroot script on stand-in packages. | Says it. |
| `tests/runners/run_hash_seed.cmake` | Runs a program twice and expects two seeds. | Says it. |
| `tests/runners/run_header_names.cmake` | Refuses one of our headers included with angle brackets. | Says it. |
| `tests/runners/run_header_sections.cmake` | Checks that each function under a section of a shared header is defined in that file. | Says it. |
| `tests/runners/run_host_sources.cmake` | Compiles every source of antic and anti for one host under the warning set. | Says it. |
| `tests/runners/run_installer_alone.cmake` | Runs the installer with a failing cmake on the path and refuses words of removed steps. | Says it. |
| `tests/runners/run_installer_github.cmake` | Pins the addresses in both installers and runs one against a fake release. | Says it. |
| `tests/runners/run_installer_options.cmake` | Pins the options, the install roots and the marker across the four install scripts. | Says less: the options alone. |
| `tests/runners/run_internal.cmake` | Checks that an internal item is refused from another package. | Lists two variables it does not read. |
| `tests/runners/run_ir_line.cmake` | Requires one line in the IR dump. | Says it. |
| `tests/runners/run_license_forms.cmake` | Checks one form of `anti license`. | Says it. |
| `tests/runners/run_license_notice.cmake` | Checks the notice of three programs against `NOTICE.txt`. | Claims a static library it does not build. |
| `tests/runners/run_licenses.cmake` | Finds the licence notice in the bytes of a program. | Says it. |
| `tests/runners/run_line_endings.cmake` | Requires LF in the programs and expected files. | Says it. |
| `tests/runners/run_link_identity.cmake` | Compares the digest of a linked program with a stored one. | Says it. |
| `tests/runners/run_linux_libc.cmake` | Links a static stand-in for both Linux hosts and runs `check-libc.cmake` on it. | Says something else: the recipe of the packer. |
| `tests/runners/run_linux_modes.cmake` | Builds a static and a dynamic Linux program and reads their ELF headers. | Says it. |
| `tests/runners/run_llvm_atomics.cmake` | Requires atomic instructions in the dev LLVM text of six targets. | Says it. |
| `tests/runners/run_llvm_coff_plugin.cmake` | Checks the imports and exports of a Windows plugin and its host. | Says it. |
| `tests/runners/run_llvm_datalayout_pin.cmake` | Compares the data layout and the features antic prints with the pinned clang. | Says it. |
| `tests/runners/run_llvm_inline_lines.cmake` | Checks that an inlined callee keeps its line under `-g`. | Says it. |
| `tests/runners/run_llvm_names.cmake` | Checks that `get-llvm.cmake` does not rewrite an lld name in place. | Says it. |
| `tests/runners/run_llvm_program.cmake` | Builds and runs one program for a macOS target, a second form of `run_program.cmake`. | Says it. |
| `tests/runners/run_llvm_run.cmake` | Checks `--keep-llvm`, the kept files of each mode and `-S`. | Says it. |
| `tests/runners/run_llvm_verify.cmake` | Runs the verifier of opt over the LLVM text of every program. | Says it. |
| `tests/runners/run_lock_order.cmake` | Requires the lock-order reports of a dev build. | Says it. |
| `tests/runners/run_lto_default.cmake` | Checks which link takes bitcode and which objects. | Says it. |
| `tests/runners/run_macos_sdk.cmake` | Checks that the SDK script finds the pinned SDK and refuses a changed one. | Says it, and not that it skips on any failure. |
| `tests/runners/run_macos_sysroot.cmake` | Checks the Zig pin and runs the sysroot script on stand-ins. | Says less: not the Apple SDK half. |
| `tests/runners/run_manifest_signature.cmake` | Runs the installer against a signed, a wrong, an unsigned and a staging manifest. | Says it. |

### The runners from may_fail_only to release_dry_run

| File | Purpose | Header |
|---|---|---|
| `tests/runners/run_may_fail_only.cmake` | Refuses a function written by hand with an error result. | Says it. |
| `tests/runners/run_memory_checks.cmake` | Checks what `--memory-checks` adds and reports per target. | Says each report names a function, and the code expects one name. |
| `tests/runners/run_memory_checks_list.cmake` | Checks the report of a pointer kept across a push. | The same. |
| `tests/runners/run_mimalloc_environment.cmake` | Runs a musl program with two variables of mimalloc. | Says it. |
| `tests/runners/run_modules.cmake` | Builds library modules and a program in one mode and compares the output. | `LIB_OPTIONS` does not reach the dev object. |
| `tests/runners/run_musl_allocator.cmake` | Reads the symbols of a musl program for the allocator. | Says it. |
| `tests/runners/run_mutex_size.cmake` | Reads the size of a Mutex for six targets. | Says it. |
| `tests/runners/run_names_once.cmake` | Refuses four shared names outside their header. | Says "every other file of src/", and skips `src/native/`. |
| `tests/runners/run_native_licenses.cmake` | Compares the licence texts of the repository with those of the runtime tree. | Says it. No `cmake_minimum_required`. |
| `tests/runners/run_native_link.cmake` | Links a test program against a native library for one target. | Says it. No `cmake_minimum_required`. |
| `tests/runners/run_native_pin.cmake` | Checks the form of a pin and that its version and digest stand nowhere else. | Says the URL too, which is not checked. |
| `tests/runners/run_native_pin_self.cmake` | Tests `run_native_pin.cmake` on a made-up tree. | Says it. |
| `tests/runners/run_no_paths.cmake` | Refuses the path of the checkout in the runtime libraries and in antic. | Says it. |
| `tests/runners/run_no_sdk_fetcher.cmake` | Refuses the name of the removed SDK fetcher. | Says it. |
| `tests/runners/run_no_workflows.cmake` | Refuses a file under `.github/workflows/`. | Says it. |
| `tests/runners/run_one_allocator.cmake` | Refuses an allocation outside `src/antic/alloc.c`. | Opens with the bare label `M03`. |
| `tests/runners/run_overview_examples.cmake` | Puts every `anti` block of the overview through the front end. | Says it. |
| `tests/runners/run_own_tools.cmake` | Shows that antic takes no tool or sysroot from the machine. | Says it. |
| `tests/runners/run_package.cmake` | Packs the host package and runs thirteen checks on it. | Says less: the key. |
| `tests/runners/run_pdb_guid.cmake` | Runs `check-pdb.cmake` on a linked Windows program. | Says it. |
| `tests/runners/run_pdb_names.cmake` | Checks the names in a PDB and that it holds no build path. | Says it. |
| `tests/runners/run_pinned_compiler.cmake` | Checks that the cache names the pinned clang and no runner a host compiler. | Says it. |
| `tests/runners/run_pinned_tools.cmake` | Runs the tool check of the configure step on the cache and on changed copies. | Says it. |
| `tests/runners/run_platform_layer.cmake` | Refuses a host `#if` outside the platform files. | Says it. |
| `tests/runners/run_plugin.cmake` | Builds a plugin and its hosts and checks loading and injection. | Says less. |
| `tests/runners/run_plugin_versions.cmake` | Loads six versions of a library to check the version rules. | Says "one host", and builds three. |
| `tests/runners/run_pre_commit.cmake` | Drives the pre-commit hook in a scratch repository. | Lists `PYTHON`, which only decides a skip. |
| `tests/runners/run_probe.cmake` | Compares the ABI probe in C with the one in Anti. | Says it. |
| `tests/runners/run_profile.cmake` | Builds with a profile, merges it and builds again. | Says it. |
| `tests/runners/run_program.cmake` | Compiles one program, runs it and compares with the expected file. | Says it. |
| `tests/runners/run_publish_manifest.cmake` | Runs `publish.cmake` in its check mode over a whole area and five broken ones. | Says it. |
| `tests/runners/run_raw_output.cmake` | Checks the raw bytes rule and scans every runner for it. | Says less: three parts are missing. |
| `tests/runners/run_raylib_miniaudio_pin.cmake` | Checks that the miniaudio raylib bundles is the pinned one. | Says it. No `cmake_minimum_required`. |
| `tests/runners/run_release_dry_run.cmake` | Runs `./r --dry-run` on a clone with stand-in tools, in eight parts. | Says less: two parts are missing. |

### The runners from release_key to windows_sysroot

| File | Purpose | Header |
|---|---|---|
| `tests/runners/run_release_key.cmake` | Checks that both installers hold the public key and that a package carries none. | Names the LLVM tools, and omits the tag check. |
| `tests/runners/run_release_pins.cmake` | Checks the form of the two LLVM pins and that no script spells them. | Says less: not the directory check. |
| `tests/runners/run_release_site.cmake` | Checks the order of the setgid step and the mode of every rsync of step 9. | Says it. |
| `tests/runners/run_repo_layout.cmake` | Checks every tracked path against the allowed entries. | Says it. |
| `tests/runners/run_rt_extensions.cmake` | Refuses a compiler extension in the runtime outside five files. | Says it. |
| `tests/runners/run_rt_names.cmake` | Refuses an exported name of the runtime outside its prefixes. | Says "every symbol", and weak ones are not read. |
| `tests/runners/run_rt_wide_text.cmake` | Refuses a call of an ANSI entry point of Windows in the runtime. | Says it. |
| `tests/runners/run_runtime_frames.cmake` | Requires the frame record in `main` of every Linux and macOS runtime. | Says it. |
| `tests/runners/run_runtime_from_data_dir.cmake` | Checks that antic finds the archive of the data directory. | Says it. |
| `tests/runners/run_runtime_hosts.cmake` | Refuses outline atomics in the linux-arm64 runtime. | Says it. |
| `tests/runners/run_script_policies.cmake` | Requires `cmake_minimum_required` in the scripts two list files name. | Says "every script". |
| `tests/runners/run_shared_inputs.cmake` | Builds shared libraries with three options and reads their dependencies. | Says it, and not the silent skip. |
| `tests/runners/run_simd_width.cmake` | Counts the lines of one instruction in the assembly of a program. | Says it. |
| `tests/runners/run_source_cap.cmake` | Checks the size limit of a source. | Says it. |
| `tests/runners/run_source_nul.cmake` | Checks the refusal of a NUL byte at eleven positions. | Says it. |
| `tests/runners/run_sources.cmake` | Checks `licenses/sources.txt` against the pins and the licence texts. | Says less: not the Zig line. |
| `tests/runners/run_std_collection_modules.cmake` | Tests the ordering function of `tools/std-modules.cmake`. | Says it. |
| `tests/runners/run_std_tests.cmake` | Runs `anti test` on one module in both modes. | Says it. |
| `tests/runners/run_symbols.cmake` | Looks up two functions in a program built with `-g`. | Says it. |
| `tests/runners/run_symbols_layout.cmake` | Compares the sections of a shipped program with those of its debug link. | Says it. |
| `tests/runners/run_sysroot_links.cmake` | Checks that the sysroot script turns an absolute link relative. | Says it. |
| `tests/runners/run_trace.cmake` | Runs a program and matches its output against pattern files. | Says it. |
| `tests/runners/run_trap.cmake` | Builds a program that must stop and compares its message. | Says it. |
| `tests/runners/run_trap_table.cmake` | Checks the trap of an unset table entry per build mode. | Says it. |
| `tests/runners/run_unicode_paths.cmake` | Runs antic and anti with names outside ASCII. | Says antic runs llvm-mc. |
| `tests/runners/run_unwind.cmake` | Compares the Windows unwind data of a program with an expected file. | Says it. |
| `tests/runners/run_warning_names.cmake` | Holds the warning table of antic equal to the note. | Says it. |
| `tests/runners/run_warning_set.cmake` | Refuses a warning flag outside the two warning files. | Says "any other build file", and one is not read. |
| `tests/runners/run_warnings.cmake` | Runs antic and requires or forbids texts in its messages. | Says it. |
| `tests/runners/run_windows_addresses.cmake` | Refuses 32-bit address relocations in a windows-x86_64 object. | Says it. |
| `tests/runners/run_windows_sysroot.cmake` | Checks the mingw-w64 pins and runs the sysroot script on a stand-in release. | Says less: not the pin checks. |
