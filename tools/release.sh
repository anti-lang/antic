#!/bin/sh
# Make a release of Anti, from a pushed main to a published download.
#
#   ./r
#   ./r --dry-run
#   ./r --resume
#   ./r --skip-vms
#
# A dry run performs steps 1 to 5 and prints a plan for the rest.
# --resume starts at the first step whose output is missing.
# --skip-vms leaves the two VMs out and marks a pre-release.
#
# The version stands in tools/version, with its entry in CHANGELOG.md.
# Everything a run writes goes under build/dist, and a rerun keeps what
# the earlier steps produced, so a failure is resumed rather than
# restarted. The preflight runs every time.
#
# Two steps hold a secret and stay manual. RELEASE_KEY names the
# encrypted private key, whose passphrase openssl asks for once, and gh
# holds the login that the preflight reads. Without the key the run
# prints the two signing commands and stops before the tag.
#
# docs/work-order-release-script.md holds the eleven steps.
set -eu

root=$(cd "$(dirname "$0")" && pwd)
[ -f "$root/tools/version" ] || root=$(dirname "$root")
[ -f "$root/tools/version" ] || {
    echo "r: tools/version is missing" >&2
    exit 1
}

version=$(sed -n '1p' "$root/tools/version" | tr -d ' \r\n')
tag=v$version
hosts='macos-arm64 macos-x86_64 linux-x86_64 linux-arm64 windows-x86_64 windows-arm64'
cpu_levels=$root/tools/cpu-levels
download_base=$(grep -v '^#' "$root/tools/download-base" | tr -d ' \r\n')

dry_run=no
skip_vms=no
for argument in "$@"; do
    case $argument in
    --dry-run) dry_run=yes ;;
    --resume) ;;
    --skip-vms) skip_vms=yes ;;
    *)
        echo "usage: ./r [--dry-run] [--resume] [--skip-vms]" >&2
        exit 2
        ;;
    esac
done

# DESIGN: a dry run writes under a directory of its own. No file it
# leaves behind stands in for the output of a step of a release. The two
# runs share nothing but the tree they read.
dist=$root/build/dist
[ "$dry_run" = no ] || dist=$root/build/dist/dry-run
state=$dist/state
logs=$dist/logs
# DESIGN: the packages and the symbols archives of a release stand in one
# directory under one SHA256SUMS. The packer writes that manifest, step 4
# extends it and step 6 signs it in place. The directory is what
# tools/publish.cmake uploads and what a user downloads from. A file
# beside the twelve is a file a user takes for part of the release.
packages=$dist/packages
work=$dist/work
# DESIGN: the PDB of a Windows program stands outside the package, because
# a package carries no symbols archive of its own. The packer writes it
# here and step 4 folds it into the symbols archive of its host.
symbols=$dist/symbols
export_tree=$dist/export

die() {
    printf 'r: %s\n' "$*" >&2
    exit 1
}

say() {
    printf '  %s\n' "$*"
}

# Print the title of a step, and answer whether it has to run. A step
# whose stamp stands in state/ ran in an earlier call of ./r.
starts() {
    if [ -f "$state/$1-$2" ]; then
        printf 'r: step %s, %s, done in an earlier run\n' "${1#0}" "$3"
        return 1
    fi
    printf 'r: step %s, %s\n' "${1#0}" "$3"
    return 0
}

# Record that a step is done. Its stamp holds the time it finished.
finished() {
    mkdir -p "$state"
    date -u '+%Y-%m-%dT%H:%M:%SZ' > "$state/$1-$2"
}

# Print the entry of the version from CHANGELOG.md, without its heading.
changelog_entry() {
    awk -v want="## $version " '
        index($0, want) == 1 { inside = 1; next }
        inside && /^## / { exit }
        inside { print }' "$root/CHANGELOG.md"
}

# Print the value of a cache entry of the default build.
cached() {
    sed -n "s/^$1:[A-Z]*=//p" "$root/build/CMakeCache.txt"
}

# Print the digest of a file, in the form SHA256SUMS holds.
digest_of() {
    shasum -a 256 "$1" | cut -d ' ' -f 1
}

# The file name of the package of a host, and of its symbols.
package_name() {
    printf 'anti-%s-%s.tar.xz\n' "$version" "$1"
}

symbols_name() {
    printf 'anti-%s-%s-symbols.zip\n' "$version" "$1"
}

# Step 1. Nothing is built before every one of these holds.
preflight() {
    printf 'r: step 1, preflight\n'
    [ "$(uname -s)" = Darwin ] ||
        die "a release is made on the development Mac, which packs every host"
    case $version in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) die "tools/version holds '$version', which is no version" ;;
    esac

    if git -C "$root" rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
        die "$tag is a tag of this checkout already, and a published version is never rebuilt"
    fi
    if [ -n "$(git -C "$root" ls-remote --tags origin "refs/tags/$tag")" ]; then
        die "origin holds the tag $tag already, and a published version is never rebuilt"
    fi
    say "$version is no tag here and none on origin"

    [ -f "$root/CHANGELOG.md" ] || die "CHANGELOG.md is missing"
    entry=$(changelog_entry)
    [ -n "$(printf '%s' "$entry" | tr -d ' \n')" ] ||
        die "CHANGELOG.md holds no entry '## $version <date>'"
    say "CHANGELOG.md holds the entry of $version"

    branch=$(git -C "$root" symbolic-ref --quiet --short HEAD || echo "")
    [ "$branch" = main ] || die "the branch is '$branch', and a release is made on main"
    git -C "$root" diff --quiet ||
        die "the tree holds an uncommitted change, and a release names a commit"
    git -C "$root" diff --cached --quiet ||
        die "the index holds an uncommitted change, and a release names a commit"
    head=$(git -C "$root" rev-parse HEAD)
    pushed=$(git -C "$root" ls-remote origin refs/heads/main | cut -f1)
    [ "$head" = "$pushed" ] ||
        die "origin holds $pushed of main and this tree $head. Push before a release."
    untracked=$(git -C "$root" ls-files --others --exclude-standard | wc -l | tr -d ' ')
    [ "$untracked" = 0 ] ||
        say "$untracked untracked files stay out of the release, which packs the commit"
    say "main is committed and pushed at $(echo "$head" | cut -c1-7)"

    gh auth status > /dev/null 2>&1 ||
        die "gh is not logged in. Run gh auth login."
    say "gh is logged in"

    if [ "$skip_vms" = yes ]; then
        say "the VMs are skipped, so the release is marked as a pre-release"
    else
        for machine in anti-linux anti-windows; do
            grep -q "^Host $machine\$" "$HOME/.ssh/config" ||
                die "~/.ssh/config names no $machine"
            ssh -n -o BatchMode=yes -o ConnectTimeout=20 "$machine" true ||
                die "$machine does not answer"
            say "$machine answers"
        done
    fi

    [ -f "$root/build/CMakeCache.txt" ] || {
        say "build/ holds no cache, so the downloads run first"
        cmake -S "$root" -B "$root/build" > "$dist/configure.log" 2>&1 ||
            die "the download step failed, and $dist/configure.log holds its output"
    }
    for name in ANTIC_CLANG_DIR ANTIC_LLVM_DIR ANTIC_SYSROOT_DIR ANTIC_RAYLIB_DIR; do
        value=$(cached "$name")
        [ -n "$value" ] || die "build/CMakeCache.txt names no $name"
    done
    # DESIGN: a release is of one commit. The state names the commit its
    # steps ran on. A run on another one starts over, rather than
    # publishing a package of one commit beside a suite of another.
    if [ -f "$state/head" ] && [ "$(cat "$state/head")" != "$head" ]; then
        say "the state is of $(cut -c1-7 < "$state/head"), so the steps run again"
        rm -rf "$state" "$logs" "$packages" "$work" "$export_tree"
    fi
    mkdir -p "$state"
    echo "$head" > "$state/head"

    clang_dir=$(cached ANTIC_CLANG_DIR)
    llvm_dir=$(cached ANTIC_LLVM_DIR)
    sysroot_dir=$(cached ANTIC_SYSROOT_DIR)
    raylib_dir=$(cached ANTIC_RAYLIB_DIR)
    llvm_bin=$llvm_dir/bin
    say "the downloads of build/ are in place"
}

# Step 2. The suite of this machine, in an export of the commit, and
# then the two sanitizer suites. The export carries the tracked files
# alone, so nothing of the working directory reaches a package.
suite() {
    starts 02 suite "the suite of the Mac, then ASan and UBSan" || return 0
    rm -rf "$export_tree"
    mkdir -p "$export_tree" "$logs"
    git -C "$root" checkout-index -a --prefix="$export_tree/" ||
        die "step 2: the export of the tree failed"
    say "the commit is exported to $export_tree"

    paths="-DANTIC_CLANG_DIR=$clang_dir -DANTIC_LLVM_DIR=$llvm_dir"
    paths="$paths -DANTIC_SYSROOT_DIR=$sysroot_dir -DANTIC_RAYLIB_DIR=$raylib_dir"
    # shellcheck disable=SC2086
    cmake -S "$export_tree" -B "$export_tree/build" $paths \
        > "$logs/mac-configure.log" 2>&1 ||
        die "step 2: the configure failed, see $logs/mac-configure.log"
    cmake --build "$export_tree/build" -j 8 > "$logs/mac-build.log" 2>&1 ||
        die "step 2: the build failed, see $logs/mac-build.log"
    ctest --test-dir "$export_tree/build" -j 8 > "$logs/mac.log" 2>&1 ||
        die "step 2: the suite failed, see $logs/mac.log"
    say "the Mac: $(grep 'tests passed' "$logs/mac.log")"

    for preset in asan ubsan; do
        (cd "$export_tree" && cmake --preset "$preset") \
            > "$logs/$preset-configure.log" 2>&1 ||
            die "step 2: the $preset configure failed, see $logs/$preset-configure.log"
        cmake --build "$export_tree/build-$preset" -j 8 \
            > "$logs/$preset-build.log" 2>&1 ||
            die "step 2: the $preset build failed, see $logs/$preset-build.log"
        ctest --test-dir "$export_tree/build-$preset" -j 8 \
            > "$logs/$preset.log" 2>&1 ||
            die "step 2: the $preset suite failed, see $logs/$preset.log"
        say "$preset: $(grep 'tests passed' "$logs/$preset.log")"
    done
    finished 02 suite
}

# Unpack the bin directory of a package into work/<host>.
unpack_binaries() {
    rm -rf "$work/$1"
    mkdir -p "$work/$1"
    tar -xf "$packages/$(package_name "$1")" -C "$work/$1" anti/bin ||
        die "$(package_name "$1") holds no anti/bin"
}

# The names of the two programs of a package, with the suffix of its host.
programs_of() {
    case $1 in
    windows-*) echo "antic.exe anti.exe" ;;
    *) echo "antic anti" ;;
    esac
}

# Step 3. One package per host, each checked before the next step reads
# it. The runtime levels come from tools/cpu-levels. The two Linux
# packages name no glibc above the pinned one, and the macOS x86_64
# package runs here under Rosetta.
build_packages() {
    starts 03 packages "the six packages" || return 0
    mkdir -p "$packages" "$work"
    list=$(echo "$hosts" | tr ' ' ';')
    rm -rf "$symbols"
    cmake -DDEST="$packages" -DCLANG="$clang_dir/bin/clang" \
        -DLLVM_BIN="$llvm_bin" -DSYSROOT="$sysroot_dir" \
        -DRUNTIME="$export_tree/build/runtime" -DHOSTS="$list" \
        -DSYMBOLS="$symbols" \
        -P "$root/tools/pack-anti.cmake" > "$logs/pack.log" 2>&1 ||
        die "step 3: the packer failed, see $logs/pack.log"

    for host in $hosts; do
        name=$(package_name "$host")
        [ -f "$packages/$name" ] || die "step 3: the packer wrote no $name"
        cmake -DARCHIVE="$packages/$name" -DLEVELS="$cpu_levels" \
            -P "$root/tools/check-cpu.cmake" > /dev/null ||
            die "step 3: $name carries the wrong runtime levels"
        say "$name, $(digest_of "$packages/$name" | cut -c1-12), levels of $cpu_levels"
    done

    for host in linux-x86_64 linux-arm64; do
        unpack_binaries "$host"
        for program in $(programs_of "$host"); do
            cmake -DBINARY="$work/$host/anti/bin/$program" \
                -DREADOBJ="$llvm_bin/llvm-readobj" \
                -P "$root/tools/check-libc.cmake" > /dev/null ||
                die "step 3: $host: $program links the libc of this machine"
        done
        say "$host: both programs link the pinned sysroot"
    done

    unpack_binaries macos-x86_64
    printed=$(arch -x86_64 "$work/macos-x86_64/anti/bin/antic" --version) ||
        die "step 3: the macos-x86_64 antic does not run under Rosetta"
    [ "$printed" = "antic $version" ] ||
        die "step 3: the macos-x86_64 antic prints '$printed'"
    say "macos-x86_64 under Rosetta: $printed"
    finished 03 packages
}

# Step 4. The symbols of every shipped program, beside the packages and
# never inside one. The packer passes no -g, so the symbol table of a
# binary is what names a frame of a report from a user.
#
# DESIGN: the section headers say nothing here. A static musl program
# carries the debug sections of the sysroot's own objects, whose strings
# name __libc_malloc and tsd_used and no source of antic. The first dry
# run stopped on them. What the archive needs is a symbol table with
# entries, and that is what the step reads.
#
# DESIGN: a Windows program has no symbol table at all, so the map of its
# sections goes in and its PDB with it. The packer linked that PDB with
# /DEBUG and left it in $symbols. What ties the two together is the
# CodeView record of the executable, which tools/check-pdb.cmake reads
# against the GUID the PDB carries. A record that names a path rather
# than a file name is refused there as well, because it would be the path
# of this machine.
build_symbols() {
    starts 04 symbols "the symbols of the twelve programs" || return 0
    mkdir -p "$packages" "$work"
    for host in $hosts; do
        name=$(package_name "$host")
        if tar -tf "$packages/$name" | grep -q 'symbols\.zip$'; then
            die "step 4: $name carries a symbols archive, which stands beside it"
        fi
        unpack_binaries "$host"
        rm -rf "$work/$host/syms"
        mkdir -p "$work/$host/syms"
        for program in $(programs_of "$host"); do
            binary=$work/$host/anti/bin/$program
            table=$work/$host/syms/$program.syms
            "$llvm_bin/llvm-objdump" --syms "$binary" > "$table" ||
                die "step 4: $host: llvm-objdump read no symbol of $program"
            # The three object formats spell a section of code their own
            # way, so the count is of the rows below the heading.
            rows=$(sed -n '/^SYMBOL TABLE:/,$p' "$table" | grep -c .)
            if [ "$rows" -le 1 ]; then
                # lld-link writes the symbols of a program to a PDB, so a
                # shipped .exe carries a table of no rows. The archive
                # then holds the map of the sections, which is what the
                # binary itself has, and the PDB below.
                "$llvm_bin/llvm-objdump" --section-headers "$binary" > "$table" ||
                    die "step 4: $host: llvm-objdump read no section of $program"
                say "$host: $program has no symbol table, so its sections go in"
            fi
            case $host in
            windows-*)
                pdb=$symbols/$host/${program%.exe}.pdb
                [ -f "$pdb" ] ||
                    die "step 4: $host: the packer left no PDB of $program at $pdb"
                cmake -DBINARY="$binary" -DPDB="$pdb" \
                    -DREADOBJ="$llvm_bin/llvm-readobj" \
                    -P "$root/tools/check-pdb.cmake" > /dev/null ||
                    die "step 4: $host: $program does not name the PDB beside it"
                cp "$pdb" "$work/$host/syms/" ||
                    die "step 4: $host: $pdb did not copy"
                ;;
            esac
        done
        case $host in
        windows-*) contents='./*.syms ./*.pdb' ;;
        *) contents='./*.syms' ;;
        esac
        archive=$packages/$(symbols_name "$host")
        rm -f "$archive"
        # The globs expand in the directory of the archive, which is why
        # they stay unquoted here.
        (cd "$work/$host/syms" && zip -q -j "$archive" $contents) ||
            die "step 4: $host: zip wrote no symbols archive"
        say "$(symbols_name "$host"), $(digest_of "$archive" | cut -c1-12)"
    done

    # The packer wrote a line per package. The six archives join them, so
    # that the directory of the release carries one manifest of twelve
    # files. The step is run again after a failure, and a line it wrote
    # before is replaced rather than doubled.
    manifest=$packages/SHA256SUMS
    [ -f "$manifest" ] || die "step 4: the packer wrote no SHA256SUMS"
    for host in $hosts; do
        grep -q "  $(package_name "$host")\$" "$manifest" ||
            die "step 4: SHA256SUMS names no $(package_name "$host")"
    done
    grep -v -e '-symbols\.zip$' "$manifest" > "$work/SHA256SUMS.new"
    for host in $hosts; do
        (cd "$packages" && shasum -a 256 "$(symbols_name "$host")") \
            >> "$work/SHA256SUMS.new"
    done
    sort -k2 "$work/SHA256SUMS.new" > "$manifest"
    rm -f "$work/SHA256SUMS.new"
    say "SHA256SUMS of $packages names $(wc -l < "$manifest" | tr -d ' ') files"
    finished 04 symbols
}

# Write the shell script that a Linux VM runs, and print its path. It
# extracts the tree, runs the suite, installs the package of that host
# from a directory of the machine, checks the install and removes it.
write_linux_script() {
    cat > "$work/vm-linux.sh" <<LINUX
set -eu
cd "\$HOME/antic-check"
tar -xmf "\$HOME/anti-release/tree.tar"
A=\$HOME/anti
opts="-DANTIC_CLANG_DIR=\$A/clang -DANTIC_LLVM_DIR=\$A/toolchain"
opts="\$opts -DANTIC_SYSROOT_DIR=\$A/sysroot -DANTIC_RAYLIB_DIR=\$A/raylib/raylib-6.0"
cmake -S . -B build \$opts > release-configure.log 2>&1
cmake --build build -j"\$(nproc)" > release-build.log 2>&1
ctest --test-dir build -j"\$(nproc)" > release-ctest.log 2>&1
grep 'tests passed' release-ctest.log

home=\$HOME/anti-release/installed
rm -rf "\$home"
if ANTI_VERSION=$version ANTI_BASE=file://\$HOME/anti-release \\
    ANTI_HOME=\$home ANTI_REPLACE=yes ANTI_PATH=no ANTI_MICROSOFT=no \\
    sh "\$HOME/anti-release/install.sh" > release-unsigned.log 2>&1; then
    echo "the installer took a manifest without a signature"
    exit 1
fi
grep -q 'SHA256SUMS.sig' release-unsigned.log || {
    echo "the installer stopped for another reason than the signature"
    cat release-unsigned.log
    exit 1
}
echo "the installer refuses a manifest without a signature"
rm -rf "\$home"
ANTI_VERSION=$version ANTI_BASE=file://\$HOME/anti-release ANTI_STAGING=yes \\
    ANTI_HOME=\$home ANTI_REPLACE=yes ANTI_PATH=no ANTI_MICROSOFT=no \\
    sh "\$HOME/anti-release/install.sh" > release-install.log 2>&1
"\$home/bin/antic" --version
"\$home/bin/anti" --version
cd "\$HOME/anti-release"
printf 'import anti.io;\n\nfn main() -> int\n{\n    io.print("hello");\n    return 0;\n}\n' > hello.anti
"\$home/bin/antic" hello.anti -o hello
./hello
ANTI_HOME=\$home ANTI_REMOVE=yes sh "\$HOME/anti-release/uninstall.sh" \\
    > release-uninstall.log 2>&1
[ ! -d "\$home" ] || { echo "the uninstaller left \$home"; exit 1; }
echo "the install of linux-arm64 is checked and removed"
LINUX
    printf '%s\n' "$work/vm-linux.sh"
}

# The same for the Windows VM, as a cmd file with CRLF line endings. The
# build there needs the environment of vcvarsall.bat, and a command file
# is what ssh runs on that machine.
write_windows_script() {
    cat > "$work/vm-windows.cmd" <<WINDOWS
@echo off
setlocal
set VC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
cd /d %USERPROFILE%\antic-check
tar -xmf %USERPROFILE%\anti-release\tree.tar
call "%VC%" arm64 > nul
if exist build (cmake -S . -B build > release-configure.log 2>&1) else (cmake -S . -B build -G Ninja > release-configure.log 2>&1)
if errorlevel 1 (echo the configure failed & exit /b 1)
cmake --build build > release-build.log 2>&1
if errorlevel 1 (echo the build failed & exit /b 1)
ctest --test-dir build -j4 > release-ctest.log 2>&1
if errorlevel 1 (echo the suite failed & findstr /R /C:"^	 *[0-9]* - " release-ctest.log & exit /b 1)
findstr /C:"tests passed" release-ctest.log
set ANTI_VERSION=$version
set ANTI_BASE=%USERPROFILE%\anti-release
set ANTI_HOME=%USERPROFILE%\anti-release\installed
set ANTI_REPLACE=yes
set ANTI_PATH=no
set ANTI_MICROSOFT=no
if exist "%ANTI_HOME%" rmdir /s /q "%ANTI_HOME%"
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\install.ps1 > release-unsigned.log 2>&1
if not errorlevel 1 (echo the installer took a manifest without a signature & exit /b 1)
findstr /C:"SHA256SUMS.sig" release-unsigned.log > nul
if errorlevel 1 (echo the installer stopped for another reason than the signature & type release-unsigned.log & exit /b 1)
echo the installer refuses a manifest without a signature
if exist "%ANTI_HOME%" rmdir /s /q "%ANTI_HOME%"
set ANTI_STAGING=yes
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\install.ps1 > release-install.log 2>&1
if errorlevel 1 (echo the install failed & type release-install.log & exit /b 1)
"%ANTI_HOME%\bin\antic.exe" --version
"%ANTI_HOME%\bin\anti.exe" --version
cd /d %USERPROFILE%\anti-release
set ANTI_REMOVE=yes
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\uninstall.ps1 > release-uninstall.log 2>&1
if exist "%ANTI_HOME%" (echo the uninstaller left %ANTI_HOME% & exit /b 1)
echo the install of windows-arm64 is checked and removed
WINDOWS
    # CRLF, which cmd reads and a Unix line ending breaks.
    awk '{ printf "%s\r\n", $0 }' "$work/vm-windows.cmd" > "$work/vm-windows.crlf"
    mv "$work/vm-windows.crlf" "$work/vm-windows.cmd"
    printf '%s\n' "$work/vm-windows.cmd"
}

# Send the tree, the package of a host and the four scripts of the two
# shells to a VM. The Windows machine answers to cmd rather than to a
# shell, so the directory it takes them in is made with its own commands.
send_to_vm() {
    machine=$1
    host=$2
    if [ "$machine" = anti-windows ]; then
        ssh -n -o BatchMode=yes "$machine" \
            'cmd /c rmdir /s /q %USERPROFILE%\anti-release' > /dev/null 2>&1 || true
        ssh -n -o BatchMode=yes "$machine" \
            "cmd /c mkdir %USERPROFILE%\\anti-release\\anti\\$version" ||
            die "step 5: $machine does not take a directory"
    else
        ssh -n -o BatchMode=yes "$machine" \
            "rm -rf anti-release && mkdir -p anti-release/anti/$version" ||
            die "step 5: $machine does not take a directory"
    fi
    scp -q -o BatchMode=yes "$work/tree.tar" "$machine:anti-release/tree.tar" ||
        die "step 5: the tree did not reach $machine"
    # DESIGN: the manifest travels without its signature, which step 6
    # writes after this step. The VM therefore checks both halves of the
    # rule: the installer refuses the unsigned manifest, and takes it
    # with ANTI_STAGING, which only a release sets.
    scp -q -o BatchMode=yes "$packages/$(package_name "$host")" \
        "$packages/SHA256SUMS" "$machine:anti-release/anti/$version/" ||
        die "step 5: the package did not reach $machine"
    scp -q -o BatchMode=yes "$root/tools/install.sh" "$root/tools/install.ps1" \
        "$root/tools/uninstall.sh" "$root/tools/uninstall.ps1" \
        "$machine:anti-release/" ||
        die "step 5: the installers did not reach $machine"
}

# Step 5. Both VMs run the suite of the commit and install the package
# of their host from the files of this run, before anything is published.
vm_checks() {
    if [ "$skip_vms" = yes ]; then
        printf 'r: step 5, the VMs, skipped by --skip-vms\n'
        return 0
    fi
    starts 05 vms "the two VMs" || return 0
    mkdir -p "$work" "$logs"
    (cd "$root" && git ls-files -z | xargs -0 tar cf "$work/tree.tar") ||
        die "step 5: the tree did not pack"

    send_to_vm anti-linux linux-arm64
    linux_script=$(write_linux_script)
    scp -q -o BatchMode=yes "$linux_script" anti-linux:anti-release/run.sh ||
        die "step 5: the script did not reach anti-linux"
    ssh -n -o BatchMode=yes anti-linux 'sh $HOME/anti-release/run.sh' \
        > "$logs/linux.log" 2>&1 ||
        die "step 5: anti-linux failed, see $logs/linux.log"
    say "anti-linux: $(grep 'tests passed' "$logs/linux.log" || echo 'the suite printed no count')"
    grep -q "antic $version" "$logs/linux.log" ||
        die "step 5: the install on anti-linux printed no antic $version"
    grep -q "refuses a manifest without a signature" "$logs/linux.log" ||
        die "step 5: the installer of anti-linux checked no signature"
    say "anti-linux: the package installs, compiles a program and uninstalls"

    send_to_vm anti-windows windows-arm64
    windows_script=$(write_windows_script)
    scp -q -o BatchMode=yes "$windows_script" 'anti-windows:anti-release/run.cmd' ||
        die "step 5: the script did not reach anti-windows"
    ssh -n -o BatchMode=yes anti-windows 'cmd /c %USERPROFILE%\anti-release\run.cmd' \
        > "$logs/windows.log" 2>&1 ||
        die "step 5: anti-windows failed, see $logs/windows.log"
    say "anti-windows: $(grep 'tests passed' "$logs/windows.log" || echo 'the suite printed no count')"
    grep -q "antic $version" "$logs/windows.log" ||
        die "step 5: the install on anti-windows printed no antic $version"
    grep -q "refuses a manifest without a signature" "$logs/windows.log" ||
        die "step 5: the installer of anti-windows checked no signature"
    say "anti-windows: the package installs and uninstalls"
    finished 05 vms
}

# The assets of the release: the six packages, the six symbols archives,
# the manifest and its signature.
release_files() {
    for host in $hosts; do
        printf '%s/%s\n' "$packages" "$(package_name "$host")"
    done
    for host in $hosts; do
        printf '%s/%s\n' "$packages" "$(symbols_name "$host")"
    done
}

# Step 6. The signature of the one manifest, written in place beside the
# twelve files it names. The packer wrote the lines of the six packages
# and step 4 added the six archives. This step adds no digest. It checks
# that the manifest is the whole release, and signs it.
digests() {
    manifest=$packages/SHA256SUMS
    hashed=$work/SHA256SUMS.sha256
    if [ "$dry_run" = yes ]; then
        printf 'r: step 6, the signature of the manifest\n'
        say "would sign $manifest with \$RELEASE_KEY into SHA256SUMS.sig"
        return 0
    fi
    starts 06 digests "the signature of the manifest" || return 0
    mkdir -p "$work"
    for file in $(release_files); do
        [ -f "$file" ] || die "step 6: $file is missing"
        grep -q "  $(basename "$file")\$" "$manifest" ||
            die "step 6: SHA256SUMS names no $(basename "$file")"
    done
    lines=$(wc -l < "$manifest" | tr -d ' ')
    [ "$lines" = 12 ] ||
        die "step 6: SHA256SUMS holds $lines lines, and a release has twelve files"
    say "SHA256SUMS names the twelve files of the release"

    # A signature made by hand, after an earlier run stopped without the
    # key, is taken when it verifies against the manifest of this run.
    if [ -f "$packages/SHA256SUMS.sig" ]; then
        openssl dgst -sha256 -binary -out "$hashed" "$manifest"
        if openssl pkeyutl -verify -pubin -inkey "$root/keys/release.pem" \
            -in "$hashed" -sigfile "$packages/SHA256SUMS.sig" \
            > /dev/null 2>&1; then
            say "the signature beside the manifest verifies against keys/release.pem"
            check_area
            finished 06 digests
            return 0
        fi
        rm -f "$packages/SHA256SUMS.sig"
        say "the signature beside the manifest is of another manifest, so it goes"
    fi

    key=${RELEASE_KEY:-}
    if [ -z "$key" ] || [ ! -f "$key" ]; then
        printf 'r: the release key is missing, so the run stops before the tag\n'
        printf 'r: sign the manifest and run ./r again:\n'
        printf '    openssl dgst -sha256 -binary -out %s %s\n' "$hashed" "$manifest"
        printf '    openssl pkeyutl -sign -inkey <release-key.enc.pem> -in %s -out %s/SHA256SUMS.sig\n' \
            "$hashed" "$packages"
        die "RELEASE_KEY names no file"
    fi
    openssl dgst -sha256 -binary -out "$hashed" "$manifest"
    printf 'r: openssl asks for the passphrase of %s\n' "$key"
    openssl pkeyutl -sign -inkey "$key" -in "$hashed" \
        -out "$packages/SHA256SUMS.sig" || die "step 6: openssl signed nothing"
    # A signature that fails the check never reaches a release.
    openssl pkeyutl -verify -pubin -inkey "$root/keys/release.pem" \
        -in "$hashed" -sigfile "$packages/SHA256SUMS.sig" \
        > /dev/null 2>&1 ||
        die "step 6: the signature does not verify against keys/release.pem"
    say "the signature verifies against keys/release.pem"
    check_area
    finished 06 digests
}

# The checks tools/publish.cmake makes before it uploads the directory of
# a version, run here on the directory that goes up. A leftover file or a
# digest that moved stops the release before the tag rather than after
# the upload.
check_area() {
    mkdir -p "$logs"
    cmake -DDIR="$packages" -DCHECK_ONLY=yes -P "$root/tools/publish.cmake" \
        > "$logs/area.log" 2>&1 ||
        die "step 6: $packages is no download area, see $logs/area.log"
    say "$(grep -o '[0-9]* files, each named and each digest right' "$logs/area.log")"
}

# The SHA-256 fingerprint of the public release key, which the site
# publishes beside the digests.
key_fingerprint() {
    openssl pkey -pubin -in "$root/keys/release.pem" -outform DER |
        openssl dgst -sha256 | sed 's/^.*= //'
}

# The repository on GitHub, from the origin remote.
repository() {
    git -C "$root" remote get-url origin |
        sed -E 's|^.*[:/]([^/]+/[^/]+)$|\1|' | sed -E 's|\.git$||'
}

# Step 7. The signed tag, and the release with its twelve assets and
# the two files of the manifest.
tag_and_release() {
    pre=""
    [ "$skip_vms" = no ] || pre=" as a pre-release, since step 5 did not run"
    if [ "$dry_run" = yes ]; then
        printf 'r: step 7, the tag and the release\n'
        say "would tag $tag on $(git -C "$root" rev-parse --short HEAD) and push it"
        say "would create the release $tag of $(repository)$pre"
        say "would upload 14 files, the body from the entry of CHANGELOG.md"
        return 0
    fi
    starts 07 release "the tag and the release" || return 0
    if ! git -C "$root" rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
        git -C "$root" tag -s "$tag" -m "Anti $version" ||
            die "step 7: the signed tag failed. gpg holds the key of the tag."
    fi
    git -C "$root" push -q origin "refs/tags/$tag" ||
        die "step 7: the tag did not reach origin"
    say "$tag is on origin"

    changelog_entry > "$work/notes.md"
    draft="--draft"
    [ "$skip_vms" = no ] || draft="$draft --prerelease"
    # DESIGN: the release stays a draft until its last file is up, so a
    # failed upload leaves no published release with files missing.
    # shellcheck disable=SC2086
    gh release create "$tag" --repo "$(repository)" --verify-tag $draft \
        --title "Anti $version" --notes-file "$work/notes.md" > /dev/null ||
        die "step 7: gh wrote no release"
    for file in $(release_files) "$packages/SHA256SUMS" "$packages/SHA256SUMS.sig"; do
        gh release upload "$tag" "$file" --repo "$(repository)" ||
            die "step 7: the upload of $file failed, and $tag stays a draft"
        say "uploaded $(basename "$file")"
    done
    url=$(gh release edit "$tag" --repo "$(repository)" --draft=false)
    say "published $url"
    finished 07 release
}

# Step 8. The one workflow run a release is allowed. A failure leaves
# the release as a pre-release, and the report says so.
matrix() {
    if [ "$dry_run" = yes ]; then
        printf 'r: step 8, the runner matrix\n'
        say "would run the workflow test.yml on $tag and wait for it"
        return 0
    fi
    starts 08 matrix "the runner matrix" || return 0
    gh workflow run test.yml --repo "$(repository)" --ref "$tag" ||
        die "step 8: the workflow did not start"
    sleep 20
    id=$(gh run list --repo "$(repository)" --workflow test.yml \
        --branch "$tag" --limit 1 --json databaseId --jq '.[0].databaseId')
    [ -n "$id" ] || die "step 8: no run of test.yml on $tag"
    say "the run is $id"
    echo "$id" > "$dist/matrix-run"
    if gh run watch "$id" --repo "$(repository)" --exit-status > "$logs/matrix.log" 2>&1; then
        say "the matrix is green"
    else
        gh release edit "$tag" --repo "$(repository)" --prerelease > /dev/null
        die "step 8: the run $id failed. $tag stays a pre-release until it is green."
    fi
    finished 08 matrix
}

# Write the index of the download area, which the site's build reads.
# It names the version, the six packages with their digests and URLs,
# and the fingerprint of the key that signs the manifest.
write_index() {
    release_url=https://github.com/$(repository)/releases/download/$tag
    {
        printf '# The published packages of Anti. The release script of\n'
        printf '# antic writes the entries, and the build of the site\n'
        printf '# publishes them.\n'
        printf '\n[anti]\n'
        printf 'version = "%s"\n' "$version"
        printf 'released = "%s"\n' "$(date -u '+%Y-%m-%d')"
        printf 'base = "%s/anti/%s"\n' "$download_base" "$version"
        printf 'release = "%s"\n' "$release_url"
        printf 'key_fingerprint = "%s"\n' "$(key_fingerprint)"
        for host in $hosts; do
            name=$(package_name "$host")
            printf '\n[anti.%s]\n' "$host"
            printf 'file = "%s"\n' "$name"
            printf 'sha256 = "%s"\n' "$(digest_of "$packages/$name")"
            printf 'url = "%s/%s"\n' "$release_url" "$name"
        done
    } > "$1"
}

# Step 9. The index of the site, in the checkout that ANTI_SITE names.
# Nothing binary goes there: the site's build takes the packages from
# the release and publishes them under the download base.
site() {
    if [ "$dry_run" = yes ]; then
        printf 'r: step 9, the site\n'
        write_index "$dist/index.toml"
        say "would write index.toml of downloads/ and push it to \$ANTI_SITE"
        say "the file it would write stands in $dist/index.toml"
        return 0
    fi
    starts 09 site "the site" || return 0
    checkout=${ANTI_SITE:-}
    [ -n "$checkout" ] && [ -d "$checkout/.git" ] ||
        die "step 9: ANTI_SITE names no checkout of the site"
    mkdir -p "$checkout/downloads"
    write_index "$checkout/downloads/index.toml"
    git -C "$checkout" add downloads/index.toml
    git -C "$checkout" commit -q -m "Publish Anti $version" ||
        say "the site holds this index already"
    git -C "$checkout" push -q || die "step 9: the index did not reach the site"
    say "downloads/index.toml of the site names $version"
    finished 09 site
}

# Step 10. The install a user makes, from the site and in a directory of
# its own. It compiles a program for this host and links one for the
# other five targets.
verify() {
    if [ "$dry_run" = yes ]; then
        printf 'r: step 10, the check from outside\n'
        say "would install $version from ${download_base%/downloads/resources} into $dist/verify"
        say "would compile a program for this host and link one for the other five"
        return 0
    fi
    starts 10 verify "the check from outside" || return 0
    rm -rf "$dist/verify"
    mkdir -p "$dist/verify"
    home=$dist/verify/anti
    site_root=${download_base%/downloads/resources}
    curl -fsSL "$site_root/install.sh" > "$dist/verify/install.sh" ||
        die "step 10: the installer did not download from $site_root"
    ANTI_VERSION=$version ANTI_HOME=$home ANTI_REPLACE=yes ANTI_PATH=no \
        ANTI_MICROSOFT=no sh "$dist/verify/install.sh" \
        > "$logs/verify.log" 2>&1 ||
        die "step 10: the install failed, see $logs/verify.log"
    printed=$("$home/bin/anti" --version)
    [ "$printed" = "anti $version" ] || die "step 10: anti prints '$printed'"
    printed=$("$home/bin/antic" --version)
    [ "$printed" = "antic $version" ] || die "step 10: antic prints '$printed'"
    say "anti and antic print $version"

    cat > "$dist/verify/hello.anti" <<'HELLO'
import anti.io;

fn main() -> int
{
    io.print("hello");
    return 0;
}
HELLO
    host_target=$("$home/bin/antic" --print-host-target | tr -d ' \r\n')
    (cd "$dist/verify" && "$home/bin/antic" hello.anti -o hello) ||
        die "step 10: the program did not compile for $host_target"
    [ "$("$dist/verify/hello")" = hello ] || die "step 10: the program printed nothing"
    # A fresh install links for the four targets whose sysroot it
    # carries. The C runtime of Windows is Microsoft's, and the
    # installer takes it only from a user who accepts their licence.
    for target in $hosts; do
        [ "$target" != "$host_target" ] || continue
        if [ ! -d "$home/sysroot/$target" ]; then
            say "$target waits for the C runtime of Microsoft, which this install left out"
            continue
        fi
        (cd "$dist/verify" && "$home/bin/antic" --target "$target" hello.anti \
            -o "hello-$target") || die "step 10: nothing linked for $target"
        say "links for $target"
    done
    finished 10 verify
}

# Step 11. The report of the release, committed as its last commit.
report() {
    file=docs/reports/$(date -u '+%Y-%m-%d')-release-$version.md
    if [ "$dry_run" = yes ]; then
        printf 'r: step 11, the report\n'
        say "would write $file and push it as the last commit"
        return 0
    fi
    starts 11 report "the report" || return 0
    {
        printf '# Release %s\n\n' "$version"
        printf 'Made by `./r` on %s.\n\n' "$(date -u '+%Y-%m-%d')"
        printf '## Steps\n\n'
        for stamp in "$state"/[0-9]*; do
            printf -- '- %s, at %s\n' "$(basename "$stamp")" "$(cat "$stamp")"
        done
        printf '\n## Counts\n\n'
        for name in mac asan ubsan linux windows; do
            [ -f "$logs/$name.log" ] || continue
            printf -- '- %s: %s\n' "$name" \
                "$(grep 'tests passed' "$logs/$name.log" || echo 'no count')"
        done
        printf '\n## Digests\n\n```text\n'
        cat "$packages/SHA256SUMS"
        printf '```\n'
        if [ -f "$dist/matrix-run" ]; then
            printf '\nThe run of the matrix is %s.\n' "$(cat "$dist/matrix-run")"
        fi
    } > "$root/$file"
    git -C "$root" add "$file"
    git -C "$root" commit -q -m "Report the release of $version"
    git -C "$root" push -q origin main
    say "$file is pushed"
    finished 11 report
}

mkdir -p "$dist"
if [ "$dry_run" = yes ]; then
    printf 'r: Anti %s, a dry run. Nothing is signed, tagged or uploaded.\n' "$version"
else
    printf 'r: Anti %s\n' "$version"
fi
preflight
suite
build_packages
build_symbols
vm_checks
digests
tag_and_release
matrix
site
verify
report
if [ "$dry_run" = yes ]; then
    printf 'r: the dry run of %s is done, and its files stand in %s\n' "$version" "$dist"
else
    printf 'r: Anti %s is released\n' "$version"
fi
