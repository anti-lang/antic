# Build and time the benchmark programs of tests/bench and print a table.
# Each program is compiled in release mode and in dev mode, the release
# executable runs, and so does its C twin. The table holds the median of
# the runs of each step, in milliseconds. It judges nothing: the numbers
# are the result. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory, holding lib/<target>/ and std/
#   BENCH     the directory of the programs, tests/bench
#   C_DIR     the directory of the C executables, bench_<name> each
#   WORK      a directory for the objects and the executables
#   OPT       optional opt executable
#   LLC       optional llc executable
#   OPTIONS   optional options of antic, separated by commas, such as
#             --backend,native for an antic that still has that option
#   RUNS      optional number of runs of each executable, 5 by default
#   BUILDS    optional number of builds of each program, 3 by default
#   OUTPUT    optional file that receives the table as well
#
# Every Anti program prints one line, and so does its C twin. A program
# whose line differs from that of its twin stops the run, since the two
# then do different work and their times compare nothing.
#
# Dev mode compiles the program's module alone and links the dev objects
# of the modules of the standard library it imports. Those objects are
# built once before the timing starts, as `anti build` keeps them in its
# cache, so a dev compile time covers the program's own module and the
# link.

set(programs scalar_loop objects builder simd_loop map_work)
set(dev_modules lang mem text collection collection/map)

if(NOT DEFINED RUNS)
    set(RUNS 5)
endif()
if(NOT DEFINED BUILDS)
    set(BUILDS 3)
endif()
string(REPLACE "," ";" options "${OPTIONS}")
set(tools "")
if(DEFINED OPT AND NOT OPT STREQUAL "")
    list(APPEND tools --opt "${OPT}")
endif()
if(DEFINED LLC AND NOT LLC STREQUAL "")
    list(APPEND tools --llc "${LLC}")
endif()
set(exe_suffix "")
set(object_suffix ".o")
if(CMAKE_HOST_WIN32)
    set(exe_suffix ".exe")
    set(object_suffix ".obj")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/release" "${WORK}/dev" "${WORK}/std")

# Run a command, stop with its output when it fails, and set the variable
# after `into` to the time it took in microseconds.
function(timed into)
    string(TIMESTAMP start "%s%f" UTC)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    string(TIMESTAMP stop "%s%f" UTC)
    if(NOT status EQUAL 0)
        list(JOIN ARGN " " line)
        message(FATAL_ERROR "${line} failed with ${status}\n${out}${err}")
    endif()
    math(EXPR took "${stop} - ${start}")
    set(${into} "${took}" PARENT_SCOPE)
    set(${into}_output "${out}" PARENT_SCOPE)
endfunction()

# Set the variable after `into` to the median of the times after it, in
# milliseconds with one decimal.
function(median into)
    list(SORT ARGN COMPARE NATURAL)
    list(LENGTH ARGN count)
    math(EXPR middle "${count} / 2")
    list(GET ARGN ${middle} micro)
    math(EXPR whole "${micro} / 1000")
    math(EXPR tenth "(${micro} % 1000) / 100")
    set(${into} "${whole}.${tenth}" PARENT_SCOPE)
endfunction()

set(dev_objects "")
foreach(module IN LISTS dev_modules)
    string(REPLACE "/" "_" name "${module}")
    timed(unused "${ANTIC}" --dev ${options} ${tools} --runtime "${RUNTIME}"
        -I "${RUNTIME}/std" -o "${WORK}/std/anti_${name}"
        "${RUNTIME}/std/anti/${module}.antl")
    list(APPEND dev_objects "${WORK}/std/anti_${name}${object_suffix}")
endforeach()

set(table "| Program | C run | Anti run | Anti / C | Release compile | Dev compile |\n")
string(APPEND table "|---|---:|---:|---:|---:|---:|\n")
foreach(name IN LISTS programs)
    set(source "${BENCH}/${name}.anti")
    set(release_times "")
    set(dev_times "")
    foreach(build RANGE 1 ${BUILDS})
        timed(took "${ANTIC}" ${options} ${tools} --runtime "${RUNTIME}"
            -o "${WORK}/release/${name}" "${source}")
        list(APPEND release_times ${took})
        timed(took "${ANTIC}" --dev ${options} ${tools} --runtime "${RUNTIME}"
            -I "${RUNTIME}/std" -o "${WORK}/dev/${name}" "${source}"
            ${dev_objects})
        list(APPEND dev_times ${took})
    endforeach()
    set(anti_times "")
    set(c_times "")
    foreach(run RANGE 1 ${RUNS})
        timed(took "${WORK}/release/${name}${exe_suffix}")
        list(APPEND anti_times ${took})
        set(anti_line "${took_output}")
        timed(took "${C_DIR}/bench_${name}${exe_suffix}")
        list(APPEND c_times ${took})
        set(c_line "${took_output}")
    endforeach()
    if(NOT anti_line STREQUAL c_line)
        message(FATAL_ERROR
            "${name}: the Anti program prints ${anti_line}and its C twin ${c_line}")
    endif()
    median(release "${release_times}")
    median(dev "${dev_times}")
    median(anti "${anti_times}")
    median(c "${c_times}")
    list(SORT anti_times COMPARE NATURAL)
    list(SORT c_times COMPARE NATURAL)
    list(LENGTH anti_times count)
    math(EXPR middle "${count} / 2")
    list(GET anti_times ${middle} anti_micro)
    list(GET c_times ${middle} c_micro)
    math(EXPR ratio "(${anti_micro} * 100 + ${c_micro} / 2) / ${c_micro}")
    math(EXPR ratio_whole "${ratio} / 100")
    math(EXPR ratio_part "${ratio} % 100")
    if(ratio_part LESS 10)
        set(ratio_part "0${ratio_part}")
    endif()
    string(APPEND table "| ${name} | ${c} ms | ${anti} ms | "
        "${ratio_whole}.${ratio_part} | ${release} ms | ${dev} ms |\n")
endforeach()

message("${table}")
if(DEFINED OUTPUT AND NOT OUTPUT STREQUAL "")
    file(WRITE "${OUTPUT}" "${table}")
endif()
