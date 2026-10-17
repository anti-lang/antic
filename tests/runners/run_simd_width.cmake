# Write the assembly of a program for one target and level and count the
# lines that hold an instruction. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive, whose bin/ holds opt and llc
#   TARGET    the antic target name
#   LEVEL     the --cpu value
#   SOURCE    the .anti file
#   WORK      a directory for the assembly
#   PATTERN   a regular expression of the instruction after the tab that
#             starts its line
#   COUNT     how often it must stand in the assembly

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
set(assembly "${WORK}/${name}.${TARGET}.${LEVEL}.s")
file(MAKE_DIRECTORY "${WORK}")
antic_build(--runtime "${RUNTIME}" -S --target "${TARGET}" --cpu "${LEVEL}" -o
            "${assembly}" "${SOURCE}")

file(READ "${assembly}" text)
string(REGEX MATCHALL "\n\t${PATTERN}" found "${text}")
list(LENGTH found n)
if(NOT n EQUAL COUNT)
    message(FATAL_ERROR
        "${TARGET} at ${LEVEL} writes '${PATTERN}' ${n} times in ${assembly}, "
        "and not ${COUNT}")
endif()
