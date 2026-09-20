#!/bin/sh
# Remove the Anti that tools/install.sh put in the directories of the
# platform.
#
#   curl -fsSL https://anti-lang.com/uninstall.sh | sh
#   curl -fsSL https://anti-lang.com/uninstall.sh | sh -s -- --intel  # docs-style:ignore
#
# It deletes the data directory and the two executables it put on the
# PATH, and nothing else. The line the installer added to the shell
# profile goes with them. A line the user wrote is named rather than
# removed.
#
# DESIGN: a directory is removed only when it holds the marker file
# .anti-install that the installer wrote. ANTI_HOME takes whatever it is
# given. A value left over from another run, or a directory shared with
# something else, would otherwise take its siblings with it.
#
# The option --arm or --intel removes the install of that processor. It
# stands in the data directory of the name anti-<cpu> when it is not the
# one of this machine. ANTI_ARCH holds the same choice as arm64 or
# x86_64.
#
#   ANTI_ARCH       arm64 or x86_64, default the processor of this machine
#   ANTI_HOME       one tree to remove instead
#   ANTI_REMOVE     yes or no to removing the directory
set -eu

remove=${ANTI_REMOVE:-}
arch=${ANTI_ARCH:-}

for argument in "$@"; do
    case $argument in
    --arm | --arm64) arch=arm64 ;;
    --intel | --x86_64 | --x64) arch=x86_64 ;;
    *)
        echo "anti: unknown option $argument" >&2
        echo "usage: uninstall.sh [--arm | --intel]" >&2
        exit 2
        ;;
    esac
done

case "$(uname -m)" in
arm64 | aarch64) native=arm64 ;;
x86_64) native=x86_64 ;;
*) native="" ;;
esac
app=anti
if [ -n "$arch" ] && [ "$arch" != "$native" ]; then
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
one_tree=no
if [ -n "${ANTI_HOME:-}" ]; then
    home=$ANTI_HOME
    one_tree=yes
else
    home=$data_root/$app
fi

say() {
    echo "anti: $*"
}

if [ ! -e "$home" ]; then
    say "$home does not exist, so nothing is installed there"
    exit 0
fi
if [ ! -f "$home/.anti-install" ]; then
    say "$home holds no .anti-install, so no installer of Anti wrote it"
    say "nothing removed"
    exit 1
fi

size=$(du -sh "$home" 2>/dev/null | cut -f 1)
say "$home holds $size"
case $remove in
y | Y | yes | Yes) ;;
n | N | no | No) say "nothing removed"; exit 0 ;;
*)
    if { exec 3</dev/tty; } 2>/dev/null; then
        printf 'Remove %s? [Y/n] ' "$home" >&2
        answer=""
        read -r answer <&3 || true
        exec 3<&-
        case $answer in
        n | N | no | No) say "nothing removed"; exit 0 ;;
        esac
    fi
    ;;
esac

rm -rf "$home"
say "removed $home"

# The two executables the installer copied to the bin directory, and only
# those two. A file of the user's own with another name stays.
if [ "$one_tree" = no ]; then
    for program in antic anti; do
        if [ -f "$bin_dir/$program" ]; then
            rm -f "$bin_dir/$program"
            say "removed $bin_dir/$program"
        fi
    done
fi

# The installer marks its own line with a comment, so only that line
# goes. A line that the user wrote is named rather than edited.
for profile in "$HOME/.zshrc" "$HOME/.bashrc" "$HOME/.bash_profile" \
               "$HOME/.profile"; do
    if [ ! -f "$profile" ]; then
        continue
    fi
    if grep -q "# anti$" "$profile"; then
        grep -v "# anti$" "$profile" > "$profile.anti-new" &&
            mv "$profile.anti-new" "$profile"
        say "removed the line for Anti from $profile"
    elif grep -q "$bin_dir" "$profile"; then
        say "a line of your own names Anti in $profile"
    fi
done
