#!/bin/sh
# Make a release of Anti, from a pushed main to a published download.
#
#   ./r
#   ./r --dry-run
#   ./r --resume
#   ./r --skip-vms
#
# A dry run performs steps 1 to 5 and prints a plan for the rest. On a
# version that is a tag already it warns where a real run refuses.
# --resume starts at the first step whose output is missing.
# --skip-vms leaves the two VMs out and marks a pre-release.
#
# The version stands in tools/version, with its entry in CHANGELOG.md.
# Everything a run writes goes under build/dist, and a rerun keeps what
# the earlier steps produced, so a failure is resumed rather than
# restarted. The preflight runs every time.
#
# Two steps hold a secret and stay manual. The private key that signs
# SHA256SUMS stands at tools/keys/private/release-key.pem, which .gitignore
# excludes and which git therefore never sees. gh holds the login that
# the preflight reads. Without the key the run prints the two signing
# commands and stops before the tag.
#
# docs/work-order-release-script.md holds the steps. Step 8, the runner
# matrix, is gone since Anti runs no CI, and the others keep their numbers.
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
# DESIGN: the signing key has one path, which the tooling knows. A
# release needs no environment variable for it, so no run of ./r depends
# on a shell that was set up right. .gitignore excludes tools/keys/private, and
# check_key proves before every signature that git sees neither the file
# nor its directory.
key_path=tools/keys/private/release-key.pem
release_key=$root/$key_path
site_row() {
    sed -n "s/^$1=//p" "$root/tools/site-base" | tr -d ' \r\n'
}
site_base=$(site_row site)
signature_url=$(site_row signature |
    sed "s|@SITE@|$site_base|g; s|@VERSION@|$version|g")
key_url=$(site_row key | sed "s|@SITE@|$site_base|g")
# The paths under the webroot, which is what the site serves.
signature_path=${signature_url#"$site_base/"}
site_key_path=${key_url#"$site_base/"}

dry_run=no
skip_vms=no
# The warning of a dry run on a version that is a tag already.
tag_warning=""
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
# The CMake tree of the host preset, whose cache names the downloads, and
# the one name of that tree in an export of the commit.
host_tree=build/host
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

# Print the value of a cache entry of the host build.
cached() {
    sed -n "s/^$1:[A-Z]*=//p" "$root/$host_tree/CMakeCache.txt"
}

# Print the digest of a file, in the form SHA256SUMS holds.
digest_of() {
    shasum -a 256 "$1" | cut -d ' ' -f 1
}

# Prove that git sees nothing of the signing key, and read which form it
# holds. It sets key_form to plaintext or encrypted.
#
# DESIGN: where the key sits does not matter. That git cannot see it
# does. A key git tracks is a published key. A key no rule ignores is one
# `git add -A` from being tracked. Both are refused by name before
# anything is signed. The key of release@anti-lang.com is plaintext,
# because it stands on the offline Mac mini that cuts every release. That
# machine is the protection, and a passphrase would add a step to every
# release rather than a defence.
check_key() {
    [ -f "$release_key" ] ||
        die "step 6: $key_path is missing"
    if git -C "$root" ls-files --error-unmatch "$key_path" > /dev/null 2>&1; then
        die "step 6: git tracks $key_path, and a tracked signing key is a published one. Remove it from the index before a release."
    fi
    git -C "$root" check-ignore -q "$key_path" ||
        die "step 6: no rule of .gitignore excludes $key_path, so one git add -A publishes the signing key"
    case $(head -1 "$release_key") in
    *'BEGIN ENCRYPTED PRIVATE KEY'*) key_form=encrypted ;;
    *'BEGIN PRIVATE KEY'* | *'BEGIN EC PRIVATE KEY'* | *'BEGIN RSA PRIVATE KEY'*)
        key_form=plaintext
        ;;
    *) die "step 6: $key_path holds no private key" ;;
    esac
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

    # DESIGN: a release that stopped after step 7 holds a tag of its own,
    # and its rerun has to go on at the step that failed. The stamp of
    # step 7 in the state of this commit is what tells that tag from a
    # published version. A tag without it is a version that is out.
    #
    # DESIGN: a dry run on a published version warns and goes on, so the
    # dry run checks the script at any time and not only before a
    # release. A real run refuses it here, before anything is built.
    own_tag=no
    if [ -f "$state/07-release" ] &&
        [ "$(cat "$state/head" 2> /dev/null)" = "$(git -C "$root" rev-parse HEAD)" ]; then
        own_tag=yes
    fi
    if [ "$own_tag" = no ]; then
        published=""
        if git -C "$root" rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
            published="$tag is a tag of this checkout already"
        elif [ -n "$(git -C "$root" ls-remote --tags origin "refs/tags/$tag")" ]; then
            published="origin holds the tag $tag already"
        fi
        if [ -z "$published" ]; then
            say "$version is no tag here and none on origin"
        elif [ "$dry_run" = no ]; then
            die "$published, and a published version is never rebuilt"
        else
            tag_warning="$published, and a real run refuses a published version"
            say "warning: $tag_warning"
        fi
    else
        say "$tag is the tag that step 7 of this run made, and the run goes on"
    fi

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

    # DESIGN: a key that step 6 would refuse is refused here, before the
    # suites, the packages and the two VMs. A key that is not there is no
    # refusal. The run then stops before the tag, with the two signing
    # commands, which is what a machine without the key is for.
    if [ -f "$release_key" ]; then
        check_key
        say "$key_path holds a $key_form key, which git neither tracks nor sees"
    else
        say "$key_path is missing, so the run would stop before the tag"
    fi

    # DESIGN: step 9 publishes the text of the site, and the webroot it
    # writes to is read here rather than after the tag exists. A release
    # that stops at step 9 has published its binaries and named them
    # nowhere.
    case ${ANTI_SITE:-} in
    "") die "ANTI_SITE names no webroot of the site, and step 9 publishes its text there" ;;
    *:/*) ;;
    *) die "ANTI_SITE holds '$ANTI_SITE', and step 9 rsyncs to <host>:<webroot>" ;;
    esac
    site_host=${ANTI_SITE%%:*}
    ssh -n -o BatchMode=yes -o ConnectTimeout=20 "$site_host" true ||
        die "$site_host does not answer, and step 9 rsyncs the text of the site there"
    say "$site_host answers, and takes the text of the site"

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

    [ -f "$root/$host_tree/CMakeCache.txt" ] || {
        say "$host_tree/ holds no cache, so the downloads run first"
        cmake -S "$root" -B "$root/$host_tree" > "$dist/configure.log" 2>&1 ||
            die "the download step failed, and $dist/configure.log holds its output"
    }
    for name in ANTIC_CLANG_DIR ANTIC_LLVM_DIR ANTIC_SYSROOT_DIR ANTIC_RAYLIB_DIR; do
        value=$(cached "$name")
        [ -n "$value" ] || die "$host_tree/CMakeCache.txt names no $name"
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
    say "the downloads of $host_tree/ are in place"

    # DESIGN: the runtime archive holds the LLVM tools of this machine,
    # and a package carries the tools of its own host. The tools of the
    # other five hosts come from the same release, into llvm-tools/<host>
    # beside the llvm/ of the downloads, through tools/get-llvm.cmake,
    # which checks each archive against the pin, the manifest and the
    # signature. A host whose directory is in place is not fetched again,
    # and step 3 hands the directory to the packer.
    tools_dir=$(dirname "$llvm_dir")/llvm-tools
    machine=macos-$(uname -m)
    mkdir -p "$dist"
    for host in $hosts; do
        [ "$host" != "$machine" ] || continue
        if [ ! -f "$tools_dir/$host/bin/llvm-version" ]; then
            cmake -DHOST="$host" -DDEST="$tools_dir/$host" \
                -P "$root/tools/get-llvm.cmake" > "$dist/tools-$host.log" 2>&1 ||
                die "the LLVM tools of $host did not download, and $dist/tools-$host.log holds the output"
            [ -f "$tools_dir/$host/bin/llvm-version" ] ||
                die "tools/get-llvm.cmake laid out no $tools_dir/$host/bin"
        fi
        say "the LLVM tools of $host are in $tools_dir/$host"
    done
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
    cmake -S "$export_tree" -B "$export_tree/$host_tree" $paths \
        > "$logs/mac-configure.log" 2>&1 ||
        die "step 2: the configure failed, see $logs/mac-configure.log"
    cmake --build "$export_tree/$host_tree" -j 8 > "$logs/mac-build.log" 2>&1 ||
        die "step 2: the build failed, see $logs/mac-build.log"
    ctest --test-dir "$export_tree/$host_tree" -j 8 > "$logs/mac.log" 2>&1 ||
        die "step 2: the suite failed, see $logs/mac.log"
    say "the Mac: $(grep 'tests passed' "$logs/mac.log")"

    for preset in asan ubsan; do
        (cd "$export_tree" && cmake --preset "$preset") \
            > "$logs/$preset-configure.log" 2>&1 ||
            die "step 2: the $preset configure failed, see $logs/$preset-configure.log"
        cmake --build "$export_tree/build/$preset" -j 8 \
            > "$logs/$preset-build.log" 2>&1 ||
            die "step 2: the $preset build failed, see $logs/$preset-build.log"
        ctest --test-dir "$export_tree/build/$preset" -j 8 \
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
        -DRUNTIME="$export_tree/$host_tree/runtime" -DHOSTS="$list" \
        -DSYMBOLS="$symbols" -DTOOLS="$tools_dir" \
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

# DESIGN: the test of a release is what a fresh install builds with the
# network off, which Eddie decided on 2026-09-27 under "Binary
# distribution" in docs/decisions.md and decision 8 of
# docs/work-order-distribution.md places in this step. These are the
# commands that take a VM off the network, put it back and ask whether it
# reaches the network. Each runs on the VM over ssh from this Mac, and
# docs/vm-setup.md shows the same six, which the test release_dry_run
# holds.
#
# DESIGN: off is one rule of the firewall of the VM, which refuses every
# packet that leaves for another address than the one of this Mac, the
# first word of SSH_CONNECTION there. The session that gives the command
# therefore stays up, and so does every later one. On removes the rule. No
# route and no interface changes, so nothing a lease of DHCP renews puts
# the network back in the middle of the check. Linux takes a table of
# nftables under sudo, which a restart of the VM forgets. Windows takes a
# rule of its firewall that blocks the two ranges around the address of
# the Mac and all of IPv6, and its DNS client answers again a few seconds
# after the rule went, which is why the step waits for the probe.
# shellcheck disable=SC2016
linux_network_off='sudo nft "add table inet anti_offline; add chain inet anti_offline out { type filter hook output priority 0; }; add rule inet anti_offline out oifname lo accept; add rule inet anti_offline out ip daddr ${SSH_CONNECTION%% *} accept; add rule inet anti_offline out reject"'
linux_network_on='sudo nft destroy table inet anti_offline'
linux_network_probe='curl -fsS --max-time 10 -o /dev/null https://github.com'
# shellcheck disable=SC2016
windows_network_off='powershell -Command "$m = [version]$env:SSH_CONNECTION.Split()[0]; $n = \"$($m.Major).$($m.Minor).$($m.Build)\"; netsh advfirewall firewall add rule name=anti-offline dir=out action=block \"remoteip=0.0.0.0-$n.$($m.Revision - 1),$n.$($m.Revision + 1)-255.255.255.255,::/1,8000::/1\""'
windows_network_on='netsh advfirewall firewall delete rule name=anti-offline && ipconfig /flushdns'
windows_network_probe='curl.exe -fsS --max-time 10 -o NUL https://github.com'

# Run the command of a VM that turns its network off or on, or the probe
# that succeeds when the VM reaches the network.
network() {
    case $1:$2 in
    anti-linux:off) remote=$linux_network_off ;;
    anti-linux:on) remote=$linux_network_on ;;
    anti-linux:probe) remote=$linux_network_probe ;;
    anti-windows:off) remote=$windows_network_off ;;
    anti-windows:on) remote=$windows_network_on ;;
    anti-windows:probe) remote=$windows_network_probe ;;
    *) die "step 5: no command of $1 for the network is called $2" ;;
    esac
    ssh -n -o BatchMode=yes "$1" "$remote"
}

# DESIGN: a VM that this run took off the network gets it back on every
# exit, the refusal of a later check and an interrupt alike. A VM left
# off the network fails the next thing anyone does on it, long after the
# release that left it so.
offline_machine=""
restore_network() {
    [ -n "$offline_machine" ] || return 0
    network "$offline_machine" on > /dev/null 2>&1 ||
        printf 'r: the network of %s is still off, and docs/vm-setup.md holds the command that turns it on\n' \
            "$offline_machine" >&2
    offline_machine=""
}
trap restore_network EXIT
trap 'exit 130' INT
trap 'exit 143' HUP TERM

# Write the shell script that a Linux VM runs, and print its path. It
# takes the phase of the step as its argument. install extracts the tree
# and installs the package of that host from a directory of the machine.
# offline runs the check of the install and removes it, both with the
# network off. suite builds the tree and runs its tests.
write_linux_script() {
    cat > "$work/vm-linux.sh" <<LINUX
set -eu
cd "\$HOME/antic-check"
# The tools of the VM stand in the data directory of its user, under a
# name of their own. A reset of that user is then those directories and
# never the machine. docs/vm-setup.md installs them there.
A=\${XDG_DATA_HOME:-\$HOME/.local/share}/anti-vm
# DESIGN: the install the VM checks is the one a user gets, in the
# directories of the platform, and not a tree under ANTI_HOME. The
# executables land on the PATH and the archive in the data directory,
# which is the pair antic has to find its runtime through.
data=\${XDG_DATA_HOME:-\$HOME/.local/share}/anti
bin=\${XDG_BIN_HOME:-\$HOME/.local/bin}
# DESIGN: the logs stand beside the files of the run and never in the
# tree. That tree has no history, so its test repo_layout reads the
# directory, and a log at its top level is an entry outside the layout.
logs=\$HOME/anti-release
case \$1 in
install)
    tar -xmf "\$HOME/anti-release/tree.tar"
    rm -rf "\$data" "\$bin/antic" "\$bin/anti"
    if ANTI_VERSION=$version ANTI_BASE=file://\$HOME/anti-release \\
        ANTI_REPLACE=yes ANTI_PATH=yes \\
        sh "\$HOME/anti-release/install.sh" > "\$logs/release-unsigned.log" 2>&1; then
        echo "the installer took a manifest without a signature"
        exit 1
    fi
    grep -q 'SHA256SUMS.sig' "\$logs/release-unsigned.log" || {
        echo "the installer stopped for another reason than the signature"
        cat "\$logs/release-unsigned.log"
        exit 1
    }
    echo "the installer refuses a manifest without a signature"
    rm -rf "\$data"
    ANTI_VERSION=$version ANTI_BASE=file://\$HOME/anti-release ANTI_STAGING=yes \\
        ANTI_REPLACE=yes ANTI_PATH=yes \\
        sh "\$HOME/anti-release/install.sh" > "\$logs/release-install.log" 2>&1
    [ -f "\$data/.anti-install" ] || { echo "the installer wrote no marker"; exit 1; }
    "\$bin/antic" --version
    "\$bin/anti" --version
    cd "\$HOME/anti-release"
    printf 'import anti.io;\n\nfn main() -> int\n{\n    io.print("hello");\n    return 0;\n}\n' > hello.anti
    # No --runtime: antic on the PATH finds the archive of the data directory.
    "\$bin/antic" hello.anti -o hello
    ./hello
    echo
    echo "the package of linux-arm64 is installed"
    ;;
offline)
    # DESIGN: the check names no archive and no tool. antic and anti of
    # the bin directory find the archive of the data directory, and the
    # script gives each a PATH of that bin directory alone.
    cmake -DBIN="\$bin" -DARCHIVE="\$data" -DROOT="\$HOME/antic-check" \\
        -DHOST=linux-arm64 -DWORK="\$HOME/anti-release/offline" \\
        -P tools/check-offline.cmake
    # DESIGN: the install goes before the network comes back, so the
    # suite runs on a machine that holds no install, as it did before
    # this step checked one. The antic of a build tree finds no archive
    # above its directory and would take the one of the data directory.
    #
    # A tree with no marker is refused, which is what keeps an uninstaller
    # from taking a directory that no installer of Anti wrote.
    mkdir -p "\$HOME/anti-release/not-an-install"
    if ANTI_HOME=\$HOME/anti-release/not-an-install ANTI_REMOVE=yes \\
        sh "\$HOME/anti-release/uninstall.sh" > "\$logs/release-marker.log" 2>&1; then
        echo "the uninstaller removed a directory with no marker"
        exit 1
    fi
    [ -d "\$HOME/anti-release/not-an-install" ] ||
        { echo "the uninstaller removed a directory with no marker"; exit 1; }
    echo "the uninstaller refuses a directory without the marker"
    ANTI_REMOVE=yes sh "\$HOME/anti-release/uninstall.sh" \\
        > "\$logs/release-uninstall.log" 2>&1
    [ ! -d "\$data" ] || { echo "the uninstaller left \$data"; exit 1; }
    [ ! -f "\$bin/antic" ] || { echo "the uninstaller left \$bin/antic"; exit 1; }
    echo "the install of linux-arm64 is checked and removed"
    ;;
suite)
    opts="-DANTIC_CLANG_DIR=\$A/clang -DANTIC_LLVM_DIR=\$A/toolchain"
    opts="\$opts -DANTIC_SYSROOT_DIR=\$A/sysroot -DANTIC_RAYLIB_DIR=\$A/raylib/raylib-6.0"
    cmake -S . -B $host_tree \$opts > "\$logs/release-configure.log" 2>&1
    cmake --build $host_tree -j"\$(nproc)" > "\$logs/release-build.log" 2>&1
    ctest --test-dir $host_tree -j"\$(nproc)" > "\$logs/release-ctest.log" 2>&1
    grep 'tests passed' "\$logs/release-ctest.log"
    ;;
*)
    echo "run.sh takes install, offline or suite"
    exit 2
    ;;
esac
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
rem DESIGN: the install the VM checks is the one a user gets, in the
rem directories of Windows and not a tree under ANTI_HOME. DATA holds the
rem archive and BIN the two executables that go on the PATH.
set DATA=%LOCALAPPDATA%\anti
set BIN=%LOCALAPPDATA%\Programs\anti\bin
rem DESIGN: the logs stand beside the files of the run and never in the
rem tree. That tree has no history, so its test repo_layout reads the
rem directory, and a log at its top level is an entry outside the layout.
set LOGS=%USERPROFILE%\anti-release
if "%1"=="install" goto install
if "%1"=="offline" goto offline
if "%1"=="suite" goto suite
echo run.cmd takes install, offline or suite
exit /b 2

:install
tar -xmf %USERPROFILE%\anti-release\tree.tar
set ANTI_VERSION=$version
set ANTI_BASE=%USERPROFILE%\anti-release
set ANTI_REPLACE=yes
set ANTI_PATH=yes
if exist "%DATA%" rmdir /s /q "%DATA%"
if exist "%BIN%" rmdir /s /q "%BIN%"
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\install.ps1 > "%LOGS%\release-unsigned.log" 2>&1
if not errorlevel 1 (echo the installer took a manifest without a signature & exit /b 1)
findstr /C:"SHA256SUMS.sig" "%LOGS%\release-unsigned.log" > nul
if errorlevel 1 (echo the installer stopped for another reason than the signature & type "%LOGS%\release-unsigned.log" & exit /b 1)
echo the installer refuses a manifest without a signature
if exist "%DATA%" rmdir /s /q "%DATA%"
set ANTI_STAGING=yes
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\install.ps1 > "%LOGS%\release-install.log" 2>&1
if errorlevel 1 (echo the install failed & type "%LOGS%\release-install.log" & exit /b 1)
if not exist "%DATA%\.anti-install" (echo the installer wrote no marker & exit /b 1)
"%BIN%\antic.exe" --version
"%BIN%\anti.exe" --version
cd /d %USERPROFILE%\anti-release
powershell -Command "'fn main() -> int','{','    return 0;','}' | Set-Content hello.anti"
rem No --runtime: antic of the bin directory finds the archive of DATA.
"%BIN%\antic.exe" hello.anti -o hello.exe
if errorlevel 1 (echo antic did not find the runtime archive & exit /b 1)
hello.exe
if errorlevel 1 (echo the hello program of the install failed & exit /b 1)
echo the package of windows-arm64 is installed
exit /b 0

:offline
rem DESIGN: the check names no archive and no tool. antic and anti of BIN
rem find the archive of DATA, and the script gives each a PATH of BIN alone.
cmake -DBIN="%BIN%" -DARCHIVE="%DATA%" -DROOT=%USERPROFILE%\antic-check -DHOST=windows-arm64 -DWORK=%USERPROFILE%\anti-release\offline -P tools\check-offline.cmake
if errorlevel 1 (echo the check of the install failed & exit /b 1)
rem DESIGN: the install goes before the network comes back, so the suite
rem runs on a machine that holds no install, as it did before this step
rem checked one. The antic of a build tree finds no archive above its
rem directory and would take the one of DATA.
rem
rem A tree with no marker is refused, which is what keeps an uninstaller
rem from taking a directory that no installer of Anti wrote.
if not exist "%USERPROFILE%\anti-release\not-an-install" mkdir "%USERPROFILE%\anti-release\not-an-install"
set ANTI_REMOVE=yes
set ANTI_HOME=%USERPROFILE%\anti-release\not-an-install
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\uninstall.ps1 > "%LOGS%\release-marker.log" 2>&1
if not exist "%USERPROFILE%\anti-release\not-an-install" (echo the uninstaller removed a directory with no marker & exit /b 1)
echo the uninstaller refuses a directory without the marker
set ANTI_HOME=
powershell -ExecutionPolicy Bypass -File %USERPROFILE%\anti-release\uninstall.ps1 > "%LOGS%\release-uninstall.log" 2>&1
if exist "%DATA%" (echo the uninstaller left %DATA% & exit /b 1)
if exist "%BIN%\antic.exe" (echo the uninstaller left %BIN%\antic.exe & exit /b 1)
echo the install of windows-arm64 is checked and removed
exit /b 0

:suite
call "%VC%" arm64 > nul
rem The tools of the VM stand in its data directory, as on the Linux VM
rem and as docs/vm-setup.md installs them. Without these four options a
rem fresh build\host downloads clang and the LLVM tools and stops at
rem raylib, which has no fetcher in the configure.
set A=%LOCALAPPDATA%/anti-vm
set OPTS=-DANTIC_CLANG_DIR=%A%/clang -DANTIC_LLVM_DIR=%A%/toolchain -DANTIC_SYSROOT_DIR=%A%/sysroot -DANTIC_RAYLIB_DIR=%A%/raylib/raylib-6.0
if exist build\host (cmake -S . -B build\host %OPTS% > "%LOGS%\release-configure.log" 2>&1) else (cmake -S . -B build\host -G Ninja %OPTS% > "%LOGS%\release-configure.log" 2>&1)
if errorlevel 1 (echo the configure failed & exit /b 1)
cmake --build build\host > "%LOGS%\release-build.log" 2>&1
if errorlevel 1 (echo the build failed & exit /b 1)
ctest --test-dir build\host -j4 > "%LOGS%\release-ctest.log" 2>&1
if errorlevel 1 (echo the suite failed & findstr /R /C:"^	 *[0-9]* - " "%LOGS%\release-ctest.log" & exit /b 1)
findstr /C:"tests passed" "%LOGS%\release-ctest.log"
exit /b 0
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
    if [ "$machine" = anti-windows ]; then
        scp -q -o BatchMode=yes "$(write_windows_script)" \
            "$machine:anti-release/run.cmd" ||
            die "step 5: the script did not reach $machine"
    else
        scp -q -o BatchMode=yes "$(write_linux_script)" \
            "$machine:anti-release/run.sh" ||
            die "step 5: the script did not reach $machine"
    fi
}

# Run one phase of the script of a VM, and add what it prints to the log
# of that machine. The Windows machine answers to cmd, which runs the
# command file by its path: under a `cmd /c` of its own the phase arrives
# with the closing quote of the command line.
vm_phase() {
    if [ "$1" = anti-windows ]; then
        ssh -n -o BatchMode=yes "$1" \
            "%USERPROFILE%\\anti-release\\run.cmd $2" >> "$3" 2>&1
    else
        ssh -n -o BatchMode=yes "$1" \
            "sh \$HOME/anti-release/run.sh $2" >> "$3" 2>&1
    fi
}

# The log of a VM, which every phase of the step adds to.
vm_log() {
    printf '%s\n' "$logs/${1#anti-}.log"
}

# Install the package of host on a VM, from the files of this run and with
# the network on. The installer's own check is the version each of the two
# programs prints.
vm_install() {
    machine=$1
    log=$(vm_log "$machine")
    : > "$log"
    send_to_vm "$machine" "$2"
    vm_phase "$machine" install "$log" ||
        die "step 5: $machine failed, see $log"
    grep -q "antic $version" "$log" ||
        die "step 5: the install on $machine printed no antic $version"
    grep -q "refuses a manifest without a signature" "$log" ||
        die "step 5: the installer of $machine checked no signature"
    say "$machine: the package installs and compiles a program"
}

# Turn the network of a VM off, run the check of its install and turn the
# network on again.
#
# DESIGN: the probe is read on both sides of the check. A command that
# turned nothing off would let the check pass on a VM that downloads what
# its package lacks, and the probe after the command that turns the
# network on says that the VM is as the step found it. ssh ends with 255
# when the VM does not answer, which is no answer of the probe. A check
# that failed is reported after the network is back.
vm_offline() {
    machine=$1
    log=$(vm_log "$machine")
    offline_machine=$machine
    network "$machine" off >> "$log" 2>&1 ||
        die "step 5: the network of $machine did not turn off, see $log"
    answer=0
    network "$machine" probe >> "$log" 2>&1 || answer=$?
    [ "$answer" != 255 ] ||
        die "step 5: $machine does not answer with its network off, see $log"
    [ "$answer" != 0 ] ||
        die "step 5: $machine still reaches the network after the command that turns it off"
    say "$machine: the network is off"
    checked=yes
    vm_phase "$machine" offline "$log" || checked=no
    network "$machine" on >> "$log" 2>&1 ||
        die "step 5: the network of $machine did not turn on, see $log"
    tries=0
    until network "$machine" probe >> "$log" 2>&1; do
        tries=$((tries + 1))
        [ "$tries" -lt 12 ] ||
            die "step 5: $machine does not reach the network again, see $log"
        sleep 5
    done
    offline_machine=""
    say "$machine: the network is on again"
    [ "$checked" = yes ] ||
        die "step 5: $machine failed with the network off, see $log"
    grep -q "links for the other five targets" "$log" ||
        die "step 5: the check on $machine built no program for the other targets"
    say "$machine: the install builds with the network off, and uninstalls"
}

# Build the tree on a VM and run its suite, with the network on.
vm_suite() {
    machine=$1
    log=$(vm_log "$machine")
    vm_phase "$machine" suite "$log" ||
        die "step 5: $machine failed, see $log"
    say "$machine: $(grep 'tests passed' "$log" || echo 'the suite printed no count')"
}

# Step 5. Both VMs install the package of their host from the files of
# this run, build with it while their network is off and run the suite of
# the commit, before anything is published.
vm_checks() {
    if [ "$skip_vms" = yes ]; then
        printf 'r: step 5, the VMs, skipped by --skip-vms\n'
        return 0
    fi
    starts 05 vms "the two VMs" || return 0
    mkdir -p "$work" "$logs"
    (cd "$root" && git ls-files -z | xargs -0 tar cf "$work/tree.tar") ||
        die "step 5: the tree did not pack"
    for pair in anti-linux:linux-arm64 anti-windows:windows-arm64; do
        vm_install "${pair%%:*}" "${pair#*:}"
        vm_offline "${pair%%:*}"
        vm_suite "${pair%%:*}"
    done
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
        if [ -f "$release_key" ]; then
            check_key
            say "would sign $manifest into SHA256SUMS.sig with the $key_form key $key_path"
        else
            say "would sign $manifest into SHA256SUMS.sig with $key_path"
            say "$key_path is missing here, so a run would stop before the tag"
        fi
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
        if openssl pkeyutl -verify -pubin -inkey "$root/tools/keys/release.pem" \
            -in "$hashed" -sigfile "$packages/SHA256SUMS.sig" \
            > /dev/null 2>&1; then
            say "the signature beside the manifest verifies against tools/keys/release.pem"
            check_area
            finished 06 digests
            return 0
        fi
        rm -f "$packages/SHA256SUMS.sig"
        say "the signature beside the manifest is of another manifest, so it goes"
    fi

    if [ ! -f "$release_key" ]; then
        printf 'r: the release key is missing, so the run stops before the tag\n'
        printf 'r: sign the manifest on the machine that holds it and run ./r again:\n'
        printf '    openssl dgst -sha256 -binary -out %s %s\n' "$hashed" "$manifest"
        printf '    openssl pkeyutl -sign -inkey %s -in %s -out %s/SHA256SUMS.sig\n' \
            "$key_path" "$hashed" "$packages"
        die "$key_path is missing"
    fi
    check_key
    openssl dgst -sha256 -binary -out "$hashed" "$manifest"
    if [ "$key_form" = encrypted ]; then
        printf 'r: openssl asks for the passphrase of %s\n' "$key_path"
    else
        say "$key_path holds a plaintext key, and the machine it stands on is the protection"
    fi
    openssl pkeyutl -sign -inkey "$release_key" -in "$hashed" \
        -out "$packages/SHA256SUMS.sig" || die "step 6: openssl signed nothing"
    # A signature that fails the check never reaches a release.
    openssl pkeyutl -verify -pubin -inkey "$root/tools/keys/release.pem" \
        -in "$hashed" -sigfile "$packages/SHA256SUMS.sig" \
        > /dev/null 2>&1 ||
        die "step 6: the signature does not verify against tools/keys/release.pem"
    say "the signature verifies against tools/keys/release.pem"
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
    openssl pkey -pubin -in "$root/tools/keys/release.pem" -outform DER |
        openssl dgst -sha256 | sed 's/^.*= //'
}

# The repository on GitHub, from the origin remote.
repository() {
    git -C "$root" remote get-url origin |
        sed -E 's|^.*[:/]([^/]+/[^/]+)$|\1|' | sed -E 's|\.git$||'
}

# Step 7. The tag, and the release with its twelve assets and the
# manifest that names them.
#
# DESIGN: the tag is annotated and unsigned, as in llvm-tools.
# SHA256SUMS.sig of step 6 is the one signature of a release, so a release
# asks gpg for no key.
#
# DESIGN: SHA256SUMS.sig stays off the release. The binaries and the
# signature that covers them live on two hosts, so a forged release needs
# both. Step 9 publishes the signature on anti-lang.com, and both
# installers read it from there.
tag_and_release() {
    pre=""
    [ "$skip_vms" = no ] || pre=" as a pre-release, since step 5 did not run"
    if [ "$dry_run" = yes ]; then
        printf 'r: step 7, the tag and the release\n'
        say "would tag $tag on $(git -C "$root" rev-parse --short HEAD) and push it"
        say "would create the release $tag of $(repository)$pre"
        say "would upload 13 files, the six packages, the six symbols archives and SHA256SUMS"
        say "would upload no SHA256SUMS.sig, which step 9 publishes on the site"
        say "the body of the release is the entry of CHANGELOG.md"
        return 0
    fi
    starts 07 release "the tag and the release" || return 0
    if ! git -C "$root" rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
        git -C "$root" tag -a "$tag" -m "Anti $version" ||
            die "step 7: git made no tag $tag"
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
    for file in $(release_files) "$packages/SHA256SUMS"; do
        gh release upload "$tag" "$file" --repo "$(repository)" ||
            die "step 7: the upload of $file failed, and $tag stays a draft"
        say "uploaded $(basename "$file")"
    done
    url=$(gh release edit "$tag" --repo "$(repository)" --draft=false)
    say "published $url"
    finished 07 release
}

# Write the index of the download area, which the site's build reads.
# It names the version, the six packages with their digests and URLs,
# and the fingerprint of the key that signs the manifest.
# The downloads page, from tools/downloads.html.in and the digests of the
# packages step 3 wrote. Every file it names is an asset of the release.
write_page() {
    release_url=https://github.com/$(repository)/releases/download/$tag
    mkdir -p "$work"
    : > "$work/rows.html"
    for host in $hosts; do
        name=$(package_name "$host")
        {
            printf '        <tr><td>%s</td>\n' "$host"
            printf '          <td><a href="%s/%s">%s</a></td>\n' \
                "$release_url" "$name" "$name"
            printf '          <td class="digest">%s</td></tr>\n' \
                "$(digest_of "$packages/$name")"
        } >> "$work/rows.html"
    done
    awk -v rows="$work/rows.html" '
        /@ROWS@/ {
            while ((getline line < rows) > 0) {
                print line
            }
            next
        }
        { print }' "$root/tools/downloads.html.in" |
        sed -e "s|@VERSION@|$version|g" \
            -e "s|@TAG@|$tag|g" \
            -e "s|@RELEASED@|$(date -u '+%Y-%m-%d')|g" \
            -e "s|@RELEASE@|https://github.com/$(repository)/releases/tag/$tag|g" \
            -e "s|@SIGNATURE@|$signature_url|g" \
            -e "s|@DOWNLOAD@|$release_url|g" \
            -e "s|@KEY@|$key_url|g" \
            -e "s|@FINGERPRINT@|$(key_fingerprint)|g" > "$1"
}

# Step 9. The text of the site, in the webroot that ANTI_SITE names on the
# host that serves anti-lang.com. It carries no git clone and no build, so
# the files are rsynced as they are.
#
# DESIGN: nothing binary reaches the site, and the signature of the
# manifest reaches nothing else. The GitHub release holds the packages,
# the symbols archives and SHA256SUMS. This host holds SHA256SUMS.sig and
# the public key beside the two installers and the downloads page. A
# forged release therefore needs both hosts. Whoever takes GitHub changes
# binaries that the signature no longer covers. Whoever takes this host
# signs nothing, because the private key is on neither. A signature stored
# beside the binaries it covers would leave the private key as the only
# thing between an attacker and a release.
#
# The mode is spelled for openrsync, which macOS ships as rsync and which
# refuses --chmod=F640 and --chmod=0640. The webroot is setgid, so a file
# rsynced into it keeps the group of the server.
site() {
    page=$dist/downloads-index.html
    signature=$packages/SHA256SUMS.sig
    if [ "$dry_run" = yes ]; then
        printf 'r: step 9, the site\n'
        write_page "$page"
        say "would rsync tools/install.sh and tools/install.ps1 to ${ANTI_SITE:-\$ANTI_SITE}/"
        say "would rsync the downloads page to ${ANTI_SITE:-\$ANTI_SITE}/downloads/index.html"
        say "would rsync SHA256SUMS.sig to ${ANTI_SITE:-\$ANTI_SITE}/$signature_path"
        say "would rsync tools/keys/release.pem to ${ANTI_SITE:-\$ANTI_SITE}/$site_key_path"
        say "would read $signature_url and $key_url back"
        say "would send nothing else, and no binary"
        say "the page it would publish stands in $page"
        return 0
    fi
    starts 09 site "the site" || return 0
    destination=${ANTI_SITE:-}
    [ -n "$destination" ] ||
        die "step 9: ANTI_SITE names no webroot of the site"
    [ -f "$signature" ] ||
        die "step 9: $signature is missing, and the site serves the signature of a release"
    write_page "$page"
    rsync --chmod=u=rw,g=r,o= "$root/tools/install.sh" \
        "$root/tools/install.ps1" "$destination/" ||
        die "step 9: the installers did not reach $destination"
    say "install.sh and install.ps1 stand in $destination"
    rsync --chmod=u=rw,g=r,o= "$page" "$destination/downloads/index.html" ||
        die "step 9: the downloads page did not reach $destination"
    say "the downloads page of $version stands in $destination/downloads"

    # DESIGN: the signature of this version goes in a directory of its
    # own. The signature of every release then stays fetchable, and an
    # installer of an older version keeps working. openrsync has no
    # --mkpath, so the directory is made over ssh first.
    #
    # DESIGN: the webroot is setgid. A file the server reads carries the
    # group it inherits there. This server hands a new directory the group
    # of its parent and not the bit. A directory two levels down then
    # falls to the group of the user. The 0.1.0 directory of the first
    # release did, and anti-lang.com answered 403 for a signature that
    # stood in place.
    #
    # DESIGN: the bit is set on the directory above before the directory
    # of the version is made. `chmod g+s` on a directory that exists sets
    # no group, and a version directory made first keeps the wrong one.
    # Both orders were run against the server on 2026-09-21. The one that
    # sets first answers 200 and the other 403.
    site_host=${destination%%:*}
    site_root=${destination#*:}
    signature_directory=$site_root/$(dirname "$signature_path")
    signature_parent=$(dirname "$signature_directory")
    ssh -n -o BatchMode=yes "$site_host" \
        "mkdir -p '$signature_parent' && chmod g+s '$signature_parent' &&
         mkdir -p '$signature_directory' && chmod g+s '$signature_directory'" ||
        die "step 9: $site_host made no directory for the signature"
    rsync --chmod=u=rw,g=r,o= "$signature" "$destination/$signature_path" ||
        die "step 9: SHA256SUMS.sig did not reach $destination"
    say "SHA256SUMS.sig of $version stands in $destination/$signature_path"
    rsync --chmod=u=rw,g=r,o= "$root/tools/keys/release.pem" \
        "$destination/$site_key_path" ||
        die "step 9: tools/keys/release.pem did not reach $destination"

    # The two files a user's installer reads from here, read back over
    # HTTPS. A release proves them rather than trusting that an earlier
    # one put them there.
    mkdir -p "$work"
    curl -fsSL "$key_url" > "$work/site-key.pem" ||
        die "step 9: $key_url answered nothing"
    cmp -s "$work/site-key.pem" "$root/tools/keys/release.pem" ||
        die "step 9: the key at $key_url is not tools/keys/release.pem"
    say "$key_url is the public key of the release"
    curl -fsSL "$signature_url" > "$work/site-signature.sig" ||
        die "step 9: $signature_url answered nothing"
    cmp -s "$work/site-signature.sig" "$signature" ||
        die "step 9: the signature at $signature_url is not the one of this release"
    openssl dgst -sha256 -binary -out "$work/SHA256SUMS.sha256" \
        "$packages/SHA256SUMS"
    openssl pkeyutl -verify -pubin -inkey "$work/site-key.pem" \
        -in "$work/SHA256SUMS.sha256" -sigfile "$work/site-signature.sig" \
        > /dev/null 2>&1 ||
        die "step 9: the signature of the site does not cover the manifest of the release"
    say "the signature of the site covers SHA256SUMS of the GitHub release"
    finished 09 site
}

# Step 10. The install a user makes, from the site and in a directory of
# its own. It compiles a program for this host and links one for the
# other five targets.
verify() {
    if [ "$dry_run" = yes ]; then
        printf 'r: step 10, the check from outside\n'
        say "would install $version from $site_base into $dist/verify"
        say "would compile a program for this host and link one for the other five"
        return 0
    fi
    starts 10 verify "the check from outside" || return 0
    rm -rf "$dist/verify"
    mkdir -p "$dist/verify"
    home=$dist/verify/anti
    site_root=$site_base
    curl -fsSL "$site_root/install.sh" > "$dist/verify/install.sh" ||
        die "step 10: the installer did not download from $site_root"
    ANTI_VERSION=$version ANTI_HOME=$home ANTI_REPLACE=yes ANTI_PATH=no \
        sh "$dist/verify/install.sh" \
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
    # A fresh install links for the other five targets, since the package
    # carries the sysroot of every one.
    for target in $hosts; do
        [ "$target" != "$host_target" ] || continue
        [ -d "$home/sysroot/$target" ] ||
            die "step 10: the install carries no sysroot of $target"
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
site
verify
report
if [ "$dry_run" = yes ]; then
    printf 'r: the dry run of %s is done, and its files stand in %s\n' "$version" "$dist"
    [ -z "$tag_warning" ] || printf 'r: warning: %s\n' "$tag_warning"
else
    printf 'r: Anti %s is released\n' "$version"
fi
