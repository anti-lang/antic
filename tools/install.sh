#!/bin/sh
# Install Anti for the user who runs it, in the directories of the
# platform.
#
#   curl -fsSL https://anti-lang.com/install.sh | sh
#   curl -fsSL https://anti-lang.com/install.sh | sh -s -- --intel  # docs-style:ignore
#
# It downloads the package of this host. It checks the SHA-256 against the
# SHA256SUMS beside it, unpacks it and installs the sysroot of the host.
# The SDK of macOS belongs to Apple and the C runtime of Windows to
# Microsoft. The script asks before it takes either of them.
#
# The option --arm or --intel takes the package of that processor rather
# than the one of this machine. A machine that emulates the other
# processor runs it. ANTI_ARCH holds the same choice as arm64 or x86_64.
#
# Each question takes yes or no from a variable of its own name.
#
#   ANTI_VERSION: the version to install, default the newest.
#   ANTI_BASE: where the packages are served from, default anti-lang.com.
#              A release checks its own packages through it, before they
#              are published, with a file:// prefix of a local directory.
#   ANTI_STAGING: the base is the staging area of a release, whose
#              manifest step 6 of ./r signs after the checks of step 5.
#              It takes a manifest without a signature, and never one
#              whose signature is wrong.
#   ANTI_ARCH: arm64 or x86_64, default the processor of this machine.
#   ANTI_HOME: one tree to install everything under, executables
#              included. Without it the install follows the platform:
#              the executables in $XDG_BIN_HOME or ~/.local/bin, the
#              toolchain and the runtime archive in $XDG_DATA_HOME/anti
#              or ~/.local/share/anti. The package of another processor
#              takes the data directory of the name anti-<cpu> and keeps
#              its executables there.
#   ANTI_REPLACE: replace an install that is there.
#   ANTI_PATH: add the line to the shell profile.
#   ANTI_MICROSOFT: let xwin fetch the CRT and the Windows SDK.
set -eu

base=${ANTI_BASE:-https://anti-lang.com/downloads/resources}

# DESIGN: the installer carries the public key that checks SHA256SUMS.sig of
# the LLVM tools, and the package carries none. anti-lang.com serves this
# script, and GitHub serves the tools. A key that travelled with them could
# be replaced with them. anti-lang.com serves the same key as keys/release.pem.
release_key='-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEao0Di9RL8gvG6oA9x7gIDJ7/zLn6
/J5i5dgtCf82Hvpro/4umWhaPA8APgrIJKLD4XDvTqhLijckvFxj0f3Bhg==
-----END PUBLIC KEY-----'

version=${ANTI_VERSION:-}
arch=${ANTI_ARCH:-}

for argument in "$@"; do
    case $argument in
    --arm | --arm64) arch=arm64 ;;
    --intel | --x86_64 | --x64) arch=x86_64 ;;
    *)
        echo "anti: unknown option $argument" >&2
        echo "usage: install.sh [--arm | --intel]" >&2
        exit 2
        ;;
    esac
done

say() {
    echo "anti: $*"
}

fail() {
    echo "anti: $*" >&2
    exit 1
}

# Ask one yes or no question. The variable of its name answers it, and a
# plain return accepts. A pipe leaves no terminal on standard input, so
# the question goes to /dev/tty, and without one the default stands.
ask() {
    eval "given=\${ANTI_$1:-}"
    case $given in
    y | Y | yes | Yes) return 0 ;;
    n | N | no | No) return 1 ;;
    esac
    if ! { exec 3</dev/tty; } 2>/dev/null; then
        return 0
    fi
    answer=""
    printf '%s [Y/n] ' "$2" >&2
    read -r answer <&3 || true
    exec 3<&-
    case $answer in
    n | N | no | No) return 1 ;;
    *) return 0 ;;
    esac
}

sha256() {
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | cut -d ' ' -f 1
    else
        sha256sum "$1" | cut -d ' ' -f 1
    fi
}

# DESIGN: SHA256SUMS says which bytes are the package, so no line of it is
# read before openssl has checked SHA256SUMS.sig against the key above.
# The digest of the download proves nothing on its own: whoever serves the
# package serves the manifest beside it. A missing openssl therefore stops
# the install here. It only warns for the LLVM tools, whose digest
# tools/llvm-pin of the package carries as well.
check_manifest() {
    if curl -fsSL -o "$work/SHA256SUMS.sig" \
        "$base/anti/$version/SHA256SUMS.sig" 2>/dev/null; then
        command -v openssl >/dev/null 2>&1 ||
            fail "openssl is missing, and without it SHA256SUMS.sig is no signature of anything"
        printf '%s\n' "$release_key" > "$work/release.pem"
        openssl dgst -sha256 -binary -out "$work/SHA256SUMS.sha256" \
            "$work/SHA256SUMS"
        openssl pkeyutl -verify -pubin -inkey "$work/release.pem" \
            -in "$work/SHA256SUMS.sha256" -sigfile "$work/SHA256SUMS.sig" \
            >/dev/null 2>&1 ||
            fail "SHA256SUMS of $version carries no signature of the key of Anti"
        say "SHA256SUMS carries the signature of the key of Anti"
        return 0
    fi
    case ${ANTI_STAGING:-no} in
    y | Y | yes | Yes)
        say "warning: the staging area of a release holds no SHA256SUMS.sig, so this installer checked the digest and not the signature"
        ;;
    *)
        fail "$base/anti/$version holds no SHA256SUMS.sig, and an unsigned manifest names no package"
        ;;
    esac
}

case "$(uname -s)" in
Darwin) os=macos ;;
Linux) os=linux ;;
*) fail "no package for $(uname -s)" ;;
esac
case "$(uname -m)" in
arm64 | aarch64) native=arm64 ;;
x86_64) native=x86_64 ;;
*) fail "no package for $(uname -m)" ;;
esac
if [ -z "$arch" ]; then
    arch=$native
fi
case $arch in
arm64 | x86_64) ;;
*) fail "the processor $arch has no package, only arm64 and x86_64" ;;
esac
host=$os-$arch

# DESIGN: an install of Anti is local to the user and follows the
# conventions of the platform. The toolchain and the runtime archive go
# in the data directory, and the two executables in the bin directory,
# which is on the PATH. "Names and publication" in docs/decisions.md
# holds the rule. src/userdirs.c builds the same paths for antic, anti.os
# gives them to a program, and the test installer_options pins the
# spellings together.
#
# A package of the other processor runs under emulation and stands beside
# the native one in a data directory of its own. Its executables stay in
# the bin/ of that tree, because the PATH names one antic and it is the
# one this machine runs without emulation.
app=anti
if [ "$arch" != "$native" ]; then
    app=anti-$arch
fi
data_root=${XDG_DATA_HOME:-}
case $data_root in
/*) ;;
*) data_root=$HOME/.local/share ;;
esac
bin_dir=${XDG_BIN_HOME:-}
case $bin_dir in
/*) ;;
*) bin_dir=$HOME/.local/bin ;;
esac

# DESIGN: ANTI_HOME names one tree that carries everything, the two
# executables included, and nothing outside it is written. Step 10 of a
# release checks a download that way, into a directory of its own. A user
# who wants the whole install in one place asks for it the same way.
one_tree=no
if [ -n "${ANTI_HOME:-}" ]; then
    home=$ANTI_HOME
    one_tree=yes
else
    home=$data_root/$app
fi
if [ "$arch" != "$native" ]; then
    say "installing the $arch package, which this machine runs under emulation"
fi

if [ -z "$version" ]; then
    version=$(curl -fsSL "$base/anti/latest" | tr -d ' \n')
fi
asset=anti-$version-$host.tar.xz
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

say "downloading $asset"
curl -fsSL -o "$work/$asset" "$base/anti/$version/$asset"
curl -fsSL -o "$work/SHA256SUMS" "$base/anti/$version/SHA256SUMS"
check_manifest
want=$(grep " $asset\$" "$work/SHA256SUMS" | cut -d ' ' -f 1)
got=$(sha256 "$work/$asset")
if [ "$want" != "$got" ]; then
    fail "$asset: SHA-256 $got, expected $want"
fi

if [ -e "$home" ] && ! ask REPLACE "$home exists. Replace it?"; then
    fail "$home exists. Set ANTI_HOME elsewhere to keep it"
fi
rm -rf "$home"
mkdir -p "$(dirname "$home")"
tar -xJf "$work/$asset" -C "$work"
mv "$work/anti" "$home"
# The marker that tells a tree this installer wrote from any other
# directory. The uninstaller removes nothing without it.
printf '%s\n' "$version" > "$home/.anti-install"
say "installed $version in $home"

# DESIGN: the site serves this installer, so it is always the newest.
# The CMake scripts it drives come from the package, which is as old as
# the version installed. The number rises when the options it passes
# change, and a package older than that fails here rather than halfway.
PACKAGE_API=1
api=0
if [ -f "$home/tools/package-api" ]; then
    api=$(tr -d ' \n' < "$home/tools/package-api")
fi
if [ "$api" -lt "$PACKAGE_API" ]; then
    fail "Anti $version speaks version $api of the installer interface, and \
this installer needs $PACKAGE_API. Install a newer version of Anti."
fi

# DESIGN: the LLVM tools come from the release that tools/llvm-pin of the
# package names. The pinned digest decides, and openssl checks the signature
# of SHA256SUMS against the key of this installer. macOS and Linux carry
# openssl. A package without the pin carries the tools itself.
llvm_pin=$home/tools/llvm-pin
if [ -f "$llvm_pin" ]; then
    row() {
        sed -n "s/^$1=//p" "$llvm_pin"
    }
    llvm_version=$(tr -d ' \n' < "$home/tools/llvm-version")
    tag=$(row tag | sed "s/@VERSION@/$llvm_version/g")
    release=$(row release | sed "s/@TAG@/$tag/g")
    tools=$(row file | sed "s/@TAG@/$tag/g; s/@HOST@/$host/g")
    digest=$(row "$host-digest")
    if [ -z "$digest" ]; then
        fail "tools/llvm-pin names no LLVM tools for $host"
    fi
    say "downloading $tools"
    curl -fsSL -o "$work/$tools" "$release/$tools"
    curl -fsSL -o "$work/llvm-sums" "$release/SHA256SUMS"
    curl -fsSL -o "$work/llvm-sums.sig" "$release/SHA256SUMS.sig"
    got=$(sha256 "$work/$tools")
    if [ "$got" != "$digest" ]; then
        fail "$tools: SHA-256 $got, expected $digest"
    fi
    listed=$(grep "  $tools\$" "$work/llvm-sums" | cut -d ' ' -f 1)
    if [ "$listed" != "$digest" ]; then
        fail "SHA256SUMS of $tag lists '$listed' for $tools, and the pin $digest"
    fi
    if command -v openssl >/dev/null 2>&1; then
        printf '%s\n' "$release_key" > "$work/release.pem"
        openssl dgst -sha256 -binary -out "$work/llvm-sums.sha256" \
            "$work/llvm-sums"
        if ! openssl pkeyutl -verify -pubin -inkey "$work/release.pem" \
            -in "$work/llvm-sums.sha256" -sigfile "$work/llvm-sums.sig" \
            >/dev/null 2>&1; then
            fail "SHA256SUMS of $tag carries no signature of the key of Anti"
        fi
    else
        say "warning: openssl is missing, so this installer checked the digest of $tools and not the signature"
    fi
    tar -xJf "$work/$tools" -C "$home" bin licenses
fi

# lld answers to its four names through argv[0], and the archive carries
# one copy of it.
if [ -e "$home/bin/lld" ]; then
    for name in ld.lld ld64.lld lld-link; do
        if [ ! -e "$home/bin/$name" ]; then
            cp "$home/bin/lld" "$home/bin/$name"
        fi
    done
fi

# CMake installs the sysroot of this host. The package holds the script
# and the pins, and the pinned CMake stands in when the host has none.
cmake=$(command -v cmake || true)
if [ -z "$cmake" ]; then
    pin=$(grep "^$host-url=" "$home/tools/cmake-pin" | cut -d = -f 2-)
    cmake_version=$(tr -d ' \n' < "$home/tools/cmake-version")
    url=$(echo "$pin" | sed "s/@VERSION@/$cmake_version/g")
    digest=$(grep "^$host-digest=" "$home/tools/cmake-pin" | cut -d = -f 2-)
    say "installing CMake $cmake_version, which the sysroot step needs"
    curl -fsSL -o "$work/cmake.tar.gz" "$url"
    got=$(sha256 "$work/cmake.tar.gz")
    if [ "$digest" != "$got" ]; then
        fail "CMake: SHA-256 $got, expected $digest"
    fi
    mkdir -p "$home/tools/cmake"
    tar -xzf "$work/cmake.tar.gz" -C "$home/tools/cmake" --strip-components=1
    cmake=$(find "$home/tools/cmake" -name cmake -type f -perm -u+x | head -1)
fi

# DESIGN: a package that carries Zig's stubs links for macOS with no SDK.
# A program that names a framework takes the SDK of the Command Line Tools,
# which antic finds on its own. A package without the stubs, as 0.1.0 is,
# takes the stubs of the Command Line Tools here.
case $host in
macos-*)
    if [ -f "$home/sysroot/macos-$arch/usr/lib/libSystem.tbd" ]; then
        say "the package links for macOS with Zig's stubs of libSystem"
    else
        if [ ! -d /Library/Developer/CommandLineTools/SDKs ]; then
            say "the macOS SDK is missing. Apple installs it with xcode-select --install"
            if ask "Run xcode-select --install now?"; then
                xcode-select --install || true
                say "run this installer again when the Command Line Tools are in place"
                exit 0
            fi
            fail "no SDK, so no linking for macOS"
        fi
        say "taking the SDK stubs from the Command Line Tools"
        "$cmake" -DDEST="$home/sysroot" -DLLVM_BIN="$home/bin" \
            -DTARGETS="macos-arm64;macos-x86_64" \
            -P "$home/tools/get-sysroot.cmake" >/dev/null
    fi
    ;;
esac

if ask MICROSOFT "Also install the Microsoft CRT and Windows SDK, so that this host builds Windows programs?"; then
    say "xwin downloads them, and Microsoft licenses them to you"
    # The progress bar of xwin draws only when its own standard output is
    # a terminal. CMake writes the command and this shell runs it, so the
    # bar has the terminal of the user rather than a pipe.
    "$cmake" -DDEST="$home/sysroot" -DLLVM_BIN="$home/bin" \
        -DACCEPT_LICENSE=yes -DSPLAT=script \
        -DTARGETS="windows-x86_64;windows-arm64" \
        -P "$home/tools/get-sysroot.cmake" >/dev/null
    sh "$home/sysroot/.download/splat.sh"
    "$cmake" -DDEST="$home/sysroot" -DLLVM_BIN="$home/bin" \
        -DACCEPT_LICENSE=yes -DSPLAT=done \
        -DTARGETS="windows-x86_64;windows-arm64" \
        -P "$home/tools/get-sysroot.cmake"
else
    say "Windows programs need the Microsoft CRT. Add it later with:"
    echo "    $cmake -DDEST=$home/sysroot -DLLVM_BIN=$home/bin \\"
    echo "        -DACCEPT_LICENSE=yes -DTARGETS='windows-x86_64;windows-arm64' \\"
    echo "        -P $home/tools/get-sysroot.cmake"
fi

# DESIGN: the PATH names one antic, and it is the one this machine runs
# without emulation. A package of the other processor is therefore called
# by its path, and so is an install that ANTI_HOME put in one tree.
if [ "$arch" != "$native" ] || [ "$one_tree" = yes ]; then
    say "run it with: $home/bin/antic hello.anti -o hello"
    exit 0
fi

# The two executables go in the bin directory of the user, and the rest
# of the archive stays where it is. antic finds it by the same rule that
# src/userdirs.c holds: the directory above itself when that one carries
# lib/, and the user's data directory otherwise.
mkdir -p "$bin_dir"
for program in antic anti; do
    cp "$home/bin/$program" "$bin_dir/$program"
done
say "put antic and anti in $bin_dir"

# The profile of the login shell. The marker lets the uninstaller remove
# its own line and leave one that the user wrote.
case ${SHELL:-} in
*/zsh) profile=$HOME/.zshrc ;;
*/bash) profile=$HOME/.bashrc ;;
*) profile=$HOME/.profile ;;
esac
line="export PATH=\"$bin_dir:\$PATH\"  # anti"
case :${PATH}: in
*:"$bin_dir":*)
    say "$bin_dir is already on the PATH"
    ;;
*)
    if [ -f "$profile" ] && grep -q "# anti\$" "$profile"; then
        say "$profile already holds the line for Anti"
    elif ask PATH "Add $bin_dir to the PATH in $profile?"; then
        printf '%s\n' "$line" >> "$profile"
        say "added $bin_dir to the PATH in $profile, which a new shell reads"
    else
        say "add this line to your shell profile yourself:"
        echo "    $line"
    fi
    ;;
esac
say "then compile a program with: antic hello.anti -o hello"
