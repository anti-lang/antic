# Every native library of the runtime archive has its licence text in
# LICENSES/ of the repository, and the build copies the same bytes from the
# pinned release into licenses/ of the runtime tree, where the packer and
# anti license read it. A pin whose release changes the text fails here
# until LICENSES/ follows.
#
#   cmake -DROOT=<repository> -DRUNTIME=<runtime tree>
#         -DNAMES=<name>[,<name>] -P tests/run_native_licenses.cmake

string(REPLACE "," ";" names "${NAMES}")
if(names STREQUAL "")
    message(FATAL_ERROR "NAMES lists no native library")
endif()
foreach(name IN LISTS names)
    set(kept "${ROOT}/LICENSES/${name}.txt")
    set(built "${RUNTIME}/licenses/${name}.txt")
    foreach(file "${kept}" "${built}")
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "${file} is missing")
        endif()
    endforeach()
    file(SHA256 "${kept}" kept_digest)
    file(SHA256 "${built}" built_digest)
    if(NOT kept_digest STREQUAL built_digest)
        message(FATAL_ERROR "LICENSES/${name}.txt differs from the licence "
                            "of the pinned release in ${built}")
    endif()
endforeach()
