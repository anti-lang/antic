# The interface anti uses of antic, M02 of the audit. A file of src/anti/
# includes, of the headers of src/antic/, driver.h, antic.h and the
# shared helpers alone. driver.h and antic.h include nothing of the front
# end, the IR, the library reader or the back end, apart from the token
# list of lexer.h that antic.h hands to `anti fmt`. The shared helpers
# and notice.h include no header of a pass either, apart from that list. No file of src/anti/
# calls driver_interface, whose result is a structure of the checker.
# Run with cmake -P and ROOT, the root of the repository.

set(antic "${ROOT}/src/antic")
set(anti "${ROOT}/src/anti")
set(helpers
    antic.h applesdk.h arena.h attributes.h cpu.h modpath.h platform.h
    sha256.h target.h text.h userdirs.h)
set(failures "")

# The headers of src/antic/ that file includes, by their names.
function(antic_includes file out)
    set(names "")
    file(STRINGS "${file}" lines REGEX "^[ ]*#[ ]*include[ ]+\"")
    get_filename_component(dir "${file}" DIRECTORY)
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "^[ ]*#[ ]*include[ ]+\"([^\"]+)\".*$" "\\1"
               path "${line}")
        get_filename_component(name "${path}" NAME)
        if(path MATCHES "^\\.\\./antic/")
            list(APPEND names "${name}")
        elseif(path MATCHES "^\\.\\./")
            # A header of the runtime.
            continue()
        elseif("${dir}" STREQUAL "${antic}" AND EXISTS "${antic}/${path}")
            list(APPEND names "${name}")
        elseif(NOT EXISTS "${dir}/${path}" AND EXISTS "${antic}/${path}")
            list(APPEND names "${name}")
        endif()
    endforeach()
    set(${out} "${names}" PARENT_SCOPE)
endfunction()

# Every file in files may include, of src/antic/, the names in allowed.
function(check_includes files allowed)
    set(found "${failures}")
    foreach(file IN LISTS files)
        file(RELATIVE_PATH shown "${ROOT}" "${file}")
        if(NOT EXISTS "${file}")
            string(APPEND found "\n${shown} is missing")
            continue()
        endif()
        antic_includes("${file}" names)
        foreach(name IN LISTS names)
            if(NOT name IN_LIST allowed)
                string(APPEND found "\n${shown} includes ${name}")
            endif()
        endforeach()
    endforeach()
    set(failures "${found}" PARENT_SCOPE)
endfunction()

file(GLOB tool_files "${anti}/*.c" "${anti}/*.h")
check_includes("${tool_files}" "${helpers};driver.h")

check_includes("${antic}/driver.h" "${helpers}")
check_includes("${antic}/antic.h" "${helpers};lexer.h")

set(helper_files "")
foreach(name IN LISTS helpers ITEMS notice.h)
    get_filename_component(stem "${name}" NAME_WE)
    # antic.c is the side of antic behind the interface.
    if(stem STREQUAL "antic")
        continue()
    endif()
    foreach(file "${antic}/${stem}.h" "${antic}/${stem}.c")
        if(EXISTS "${file}")
            list(APPEND helper_files "${file}")
        endif()
    endforeach()
endforeach()
# modpath.c asks the lexer whether a segment is a keyword, and the token
# list is part of the interface.
check_includes("${helper_files}" "${helpers};alloc.h;notice.h;lexer.h")

foreach(file IN LISTS tool_files)
    file(STRINGS "${file}" calls REGEX "driver_interface")
    if(calls)
        file(RELATIVE_PATH shown "${ROOT}" "${file}")
        string(APPEND failures "\n${shown} calls driver_interface")
    endif()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "anti reaches past its interface to antic:${failures}")
endif()
