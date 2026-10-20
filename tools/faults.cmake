# Fault injection for the tests: copies of antic, of anti and of the
# runtime in which the seam of src/rt/fault.h can make a call of the
# system fail, so a test reaches the code behind a write the system
# refuses, a file that does not open or memory that runs out.
# CMakeLists.txt includes this file.
#
# DESIGN: the option chooses at configure time whether a tree builds the
# copies. The presets host, asan, ubsan and coverage of CMakePresets.json
# turn it on, so the suite of each runs the fault tests, and the preset
# release leaves it off. A Release build refuses it. The copies are
# programs and an archive of their own, antic_faults, anti_faults and
# runtime-faults/, and antic, anti and runtime/ of the same tree stay
# what a tree without the option builds. So no package, which the packer
# makes from antic, anti and runtime/, and no test that pins the bytes of
# a program, as link_identity_<target> does, meets the seam. The test
# faults_not_shipped reads the symbols of the three.
option(ANTIC_FAULTS
       "Build antic_faults, anti_faults and runtime-faults/ for the fault tests"
       OFF)
if(ANTIC_FAULTS AND CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "ANTIC_FAULTS builds the copies of the fault tests, "
                        "which no release holds. Configure a Release build "
                        "without it.")
endif()

# The runtime archive of the fault tests. lib/<host target>/<level>/ holds
# the runtime of the seam at the default level of the host's target, and
# every other entry links to the one of runtime/. A program of a fault
# test builds against it with --runtime and --lto none, since the
# directory holds no bitcode.
set(ANTIC_FAULTS_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime-faults")

# The file of the seam, which the copies alone compile.
set(ANTIC_FAULTS_SOURCE src/rt/fault.c)

# Copy the compile of target into copy, a static library of sources and
# the seam: its definitions, its include directories and its options. The
# libraries it links follow from the caller.
function(antic_faults_library copy target)
    add_library(${copy} STATIC ${ARGN} ${ANTIC_FAULTS_SOURCE})
    foreach(property COMPILE_DEFINITIONS INCLUDE_DIRECTORIES
            INTERFACE_INCLUDE_DIRECTORIES COMPILE_OPTIONS)
        get_target_property(values ${target} ${property})
        if(values)
            set_property(TARGET ${copy} PROPERTY ${property} "${values}")
        endif()
    endforeach()
    target_compile_definitions(${copy} PUBLIC ANTI_FAULTS=1)
endfunction()

# Copy the compile of the program target into copy, linked against lib.
function(antic_faults_program copy target lib)
    add_executable(${copy} ${ARGN})
    get_target_property(values ${target} COMPILE_DEFINITIONS)
    if(values)
        set_property(TARGET ${copy} PROPERTY COMPILE_DEFINITIONS "${values}")
    endif()
    target_link_libraries(${copy} PRIVATE ${lib})
endfunction()

# antic_faults and anti_faults, from the sources of antic and anti with
# the seam. CMakeLists.txt calls this after the last definition it gives
# antic_core, antic, anti_core and anti, which the copies take over.
function(antic_faults_tools)
    if(NOT ANTIC_FAULTS)
        return()
    endif()
    antic_faults_library(antic_core_faults antic_core ${ANTIC_CORE_SOURCES})
    target_include_directories(antic_core_faults SYSTEM PRIVATE
        "${ANTIC_PCRE2_INCLUDE}")
    target_link_libraries(antic_core_faults PUBLIC antic_pcre2
        Threads::Threads)
    antic_faults_program(antic_faults antic antic_core_faults
        ${ANTIC_MAIN_SOURCES})
    antic_faults_library(anti_core_faults anti_core ${ANTI_CORE_SOURCES})
    target_link_libraries(anti_core_faults PUBLIC antic_core_faults)
    antic_faults_program(anti_faults anti anti_core_faults
        ${ANTI_MAIN_SOURCES})
    antic_coverage(antic_core_faults antic_faults anti_core_faults
        anti_faults)
endfunction()

# The runtime of the seam for target, which antic_cross_runtime calls for
# every target with its triple and the options of its compile. A tree of
# the option builds it for the host's target alone, at its default level,
# as objects with the flags of runtime/ and ANTI_FAULTS. The target
# anti_rt_faults then lays out ANTIC_FAULTS_RUNTIME_DIR.
function(antic_faults_runtime target triple)
    if(NOT ANTIC_FAULTS OR NOT target STREQUAL ANTIC_HOST_TARGET)
        return()
    endif()
    set(level "${ANTIC_CPU_LEVEL_${target}}")
    set(work "${CMAKE_BINARY_DIR}/cross/${target}-${level}-faults")
    set(out "${ANTIC_FAULTS_RUNTIME_DIR}/lib/${target}/${level}")
    antic_cpu_march(march "${level}")
    antic_runtime_sections(sections "${target}")
    set(align "")
    if(target MATCHES "arm64")
        set(align -falign-functions=16)
    endif()
    antic_static_library(library_name anti_rt "${target}")
    set(sources ${ANTIC_RUNTIME_SOURCES} fault)
    if(NOT target MATCHES "^windows")
        list(REMOVE_ITEM sources ${ANTIC_RUNTIME_WINDOWS_OBJECTS})
    endif()
    set(objects "")
    foreach(source IN LISTS sources)
        set(object "${work}/${source}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND "${CMAKE_C_COMPILER}" --target=${triple} -std=c11 -O2
                ${ANTIC_C_WARNINGS} ${sections} ${align}
                "${march}" "-DANTI_CPU_LEVEL_ID=${ANTIC_CPU_ID_${level}}"
                -DANTI_FAULTS=1
                "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." ${ARGN}
                -MMD -MF "${object}.d"
                -c "${PROJECT_SOURCE_DIR}/src/rt/${source}.c" -o "${object}"
            DEPENDS "${PROJECT_SOURCE_DIR}/src/rt/${source}.c"
            DEPFILE "${object}.d"
            VERBATIM)
        list(APPEND objects "${object}")
    endforeach()
    set(library "${out}/${library_name}")
    add_custom_command(OUTPUT "${library}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${library}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" ${objects}
        DEPENDS ${objects}
        VERBATIM)
    # The links to runtime/ follow the build of every part of it, the
    # library of the runtime at each level and the native libraries among
    # them, so the script finds each entry it links to.
    add_custom_target(anti_rt_faults ALL
        COMMAND "${CMAKE_COMMAND}" "-DFROM=${ANTIC_RUNTIME_DIR}"
            "-DTO=${ANTIC_FAULTS_RUNTIME_DIR}" "-DTARGET=${target}"
            "-DLEVEL=${level}"
            -P "${PROJECT_SOURCE_DIR}/tools/faults-runtime.cmake"
        DEPENDS "${library}"
        VERBATIM)
endfunction()

# The order of the build: the links of runtime-faults/ come after every
# part of runtime/, the runtime of each level, the LLVM tools, the
# standard library and the native libraries of src/native. CMakeLists.txt
# calls this after the last directory that writes into runtime/.
function(antic_faults_finish)
    if(NOT ANTIC_FAULTS)
        return()
    endif()
    get_property(root DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY
                 BUILDSYSTEM_TARGETS)
    get_property(native DIRECTORY "${CMAKE_SOURCE_DIR}/src/native" PROPERTY
                 BUILDSYSTEM_TARGETS)
    foreach(name IN LISTS root)
        if(name MATCHES "^(anti_rt_${ANTIC_HOST_TARGET}_|llvm_tools$|anti_std$)")
            add_dependencies(anti_rt_faults ${name})
        endif()
    endforeach()
    foreach(name IN LISTS native)
        add_dependencies(anti_rt_faults ${name})
    endforeach()
endfunction()
