# Lay out the runtime archive of the fault tests, which
# tools/faults.cmake describes. Every entry of FROM but lib/ becomes a
# link in TO, and so does every entry of FROM/lib/TARGET but its levels,
# and every file of the level LEVEL beside the library of the seam that
# the build wrote there. An entry that stands already stays.
#
#   cmake -DFROM=<runtime> -DTO=<runtime-faults> -DTARGET=<target>
#         -DLEVEL=<level> -P tools/faults-runtime.cmake

cmake_minimum_required(VERSION 3.21)

foreach(name FROM TO TARGET LEVEL)
    if(NOT DEFINED ${name})
        message(FATAL_ERROR "tools/faults-runtime.cmake takes -D${name}")
    endif()
endforeach()

# Make to a link to from, unless to stands.
function(link_entry from to)
    if(EXISTS "${to}" OR IS_SYMLINK "${to}")
        return()
    endif()
    get_filename_component(directory "${to}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    file(CREATE_LINK "${from}" "${to}" SYMBOLIC)
endfunction()

file(GLOB entries RELATIVE "${FROM}" "${FROM}/*")
foreach(entry IN LISTS entries)
    if(NOT entry STREQUAL "lib")
        link_entry("${FROM}/${entry}" "${TO}/${entry}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/cpu-levels.cmake")
antic_read_cpu_levels("${CMAKE_CURRENT_LIST_DIR}/cpu-levels")
file(GLOB entries RELATIVE "${FROM}/lib/${TARGET}" "${FROM}/lib/${TARGET}/*")
foreach(entry IN LISTS entries)
    if(NOT entry IN_LIST ANTIC_CPU_LEVELS_x86_64 AND
       NOT entry IN_LIST ANTIC_CPU_LEVELS_arm64)
        link_entry("${FROM}/lib/${TARGET}/${entry}"
                   "${TO}/lib/${TARGET}/${entry}")
    endif()
endforeach()

# The files of the level, the licence stub among them. The library of the
# seam stands there already and stays, and the bitcode is left out.
file(GLOB entries RELATIVE "${FROM}/lib/${TARGET}/${LEVEL}"
     "${FROM}/lib/${TARGET}/${LEVEL}/*")
foreach(entry IN LISTS entries)
    if(NOT IS_DIRECTORY "${FROM}/lib/${TARGET}/${LEVEL}/${entry}" AND
       NOT entry MATCHES "\\.d$")
        link_entry("${FROM}/lib/${TARGET}/${LEVEL}/${entry}"
                   "${TO}/lib/${TARGET}/${LEVEL}/${entry}")
    endif()
endforeach()
