# The permissions step 9 leaves on anti-lang.com. The webroot is setgid,
# so a file that reaches it carries the group the server reads it with.
# A directory made below it carries that group only while every directory
# on the way is setgid too, and the server answers 403 for one that is
# not. The files themselves stay off the world, because the group is what
# the server reads them with.
#
#   cmake -DROOT=<repository> -P tests/run_release_site.cmake

file(READ "${ROOT}/tools/release.sh" script)

# The directory of the signature is made over ssh, and it keeps the group
# of the webroot only when it is setgid.
string(FIND "${script}" "chmod g+s" setgid)
if(setgid EQUAL -1)
    message(FATAL_ERROR "step 9 makes the directory of the signature without "
                        "the setgid bit, and the server reads no file in it")
endif()

# The bit on a directory that exists changes no group, so the directory
# above is set before the directory of the version is made.
string(FIND "${script}" "chmod g+s '$signature_parent'" parent_set)
string(FIND "${script}" "mkdir -p '$signature_directory'" version_made)
if(parent_set EQUAL -1 OR version_made EQUAL -1 OR NOT parent_set LESS version_made)
    message(FATAL_ERROR "step 9 makes the directory of the version before it "
                        "sets the one above it. The directory then carries "
                        "the group of the user who made it")
endif()

# Nothing step 9 sends is readable by the world. The group of the server
# is what reads it.
string(REGEX MATCHALL "rsync --chmod=[^ ]+" modes "${script}")
if(NOT modes)
    message(FATAL_ERROR "step 9 rsyncs without a mode")
endif()
foreach(mode ${modes})
    if(NOT mode STREQUAL "rsync --chmod=u=rw,g=r,o=")
        message(FATAL_ERROR "step 9 sends a file with `${mode}`, and the site "
                            "serves what the group of the server reads")
    endif()
endforeach()
