# Run ./r --dry-run on a copy of the tree with the version bumped, and
# check that every step wrote its output. The copy is a clone with a bare
# origin, so the preflight sees a committed and pushed main. The commands
# that build, test, pack and reach the two VMs are stand-ins on the PATH,
# which write what the real ones write and compile nothing. A release is
# an hour of work on five machines, and what this checks is the script:
# the order of the steps, the output of each, the refusals of the
# preflight and the plan it prints for the steps a dry run does not run.
# The copy is the staged tree, so a change of the script reaches this
# test after git add and not before.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCC=<compiler>
#         -DLLVM_BIN=<dir> -P tests/run_release_dry_run.cmake

if(NOT APPLE)
    message("SKIP: a release is made on the development Mac")
    return()
endif()
if(NOT EXISTS "${ROOT}/.git")
    message("SKIP: an exported tree has no history to clone")
    return()
endif()
find_program(GIT git)
if(NOT GIT)
    message("SKIP: git is missing")
    return()
endif()

set(version "99.0.0")
set(hosts macos-arm64 macos-x86_64 linux-x86_64 linux-arm64
          windows-x86_64 windows-arm64)
string(REPLACE ";" " " host_words "${hosts}")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/bin" "${WORK}/fixture")

# Run <command> and fail with <message> unless it succeeds.
function(run message)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE failed
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT failed EQUAL 0)
        message(FATAL_ERROR "${message}\n${out}${err}")
    endif()
endfunction()

# The two programs a package carries. The one of macos-x86_64 is compiled
# for that processor, because the packer checks it under Rosetta, and the
# others are compiled for this machine. Each prints the version, which is
# what the release script reads.
execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE sdk
                OUTPUT_STRIP_TRAILING_WHITESPACE ENCODING NONE)
if(NOT sdk)
    message("SKIP: this machine names no macOS SDK")
    return()
endif()
foreach(program antic anti)
    file(WRITE "${WORK}/fixture/${program}.c"
         "#include <stdio.h>\n"
         "int main(void) { printf(\"${program} ${version}\\n\"); return 0; }\n")
    run("the fixture ${program} did not compile"
        "${CC}" -O0 -isysroot "${sdk}" -o "${WORK}/fixture/${program}"
        "${WORK}/fixture/${program}.c")
    execute_process(COMMAND "${CC}" -arch x86_64 -O0 -isysroot "${sdk}"
                            -o "${WORK}/fixture/${program}-x86_64"
                            "${WORK}/fixture/${program}.c"
                    RESULT_VARIABLE failed OUTPUT_QUIET ERROR_QUIET)
    if(NOT failed EQUAL 0)
        message("SKIP: this machine compiles nothing for x86_64")
        return()
    endif()
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
    execute_process(COMMAND "${CC}" "--target=${triple}" -c
                            -o "${WORK}/fixture/${host}/windows.obj"
                            "${WORK}/fixture/windows.c"
                    RESULT_VARIABLE failed OUTPUT_QUIET ERROR_QUIET)
    if(NOT failed EQUAL 0)
        message("SKIP: this machine compiles nothing for ${triple}")
        return()
    endif()
    foreach(program antic anti)
        execute_process(
            COMMAND "${LLVM_BIN}/lld-link" /NOLOGO /DEBUG "/PDBALTPATH:%_PDB%"
                    /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE /NODEFAULTLIB
                    "/MACHINE:${machine}"
                    "/OUT:${WORK}/fixture/${host}/${program}.exe"
                    "/PDB:${WORK}/fixture/${host}/${program}.pdb"
                    "${WORK}/fixture/${host}/windows.obj"
            RESULT_VARIABLE failed OUTPUT_QUIET ERROR_QUIET)
        if(NOT failed EQUAL 0)
            message("SKIP: lld-link links nothing for ${host}")
            return()
        endif()
    endforeach()
endforeach()

# The stand-in for cmake. It writes what each call of the release script
# writes: the cache the script reads its download paths from, the runtime
# of a build, the six packages of the packer and the PDB of every Windows
# program. A call that checks a package passes, because the tests
# linux_libc, cpu_archive_levels and pdb_guid cover those three scripts.
# What this test reads of step 4 is therefore the wiring: that the packer
# leaves a PDB per Windows program, that the step finds it and that it
# goes into the archive of its host.
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
for word in \"\$@\"; do
    case \$word in
    -S) previous=source ;;
    -B) previous=build ;;
    -P) previous=script ;;
    --preset) previous=preset ;;
    --build) previous=build ;;
    -DDEST=*) dest=\${word#-DDEST=} ;;
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
ANTIC_CLANG_DIR:PATH=${ROOT}/build/clang
ANTIC_LLVM_DIR:PATH=${LLVM_BIN}/..
ANTIC_RAYLIB_DIR:PATH=${ROOT}/build/raylib
ANTIC_SYSROOT_DIR:PATH=${ROOT}/build/sysroot
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
            mkdir -p \"\$tree/anti/bin\" \"\$tree/anti/tools\"
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
    *) exit 0 ;;
    esac
fi
if [ -n \"\$preset\" ]; then
    cache \"\$PWD/build-\$preset\"
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
# there prints, the version of a program it installed and the count of a
# suite.
file(WRITE "${WORK}/bin/ssh" "#!/bin/sh
echo 'the installer refuses a manifest without a signature'
echo 'antic ${version}'
echo 'anti ${version}'
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
run("the export of the tree failed" "${GIT}" -C "${ROOT}" checkout-index -a
    --prefix=${copy}/)
run("the bare origin failed" "${GIT}" init --quiet --bare -b main
    "${WORK}/origin.git")
run("git init failed" "${GIT}" init --quiet -b main "${copy}")
run("git config failed" "${GIT}" -C "${copy}" config user.email "r@example.com")
run("git config failed" "${GIT}" -C "${copy}" config user.name "Release Test")
run("git remote failed" "${GIT}" -C "${copy}" remote add origin
    "${WORK}/origin.git")
file(WRITE "${copy}/tools/version" "${version}\n")
file(READ "${copy}/CHANGELOG.md" changelog)
string(REPLACE "\n## " "\n## ${version} - 2026-09-20\n\nThe version of the test of the release script.\n\n## " changelog "${changelog}")
file(WRITE "${copy}/CHANGELOG.md" "${changelog}")
run("git add failed" "${GIT}" -C "${copy}" add -A)
run("the commit failed" "${GIT}" -C "${copy}" commit --quiet
    -m "Release ${version}")
run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)

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

# A dry run writes under build/dist/dry-run, so that nothing it leaves
# behind stands in for a step of a release.
set(dist "${copy}/build/dist/dry-run")
foreach(name state/02-suite state/03-packages state/04-symbols state/05-vms
        logs/mac.log logs/asan.log logs/ubsan.log logs/linux.log
        logs/windows.log packages/SHA256SUMS)
    if(NOT EXISTS "${dist}/${name}")
        message(FATAL_ERROR "the dry run wrote no ${name}\n${out}${err}")
    endif()
endforeach()
# DESIGN: the packages and the symbols archives stand in one directory
# under one manifest, which is the directory tools/publish.cmake uploads
# and step 6 signs. A file beside the twelve is a file a user downloads.
file(READ "${dist}/packages/SHA256SUMS" manifest)
foreach(host IN LISTS hosts)
    foreach(name "anti-${version}-${host}.tar.xz"
            "anti-${version}-${host}-symbols.zip")
        if(NOT EXISTS "${dist}/packages/${name}")
            message(FATAL_ERROR "the dry run wrote no packages/${name}\n${out}${err}")
        endif()
        if(NOT manifest MATCHES "  ${name}\n")
            message(FATAL_ERROR "SHA256SUMS names no ${name}\n${manifest}")
        endif()
    endforeach()
endforeach()
string(REGEX MATCHALL "\n" rows "${manifest}")
list(LENGTH rows count)
if(NOT count EQUAL 12)
    message(FATAL_ERROR "SHA256SUMS holds ${count} lines, and a release has "
                        "twelve files\n${manifest}")
endif()
file(GLOB beside RELATIVE "${dist}/packages" "${dist}/packages/*")
foreach(name IN LISTS beside)
    if(NOT name STREQUAL "SHA256SUMS" AND NOT manifest MATCHES "  ${name}\n")
        message(FATAL_ERROR "${name} stands beside the twelve files of the "
                            "release, and SHA256SUMS does not name it")
    endif()
endforeach()

# DESIGN: a Windows program has no symbol table, so its archive holds the
# map of its sections and the PDB that lld-link wrote beside it. Step 4
# reads the CodeView record of the executable against the GUID of that
# PDB before either goes in, and refuses a record that names a path.
foreach(host windows-x86_64 windows-arm64)
    set(archive "${dist}/packages/anti-${version}-${host}-symbols.zip")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
                    RESULT_VARIABLE listed OUTPUT_VARIABLE entries
                    ERROR_VARIABLE listed_err ENCODING NONE)
    if(NOT listed EQUAL 0)
        message(FATAL_ERROR "${archive} does not list\n${entries}${listed_err}")
    endif()
    string(APPEND entries "${listed_err}")
    foreach(name antic.exe.syms anti.exe.syms antic.pdb anti.pdb)
        if(NOT entries MATCHES "${name}")
            message(FATAL_ERROR "${archive} holds no ${name}\n${entries}")
        endif()
    endforeach()
endforeach()

# Nothing is signed, tagged or uploaded, and the run says what it would do.
foreach(name packages/SHA256SUMS.sig state/06-digests state/07-release)
    if(EXISTS "${dist}/${name}")
        message(FATAL_ERROR "the dry run wrote ${name}")
    endif()
endforeach()
foreach(line "would sign" "would tag v${version}" "would create the release"
        "would run the workflow" "would rsync tools/install.sh"
        "would rsync the downloads page" "keys/release.pem" "would install")
    if(NOT out MATCHES "${line}")
        message(FATAL_ERROR "the dry run does not say what it would do: "
                            "`${line}` is missing\n${out}${err}")
    endif()
endforeach()

# A version that is a tag is published, and a second run refuses it.
run("the tag failed" "${GIT}" -C "${copy}" tag "v${version}")
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "v${version}")
    message(FATAL_ERROR "./r packed a version that is already a tag\n${out}${err}")
endif()
run("the tag did not go" "${GIT}" -C "${copy}" tag -d "v${version}")

# An uncommitted change is refused, since a release names a commit that
# origin holds.
file(APPEND "${copy}/CHANGELOG.md" "\n")
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "uncommitted")
    message(FATAL_ERROR "./r packed a tree with an uncommitted change\n"
                        "${out}${err}")
endif()

# A version without an entry in the changelog is refused as well.
file(READ "${copy}/CHANGELOG.md" changelog)
string(REPLACE "## ${version} - 2026-09-20" "## 98.0.0 - 2026-09-20"
       changelog "${changelog}")
file(WRITE "${copy}/CHANGELOG.md" "${changelog}")
run("the commit failed" "${GIT}" -C "${copy}" commit --quiet -a -m "No entry")
run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "CHANGELOG.md")
    message(FATAL_ERROR "./r packed a version the changelog does not name\n"
                        "${out}${err}")
endif()

# tools/check-cpu.cmake reads the levels of a package and refuses one that
# lacks a level of tools/cpu-levels.
set(tree "${WORK}/package")
foreach(target IN LISTS hosts)
    set(levels v1 v2 v3)
    set(library libanti_rt.a)
    if(target MATCHES "arm64$")
        set(levels armv8.0 armv8.2 armv8.5)
    endif()
    if(target MATCHES "^windows-")
        set(library anti_rt.lib)
    endif()
    foreach(level IN LISTS levels)
        file(WRITE "${tree}/anti/lib/${target}/${level}/${library}" "")
    endforeach()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DTREE=${tree}"
                        "-DLEVELS=${ROOT}/tools/cpu-levels"
                        -P "${ROOT}/tools/check-cpu.cmake"
                RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "tools/check-cpu.cmake refused a whole package\n"
                        "${out}${err}")
endif()
file(REMOVE_RECURSE "${tree}/anti/lib/linux-arm64/armv8.2")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DTREE=${tree}"
                        "-DLEVELS=${ROOT}/tools/cpu-levels"
                        -P "${ROOT}/tools/check-cpu.cmake"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "armv8.2")
    message(FATAL_ERROR "tools/check-cpu.cmake took a package without the "
                        "armv8.2 runtime of linux-arm64\n${out}${err}")
endif()

file(REMOVE_RECURSE "${WORK}")
