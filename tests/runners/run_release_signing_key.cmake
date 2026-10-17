# ./r --dry-run on the copy of tests/release_copy.cmake takes a plaintext
# and an encrypted signing key that git cannot see, refuses one that git
# tracks or that no rule of .gitignore excludes and a file that holds no
# private key, and says it would stop before the tag without a key.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCC=<compiler>
#         -DHOST_LINK=<options> -DSDK=<Apple SDK> -DLLVM_BIN=<dir>
#         -P tests/runners/run_release_signing_key.cmake
#
# tests/release_copy.cmake describes the values.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../release_copy.cmake")
if(release_skipped)
    return()
endif()

# DESIGN: the signing key has one path, tools/keys/private/release-key.pem, and
# no environment variable names it. What a release proves before it signs
# is that git sees neither the file nor its directory: a tracked key is a
# published key, and a key no rule ignores is one `git add -A` from being
# tracked. The key is plaintext, because it stands on an offline machine.
find_program(OPENSSL openssl)
if(NOT OPENSSL)
    message("SKIP: openssl is missing")
    return()
endif()
set(key_path "tools/keys/private/release-key.pem")

# Run ./r --dry-run on the copy and answer with its output.
function(key_run out_variable log_variable)
    set(ENV{PATH} "${WORK}/bin:${saved_path}")
    execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                    RESULT_VARIABLE failed OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(ENV{PATH} "${saved_path}")
    set("${out_variable}" "${failed}" PARENT_SCOPE)
    set("${log_variable}" "${out}${err}" PARENT_SCOPE)
endfunction()

# A plaintext key that .gitignore excludes and git does not track.
# The directory is ignored, so a checkout of the tree holds none.
file(MAKE_DIRECTORY "${copy}/tools/keys/private")
tool_run("openssl wrote no plaintext key" "${OPENSSL}" ecparam -name prime256v1
         -genkey -noout -out "${copy}/${key_path}")
key_run(failed log)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "./r refused a key that git cannot see\n${log}")
endif()
if(NOT "${log}" MATCHES "plaintext key")
    message(FATAL_ERROR "./r does not say which form of key it would sign "
                        "with\n${log}")
endif()

# An encrypted key at the same path is taken as readily.
file(RENAME "${copy}/${key_path}" "${WORK}/plaintext.pem")
tool_run("openssl wrote no encrypted key" "${OPENSSL}" pkcs8 -topk8
         -in "${WORK}/plaintext.pem" -out "${copy}/${key_path}"
         -passout pass:secret)
key_run(failed log)
if(NOT failed EQUAL 0 OR NOT "${log}" MATCHES "encrypted key")
    message(FATAL_ERROR "./r refused an encrypted key\n${log}")
endif()
file(REMOVE "${copy}/${key_path}")
file(COPY "${WORK}/plaintext.pem" DESTINATION "${copy}/tools/keys/private")
file(RENAME "${copy}/tools/keys/private/plaintext.pem" "${copy}/${key_path}")

# A key git tracks is refused, and the refusal names the file. The
# add needs -f, which is the whole point: one of those publishes it.
# It is committed, because the preflight refuses a dirty index first.
tool_run("git add failed" "${GIT}" -C "${copy}" add -f "${key_path}")
tool_run("the commit failed" "${GIT}" -C "${copy}" commit --quiet
         -m "The key, by accident")
tool_run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)
key_run(refused log)
if(refused EQUAL 0)
    message(FATAL_ERROR "./r signed with a key that git tracks\n${log}")
endif()
if(NOT "${log}" MATCHES "tracks ${key_path}")
    message(FATAL_ERROR "the refusal of a tracked key names no "
                        "file\n${log}")
endif()
tool_run("git rm failed" "${GIT}" -C "${copy}" rm --quiet --cached "${key_path}")
tool_run("the commit failed" "${GIT}" -C "${copy}" commit --quiet
         -m "The key, out of the index again")
tool_run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)

# A key that no rule of .gitignore excludes is refused as well, even
# though it is untracked, because nothing keeps it untracked.
file(READ "${copy}/.gitignore" ignores)
string(REPLACE "tools/keys/private/\n" "" without "${ignores}")
if(without STREQUAL ignores)
    message(FATAL_ERROR ".gitignore of the tree excludes no tools/keys/private")
endif()
file(WRITE "${copy}/.gitignore" "${without}")
tool_run("git add failed" "${GIT}" -C "${copy}" add .gitignore)
tool_run("the commit failed" "${GIT}" -C "${copy}" commit --quiet
         -m "No rule for the key")
tool_run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)
key_run(refused log)
if(refused EQUAL 0 OR NOT "${log}" MATCHES "${key_path}")
    message(FATAL_ERROR "./r signed with a key that .gitignore does not "
                        "exclude\n${log}")
endif()
file(WRITE "${copy}/.gitignore" "${ignores}")
tool_run("git add failed" "${GIT}" -C "${copy}" add .gitignore)
tool_run("the commit failed" "${GIT}" -C "${copy}" commit --quiet
         -m "The rule again")
tool_run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)

# A file at that path that holds no private key stops the run.
file(WRITE "${copy}/${key_path}" "-----BEGIN PUBLIC KEY-----\n")
key_run(refused log)
if(refused EQUAL 0 OR NOT "${log}" MATCHES "no private key")
    message(FATAL_ERROR "./r took a file that holds no private key\n${log}")
endif()
file(REMOVE "${copy}/${key_path}")

# Without the key the run says it would stop before the tag, which is
# what every machine but the one that holds the key does.
key_run(failed log)
if(NOT failed EQUAL 0 OR NOT "${log}" MATCHES "stop before the tag")
    message(FATAL_ERROR "./r says nothing of the missing key\n${log}")
endif()

file(REMOVE_RECURSE "${WORK}")
