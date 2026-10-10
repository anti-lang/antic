#!/bin/sh
# Install Anti for the user who runs it, in the directories of the
# platform.
#
#   curl -fsSL https://anti-lang.com/install.sh | bash
#   curl -fsSL https://anti-lang.com/install.sh | bash -s -- --intel  # docs-style:ignore
#
# It downloads the package of this host and SHA256SUMS from the GitHub
# release of the version, and SHA256SUMS.sig from anti-lang.com. It checks
# the signature first, then the SHA-256 of the package against the
# manifest, unpacks the package and puts antic and anti on the path. The
# package carries everything a build takes, the LLVM tools and the
# sysroot of every target among them, so nothing else is downloaded and
# nothing of the package is run but its two programs, whose --version is
# the check of the install. The script is POSIX sh, and bash runs it.
#
# The option --arm or --intel takes the package of that processor rather
# than the one of this machine. A machine that emulates the other
# processor runs it. ANTI_ARCH holds the same choice as arm64 or x86_64.
#
# Each question takes yes or no from a variable of its own name.
#
#   ANTI_VERSION: the version to install, default the newest, which the
#              latest-release API names.
#   ANTI_BASE: a staging area to install from rather than the release,
#              in the layout <base>/anti/<version>/<file> that the packer
#              writes, usually with a file:// prefix. A release checks its
#              own packages through it before they are published. It takes
#              ANTI_VERSION with it, because a staging area names no
#              newest version.
#   ANTI_GITHUB: the release area, default the releases of
#              anti-lang/antic. The tests of the installer stand a fake
#              one on disk, and a fork names its own.
#   ANTI_GITHUB_API: the latest-release API that names the newest version,
#              default the one of anti-lang/antic.
#   ANTI_SITE_BASE: the site that serves SHA256SUMS.sig, default
#              anti-lang.com. The tests of the installer stand a fake one
#              on disk.
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
set -eu

# DESIGN: the two halves of a release stand on two hosts, and a forged
# release needs both of them. The GitHub release of the tag holds the
# packages, the symbols archives and SHA256SUMS. anti-lang.com holds
# SHA256SUMS.sig, the public key, the two installers and the downloads
# page. Whoever takes GitHub changes binaries the signature no longer
# covers. Whoever takes the site signs nothing, because the private key
# is on neither host. A signature stored beside the binaries it covers
# would leave the private key as the only thing between an attacker and a
# release. tools/release-base and tools/site-base of the repository name
# these addresses, and the test installer_github pins them against this
# copy.
github=${ANTI_GITHUB:-https://github.com/anti-lang/antic/releases/download}
github_api=${ANTI_GITHUB_API:-https://api.github.com/repos/anti-lang/antic/releases/latest}
site=${ANTI_SITE_BASE:-https://anti-lang.com}
base=${ANTI_BASE:-}

# DESIGN: the installer carries the public key that checks SHA256SUMS.sig
# of the package. The download carries no key of its own. anti-lang.com
# serves the installer, and GitHub serves the package. A key that
# travelled with it could be replaced with it. anti-lang.com serves the
# same key as tools/keys/release.pem.
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
# package serves the manifest beside it. The signature comes from the
# other host for that reason. openssl checks it against the key the
# installer carries, never against one fetched beside it. A missing
# openssl therefore stops the install here.
check_manifest() {
    if curl -fsSL -o "$work/SHA256SUMS.sig" "$signature" 2>/dev/null; then
        command -v openssl >/dev/null 2>&1 ||
            fail "openssl is missing, and without it SHA256SUMS.sig is no signature of anything"
        printf '%s\n' "$release_key" > "$work/release.pem"
        openssl dgst -sha256 -binary -out "$work/SHA256SUMS.sha256" \
            "$work/SHA256SUMS"
        openssl pkeyutl -verify -pubin -inkey "$work/release.pem" \
            -in "$work/SHA256SUMS.sha256" -sigfile "$work/SHA256SUMS.sig" \
            >/dev/null 2>&1 ||
            fail "SHA256SUMS of $version carries no signature of the key of Anti"
        say "SHA256SUMS carries the signature of the key of Anti, from ${signature%/*}"
        return 0
    fi
    case ${ANTI_STAGING:-no} in
    y | Y | yes | Yes)
        say "warning: the staging area of a release holds no SHA256SUMS.sig, so this installer checked the digest and not the signature"
        ;;
    *)
        fail "$signature answered nothing, and an unsigned manifest names no package"
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
# holds the rule. src/antic/userdirs.c builds the same paths for antic, anti.os
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

# The newest version is the tag of the newest release, which the
# latest-release API names. A staging area holds one version and no API,
# so a run against one takes the version from the caller.
if [ -z "$version" ]; then
    if [ -n "$base" ]; then
        fail "ANTI_BASE names a staging area, which names no newest version. Set ANTI_VERSION with it."
    fi
    version=$(curl -fsSL "$github_api" |
        sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"v\{0,1\}\([^"]*\)".*/\1/p')
    [ -n "$version" ] || fail "$github_api names no newest version of Anti"
fi

# The assets of a release stand under its tag, and the signature of the
# manifest stands on the site. A staging area of a release holds both in
# the layout the packer writes, one directory per version, because step 5
# installs before step 9 publishes anything.
if [ -n "$base" ]; then
    area=$base/anti/$version
    signature=$area/SHA256SUMS.sig
else
    area=$github/v$version
    signature=$site/downloads/anti/$version/SHA256SUMS.sig
fi
asset=anti-$version-$host.tar.xz
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

say "downloading $asset"
curl -fsSL -o "$work/$asset" "$area/$asset"
curl -fsSL -o "$work/SHA256SUMS" "$area/SHA256SUMS"
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

# DESIGN: the check of an install is the two programs of the package,
# run from where they were unpacked. The package carries the LLVM tools
# in bin/ and the sysroot of every target in sysroot/, so nothing is
# downloaded or laid out here and no script of the package runs. A
# package whose programs print another version, or that this machine
# cannot run, is no install. Eddie decided this on 2026-10-08, in
# decision 7 of docs/work-order-distribution.md.
for program in antic anti; do
    printed=$("$home/bin/$program" --version 2>&1) ||
        fail "$home/bin/$program --version failed: $printed"
    [ "$printed" = "$program $version" ] ||
        fail "$home/bin/$program prints '$printed', and the package is $version"
    say "$printed"
done
say "the package carries the LLVM tools and the sysroots of all six targets, so nothing else is downloaded"

# DESIGN: the PATH names one antic, and it is the one this machine runs
# without emulation. A package of the other processor is therefore called
# by its path, and so is an install that ANTI_HOME put in one tree.
if [ "$arch" != "$native" ] || [ "$one_tree" = yes ]; then
    say "run it with: $home/bin/antic hello.anti -o hello"
    exit 0
fi

# The two executables go in the bin directory of the user, and the rest
# of the archive stays where it is. antic finds it by the same rule that
# src/antic/userdirs.c holds: the directory above itself when that one carries
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
