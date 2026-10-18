# One option of `anti`, beside the run without it. Run with cmake -P and
# these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   CASE      the case to run, one of the list below
#   RUNTIME   the runtime archive
#   LLVM_AR   the archiver
#   OBJDUMP   llvm-objdump of the pinned release
#   READOBJ   llvm-readobj of the pinned release
#   CLANG_DIR the pinned clang of the build, which the cases of bind take
#   FIXTURES  tests/, which holds anti-build/shapes and anti-symbols/tracer
#   HOST      the target name of this host
#   WORK      a directory this run writes into
#
# The cases:
#   strip_docs         `build --strip-docs` writes a library file without
#                      its doc text
#   soname_macos       `build --lib shared --soname` writes the install
#                      name and the two versions of a library of macOS,
#                      and without it the install name alone
#   soname_linux       it writes the soname of a library of Linux
#   soname_linux_dist  and dist/ holds the file that soname names, with
#                      the link beside it
#   soname_windows     it changes no header and no export of a library of
#                      Windows
#   target_all         `build --target all` builds for the six targets
#   target_all_cpu     and --cpu beside it is refused at the first target
#                      of the other architecture
#   check_root_library `check -I` finds a library file under the root
#   check_root_source  `check -I` gives a source under the root its
#                      module path
#   bind_define_name   `bind -D <name>` reaches clang
#   bind_define_value  `bind -D <name>=<value>` reaches clang
#   bind_define_shim   `bind -D` reaches the shim
#   bind_target        `bind --target` reads the header as clang reads it
#                      for Linux and for Windows
#   symbols_from       `symbols inventory --from` reads the archives of
#                      another directory
#   symbols_target     `symbols inventory` finds a program that `build
#                      --target` wrote for Linux and one for Windows
#   targets_default    `default` of `[targets]` in the manifest builds for
#                      the targets it names and not for the host
#   release_path       `build --release` builds a path dependency in
#                      release mode and reads its library there
#   library_root       a library project builds without the module whose
#                      path is the name of the package, with a warning
#
# DESIGN: each case runs anti twice, once without the option and once
# with it, on two copies of the same files, and names what differs. A case
# that read the run with the option alone would pass for an option that
# anti takes and ignores.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../pinned_clang.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

foreach(value ANTI CASE RUNTIME WORK)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Run anti in a directory. The cache of the repositories is the user's,
# so the run names its own.
function(anti directory out_status out_stdout out_stderr)
    anti_run(status out err "${directory}" ${ARGN})
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_stdout} "${out}" PARENT_SCOPE)
    set(${out_stderr} "${err}" PARENT_SCOPE)
endfunction()

# Run anti in a directory, and stop unless it ends with 0.
function(anti_passes what directory)
    anti("${directory}" status out err ${ARGN})
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${what} ended with ${status}\n${out}${err}")
    endif()
    set(anti_stdout "${out}" PARENT_SCOPE)
    set(anti_stderr "${err}" PARENT_SCOPE)
endfunction()

# Run a tool of the pinned release on a file and give what it prints.
function(tool_output out)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
                    OUTPUT_VARIABLE text ERROR_VARIABLE err ENCODING NONE)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${ARGN} ended with ${status}\n${err}")
    endif()
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

set(tools --runtime "${RUNTIME}" --llvm-ar "${LLVM_AR}")

# The library project of tests/anti-build/shapes twice: `plain` for the
# run without the option and `option` for the run with it.
function(two_projects)
    foreach(copy plain option)
        file(COPY "${FIXTURES}/anti-build/shapes" DESTINATION "${WORK}/${copy}")
    endforeach()
    set(plain "${WORK}/plain/shapes" PARENT_SCOPE)
    set(option "${WORK}/option/shapes" PARENT_SCOPE)
endfunction()

# The page that `anti doc --markdown` writes for the library file of the
# project at directory.
function(page_of directory out)
    set(library "${directory}/dist/${HOST}/dev/com/example/shapes.antl")
    if(NOT EXISTS "${library}")
        message(FATAL_ERROR "the build wrote no ${library}")
    endif()
    anti_passes("anti doc of ${library}" "${directory}" doc --markdown
                -o "${directory}/pages" --work "${directory}/doc-work"
                --runtime "${RUNTIME}" "${library}")
    file(READ "${directory}/pages/com.example.shapes.md" page)
    set(${out} "${page}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "strip_docs")
    two_projects()
    anti_passes("anti build" "${plain}" build ${tools})
    anti_passes("anti build --strip-docs" "${option}" build --strip-docs
                ${tools})
    page_of("${plain}" with_docs)
    page_of("${option}" stripped)
    set(module_doc "A package with a licence, an attribution line and one function.")
    set(item_doc "The area of a rectangle of `w` by `h`.")
    holds("${with_docs}" "${module_doc}" "the page of the build without the option")
    holds("${with_docs}" "${item_doc}" "the page of the build without the option")
    holds("${with_docs}" "area" "the page of the build without the option")
    lacks("${stripped}" "${module_doc}" "the page of --strip-docs")
    lacks("${stripped}" "${item_doc}" "the page of --strip-docs")
    holds("${stripped}" "area" "the page of --strip-docs")
    return()
endif()

# The shared library of the shapes project for a target, without --soname
# in `plain` and with it in `option`. The version of the package is 2.0.1.
function(two_libraries target)
    two_projects()
    anti_passes("anti build --lib shared" "${plain}" build --lib shared
                --target ${target} ${tools})
    anti_passes("anti build --lib shared --soname" "${option}" build
                --lib shared --soname --target ${target} ${tools})
    set(plain "${plain}/dist/${target}/dev" PARENT_SCOPE)
    set(option "${option}/dist/${target}/dev" PARENT_SCOPE)
endfunction()

# The name, the current version and the compatibility version that the
# load command LC_ID_DYLIB of a library of macOS holds.
function(dylib_id file out)
    tool_output(headers "${OBJDUMP}" --macho --private-headers "${file}")
    if(NOT headers MATCHES "cmd LC_ID_DYLIB\n[^\n]*\n +name ([^ \n]+)[^\n]*\n[^\n]*\n +current version ([0-9.]+)\n *compatibility version ([0-9.]+)\n")
        message(FATAL_ERROR "${file} holds no LC_ID_DYLIB:\n${headers}")
    endif()
    set(${out} "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3}"
        PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "soname_macos")
    two_libraries(${HOST})
    dylib_id("${plain}/libshapes.dylib" without)
    dylib_id("${option}/libshapes.dylib" with)
    # R151 of "Rules of 2026-10-13": without --soname the install name is
    # @rpath/<file name> as well, and the library carries no version.
    if(NOT without STREQUAL "@rpath/libshapes.dylib 0.0.0 0.0.0")
        message(FATAL_ERROR "the library without --soname has the install "
                            "name and the versions `${without}`, and the "
                            "test expects `@rpath/libshapes.dylib 0.0.0 "
                            "0.0.0`")
    endif()
    if(NOT with STREQUAL "@rpath/libshapes.dylib 2.0.1 2.0.0")
        message(FATAL_ERROR "the library of --soname has the install name, "
                            "the current version and the compatibility "
                            "version `${with}`, and the test expects "
                            "`@rpath/libshapes.dylib 2.0.1 2.0.0`")
    endif()
    return()
endif()

if(CASE STREQUAL "soname_linux")
    two_libraries(linux-x86_64)
    tool_output(without "${READOBJ}" --dynamic-table "${plain}/libshapes.so")
    tool_output(with "${READOBJ}" --dynamic-table "${option}/libshapes.so")
    lacks("${without}" "SONAME" "the library without --soname")
    holds("${with}" "Library soname: [libshapes.so.2]"
          "the library of --soname")
    return()
endif()

if(CASE STREQUAL "soname_linux_dist")
    two_libraries(linux-x86_64)
    if(EXISTS "${plain}/libshapes.so.2")
        message(FATAL_ERROR "the build without --soname wrote libshapes.so.2")
    endif()
    file(GLOB found RELATIVE "${option}" "${option}/libshapes*")
    if(NOT EXISTS "${option}/libshapes.so.2")
        message(FATAL_ERROR "dist/ of --soname holds no libshapes.so.2, the "
                            "file the soname names. It holds: ${found}")
    endif()
    if(NOT IS_SYMLINK "${option}/libshapes.so")
        message(FATAL_ERROR "libshapes.so in dist/ of --soname is no link "
                            "to libshapes.so.2")
    endif()
    file(READ_SYMLINK "${option}/libshapes.so" target)
    if(NOT target STREQUAL "libshapes.so.2")
        message(FATAL_ERROR "libshapes.so links to ${target}")
    endif()
    return()
endif()

# The headers and the exports of a library of Windows, as llvm-readobj
# prints them, without the name of the file and the time of the link.
# DESIGN: the link of a Windows library writes its time into the file, so
# two links of one input differ in those bytes and the case compares no
# whole file.
function(dll_facts file out)
    tool_output(text "${READOBJ}" --file-headers --coff-exports "${file}")
    string(REGEX REPLACE "(^|\n)File: [^\n]*" "\\1" text "${text}")
    string(REGEX REPLACE "\n *TimeDateStamp: [^\n]*" "" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "soname_windows")
    two_libraries(windows-x86_64)
    dll_facts("${plain}/shapes.dll" without)
    dll_facts("${option}/shapes.dll" with)
    holds("${with}" "MajorImageVersion: 0\n" "the library of --soname")
    holds("${with}" "MinorImageVersion: 0\n" "the library of --soname")
    holds("${with}" "Name: anti_licenses\n" "the library of --soname")
    if(NOT without STREQUAL with)
        message(FATAL_ERROR "--soname changed the headers or the exports of "
                            "a library of Windows, which has no version in "
                            "its file:\n${without}\nand:\n${with}")
    endif()
    file(SHA256 "${plain}/shapes.h" without)
    file(SHA256 "${option}/shapes.h" with)
    if(NOT without STREQUAL with)
        message(FATAL_ERROR "--soname changed shapes.h")
    endif()
    return()
endif()

# The directories of dist/ in the project at directory, sorted, with a
# space between them.
function(dist_of directory out)
    file(GLOB found RELATIVE "${directory}/dist" "${directory}/dist/*")
    list(SORT found)
    list(JOIN found " " line)
    set(${out} "${line}" PARENT_SCOPE)
endfunction()

set(six_targets "linux-arm64 linux-x86_64 macos-arm64 macos-x86_64 windows-arm64 windows-x86_64")

if(CASE STREQUAL "target_all")
    foreach(copy plain option)
        file(COPY "${FIXTURES}/anti-build/app" DESTINATION "${WORK}/${copy}")
    endforeach()
    anti_passes("anti build" "${WORK}/plain/app" build --runtime "${RUNTIME}")
    anti_passes("anti build --target all" "${WORK}/option/app" build
                --target all --runtime "${RUNTIME}")
    dist_of("${WORK}/plain/app" plain)
    dist_of("${WORK}/option/app" option)
    if(NOT plain STREQUAL HOST)
        message(FATAL_ERROR "anti build wrote dist/ for `${plain}`, and the "
                            "test expects ${HOST} alone")
    endif()
    if(NOT option STREQUAL six_targets)
        message(FATAL_ERROR "anti build --target all wrote dist/ for "
                            "`${option}`, and the test expects the six "
                            "targets")
    endif()
    return()
endif()

if(CASE STREQUAL "target_all_cpu")
    # v1 is a level of x86_64. The targets build in the order of
    # `antic --print-targets`, whose second is the first of ARM64.
    file(COPY "${FIXTURES}/anti-build/app" DESTINATION "${WORK}")
    anti("${WORK}/app" status out err build --target all --cpu v1
         --runtime "${RUNTIME}")
    if(NOT status STREQUAL "1" OR NOT out STREQUAL "" OR NOT err STREQUAL
       "anti: v1 is no processor level of linux-arm64\n")
        message(FATAL_ERROR "anti build --target all --cpu v1 ended with "
                            "${status} and wrote:\n${out}${err}")
    endif()
    return()
endif()

# A module of a library and a program that imports it, in a project
# without a manifest.
function(shapes_and_program)
    set(project "${WORK}/project")
    file(MAKE_DIRECTORY "${project}/roots/com/example" "${project}/app")
    # anti check compares the bytes of each file with what anti fmt writes.
    write_bytes("${project}/roots/com/example/shapes.anti"
         "//! The shapes.\n\n/// The area of a rectangle of `w` by `h`.\npub fn area(w: int, h: int) -> int\n{\n\treturn w * h;\n}\n")
    write_bytes("${project}/app/main.anti"
         "//! A program that reads a module of another directory.\nimport com.example.shapes;\n\nfn main() -> int\n{\n\treturn shapes.area(2, 3);\n}\n")
    set(project "${project}" PARENT_SCOPE)
endfunction()

# `anti check` of the sources without the root fails for the import, and
# the same sources with the root pass.
function(check_with_root root files_line)
    anti("${project}" status out err check --work "${WORK}/check-plain"
         --runtime "${RUNTIME}" ${ARGN})
    if(NOT status STREQUAL "1")
        message(FATAL_ERROR "anti check without -I ended with ${status}, "
                            "and the test expects 1\n${out}${err}")
    endif()
    holds("${out}${err}"
          "app/main.anti:2:8: error: cannot find module `com.example.shapes`"
          "the check without -I")
    anti("${project}" status out err check -I "${root}"
         --work "${WORK}/check-option" --runtime "${RUNTIME}" ${ARGN})
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "anti check -I ${root} ended with ${status}\n"
                            "${out}${err}")
    endif()
    holds("${out}${err}" "${files_line}" "the check with -I")
endfunction()

if(CASE STREQUAL "check_root_library")
    shapes_and_program()
    file(MAKE_DIRECTORY "${project}/libs/com/example")
    antic_build(-c -I "${project}/roots" --runtime "${RUNTIME}" -o
                "${project}/libs/com/example/shapes.antl"
                "${project}/roots/com/example/shapes.anti")
    check_with_root(libs "front end: 1 file, 1 target, 0 warnings"
                    app/main.anti)
    return()
endif()

if(CASE STREQUAL "check_root_source")
    shapes_and_program()
    check_with_root(roots "front end: 2 files, 1 target, 0 warnings"
                    roots/com/example/shapes.anti app/main.anti)
    return()
endif()

# `anti bind --clang` of a header without the option, into `plain`. Each
# case then binds the same header with the option, into `option`.
function(bind_plain header)
    file(WRITE "${WORK}/api.h" "${header}")
    anti_passes("anti bind" "${WORK}" bind --clang api.h -o plain
                --runtime "${RUNTIME}")
endfunction()

if(CASE STREQUAL "bind_define_name")
    bind_plain("#ifdef WITH_EXTRA\nint extra(int a);\n#endif\nint base(int a);\n")
    anti_passes("anti bind -D" "${WORK}" bind --clang api.h -o option
                --runtime "${RUNTIME}" -D WITH_EXTRA)
    file(READ "${WORK}/plain/api.anti" plain)
    file(READ "${WORK}/option/api.anti" option)
    set(extra "pub extern fn extra(a: c_int) -> c_int;")
    lacks("${plain}" "${extra}" "the binding without -D")
    holds("${plain}" "pub extern fn base(a: c_int) -> c_int;"
          "the binding without -D")
    holds("${option}" "${extra}" "the binding of -D WITH_EXTRA")
    return()
endif()

if(CASE STREQUAL "bind_define_value")
    bind_plain("#ifndef LIMIT\n#define LIMIT 3\n#endif\nenum { BOUND = LIMIT };\nint base(int a);\n")
    anti_passes("anti bind -D" "${WORK}" bind --clang api.h -o option
                --runtime "${RUNTIME}" -D LIMIT=7)
    file(READ "${WORK}/plain/api.anti" plain)
    file(READ "${WORK}/option/api.anti" option)
    holds("${plain}" "pub const BOUND: c_int = 3;" "the binding without -D")
    holds("${option}" "pub const BOUND: c_int = 7;"
          "the binding of -D LIMIT=7")
    return()
endif()

if(CASE STREQUAL "bind_target")
    # clang reads the header for the target, whose macros decide a
    # constant here. Both runs name a target, since the host is one of
    # the two on some machine.
    file(WRITE "${WORK}/api.h"
         "#ifdef _WIN32\nenum { SYSTEM = 1 };\n#else\nenum { SYSTEM = 2 };\n#endif\nint base(int a);\n")
    foreach(pair "linux|linux-x86_64|2" "windows|windows-x86_64|1"
            "windows_arm|windows-arm64|1")
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 out)
        list(GET parts 1 target)
        list(GET parts 2 value)
        anti_passes("anti bind --target ${target}" "${WORK}" bind --clang
                    api.h -o ${out} --runtime "${RUNTIME}" --target ${target})
        file(READ "${WORK}/${out}/api.anti" bound)
        holds("${bound}" "pub const SYSTEM: c_int = ${value};"
              "the binding of --target ${target}")
    endforeach()
    return()
endif()

if(CASE STREQUAL "bind_define_shim")
    bind_plain("#ifndef LIMIT\n#define LIMIT 3\n#endif\nstatic inline int scaled(int a) { return a * LIMIT; }\n")
    anti_passes("anti bind -D" "${WORK}" bind --clang api.h -o option
                --runtime "${RUNTIME}" -D LIMIT=7)
    file(READ "${WORK}/plain/shim_api.c" plain)
    file(READ "${WORK}/option/shim_api.c" option)
    lacks("${plain}" "#define LIMIT" "the shim without -D")
    holds("${option}" "#define LIMIT 7\n" "the shim of -D LIMIT=7")
    return()
endif()

if(CASE STREQUAL "symbols_from")
    file(COPY "${FIXTURES}/anti-symbols/tracer" DESTINATION "${WORK}")
    anti_passes("anti build --release" "${WORK}/tracer" build --release
                --runtime "${RUNTIME}")
    set(dist "${WORK}/tracer/dist/${HOST}/release")
    set(deploy "${WORK}/deploy")
    file(GLOB program "${dist}/tracer" "${dist}/tracer.exe")
    file(COPY ${program} DESTINATION "${deploy}")
    file(COPY "${dist}/tracer-symbols.zip" DESTINATION "${WORK}/archives")
    file(WRITE "${deploy}/app.toml" "[runtime]\nthreads = 2\n")

    # The archive stands in no directory the inventory reads by itself.
    anti_passes("anti symbols inventory" "${deploy}" symbols inventory
                --conf app.toml --out plain.zip)
    if(NOT anti_stdout MATCHES "^missing [^\n]*tracer[^\n]* [0-9a-f]+\nwrote plain\\.zip with 0 of 1 modules\n$")
        message(FATAL_ERROR "the inventory without --from printed:\n"
                            "${anti_stdout}")
    endif()
    anti_passes("anti symbols inventory --from" "${deploy}" symbols
                inventory --conf app.toml --from ../archives
                --out option.zip)
    if(NOT anti_stdout MATCHES "^present [^\n]*tracer[^\n]* [0-9a-f]+\nwrote option\\.zip with 1 of 1 modules\n$")
        message(FATAL_ERROR "the inventory with --from printed:\n"
                            "${anti_stdout}")
    endif()
    return()
endif()

if(CASE STREQUAL "symbols_target")
    # The same project built for two targets, each program beside its
    # archive in a directory of its own. The inventory reads an ELF file
    # and a PE file on every host, and the two builds have two ids.
    file(COPY "${FIXTURES}/anti-symbols/tracer" DESTINATION "${WORK}")
    foreach(target linux-arm64 windows-x86_64)
        anti_passes("anti build --release --target ${target}"
                    "${WORK}/tracer" build --release --target ${target}
                    --runtime "${RUNTIME}")
        set(dist "${WORK}/tracer/dist/${target}/release")
        set(deploy "${WORK}/deploy-${target}")
        file(GLOB built "${dist}/tracer" "${dist}/tracer.exe"
             "${dist}/tracer-symbols.zip")
        file(COPY ${built} DESTINATION "${deploy}")
        file(WRITE "${deploy}/app.toml" "[runtime]\nthreads = 2\n")
        anti_passes("anti symbols inventory for ${target}" "${deploy}"
                    symbols inventory --conf app.toml --out out.zip)
        if(NOT anti_stdout MATCHES "^present \\./tracer(\\.exe)? ([0-9a-f]+)\nwrote out\\.zip with 1 of 1 modules\n$")
            message(FATAL_ERROR "the inventory of the program of ${target} "
                                "printed:\n${anti_stdout}")
        endif()
        set(id_${target} "${CMAKE_MATCH_2}")
    endforeach()
    if(id_linux-arm64 STREQUAL id_windows-x86_64)
        message(FATAL_ERROR "the programs of the two targets carry one "
                            "build id, ${id_linux-arm64}")
    endif()
    return()
endif()

# The directories of one level of a directory, sorted, with a space
# between them.
function(directories_of directory out)
    file(GLOB found RELATIVE "${directory}" "${directory}/*")
    list(SORT found)
    list(JOIN found " " line)
    set(${out} "${line}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "targets_default")
    foreach(copy plain option)
        file(COPY "${FIXTURES}/anti-build/app" DESTINATION "${WORK}/${copy}")
    endforeach()
    file(APPEND "${WORK}/option/app/anti.toml"
         "\n[targets]\ndefault = [\"linux-arm64\", \"windows-x86_64\"]\n")
    foreach(copy plain option)
        anti_passes("anti build of ${copy}" "${WORK}/${copy}/app" build
                    --runtime "${RUNTIME}")
        dist_of("${WORK}/${copy}/app" ${copy})
    endforeach()
    if(NOT plain STREQUAL HOST)
        message(FATAL_ERROR "anti build wrote dist/ for `${plain}`, and the "
                            "test expects ${HOST} alone")
    endif()
    if(NOT option STREQUAL "linux-arm64 windows-x86_64")
        message(FATAL_ERROR "anti build with [targets] wrote dist/ for "
                            "`${option}`, and the test expects the two "
                            "targets of the manifest")
    endif()
    return()
endif()

if(CASE STREQUAL "release_path")
    # A program whose one dependency is the project beside it.
    foreach(copy plain option)
        set(root "${WORK}/${copy}")
        file(COPY "${FIXTURES}/anti-build/shapes" DESTINATION "${root}")
        file(WRITE "${root}/app/anti.toml"
             "[package]\nname = \"com.example.app\"\nversion = \"0.1.0\"\n\n"
             "[dependencies]\n"
             "\"com.example.shapes\" = { path = \"../shapes\" }\n")
        file(WRITE "${root}/app/src/com/example/app.anti"
             "//! A program that imports the library beside it.\n\n"
             "import com.example.shapes;\n\n"
             "fn main() -> int\n{\n\treturn 0;\n}\n")
    endforeach()
    anti_passes("anti build" "${WORK}/plain/app" build --runtime "${RUNTIME}")
    anti_passes("anti build --release" "${WORK}/option/app" build --release
                --runtime "${RUNTIME}")
    foreach(copy plain option)
        directories_of("${WORK}/${copy}/shapes/dist/${HOST}" ${copy})
    endforeach()
    if(NOT plain STREQUAL "dev")
        message(FATAL_ERROR "anti build wrote `${plain}` in dist/${HOST} of "
                            "the dependency, and the test expects dev alone")
    endif()
    if(NOT option STREQUAL "release")
        message(FATAL_ERROR "anti build --release wrote `${option}` in "
                            "dist/${HOST} of the dependency, and the test "
                            "expects release alone")
    endif()
    return()
endif()

if(CASE STREQUAL "library_root")
    # Two modules, the second of which imports the first. The project
    # `plain` holds the module of the package name beside them.
    foreach(copy plain option)
        set(root "${WORK}/${copy}")
        file(WRITE "${root}/anti.toml"
             "[package]\nname = \"com.example.app\"\nversion = \"0.1.0\"\n")
        file(WRITE "${root}/src/com/example/first.anti"
             "//! The first module.\n\n/// One.\n"
             "export fn first_one() -> c_int\n{\n\treturn 1;\n}\n")
        file(WRITE "${root}/src/com/example/second.anti"
             "//! The second module.\n\nimport com.example.first;\n\n"
             "/// Two.\nexport fn second_two() -> c_int\n{\n"
             "\treturn first.first_one() + 1;\n}\n")
    endforeach()
    file(WRITE "${WORK}/plain/src/com/example/app.anti"
         "//! The module of the package name.\n\n/// Zero.\n"
         "export fn app_zero() -> c_int\n{\n\treturn 0;\n}\n")
    foreach(copy plain option)
        anti_passes("anti build --lib static of ${copy}" "${WORK}/${copy}"
                    build --lib static ${tools})
        set(dist "${WORK}/${copy}/dist/${HOST}/dev")
        directories_of("${dist}" files)
        if(NOT files MATCHES "^app\\.h (libapp\\.a|app\\.lib)$")
            message(FATAL_ERROR "the library of ${copy} is `${files}`, and "
                                "the test expects app.h and the archive")
        endif()
        file(READ "${dist}/app.h" header_${copy})
        set(err_${copy} "${anti_stderr}")
        holds("${header_${copy}}" "first_one(void)" "the header of ${copy}")
        holds("${header_${copy}}" "second_two(void)" "the header of ${copy}")
    endforeach()
    holds("${header_plain}" "app_zero(void)" "the header of plain")
    lacks("${header_option}" "app_zero(void)" "the header of option")
    # R168 of "Rules of 2026-10-13": the project without the module warns.
    lacks("${err_plain}" "warning" "the standard error of plain")
    holds("${err_option}"
          "anti: ./anti.toml: warning: the library project com.example.app has no module com.example.app"
          "the standard error of option")
    return()
endif()

message(FATAL_ERROR "`${CASE}` is no case of this runner")
