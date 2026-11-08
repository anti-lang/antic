# The dev object of anti.lang that the fuzzer keeps in its work directory
# belongs to the antic and the library file that wrote it. The test puts a
# file in its place, as an earlier antic left it, and asks the fuzzer for
# the object. Run with cmake -P and these values:
#   ROOT     the repository
#   ANTIC    the antic executable
#   RUNTIME  the runtime directory
#   WORK     a scratch directory
#   CASE     the case, one of:
#            without_key  the file stands without the key of the fuzzer,
#                         as one of an antic before the key does
#            other_key    the key beside the file names another antic
#            same_key     the key beside the file names this antic and
#                         this library file
#
# In the first two cases the fuzzer writes the object again, and in the
# third it keeps the file.

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tools/fuzz/common.cmake")

set(STALE "the dev object of anti.lang of an earlier antic\n")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(object "${WORK}/${FUZZ_LANG_OBJECT_NAME}.o")
set(key "${WORK}/${FUZZ_LANG_KEY_NAME}")

if(CASE STREQUAL "without_key")
    file(WRITE "${object}" "${STALE}")
    set(rewritten TRUE)
elseif(CASE STREQUAL "other_key")
    file(WRITE "${object}" "${STALE}")
    file(WRITE "${key}" "antic 0\nlibrary 0\n")
    set(rewritten TRUE)
elseif(CASE STREQUAL "same_key")
    fuzz_lang_object(first "${WORK}")
    file(WRITE "${object}" "${STALE}")
    set(rewritten FALSE)
else()
    message(FATAL_ERROR "unknown CASE ${CASE}")
endif()

fuzz_lang_object(got "${WORK}")
if(NOT got STREQUAL object)
    message(FATAL_ERROR "the fuzzer named ${got}, and the object is ${object}")
endif()
file(READ "${object}" text LIMIT 100)
if(rewritten AND text STREQUAL STALE)
    message(FATAL_ERROR "the fuzzer kept the file of an earlier antic, "
                        "${object}")
endif()
if(NOT rewritten AND NOT text STREQUAL STALE)
    message(FATAL_ERROR "the fuzzer wrote ${object} again, although its "
                        "key names this antic and this library file")
endif()
