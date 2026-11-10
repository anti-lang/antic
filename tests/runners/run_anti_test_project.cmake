# Drive `anti test` in a copy of tests/anti-test/project, a project whose
# manifest names no package, and check what it reports. By Q4 of
# "Answers of 2026-11-06, second set" in docs/decisions.md, `anti test` of
# a project finds the library files of its modules without -I. Run with
# cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive
#   PROJECT   tests/anti-test/project, and tests/inject/project for the
#             case injections
#   WORK      a directory this run writes into
#   CASE      one of the cases below
#   HOST      the target name of this host, for the case unreadable_runtime
#
# Cases:
#   all_dev, all_release  `anti test` without a file runs the tests of
#                         every module of src/ in dev and in release mode
#   file_dev, file_release  `anti test src/shapes/area.anti`
#   import_changed        a change of a module the one under test imports
#                         reaches the next run
#   earlier_run           a library file that an earlier run wrote of a
#                         module stands in no later run that leaves it out
#   include_roots         -I keeps the run of before: no library file of
#                         the project but those the roots hold
#   outside               without anti.toml the run takes files alone
#   dependency_dev, dependency_release  a module that imports a module of
#                         a path dependency, whose library file stands
#                         under no root of the run
#   injections            the manifest names providers in `[inject]` and
#                         `[inject.test]`, which the key of every file the
#                         run caches holds
#   links                 a module that links a framework of macOS and a
#                         library of Linux, which the key of its runner
#                         holds on every host
#   unreadable_runtime    an archive of the runtime archive that cannot be
#                         read, which the key of the runner reads. The run
#                         names it and fails the module. Not on Windows,
#                         which keeps no permission bits

cmake_minimum_required(VERSION 3.21)

set(here "${WORK}/project")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${PROJECT}/" DESTINATION "${here}")

function(run_tests out_status out_text)
    execute_process(
        COMMAND "${ANTI}" test ${ARGN} --runtime "${RUNTIME}"
        WORKING_DIRECTORY "${here}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(expect text needle what)
    string(FIND "${text}" "${needle}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${what}: no `${needle}` in\n${text}")
    endif()
endfunction()

function(passes status text what)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${text}")
    endif()
endfunction()

# The factor of the tuning, which the tests of shapes.geometry read.
function(set_factor n)
    file(READ "${here}/src/shapes/tuning.anti" source)
    string(REGEX REPLACE "FACTOR: int = [0-9]+" "FACTOR: int = ${n}"
           source "${source}")
    file(WRITE "${here}/src/shapes/tuning.anti" "${source}")
endfunction()

if(CASE STREQUAL "all_dev" OR CASE STREQUAL "all_release")
    set(extra "")
    if(CASE STREQUAL "all_release")
        set(extra --release)
    endif()
    run_tests(status text ${extra})
    passes("${status}" "${text}" "anti test without a file")
    expect("${text}" "ok shapes.geometry.scales_by_the_factor"
           "the first test of geometry")
    expect("${text}" "ok shapes.geometry.scales_zero"
           "the second test of geometry")
    expect("${text}" "ok shapes.area.pads_the_scaled_side" "the test of area")
    expect("${text}" "anti test: 3 tests in 2 modules, 0 modules failed"
           "the count of the modules with a tests block")
    # The library files stand where a build of `anti build` in the mode
    # of the run writes them, by Q3 of "Answers of 2026-11-06, third set",
    # and the run writes none of the other mode.
    set(mode dev)
    set(other release)
    if(CASE STREQUAL "all_release")
        set(mode release)
        set(other dev)
    endif()
    file(GLOB written "${here}/build/*/${mode}/lib/shapes/tuning.antl")
    if(NOT written)
        message(FATAL_ERROR "no build/<target>/${mode}/lib/shapes/tuning.antl\n"
                "${text}")
    endif()
    file(GLOB written "${here}/build/*/${other}")
    if(written)
        message(FATAL_ERROR "the run wrote ${written}\n${text}")
    endif()
elseif(CASE STREQUAL "file_dev" OR CASE STREQUAL "file_release")
    set(extra "")
    if(CASE STREQUAL "file_release")
        set(extra --release)
    endif()
    run_tests(status text ${extra} src/shapes/area.anti)
    passes("${status}" "${text}" "anti test of one file")
    expect("${text}" "ok shapes.area.pads_the_scaled_side" "the test of area")
    expect("${text}" "anti test: 1 tests in 1 modules, 0 modules failed"
           "the count of the run")
elseif(CASE STREQUAL "import_changed")
    run_tests(status text src/shapes/geometry.anti)
    passes("${status}" "${text}" "the first run")
    set_factor(3)
    run_tests(status text src/shapes/geometry.anti)
    expect("${text}" "FAIL shapes.geometry.scales_by_the_factor"
           "the run after the change of shapes.tuning")
elseif(CASE STREQUAL "earlier_run")
    # The first run tests shapes.tuning as well, so it writes the library
    # file of that module among the files of the run.
    run_tests(status text src/shapes/tuning.anti src/shapes/geometry.anti)
    passes("${status}" "${text}" "the run of both modules")
    set_factor(3)
    run_tests(status text src/shapes/geometry.anti)
    expect("${text}" "FAIL shapes.geometry.scales_by_the_factor"
           "the run that leaves shapes.tuning out")
elseif(CASE STREQUAL "include_roots")
    # With -I the run reads the roots alone, as before. src/ holds no
    # library file, so the import of shapes.tuning finds none.
    run_tests(status text -I src src/shapes/geometry.anti)
    if(status EQUAL 0)
        message(FATAL_ERROR "the run with -I src passed\n${text}")
    endif()
    expect("${text}" "shapes.tuning" "the import without a library file")
    file(GLOB written "${here}/build/*/dev/lib")
    if(written)
        message(FATAL_ERROR "the run with -I wrote ${written}\n${text}")
    endif()
    # The roots of the step before: the sources and the library files
    # that `anti build` wrote.
    execute_process(COMMAND "${ANTI}" build --runtime "${RUNTIME}"
        WORKING_DIRECTORY "${here}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    passes("${status}" "${out}${err}" "anti build")
    file(GLOB lib "${here}/build/*/dev/lib")
    run_tests(status text -I src -I "${lib}" src/shapes/geometry.anti)
    passes("${status}" "${text}" "the run with -I of both roots")
    expect("${text}" "anti test: 2 tests in 1 modules, 0 modules failed"
           "the count of the run with -I")
elseif(CASE STREQUAL "dependency_dev" OR CASE STREQUAL "dependency_release")
    set(extra "")
    if(CASE STREQUAL "dependency_release")
        set(extra --release)
    endif()
    file(WRITE "${WORK}/lengths/anti.toml"
         "[package]\nname = \"com.example.lengths\"\nversion = \"0.1.0\"\n")
    file(WRITE "${WORK}/lengths/src/com/example/lengths.anti"
         "//! Lengths.\n\n/// Twice n.\npub fn doubled(n: int) -> int\n{\n"
         "\treturn n * 2;\n}\n")
    file(WRITE "${here}/anti.toml"
         "[package]\nname = \"shapes.main\"\n\n[dependencies]\n"
         "\"com.example.lengths\" = { path = \"../lengths\" }\n")
    file(WRITE "${here}/src/shapes/measure.anti"
         "//! A measure over a path dependency.\n\n"
         "import com.example.lengths;\n\n"
         "/// n doubled by the dependency.\npub fn measured(n: int) -> int\n{\n"
         "\treturn lengths.doubled(n);\n}\n\ntests\n{\n"
         "\tfn doubles_through_the_dependency()\n\t{\n"
         "\t\tlet n = measured(4);\n\t\tassert(n == 8);\n\t}\n}\n")
    run_tests(status text ${extra} src/shapes/measure.anti)
    passes("${status}" "${text}" "anti test of a module of a dependency")
    expect("${text}" "ok shapes.measure.doubles_through_the_dependency"
           "the test through the dependency")
elseif(CASE STREQUAL "injections")
    run_tests(status text)
    passes("${status}" "${text}" "anti test of a project with injections")
    expect("${text}" "ok com.example.app.the_test_table_replaces_one_provider"
           "the test of the provider of the test table")
    expect("${text}" "anti test: 1 tests in 1 modules, 0 modules failed"
           "the count of the run")
elseif(CASE STREQUAL "links")
    file(WRITE "${here}/src/shapes/linked.anti"
         "//! Linked libraries of two systems.\n\n"
         "link framework \"CoreFoundation\";\nlink linux \"m\";\n\n"
         "/// Three.\npub fn three() -> int\n{\n\treturn 3;\n}\n\n"
         "tests\n{\n\tfn gives_three()\n\t{\n"
         "\t\tlet n = three();\n\t\tassert(n == 3);\n\t}\n}\n")
    run_tests(status text src/shapes/linked.anti)
    passes("${status}" "${text}" "anti test of a module that links")
    expect("${text}" "ok shapes.linked.gives_three" "the test of the module")
elseif(CASE STREQUAL "unreadable_runtime")
    # A stand-in runtime archive of links into RUNTIME, whose lib/HOST/
    # holds an archive more that nobody may read.
    set(runtime "${WORK}/runtime")
    file(MAKE_DIRECTORY "${runtime}/lib/${HOST}")
    file(GLOB parts RELATIVE "${RUNTIME}" "${RUNTIME}/*")
    foreach(part IN LISTS parts)
        if(NOT part STREQUAL "lib")
            file(CREATE_LINK "${RUNTIME}/${part}" "${runtime}/${part}"
                 SYMBOLIC)
        endif()
    endforeach()
    file(GLOB targets RELATIVE "${RUNTIME}/lib" "${RUNTIME}/lib/*")
    foreach(target IN LISTS targets)
        if(NOT target STREQUAL HOST)
            file(CREATE_LINK "${RUNTIME}/lib/${target}"
                 "${runtime}/lib/${target}" SYMBOLIC)
        endif()
    endforeach()
    file(GLOB entries RELATIVE "${RUNTIME}/lib/${HOST}"
         "${RUNTIME}/lib/${HOST}/*")
    foreach(entry IN LISTS entries)
        file(CREATE_LINK "${RUNTIME}/lib/${HOST}/${entry}"
             "${runtime}/lib/${HOST}/${entry}" SYMBOLIC)
    endforeach()
    set(unreadable "${runtime}/lib/${HOST}/unreadable.a")
    file(WRITE "${unreadable}" "")
    file(CHMOD "${unreadable}" PERMISSIONS OWNER_WRITE)
    set(RUNTIME "${runtime}")
    run_tests(status text src/shapes/area.anti)
    if(status EQUAL 0)
        message(FATAL_ERROR "the run over an unreadable archive passed\n"
                "${text}")
    endif()
    expect("${text}" "anti: cannot read ${unreadable}"
           "the archive that cannot be read")
    expect("${text}" "anti test: 1 tests in 1 modules, 1 module failed"
           "the count of the run")
elseif(CASE STREQUAL "outside")
    file(REMOVE "${here}/anti.toml")
    run_tests(status text)
    if(NOT status EQUAL 2)
        message(FATAL_ERROR "anti test outside a project gave ${status}\n"
                "${text}")
    endif()
    expect("${text}" "anti: test takes one or more .anti files"
           "the run without a file outside a project")
else()
    message(FATAL_ERROR "no case ${CASE}")
endif()
