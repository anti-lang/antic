# Drive `anti build` over one case of what it delivers and caches. Run with
# cmake -P and these values:
#   ANTI     the anti executable
#   CASE     the case
#   RUNTIME  the runtime archive
#   FIXTURES the directory tests/
#   HOST     the target name of this host
#   OTHER    a target name that is not this host's
#   WORK     a directory this run writes into
#
# The cases, each a defect of docs/audit/anti-tool.md:
#   windows_import_library  dist/ of a Windows DLL holds the import library
#                           a C program links against (defect 4)
#   path_target             a path dependency whose manifest names another
#                           default target is built for the host, where
#                           its consumer reads it (defect 2)
#   path_target_cpu         and a build for a target of the other processor
#                           with --cpu builds it at the level of the host
#                           (T144)
#   redirect_to_http        a redirect of an https:// repository to plain
#                           http:// of another host is refused (defect 11)
#   redirect_followed       and one to https:// is followed
#   redirect_loop           and a server that redirects to itself is left
#                           after ten redirects
#   lock_dropped            a package of the lock file that the manifest no
#                           longer reaches leaves the graph, the lock file
#                           and the notice, and is not fetched (defect 9)
#   cache_strip_docs        the cached library file of a build without
#                           --strip-docs is written again with it (defect 1)
#   cache_version           a new version of the manifest reaches the
#                           cached library file (defect 1)
#   cache_license           and a new licence identifier (defect 1)
#   cache_license_text      and a new text of the licence file (defect 1)
#   cache_attribution       and a new attribution line (defect 1)
#   cache_dependency        and a new dependency (defect 1)
#   cache_import_checked    a module is checked again when a module it
#                           imports changed its interface (defect 10,
#                           decision 15 of "Decisions of 2026-10-11")
#   cache_import_layout     and its object follows the new layout of a
#                           struct of that module (defect 10)

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

foreach(value ANTI CASE RUNTIME FIXTURES HOST WORK)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(tools --runtime "${RUNTIME}")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()

# A project at directory with the manifest text after its [package]
# lines, and one module of path module whose text is source.
function(write_project directory name manifest module source)
    file(WRITE "${directory}/anti.toml"
         "[package]\nname = \"${name}\"\nversion = \"1.0.0\"\n${manifest}")
    string(REPLACE "." "/" file "${module}")
    file(WRITE "${directory}/src/${file}.anti" "${source}")
endfunction()

# Run anti in a directory. The cache of the repositories is the user's,
# so the run names its own. ENV names more of the environment.
function(anti directory out_status out_text)
    cmake_parse_arguments(PARSE_ARGV 3 run "" "" "ENV")
    anti_run(status out err "${directory}" ENV ${run_ENV}
             COMMAND ${run_UNPARSED_ARGUMENTS})
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

# Run anti in a directory, and stop unless it ends with 0.
function(anti_passes what directory)
    anti("${directory}" status text ${ARGN})
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${what} ended with ${status}\n${text}")
    endif()
    set(anti_text "${text}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "windows_import_library")
    file(COPY "${FIXTURES}/anti-build/shapes" DESTINATION "${WORK}")
    set(project "${WORK}/shapes")
    anti_passes("anti build --lib shared" "${project}" build --lib shared
                --target windows-x86_64 ${tools})
    set(dist "${project}/dist/windows-x86_64/dev")
    if(NOT EXISTS "${project}/build/windows-x86_64/dev/shapes.lib")
        message(FATAL_ERROR "the link wrote no import library shapes.lib")
    endif()
    file(GLOB found RELATIVE "${dist}" "${dist}/*")
    foreach(file shapes.dll shapes.h shapes.lib)
        if(NOT EXISTS "${dist}/${file}")
            message(FATAL_ERROR "dist/ of a Windows DLL holds no ${file}. It "
                                "holds: ${found}")
        endif()
    endforeach()
    return()
endif()

if(CASE STREQUAL "path_target")
    string(CONCAT dep_source
           "//! A library whose project builds for another target.\n\n"
           "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    string(CONCAT app_source
           "//! A program over the library of a path.\n\n"
           "import com.example.dep;\n\n"
           "fn main() -> int\n{\n\treturn dep.one();\n}\n")
    write_project("${WORK}/dep" "com.example.dep"
                  "\n[targets]\ndefault = [\"${OTHER}\"]\n"
                  "com.example.dep" "${dep_source}")
    write_project("${WORK}/app" "com.example.app"
                  "\n[dependencies]\n\"com.example.dep\" = { path = \"../dep\" }\n"
                  "com.example.app" "${app_source}")
    anti_passes("the build of the consumer" "${WORK}/app" build ${tools})
    if(NOT EXISTS "${WORK}/dep/dist/${HOST}/dev/com/example/dep.antl")
        message(FATAL_ERROR "the dependency was not built for the host")
    endif()
    program_expect("the program" COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 1 OUT "")
    return()
endif()

# DESIGN: the consumer is a library project, which links nothing, so the
# case needs no sysroot of the other target.
if(CASE STREQUAL "path_target_cpu")
    if(OTHER MATCHES "x86_64$")
        set(level v1)
    else()
        set(level armv8.0)
    endif()
    string(CONCAT dep_source
           "//! A library of a path.\n\n"
           "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    string(CONCAT app_source
           "//! A library over the library of a path.\n\n"
           "import com.example.dep;\n\n"
           "/// Two.\npub fn two() -> int\n{\n\treturn dep.one() + 1;\n}\n")
    write_project("${WORK}/dep" "com.example.dep" "" "com.example.dep"
                  "${dep_source}")
    write_project("${WORK}/app" "com.example.app"
                  "\n[dependencies]\n\"com.example.dep\" = { path = \"../dep\" }\n"
                  "com.example.app" "${app_source}")
    anti_passes("the build of the consumer for ${OTHER} at ${level}"
                "${WORK}/app" build --target ${OTHER} --cpu ${level} ${tools})
    foreach(file "dep/dist/${HOST}/dev/com/example/dep.antl"
                 "app/dist/${OTHER}/dev/com/example/app.antl")
        if(NOT EXISTS "${WORK}/${file}")
            message(FATAL_ERROR "the build wrote no ${file}")
        endif()
    endforeach()
    return()
endif()

# DESIGN: no test reaches a server, so the redirect cases put a curl of
# their own first on the path. It answers as the servers of the case
# would: https://repo.example redirects every file to the same path under
# the URL of FAKE_REDIRECT, and mirror.example serves the files under
# FAKE_SERVED, with no regard to the scheme. It follows a redirect when
# it is given -L, as curl does, and it writes each URL it is asked for
# into FAKE_LOG. A shell script stands for it, so Windows runs neither
# case.
set(fake_curl [=[#!/bin/sh
out=
format=
follow=no
url=
while [ $# -gt 0 ]; do
    case "$1" in
    -o) out=$2; shift 2 ;;
    -w) format=$2; shift 2 ;;
    --etag-compare|--etag-save) shift 2 ;;
    -*L*) follow=yes; shift ;;
    -*) shift ;;
    *) url=$1; shift ;;
    esac
done
while :; do
    echo "$url" >> "$FAKE_LOG"
    location=
    case "$url" in
    https://repo.example/*)
        code=302
        location="$FAKE_REDIRECT/${url#https://repo.example/}" ;;
    *://mirror.example/*)
        file="$FAKE_SERVED/${url#*://mirror.example/}"
        if [ -f "$file" ]; then
            code=200
            cp "$file" "$out"
        else
            code=404
        fi ;;
    *) code=404 ;;
    esac
    if [ "$code" = 302 ] && [ "$follow" = yes ]; then
        url=$location
        continue
    fi
    break
done
printf '%s' "$format" | sed -e "s|%{http_code}|$code|" -e "s|%{redirect_url}|$location|"
[ "$code" -lt 400 ] || exit 22
]=])

# The library project units, served from WORK/served as version 1.0.0 of
# its package, and a program over it whose repository is
# https://repo.example. Each run of anti puts the fake curl first on the
# path, with the target of its redirects.
function(redirect_project redirect)
    string(CONCAT units_source
           "//! A library served over HTTP.\n\n"
           "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    string(CONCAT app_source
           "//! A program over the library of a repository.\n\n"
           "import com.example.units;\n\n"
           "fn main() -> int\n{\n\treturn units.one();\n}\n")
    write_project("${WORK}/units" "com.example.units" "" "com.example.units"
                  "${units_source}")
    anti_passes("the build of the library" "${WORK}/units" build ${tools})
    set(served "${WORK}/served/com.example.units")
    file(MAKE_DIRECTORY "${served}/1.0.0")
    file(COPY_FILE "${WORK}/units/dist/${HOST}/dev/com/example/units.antl"
         "${served}/1.0.0/com.example.units.antl")
    file(SHA256 "${served}/1.0.0/com.example.units.antl" digest)
    file(WRITE "${served}/index.toml"
         "name = \"com.example.units\"\nrevision = 1\n\n[[version]]\n"
         "version = \"1.0.0\"\n"
         "modules = [{ path = \"com.example.units\", sha256 = \"${digest}\" }]\n")
    write_project("${WORK}/app" "com.example.app"
                  "\n[repositories]\nremote = \"https://repo.example\"\n\n[dependencies]\n\"com.example.units\" = { version = \"1.0.0\", repo = \"remote\" }\n"
                  "com.example.app" "${app_source}")
    file(WRITE "${WORK}/bin/curl" "${fake_curl}")
    file(CHMOD "${WORK}/bin/curl" PERMISSIONS OWNER_READ OWNER_WRITE
         OWNER_EXECUTE)
    set(redirect_env "PATH=${WORK}/bin:$ENV{PATH}"
        "FAKE_REDIRECT=${redirect}" "FAKE_SERVED=${WORK}/served"
        "FAKE_LOG=${WORK}/curl.log" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "redirect_to_http")
    redirect_project("http://mirror.example")
    anti("${WORK}/app" status text build ${tools} ENV ${redirect_env})
    # The resolver adds that the repository gave no version.
    string(CONCAT expected
           "anti: https://repo.example/com.example.units/index.toml: the server redirects to http://mirror.example/com.example.units/index.toml, which is no https:// URL and no http:// URL of 127.0.0.1 or localhost"
           "\n"
           "anti: https://repo.example has no version of com.example.units that satisfies every constraint on it"
           "\n")
    if(NOT status STREQUAL "1" OR NOT text STREQUAL expected)
        message(FATAL_ERROR "the build over a redirect to http:// ended with "
                            "${status} and printed\n${text}\nand the case "
                            "expects 1 and\n${expected}")
    endif()
    file(READ "${WORK}/curl.log" asked)
    if(asked MATCHES "mirror")
        message(FATAL_ERROR "the build asked the target of the redirect:\n"
                            "${asked}")
    endif()
    return()
endif()

if(CASE STREQUAL "redirect_loop")
    redirect_project("https://repo.example")
    anti("${WORK}/app" status text build ${tools} ENV ${redirect_env})
    string(CONCAT expected
           "anti: https://repo.example/com.example.units/index.toml: the server redirects more than 10 times"
           "\n"
           "anti: https://repo.example has no version of com.example.units that satisfies every constraint on it"
           "\n")
    if(NOT status STREQUAL "1" OR NOT text STREQUAL expected)
        message(FATAL_ERROR "the build over a loop of redirects ended with "
                            "${status} and printed\n${text}\nand the case "
                            "expects 1 and\n${expected}")
    endif()
    file(STRINGS "${WORK}/curl.log" asked)
    list(LENGTH asked count)
    if(NOT count EQUAL 11)
        message(FATAL_ERROR "the build asked ${count} times, and the case "
                            "expects the first request and ten redirects")
    endif()
    return()
endif()

if(CASE STREQUAL "redirect_followed")
    redirect_project("https://mirror.example")
    anti_passes("the build over a redirect to https://" "${WORK}/app" build
                ${tools} ENV ${redirect_env})
    program_expect("the program" COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 1 OUT "")
    file(READ "${WORK}/curl.log" asked)
    if(NOT asked MATCHES "https://mirror.example/com.example.units/1.0.0/")
        message(FATAL_ERROR "the build fetched no library file from the "
                            "target of the redirect:\n${asked}")
    endif()
    return()
endif()

# The file:// URL of a directory: file:///tmp/repo, and file:///C:/repo
# for a Windows path, whose drive follows the third slash.
function(file_url path out)
    if(path MATCHES "^/")
        set(${out} "file://${path}" PARENT_SCOPE)
    else()
        set(${out} "file:///${path}" PARENT_SCOPE)
    endif()
endfunction()

if(CASE STREQUAL "lock_dropped")
    # Three packages in a repository. The index of a says that a needs
    # c, so the lock file holds c through a alone.
    set(repo "${WORK}/repo")
    foreach(package a b c)
        string(CONCAT source
               "//! A package of a repository.\n\n"
               "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
        write_project("${WORK}/${package}" "com.example.${package}" ""
                      "com.example.${package}" "${source}")
        anti_passes("the build of ${package}" "${WORK}/${package}" build
                    ${tools})
        set(at "${repo}/com.example.${package}")
        file(MAKE_DIRECTORY "${at}/1.0.0")
        file(COPY_FILE
             "${WORK}/${package}/dist/${HOST}/dev/com/example/${package}.antl"
             "${at}/1.0.0/com.example.${package}.antl")
        file(SHA256 "${at}/1.0.0/com.example.${package}.antl" digest)
        set(needs "")
        if(package STREQUAL "a")
            set(needs "{ name = \"com.example.c\", version = \"1.0.0\" }")
        endif()
        file(WRITE "${at}/index.toml"
             "name = \"com.example.${package}\"\nrevision = 1\n\n"
             "[[version]]\nversion = \"1.0.0\"\n"
             "modules = [{ path = \"com.example.${package}\", sha256 = \"${digest}\" }]\n"
             "dependencies = [${needs}]\n")
    endforeach()
    file_url("${repo}" repo_url)
    set(head "\n[repositories]\nlocal = \"${repo_url}\"\n\n[dependencies]\n")
    set(on_a "\"com.example.a\" = { version = \"1.0.0\", repo = \"local\" }\n")
    set(on_b "\"com.example.b\" = { version = \"1.0.0\", repo = \"local\" }\n")
    string(CONCAT both
           "//! A program over a and b.\n\n"
           "import com.example.a;\nimport com.example.b;\n\n"
           "fn main() -> int\n{\n\treturn a.one() + b.one();\n}\n")
    write_project("${WORK}/app" "com.example.app" "${head}${on_a}${on_b}"
                  "com.example.app" "${both}")
    anti_passes("the build over a and b" "${WORK}/app" build ${tools})
    file(READ "${WORK}/app/anti.lock" lock)
    foreach(package a b c)
        if(NOT lock MATCHES "name = \"com.example.${package}\"")
            message(FATAL_ERROR "the first lock file names no "
                                "com.example.${package}:\n${lock}")
        endif()
    endforeach()

    # The manifest drops b, and b leaves the repository and the cache.
    string(CONCAT one
           "//! A program over a.\n\n"
           "import com.example.a;\n\n"
           "fn main() -> int\n{\n\treturn a.one();\n}\n")
    write_project("${WORK}/app" "com.example.app" "${head}${on_a}"
                  "com.example.app" "${one}")
    file(REMOVE_RECURSE "${repo}/com.example.b" "${WORK}/cache")
    anti_passes("the build over a alone" "${WORK}/app" build ${tools})
    file(READ "${WORK}/app/anti.lock" lock)
    if(lock MATCHES "com.example.b")
        message(FATAL_ERROR "the lock file keeps com.example.b, which the "
                            "manifest no longer names:\n${lock}")
    endif()
    foreach(package a c)
        if(NOT lock MATCHES "name = \"com.example.${package}\"")
            message(FATAL_ERROR "the lock file names no "
                                "com.example.${package}:\n${lock}")
        endif()
    endforeach()
    file(READ "${WORK}/app/dist/${HOST}/dev/NOTICE.txt" notice)
    if(notice MATCHES "com.example.b")
        message(FATAL_ERROR "the notice names com.example.b:\n${notice}")
    endif()
    program_expect("the program" COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 1 OUT "")

    # The lock file of a and c answers the manifest, so the next build
    # reads it and writes nothing about it.
    anti_passes("the build after" "${WORK}/app" build ${tools})
    if(NOT anti_text STREQUAL "")
        message(FATAL_ERROR "the build over the new lock file printed\n"
                            "${anti_text}")
    endif()
    return()
endif()

# The printable strings of the library file that the library project at
# directory delivered for the host.
function(library_strings directory out)
    set(library "${directory}/dist/${HOST}/dev/com/example/shapes.antl")
    if(NOT EXISTS "${library}")
        message(FATAL_ERROR "the build wrote no ${library}")
    endif()
    file(STRINGS "${library}" lines)
    string(JOIN "\n" text ${lines})
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(holds text needle what)
    string(FIND "${text}" "${needle}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${what} holds no `${needle}`:\n${text}")
    endif()
endfunction()

function(lacks text needle what)
    string(FIND "${text}" "${needle}" at)
    if(NOT at LESS 0)
        message(FATAL_ERROR "${what} holds `${needle}`:\n${text}")
    endif()
endfunction()

# The library project tests/anti-build/shapes, built once into the cache,
# then changed by replacing from with to in the file named, and built
# again. Each case reads the library file of the second build.
function(cached_twice file from to)
    file(COPY "${FIXTURES}/anti-build/shapes" DESTINATION "${WORK}")
    set(project "${WORK}/shapes")
    anti_passes("the first build" "${project}" build ${tools})
    file(READ "${project}/${file}" text)
    string(REPLACE "${from}" "${to}" text "${text}")
    file(WRITE "${project}/${file}" "${text}")
    anti_passes("the second build" "${project}" build ${ARGN} ${tools})
    library_strings("${project}" second)
    set(second "${second}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "cache_strip_docs")
    set(doc "The area of a rectangle of `w` by `h`.")
    cached_twice(anti.toml "" "" --strip-docs)
    lacks("${second}" "${doc}" "the library file of --strip-docs")
    return()
endif()

if(CASE STREQUAL "cache_version")
    cached_twice(anti.toml "version = \"2.0.1\"" "version = \"3.4.5\"")
    holds("${second}" "3.4.5" "the library file of the new version")
    lacks("${second}" "2.0.1" "the library file of the new version")
    return()
endif()

if(CASE STREQUAL "cache_license")
    cached_twice(anti.toml "BSD-3-Clause" "MIT")
    lacks("${second}" "BSD-3-Clause" "the library file of the new licence")
    return()
endif()

if(CASE STREQUAL "cache_license_text")
    cached_twice(LICENSE "Redistribution and use" "Copying and use")
    holds("${second}" "Copying and use" "the library file of the new text")
    return()
endif()

if(CASE STREQUAL "cache_attribution")
    cached_twice(anti.toml "The Shapes Authors" "The Area Authors")
    holds("${second}" "Copyright 2026 The Area Authors"
          "the library file of the new attribution")
    return()
endif()

if(CASE STREQUAL "cache_dependency")
    string(CONCAT dep_source
           "//! A library of a path.\n\n"
           "/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    write_project("${WORK}/dep" "com.example.dep" "" "com.example.dep"
                  "${dep_source}")
    cached_twice(anti.toml "attribution"
                 "[dependencies]\n\"com.example.dep\" = { path = \"../dep\" }\n\n[package]\nattribution")
    holds("${second}" "com.example.dep" "the library file of the new dependency")
    return()
endif()

# A program over a module a, which imports a module b of the same
# project. b is changed between two dev builds.
function(import_project b_source a_source)
    string(CONCAT app_source
           "//! A program over a.\n\n"
           "import com.example.a;\n\n"
           "fn main() -> int\n{\n\treturn a.value();\n}\n")
    write_project("${WORK}/app" "com.example.app" "" "com.example.app"
                  "${app_source}")
    file(WRITE "${WORK}/app/src/com/example/a.anti" "${a_source}")
    file(WRITE "${WORK}/app/src/com/example/b.anti" "${b_source}")
endfunction()

if(CASE STREQUAL "cache_import_checked")
    string(CONCAT a_source
           "//! The module that imports b.\n\n"
           "import com.example.b;\n\n"
           "/// Twice the number of b.\n"
           "pub fn value() -> int\n{\n\treturn b.number() * 2;\n}\n")
    string(CONCAT b_source
           "//! The module that a imports.\n\n"
           "/// A number.\npub fn number() -> int\n{\n\treturn 3;\n}\n")
    import_project("${b_source}" "${a_source}")
    anti_passes("the first build" "${WORK}/app" build ${tools})
    program_expect("the first program"
                   COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 6 OUT "")
    # number gives a bool now, which a may not multiply.
    string(REPLACE "-> int\n{\n\treturn 3;" "-> bool\n{\n\treturn true;"
           b_source "${b_source}")
    file(WRITE "${WORK}/app/src/com/example/b.anti" "${b_source}")
    anti("${WORK}/app" status text build ${tools})
    if(status STREQUAL "0" OR NOT text MATCHES "a\\.anti:8:22: error: ")
        message(FATAL_ERROR "the build after b changed its interface ended "
                            "with ${status} and printed\n${text}\nThe case "
                            "expects a.anti to be checked again and refused")
    endif()
    return()
endif()

if(CASE STREQUAL "cache_import_layout")
    string(CONCAT a_source
           "//! The module that imports b.\n\n"
           "import com.example.b;\n\n"
           "/// The size of the pair of b.\n"
           "pub fn value() -> int\n{\n\treturn size_of(b.Pair) as int;\n}\n")
    string(CONCAT b_source
           "//! The module that a imports.\n\n"
           "/// A pair.\npub struct Pair\n{\n\tleft: i64,\n\tright: i64,\n}\n")
    import_project("${b_source}" "${a_source}")
    anti_passes("the first build" "${WORK}/app" build ${tools})
    program_expect("the first program"
                   COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 16 OUT "")
    string(REPLACE "\tright: i64,\n" "\tright: i64,\n\tthird: i64,\n"
           b_source "${b_source}")
    file(WRITE "${WORK}/app/src/com/example/b.anti" "${b_source}")
    anti_passes("the build after b grew its pair" "${WORK}/app" build ${tools})
    program_expect("the program after b grew its pair"
                   COMMAND "${WORK}/app/dist/${HOST}/dev/app${exe}"
                   STATUS 24 OUT "")
    return()
endif()

message(FATAL_ERROR "no case ${CASE}")
