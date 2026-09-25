# The collection modules of the standard library. CMakeLists.txt builds every
# .anti file under src/std/anti/collection/ with no list of its own, and the
# test std_collection_modules checks the order this file gives.
#
# DESIGN: antic resolves an import from a library file alone, so a module
# compiles after every module it imports. The order comes from the lines
# `import anti.collection.<module>` of each file, and a file that imports
# a module after it waits for that one. Modules that import each other have
# no order, and the build stops and names them.

# The directory under src/std/anti/ that holds the collection modules, and
# the path segment of their library files under std/anti/.
set(ANTI_STD_COLLECTION_DIR collection)

# Sets out to the modules under dir, each as collection/<path> without
# .anti, ordered so that a module follows every module of the tree it
# imports.
function(anti_std_collection_modules dir out)
    file(GLOB_RECURSE files RELATIVE "${dir}" "${dir}/*.anti")
    list(SORT files)
    set(pending "")
    foreach(file IN LISTS files)
        string(REGEX REPLACE "\\.anti$" "" module "${file}")
        list(APPEND pending "${module}")
        file(STRINGS "${dir}/${file}" lines
            REGEX "^[ \t]*import[ \t]+anti\\.${ANTI_STD_COLLECTION_DIR}\\.[a-z]")
        set(needs "")
        foreach(line IN LISTS lines)
            string(REGEX MATCH
                "anti\\.${ANTI_STD_COLLECTION_DIR}\\.([a-z0-9_.]*[a-z0-9_])"
                found "${line}")
            string(REPLACE "." "/" path "${CMAKE_MATCH_1}")
            list(APPEND needs "${path}")
        endforeach()
        set("needs_${module}" "${needs}")
    endforeach()

    set(ordered "")
    list(LENGTH pending left)
    while(left GREATER 0)
        set(next "")
        foreach(module IN LISTS pending)
            set(ready 1)
            foreach(need IN LISTS "needs_${module}")
                list(FIND pending "${need}" waiting)
                if(NOT waiting LESS 0 AND NOT need STREQUAL module)
                    set(ready 0)
                endif()
            endforeach()
            if(ready)
                list(APPEND next "${module}")
            endif()
        endforeach()
        if(next STREQUAL "")
            list(JOIN pending ", " names)
            message(FATAL_ERROR "the collection modules ${names} import "
                "each other, and none of them can compile first")
        endif()
        foreach(module IN LISTS next)
            list(REMOVE_ITEM pending "${module}")
            list(APPEND ordered "${ANTI_STD_COLLECTION_DIR}/${module}")
        endforeach()
        list(LENGTH pending left)
    endwhile()
    set("${out}" "${ordered}" PARENT_SCOPE)
endfunction()
