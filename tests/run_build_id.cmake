# Link one program twice and another once, and read the build id of each
# from its licence notice. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCE    the .anti file linked twice
#   OTHER     another .anti file
#   WORK      a directory for the executables
#
# The id is the line "build <64 digits>" after the begin marker. The two
# links of SOURCE give one id, since the digest is of the code, and OTHER
# gives another.

function(build_id out dir source)
    file(MAKE_DIRECTORY "${WORK}/${dir}")
    execute_process(
        COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
                -o "${WORK}/${dir}/program" "${source}"
        RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${source}\n${err}")
    endif()
    file(STRINGS "${WORK}/${dir}/program" lines REGEX "^build ")
    list(LENGTH lines count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${dir} holds ${count} build lines: ${lines}")
    endif()
    string(REGEX MATCH "^build ([0-9a-f]+)$" line "${lines}")
    string(LENGTH "${CMAKE_MATCH_1}" length)
    if(NOT length EQUAL 64)
        message(FATAL_ERROR "the build id of ${dir} is not 64 digits: ${lines}")
    endif()
    set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK}")
build_id(first first "${SOURCE}")
build_id(second second "${SOURCE}")
build_id(other other "${OTHER}")
if(NOT first STREQUAL second)
    message(FATAL_ERROR "two links of one program carry two ids: ${first} and "
                        "${second}")
endif()
if(first STREQUAL other)
    message(FATAL_ERROR "two programs carry one id: ${first}")
endif()
