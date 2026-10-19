# Runs tests/runners/run_native_pin.cmake over a repository of one made-up
# library, so the check is shown to refuse what it must and to pass what
# it must. A text that differs from the version in a dot alone passes,
# and the version or the digest spelled outside the pin fails.
#
#   cmake -DPIN=<run_native_pin.cmake> -DWORK=<dir>
#         -P tests/runners/run_native_pin_self.cmake

cmake_minimum_required(VERSION 3.21)

set(version "10.48")
set(digest "ebcc25aadf2a51fa1fefa9b8bc9e7a79b3dae86870a0f1152a22e42befd46888")
set(root "${WORK}/root")

# The script of antic_get_source of the made-up repository, in the form
# of src/native/get-source.cmake, which gives antic_download the digest of
# the pin.
string(CONCAT script
    "function(antic_get_source out name dest algorithm marker)\n"
    "    antic_download(\"\${url}\" \"\${archive}\" "
    "\"\${algorithm}\" \"\${\${key}_DIGEST}\")\n"
    "endfunction()\n")
set(call "antic_get_source(DEMO_SOURCE demo \"\${DEMO_DIR}\" SHA256\n    demo.c)\n")

# Write the made-up repository with <recipe> as the text of its recipe,
# and the call that fetches the library in the file that calls it.
function(write_root recipe)
    file(REMOVE_RECURSE "${root}")
    file(WRITE "${root}/tools/demo-pin"
         "DEMO_VERSION=${version}\nDEMO_DIGEST=${digest}\n"
         "DEMO_URL=https://example.org/demo-@VERSION@.tar.gz\n")
    file(WRITE "${root}/src/native/get-source.cmake" "${script}")
    file(WRITE "${root}/src/native/demo-source.cmake" "${call}")
    file(WRITE "${root}/src/native/demo.cmake" "${recipe}")
    file(WRITE "${root}/src/native/warnings.cmake"
         "# demo, at the release of its pin.\n")
endfunction()

# Run the check and set <status> and <output>.
function(check status output)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${root}" -DNAME=demo
                            -DSCRIPT=src/native/get-source.cmake
                            -DCALLER=src/native/demo-source.cmake
                            -DFILES=src/native/demo.cmake -DHASH=SHA256
                            -DPARTS=2 -DABSENT=-DSUPPORT_JIT
                            -DABSENT_IN=src/native/demo.cmake
                            -P "${PIN}"
                    RESULT_VARIABLE code OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(${status} "${code}" PARENT_SCOPE)
    # CMake wraps a message at its width, so the words of a refusal are
    # read with every run of spaces and line ends as one space.
    string(REGEX REPLACE "[ \n]+" " " text "${out}${err}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

# Each case: the text of the recipe, then the text the refusal names, or
# nothing when the check passes.
set(cases
    "# one dot short of the version: 10x48\n|"
    "# the version: 10.48\n|spells `10.48`"
    "# the digest: ${digest}\n|spells `${digest}`"
    "add_definitions(-DSUPPORT_JIT)\n|-DSUPPORT_JIT")
foreach(case IN LISTS cases)
    string(FIND "${case}" "|" bar)
    string(SUBSTRING "${case}" 0 ${bar} recipe)
    math(EXPR after "${bar} + 1")
    string(SUBSTRING "${case}" ${after} -1 refusal)
    write_root("${recipe}")
    check(status output)
    if(refusal STREQUAL "")
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "the check refused `${recipe}`\n${output}")
        endif()
    else()
        string(FIND "${output}" "${refusal}" at)
        if(status EQUAL 0 OR at EQUAL -1)
            message(FATAL_ERROR "the check gave ${status} for `${recipe}`, "
                                "expected a refusal naming `${refusal}`"
                                "\n${output}")
        endif()
    endif()
endforeach()

# The file of the warning flags of the libraries names no version.
write_root("")
file(WRITE "${root}/src/native/warnings.cmake" "# demo ${version}.\n")
check(status output)
string(FIND "${output}" "warnings.cmake spells `${version}`" at)
if(status EQUAL 0 OR at EQUAL -1)
    message(FATAL_ERROR "the check passed the version in the file of the "
                        "warning flags\n${output}")
endif()

# A download over plain HTTP is refused.
write_root("")
file(APPEND "${root}/src/native/get-source.cmake"
     "file(DOWNLOAD \"http://example.org/demo.tar.gz\" \"\${DEST}/demo.tar.gz\")\n")
check(status output)
string(FIND "${output}" "not HTTPS" at)
if(status EQUAL 0 OR at EQUAL -1)
    message(FATAL_ERROR "the check passed a download over HTTP\n${output}")
endif()

# Each case: the text of the script or of the call, then the text the
# refusal names. A script that hands antic_download no digest of the pin,
# a call of another algorithm and no call at all are refused.
string(REPLACE "\${\${key}_DIGEST}" "\${digest}" no_pin "${script}")
string(REPLACE "SHA256" "SHA3_256" other_algorithm "${call}")
foreach(case "script|${no_pin}|digest of the pin"
             "call|${other_algorithm}|checks demo with SHA3_256"
             "call|# no call\n|does not fetch demo")
    string(REPLACE "|" ";" case "${case}")
    list(GET case 0 kind)
    list(GET case 1 text)
    list(GET case 2 refusal)
    write_root("")
    if(kind STREQUAL "script")
        file(WRITE "${root}/src/native/get-source.cmake" "${text}")
    else()
        file(WRITE "${root}/src/native/demo-source.cmake" "${text}")
    endif()
    check(status output)
    string(FIND "${output}" "${refusal}" at)
    if(status EQUAL 0 OR at EQUAL -1)
        message(FATAL_ERROR "the check gave ${status} for the ${kind}\n"
                            "${text}\nexpected a refusal naming `${refusal}`"
                            "\n${output}")
    endif()
endforeach()

# A pin whose URL is plain HTTP, or that has none, is refused.
foreach(line "DEMO_URL=http://example.org/demo-@VERSION@.tar.gz\n" "")
    write_root("")
    file(WRITE "${root}/tools/demo-pin"
         "DEMO_VERSION=${version}\nDEMO_DIGEST=${digest}\n${line}")
    check(status output)
    string(FIND "${output}" "DEMO_URL is" at)
    if(status EQUAL 0 OR at EQUAL -1)
        message(FATAL_ERROR "the check passed a pin with `${line}` as its "
                            "URL\n${output}")
    endif()
endforeach()
