# Every GitHub Actions workflow runs only when started by hand. Hosted build
# minutes are limited, so no workflow may run on a push, a pull request or a
# schedule. Run with cmake -P and WORKFLOWS, the .github/workflows directory.

cmake_minimum_required(VERSION 3.21)

file(GLOB workflows "${WORKFLOWS}/*.yml" "${WORKFLOWS}/*.yaml")
foreach(workflow IN LISTS workflows)
    file(READ "${workflow}" content)
    if(content MATCHES "\n  (push|pull_request|pull_request_target|schedule|workflow_run|release):")
        message(FATAL_ERROR "${workflow} runs on ${CMAKE_MATCH_1}")
    endif()
    if(NOT content MATCHES "\non:\n  workflow_dispatch:\n")
        message(FATAL_ERROR "${workflow} does not run on workflow_dispatch alone")
    endif()
endforeach()

# The configure step of every host builds the native libraries, raylib
# among them, so the test workflow installs raylib on every runner. It
# once skipped Windows, and both Windows jobs stopped at the configure.
file(READ "${WORKFLOWS}/test.yml" content)
string(REGEX MATCH "\n      - name: Install raylib[^\n]*\n(        [^\n]*\n)*" step
       "${content}")
if(step STREQUAL "")
    message(FATAL_ERROR "test.yml installs no raylib")
endif()
if(step MATCHES "\n        if:")
    message(FATAL_ERROR "test.yml installs raylib on some runners alone")
endif()

# Every program test links against the sysroot of its target, which the
# configure step copies from build/deps/sysroot and does not install. The
# test workflow installs the sysroots with tools/get-sysroot.cmake before
# it builds, and both Linux jobs failed 633 tests without them.
string(FIND "${content}" "tools/get-sysroot.cmake" sysroots)
string(FIND "${content}" "- name: Build" build)
if(sysroots EQUAL -1 OR build EQUAL -1 OR sysroots GREATER build)
    message(FATAL_ERROR "test.yml installs no sysroots before the build")
endif()
