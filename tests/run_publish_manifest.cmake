# tools/publish.cmake reads a version directory before it uploads one. It
# takes the files the manifest names, with the digest each line holds, a
# signature of the manifest and nothing else. A user installs from that
# directory, and every refusal here is an install that would have failed
# or, worse, succeeded on the wrong bytes.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/run_publish_manifest.cmake

find_program(OPENSSL openssl)
if(NOT OPENSSL)
    message("SKIP: openssl is missing")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(area "${WORK}/area")
file(MAKE_DIRECTORY "${area}")

function(run message)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE failed
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT failed EQUAL 0)
        message(FATAL_ERROR "${message}\n${out}${err}")
    endif()
endfunction()

run("openssl wrote no private key" "${OPENSSL}" ecparam -name prime256v1
    -genkey -noout -out "${WORK}/private.pem")
run("openssl wrote no public key" "${OPENSSL}" ec -in "${WORK}/private.pem"
    -pubout -out "${WORK}/public.pem")

# Write the two files of the area, the manifest that names them and a
# signature of that manifest.
function(write_area)
    file(WRITE "${area}/anti-1.0.0-macos-arm64.tar.xz" "a package\n")
    file(WRITE "${area}/anti-1.0.0-macos-arm64-symbols.zip" "the symbols\n")
    set(lines "")
    foreach(name anti-1.0.0-macos-arm64.tar.xz anti-1.0.0-macos-arm64-symbols.zip)
        file(SHA256 "${area}/${name}" digest)
        string(APPEND lines "${digest}  ${name}\n")
    endforeach()
    file(WRITE "${area}/SHA256SUMS" "${lines}")
    run("openssl hashed nothing" "${OPENSSL}" dgst -sha256 -binary
        -out "${WORK}/SHA256SUMS.sha256" "${area}/SHA256SUMS")
    run("openssl signed nothing" "${OPENSSL}" pkeyutl -sign
        -inkey "${WORK}/private.pem" -in "${WORK}/SHA256SUMS.sha256"
        -out "${area}/SHA256SUMS.sig")
endfunction()

# Run the checks of tools/publish.cmake over the area, and answer with the
# result and the output.
function(check out_variable log_variable)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DDIR=${area}" -DCHECK_ONLY=yes
                "-DKEY=${WORK}/public.pem" -P "${ROOT}/tools/publish.cmake"
        RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    set("${out_variable}" "${failed}" PARENT_SCOPE)
    set("${log_variable}" "${out}${err}" PARENT_SCOPE)
endfunction()

write_area()
check(failed log)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "publish.cmake refused a whole area\n${log}")
endif()

# No signature. An installer asks for one, so a directory without it is
# never uploaded.
file(REMOVE "${area}/SHA256SUMS.sig")
check(failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "SHA256SUMS\\.sig")
    message(FATAL_ERROR "publish.cmake uploads an unsigned manifest\n${log}")
endif()

# A signature of another manifest, which is what a line added by hand
# leaves behind.
write_area()
file(APPEND "${area}/SHA256SUMS"
     "0000000000000000000000000000000000000000000000000000000000000000  another\n")
file(WRITE "${area}/another" "a file the signature does not cover\n")
check(failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "signature")
    message(FATAL_ERROR "publish.cmake uploads a manifest its signature does "
                        "not cover\n${log}")
endif()
file(REMOVE "${area}/another")

# A file the manifest does not name, which a reader takes for part of the
# release.
write_area()
file(WRITE "${area}/leftover.txt" "a file of a debugging session\n")
check(failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "leftover")
    message(FATAL_ERROR "publish.cmake uploads a file the manifest does not "
                        "name\n${log}")
endif()
file(REMOVE "${area}/leftover.txt")

# A file the manifest names and the directory lacks.
write_area()
file(REMOVE "${area}/anti-1.0.0-macos-arm64-symbols.zip")
check(failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "symbols")
    message(FATAL_ERROR "publish.cmake uploads a manifest naming a file that "
                        "is missing\n${log}")
endif()

# A file whose bytes are not the ones the manifest names.
write_area()
file(WRITE "${area}/anti-1.0.0-macos-arm64.tar.xz" "another package\n")
check(failed log)
if(failed EQUAL 0 OR NOT "${log}" MATCHES "SHA-256")
    message(FATAL_ERROR "publish.cmake uploads a file whose digest is another "
                        "one\n${log}")
endif()

file(REMOVE_RECURSE "${WORK}")
