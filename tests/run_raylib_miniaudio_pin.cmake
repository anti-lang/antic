# raylib takes its ma_ functions from the miniaudio library of the runtime
# tree, so the two pins name one miniaudio. tools/raylib-pin names the
# version that the raylib release bundles, and tools/miniaudio-pin names the
# release the library is built from. The header raudio.c reads, raylib's
# copy in src/external/, is the header of that release byte for byte.
#
#   cmake -DROOT=<repository> -DRAYLIB=<raylib release>
#         -DMINIAUDIO=<miniaudio release> -P tests/run_raylib_miniaudio_pin.cmake

function(read_pin out file key)
    file(STRINGS "${file}" line REGEX "^${key}=")
    string(REGEX REPLACE "^${key}=" "" value "${line}")
    if(NOT value MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "${key} of ${file} is `${value}`, expected a version")
    endif()
    set(${out} "${value}" PARENT_SCOPE)
endfunction()
read_pin(bundled "${ROOT}/tools/raylib-pin" RAYLIB_MINIAUDIO_VERSION)
read_pin(pinned "${ROOT}/tools/miniaudio-pin" MINIAUDIO_VERSION)
if(NOT bundled STREQUAL pinned)
    message(FATAL_ERROR "tools/raylib-pin names miniaudio ${bundled}, and "
                        "tools/miniaudio-pin names ${pinned}")
endif()

# The raylib release bundles the version its pin names.
set(copy "${RAYLIB}/src/external/miniaudio.h")
file(STRINGS "${copy}" parts
     REGEX "^#define MA_VERSION_(MAJOR|MINOR|REVISION) +[0-9]+$")
set(found "")
foreach(part IN LISTS parts)
    string(REGEX REPLACE "^.* ([0-9]+)$" "\\1" number "${part}")
    list(APPEND found "${number}")
endforeach()
list(JOIN found "." found)
if(NOT found STREQUAL bundled)
    message(FATAL_ERROR "${copy} is miniaudio ${found}, and "
                        "tools/raylib-pin names ${bundled}")
endif()

# The declarations raylib compiles against are those of the library.
set(header "${MINIAUDIO}/miniaudio.h")
file(SHA256 "${copy}" copy_digest)
file(SHA256 "${header}" header_digest)
if(NOT copy_digest STREQUAL header_digest)
    message(FATAL_ERROR "${copy} differs from ${header}")
endif()
