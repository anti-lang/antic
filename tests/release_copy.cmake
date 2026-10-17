# The copy of the tree that the tests release_* run ./r on, with the
# version bumped, and its first ./r --dry-run, which must pass and warn
# of nothing. The copy is a clone with a bare origin, so the preflight
# sees a committed and pushed main. The commands that build, test, pack
# and reach the two VMs are stand-ins on the PATH, which write what the
# real ones write and compile nothing. A release is an hour of work on
# five machines, and what the tests check is the script: the order of the
# steps, the output of each, the refusals of the preflight and the plan
# it prints for the steps a dry run does not run. The copy is the staged
# tree, so a change of the script reaches the tests after git add and not
# before.
#
# Include it after tests/program_output.cmake with these values set:
#   ROOT       the repository
#   WORK       the directory of the test, made empty here
#   CC         the C compiler of the tree, for the fixtures
#   HOST_LINK  the options of a link of a program of this host
#   SDK        the Apple SDK of the build, empty on another system
#   LLVM_BIN   the directory of lld-link
#
# It sets release_skipped where a test cannot run, after it printed the
# reason with SKIP:, and otherwise copy, dist, version, hosts, machine,
# saved_path, GIT and the output of the first dry run in out and err.

if(NOT APPLE)
    message("SKIP: a release is made on the development Mac")
    set(release_skipped ON)
    return()
endif()
if(NOT EXISTS "${ROOT}/.git")
    message("SKIP: an exported tree has no history to clone")
    set(release_skipped ON)
    return()
endif()
find_program(GIT git)
if(NOT GIT)
    message("SKIP: git is missing")
    set(release_skipped ON)
    return()
endif()

set(version "99.0.0")
set(hosts macos-arm64 macos-x86_64 linux-x86_64 linux-arm64
          windows-x86_64 windows-arm64)
string(REPLACE ";" " " host_words "${hosts}")

# The fixtures are C this test writes, under the warnings of the
# repository.
include("${CMAKE_CURRENT_LIST_DIR}/../tools/warnings.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/bin" "${WORK}/fixture")

# The two programs a package carries. The one of macos-x86_64 is compiled
# for that processor, because the packer checks it under Rosetta, and the
# others are compiled for this machine. Each prints the version, which is
# what the release script reads.
set(sdk "${SDK}")
if(NOT sdk)
    message("SKIP: this machine names no macOS SDK")
    set(release_skipped ON)
    return()
endif()
foreach(program antic anti)
    file(WRITE "${WORK}/fixture/${program}.c"
         "#include <stdio.h>\n"
         "int main(void) { printf(\"${program} ${version}\\n\"); return 0; }\n")
    tool_run("the fixture ${program} did not compile"
             "${CC}" ${ANTIC_C_WARNINGS} -O0 -isysroot "${sdk}" ${HOST_LINK}
             -o "${WORK}/fixture/${program}"
             "${WORK}/fixture/${program}.c")
    tool_run("the fixture ${program} did not compile for x86_64"
             "${CC}" ${ANTIC_C_WARNINGS} -arch x86_64 -O0
             -isysroot "${sdk}" ${HOST_LINK}
             -o "${WORK}/fixture/${program}-x86_64"
             "${WORK}/fixture/${program}.c")
endforeach()

# The two programs of a Windows host are real COFF executables with a PDB
# beside each. Step 4 reads the map of the sections of a binary with no
# symbol table and checks the CodeView record of the executable against
# the GUID of its PDB, and a Mach-O file under the name antic.exe would
# reach neither. They link nothing of the Microsoft C runtime, carry one
# function and never run.
file(WRITE "${WORK}/fixture/windows.c"
     "int mainCRTStartup(void) { return 0; }\n")
foreach(pair "windows-x86_64=x86_64-pc-windows-msvc=X64"
        "windows-arm64=aarch64-pc-windows-msvc=ARM64")
    string(REPLACE "=" ";" parts "${pair}")
    list(GET parts 0 host)
    list(GET parts 1 triple)
    list(GET parts 2 machine)
    file(MAKE_DIRECTORY "${WORK}/fixture/${host}")
    tool_run("the fixture of ${host} did not compile for ${triple}"
             "${CC}" ${ANTIC_C_WARNINGS} "--target=${triple}" -c
             -o "${WORK}/fixture/${host}/windows.obj"
             "${WORK}/fixture/windows.c")
    foreach(program antic anti)
        tool_run("lld-link did not link the fixture ${program} of ${host}"
                 "${LLVM_BIN}/lld-link" /NOLOGO /DEBUG "/PDBALTPATH:%_PDB%"
                 /pdbsourcepath:.
                 /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE /NODEFAULTLIB
                 "/MACHINE:${machine}"
                 "/OUT:${WORK}/fixture/${host}/${program}.exe"
                 "/PDB:${WORK}/fixture/${host}/${program}.pdb"
                 "${WORK}/fixture/${host}/windows.obj")
    endforeach()
endforeach()

# The stand-in for cmake. It writes what each call of the release script
# writes: the cache the script reads its download paths from, the runtime
# of a build, the six packages of the packer and the PDB of every Windows
# program. A call that checks a package passes, because the tests
# linux_libc, check_cpu and pdb_guid_<target> cover those three scripts.
# What this test reads of step 4 is therefore the wiring: that the packer
# leaves a PDB per Windows program, that the step finds it and that it
# goes into the archive of its host.
include("${ROOT}/tools/pinned-tools.cmake")
antic_llvm_archive_tools(archive_tools)
list(JOIN archive_tools " " archive_tools)
file(WRITE "${WORK}/bin/cmake" "#!/bin/sh
set -e
dest=\"\"
symbols=\"\"
hosts=\"\"
build=\"\"
source=\"\"
script=\"\"
preset=\"\"
previous=\"\"
host=\"\"
for word in \"\$@\"; do
    case \$word in
    -S) previous=source ;;
    -B) previous=build ;;
    -P) previous=script ;;
    --preset) previous=preset ;;
    --build) previous=build ;;
    -DDEST=*) dest=\${word#-DDEST=} ;;
    -DHOST=*) host=\${word#-DHOST=} ;;
    -DSYMBOLS=*) symbols=\${word#-DSYMBOLS=} ;;
    -DHOSTS=*) hosts=\$(echo \"\${word#-DHOSTS=}\" | tr ';' ' ') ;;
    *)
        case \$previous in
        source) source=\$word ;;
        build) build=\$word ;;
        script) script=\$word ;;
        preset) preset=\$word ;;
        esac
        previous=\"\"
        ;;
    esac
done

cache() {
    mkdir -p \"\$1\"
    cat > \"\$1/CMakeCache.txt\" <<EOF
ANTIC_CLANG_DIR:PATH=${ROOT}/build/deps/clang
ANTIC_LLVM_DIR:PATH=${WORK}/deps/llvm
ANTIC_RAYLIB_DIR:PATH=${ROOT}/build/deps/raylib
ANTIC_SYSROOT_DIR:PATH=${ROOT}/build/deps/sysroot
ANTIC_SYSTEM_COMPILER:BOOL=OFF
EOF
    mkdir -p \"\$1/runtime/lib\" \"\$1/runtime/std\" \"\$1/runtime/licenses\"
    cp '${WORK}/fixture/antic' \"\$1/antic\"
    cp '${WORK}/fixture/anti' \"\$1/anti\"
}

if [ -n \"\$script\" ]; then
    case \$(basename \"\$script\") in
    pack-anti.cmake)
        [ -n \"\$dest\" ] || exit 1
        mkdir -p \"\$dest\"
        sums=\"\"
        for host in \$hosts; do
            tree=\$dest/work/\$host
            rm -rf \"\$tree\"
            mkdir -p \"\$tree/anti/bin\"
            suffix=\"\"
            case \$host in windows-*) suffix=.exe ;; esac
            fixture=${WORK}/fixture
            case \$host in
            windows-*) antic=\$fixture/\$host/antic.exe; anti=\$fixture/\$host/anti.exe ;;
            *-x86_64) antic=\$fixture/antic-x86_64; anti=\$fixture/anti-x86_64 ;;
            *) antic=\$fixture/antic; anti=\$fixture/anti ;;
            esac
            cp \"\$antic\" \"\$tree/anti/bin/antic\$suffix\"
            cp \"\$anti\" \"\$tree/anti/bin/anti\$suffix\"
            # The packer leaves the PDB of a Windows program outside the
            # package, and step 4 folds it into the symbols archive.
            case \$host in
            windows-*)
                [ -n \"\$symbols\" ] || exit 1
                mkdir -p \"\$symbols/\$host\"
                cp \"\$fixture/\$host/antic.pdb\" \"\$symbols/\$host/antic.pdb\"
                cp \"\$fixture/\$host/anti.pdb\" \"\$symbols/\$host/anti.pdb\"
                ;;
            esac
            for target in ${host_words}; do
                case \$target in
                *-arm64) levels='armv8.0 armv8.2 armv8.5' ;;
                *) levels='v1 v2 v3' ;;
                esac
                library=libanti_rt.a
                case \$target in windows-*) library=anti_rt.lib ;; esac
                for level in \$levels; do
                    mkdir -p \"\$tree/anti/lib/\$target/\$level\"
                    : > \"\$tree/anti/lib/\$target/\$level/\$library\"
                done
            done
            name=anti-${version}-\$host.tar.xz
            (cd \"\$tree\" && XZ_OPT=-0 tar -cJf \"\$dest/\$name\" anti)
            rm -rf \"\$tree\"
            sums=\"\$sums\$(cd \"\$dest\" && shasum -a 256 \"\$name\")
\"
        done
        printf '%s' \"\$sums\" > \"\$dest/SHA256SUMS\"
        rmdir \"\$dest/work\" 2>/dev/null || true
        exit 0
        ;;
    get-llvm.cmake)
        # The tools of another host, laid out as bin/ of the runtime
        # archive: the names of antic_llvm_archive_tools and llvm-version.
        [ -n \"\$dest\" ] && [ -n \"\$host\" ] || exit 1
        mkdir -p \"\$dest/bin\"
        for tool in ${archive_tools}; do
            echo \"stand-in \$tool of \$host\" > \"\$dest/bin/\$tool\"
        done
        echo '${llvm_version}' > \"\$dest/bin/llvm-version\"
        exit 0
        ;;
    *) exit 0 ;;
    esac
fi
if [ -n \"\$preset\" ]; then
    cache \"\$PWD/build/\$preset\"
    exit 0
fi
if [ -n \"\$source\" ] && [ -n \"\$build\" ]; then
    cache \"\$build\"
    exit 0
fi
if [ -n \"\$build\" ]; then
    cache \"\$build\"
    exit 0
fi
exit 0
")

# The stand-in for ctest prints the line the script reads the count from.
file(WRITE "${WORK}/bin/ctest" "#!/bin/sh
echo '100% tests passed, 0 tests failed out of 478'
exit 0
")

# The stand-in for the GitHub CLI. The preflight asks for the login, and a
# dry run calls nothing else.
file(WRITE "${WORK}/bin/gh" "#!/bin/sh
case \"\$1 \$2\" in
'auth status') echo 'Logged in to github.com account anti-lang' ;;
*) echo \"gh: the dry run called \$*\" >&2; exit 1 ;;
esac
exit 0
")

# The stand-ins for the two VMs. Every command answers with the lines the
# script reads: the refusal of the unsigned manifest that the installer
# there prints, the version of a program it installed, the last line of
# the check with the network off and the count of a suite.
#
# DESIGN: the stand-in keeps the state of the network of each machine in a
# file, which the command that turns it off writes and the one that turns
# it on removes, and the probe answers by it. Every call goes into ssh.log
# with the machine and that state, so the test reads which phase ran with
# the network off. SSH_FAILS names a command that fails, and
# SSH_OFF_IGNORED makes the command that turns the network off do nothing.
file(WRITE "${WORK}/bin/ssh" "#!/bin/sh
machine=\"\"
while [ \$# -gt 0 ]; do
    case \$1 in
    -n) shift ;;
    -o) shift; shift ;;
    *) machine=\$1; shift; break ;;
    esac
done
command=\"\$*\"
state='${WORK}'/offline-\$machine
case \$command in
*'add table inet anti_offline'* | *'add rule name=anti-offline'*)
    [ -n \"\${SSH_OFF_IGNORED:-}\" ] || : > \"\$state\" ;;
*'destroy table inet anti_offline'* | *'delete rule name=anti-offline'*)
    rm -f \"\$state\" ;;
esac
network=on
[ ! -f \"\$state\" ] || network=off
printf '%s\\n' \"\$machine \$network \$command\" >> '${WORK}/ssh.log'
case \$command in
*github.com*)
    [ \$network = on ]
    exit
    ;;
esac
if [ -n \"\${SSH_FAILS:-}\" ]; then
    case \$command in
    *\"\$SSH_FAILS\"*) echo 'the stand-in fails this call'; exit 1 ;;
    esac
fi
echo 'the installer refuses a manifest without a signature'
echo 'antic ${version}'
echo 'anti ${version}'
echo 'the install builds for this host and links for the other five targets'
echo '100% tests passed, 0 tests failed out of 409'
exit 0
")
file(WRITE "${WORK}/bin/scp" "#!/bin/sh
exit 0
")
# The stand-in for the rsync of step 9, which a dry run never calls and
# which answers nothing when it does.
file(WRITE "${WORK}/bin/rsync" "#!/bin/sh
exit 0
")
foreach(name cmake ctest gh ssh scp rsync)
    execute_process(COMMAND chmod +x "${WORK}/bin/${name}")
endforeach()

# The copy of the tree, which is the staged tree of this checkout with
# the version bumped. It gets a history of one commit and a bare origin,
# so that the preflight sees a main that is committed and pushed.
set(copy "${WORK}/antic")
file(MAKE_DIRECTORY "${copy}")
tool_run("the export of the tree failed" "${GIT}" -C "${ROOT}" checkout-index -a
         --prefix=${copy}/)
tool_run("the bare origin failed" "${GIT}" init --quiet --bare -b main
         "${WORK}/origin.git")
tool_run("git init failed" "${GIT}" init --quiet -b main "${copy}")
tool_run("git config failed" "${GIT}" -C "${copy}" config user.email "r@example.com")
tool_run("git config failed" "${GIT}" -C "${copy}" config user.name "Release Test")
tool_run("git remote failed" "${GIT}" -C "${copy}" remote add origin
         "${WORK}/origin.git")
file(WRITE "${copy}/tools/version" "${version}\n")
file(READ "${copy}/CHANGELOG.md" changelog)
string(REPLACE "\n## " "\n## ${version} - 2026-09-20\n\nThe version of the test of the release script.\n\n## " changelog "${changelog}")
file(WRITE "${copy}/CHANGELOG.md" "${changelog}")
tool_run("git add failed" "${GIT}" -C "${copy}" add -A)
tool_run("the commit failed" "${GIT}" -C "${copy}" commit --quiet
         -m "Release ${version}")
tool_run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)

# DESIGN: the cache of the stand-in names a deps directory of this test,
# with the real LLVM tools of this machine behind a link, so that what the
# script fetches for the other hosts lands here and never in build/deps of
# the checkout.
get_filename_component(llvm_dir "${LLVM_BIN}" DIRECTORY)
file(MAKE_DIRECTORY "${WORK}/deps")
file(CREATE_LINK "${llvm_dir}" "${WORK}/deps/llvm" SYMBOLIC)
file(READ "${ROOT}/tools/llvm-version" llvm_version)
string(STRIP "${llvm_version}" llvm_version)
cmake_host_system_information(RESULT machine_cpu QUERY OS_PLATFORM)
set(machine "macos-${machine_cpu}")

set(saved_path "$ENV{PATH}")
# Step 9 rsyncs the text of the site to the webroot that ANTI_SITE names,
# and the preflight refuses a run without it. The stand-in for ssh
# answers for the host of this one.
set(ENV{ANTI_SITE} "site.example:/var/www/anti-lang.com/webroot")
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run
                WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
set(ENV{PATH} "${saved_path}")
file(WRITE "${WORK}/dry-run.log" "${out}${err}")
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "./r --dry-run failed:\n${out}${err}")
endif()
# A version that is no tag gives no warning.
if("${out}${err}" MATCHES "warning:")
    message(FATAL_ERROR "./r --dry-run warns on a version that is no tag\n"
                        "${out}${err}")
endif()

# A dry run writes under build/dist/dry-run, so that nothing it leaves
# behind stands in for a step of a release.
set(dist "${copy}/build/dist/dry-run")
