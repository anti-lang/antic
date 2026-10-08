# Anti runs no CI. Every suite runs on the development Mac and on the two
# VMs of docs/vm-setup.md, as the entry of 2026-10-08 under "Scope and
# toolchain" in docs/decisions.md says. A file under .github/workflows/
# would bring a workflow back, so the test refuses any, tracked or not.
# Run with cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

file(GLOB_RECURSE workflows LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/.github/workflows/*")
if(NOT workflows STREQUAL "")
    list(JOIN workflows "\n  " listed)
    message(FATAL_ERROR "Anti runs no CI, and these files stand under "
                        ".github/workflows/:\n  ${listed}")
endif()
