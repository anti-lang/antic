# The build compiles every .anti file under src/std/anti/collection/ by
# itself, so a new collection module needs no change to CMakeLists.txt.
#
#   cmake -DROOT=<repository> -DRUNTIME=<runtime dir> -DWORK=<dir>
#         -P tests/run_std_collection_modules.cmake
#
# The test orders a tree of its own through anti_std_collection_modules of
# tools/std-modules.cmake, refuses a cycle, finds a library file of the
# runtime archive for every module of the real tree, and reads that
# CMakeLists.txt names no file of the tree.

include("${ROOT}/tools/std-modules.cmake")

set(tree "${WORK}/${ANTI_STD_COLLECTION_DIR}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${tree}/sync")
file(WRITE "${tree}/list.anti"
    "import anti.collection;\nimport anti.collection.deque.{Deque};\n")
file(WRITE "${tree}/deque.anti" "import anti.collection.{Iterable};\n")
file(WRITE "${tree}/map.anti"
    "import anti.collection.set;\nimport anti.collection.list;\n")
file(WRITE "${tree}/set.anti" "import anti.text;\n")
file(WRITE "${tree}/sync/list.anti"
    "import anti.collection.map.{Map};\nimport anti.collection.list;\n")
anti_std_collection_modules("${tree}" modules)

list(LENGTH modules count)
if(NOT count EQUAL 5)
    message(FATAL_ERROR "5 modules expected, found ${count}: ${modules}")
endif()
foreach(pair "deque;list" "set;map" "list;map" "map;sync/list"
        "list;sync/list")
    list(GET pair 0 first)
    list(GET pair 1 then)
    list(FIND modules "${ANTI_STD_COLLECTION_DIR}/${first}" a)
    list(FIND modules "${ANTI_STD_COLLECTION_DIR}/${then}" b)
    if(a LESS 0 OR b LESS 0 OR NOT a LESS b)
        message(FATAL_ERROR "`${first}` must come before `${then}`: ${modules}")
    endif()
endforeach()

# Two modules that import each other have no order, and the build says so.
file(WRITE "${tree}/set.anti" "import anti.collection.sync.list;\n")
file(WRITE "${WORK}/cycle.cmake"
    "include(\"${ROOT}/tools/std-modules.cmake\")\n"
    "anti_std_collection_modules(\"${tree}\" modules)\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -P "${WORK}/cycle.cmake"
    RESULT_VARIABLE result ERROR_VARIABLE error ENCODING NONE)
if(result EQUAL 0 OR NOT error MATCHES "import each other")
    message(FATAL_ERROR "a cycle must stop the build, got: ${error}")
endif()

# Every module of the real tree reaches the runtime archive.
set(source "${ROOT}/src/std/anti/${ANTI_STD_COLLECTION_DIR}")
anti_std_collection_modules("${source}" real)
file(GLOB_RECURSE files "${source}/*.anti")
list(LENGTH files file_count)
list(LENGTH real real_count)
if(NOT file_count EQUAL real_count)
    message(FATAL_ERROR "${file_count} files, ${real_count} modules")
endif()
foreach(module IN LISTS real)
    if(NOT EXISTS "${RUNTIME}/std/anti/${module}.antl")
        message(FATAL_ERROR "no library file of `${module}`")
    endif()
endforeach()

file(READ "${ROOT}/CMakeLists.txt" lists)
if(lists MATCHES "${ANTI_STD_COLLECTION_DIR}/[a-z_]")
    message(FATAL_ERROR "CMakeLists.txt names a collection module")
endif()
