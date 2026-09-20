# DESIGN: every runner that compares antic's output byte for byte runs
# antic in this directory and names every path under it relative to it.
# A dev-mode check carries the file it names into the data of the program
# and into a library file, so an absolute path would write the build
# machine into an expected file, and two runners that spell one path
# differently would compile one module into two programs. A path outside
# this directory, such as one antic writes, is left alone.
#
# ANTIC_TESTS is the directory to run antic in, and relative_paths(<list>)
# rewrites the paths of that list in place.

get_filename_component(ANTIC_TESTS "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

function(relative_paths name)
    string(LENGTH "${ANTIC_TESTS}/" skip)
    set(out "")
    foreach(argument IN LISTS ${name})
        string(FIND "${argument}" "${ANTIC_TESTS}/" at)
        if(at EQUAL 0)
            string(SUBSTRING "${argument}" ${skip} -1 argument)
        endif()
        list(APPEND out "${argument}")
    endforeach()
    set(${name} "${out}" PARENT_SCOPE)
endfunction()
