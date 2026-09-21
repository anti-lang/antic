# The public key that checks SHA256SUMS.sig of the LLVM tools lives in the
# installers, which anti-lang.com serves, and in keys/release.pem of the
# repository. A package carries none, since whoever could replace a package
# could replace a key inside it. Each installer holds the key of the file.
#
#   cmake -DROOT=<repository> -P tests/run_release_key.cmake

set(key "${ROOT}/keys/release.pem")
if(NOT EXISTS "${key}")
    message(FATAL_ERROR "keys/release.pem, the key that checks SHA256SUMS.sig, "
                        "is missing")
endif()
file(READ "${key}" pem)
if(NOT pem MATCHES "^-----BEGIN PUBLIC KEY-----\n[A-Za-z0-9+/=\n]+-----END PUBLIC KEY-----\n$")
    message(FATAL_ERROR "keys/release.pem is not one public key in PEM form")
endif()
string(STRIP "${pem}" pem)

foreach(installer install.sh install.ps1)
    file(READ "${ROOT}/tools/${installer}" text)
    string(REGEX MATCHALL "-----BEGIN [A-Z ]*KEY-----[A-Za-z0-9+/=\r\n]+-----END [A-Z ]*KEY-----"
           blocks "${text}")
    list(LENGTH blocks count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "tools/${installer} holds ${count} keys, not one")
    endif()
    string(REPLACE "\r" "" blocks "${blocks}")
    if(NOT blocks STREQUAL pem)
        message(FATAL_ERROR "tools/${installer} holds another key than "
                            "keys/release.pem")
    endif()
endforeach()

# Nothing under tools/ is a key, because tools/ goes into the package.
file(GLOB_RECURSE keys "${ROOT}/tools/*.pem" "${ROOT}/tools/*.gpg"
     "${ROOT}/tools/*.asc")
if(keys)
    message(FATAL_ERROR "tools/ holds ${keys}, and tools/ goes into the package")
endif()

# SHA256SUMS.sig is the one signature of a release, and the key that makes
# it stands at keys/private/release-key.pem. The tag of step 7 is annotated
# and unsigned, as in llvm-tools, so a release asks gpg for no key.
file(READ "${ROOT}/tools/release.sh" script)
string(FIND "${script}" "tag -s " signed)
if(NOT signed EQUAL -1)
    message(FATAL_ERROR "tools/release.sh signs the tag with gpg, and a "
                        "release holds no gpg key")
endif()
string(FIND "${script}" "tag -a \"$tag\"" annotated)
if(annotated EQUAL -1)
    message(FATAL_ERROR "tools/release.sh makes no annotated tag")
endif()
