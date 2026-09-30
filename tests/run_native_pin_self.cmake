# Runs tests/run_native_pin.cmake over a repository of one made-up
# library, so the check is shown to refuse what it must and to pass what
# it must. A text that differs from the version in a dot alone passes,
# and the version or the digest spelled outside the pin fails.
#
#   cmake -DPIN=<run_native_pin.cmake> -DWORK=<dir>
#         -P tests/run_native_pin_self.cmake

set(version "10.48")
set(digest "ebcc25aadf2a51fa1fefa9b8bc9e7a79b3dae86870a0f1152a22e42befd46888")
set(root "${WORK}/root")

# Write the made-up repository with <recipe> as the text of its recipe.
function(write_root recipe)
    file(REMOVE_RECURSE "${root}")
    file(WRITE "${root}/tools/demo-pin"
         "DEMO_VERSION=${version}\nDEMO_DIGEST=${digest}\n")
    file(WRITE "${root}/src/native/get-demo.cmake"
         "file(DOWNLOAD \"https://example.org/demo-\${DEMO_VERSION}.tar.gz\"\n"
         "     \"\${DEST}/demo.tar.gz\"\n"
         "     EXPECTED_HASH \"SHA256=\${DEMO_DIGEST}\")\n")
    file(WRITE "${root}/src/native/demo.cmake" "${recipe}")
endfunction()

# Run the check and set <status> and <output>.
function(check status output)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${root}" -DNAME=demo
                            -DSCRIPT=src/native/get-demo.cmake
                            -DFILES=src/native/demo.cmake -DHASH=SHA256
                            -DPARTS=2 -DABSENT=-DSUPPORT_JIT
                            -DABSENT_IN=src/native/demo.cmake
                            -P "${PIN}"
                    RESULT_VARIABLE code OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(${status} "${code}" PARENT_SCOPE)
    set(${output} "${out}${err}" PARENT_SCOPE)
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

# A download over plain HTTP is refused.
write_root("")
file(WRITE "${root}/src/native/get-demo.cmake"
     "file(DOWNLOAD \"http://example.org/demo.tar.gz\" \"\${DEST}/demo.tar.gz\"\n"
     "     EXPECTED_HASH \"SHA256=\${DEMO_DIGEST}\")\n")
check(status output)
string(FIND "${output}" "not HTTPS" at)
if(status EQUAL 0 OR at EQUAL -1)
    message(FATAL_ERROR "the check passed a download over HTTP\n${output}")
endif()
