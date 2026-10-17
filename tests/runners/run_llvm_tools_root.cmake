# bin/ of the runtime archive is a part of the product, and the root
# CMakeLists.txt builds it, beside the copies of the runtimes of
# AddressSanitizer and of the profile. Its target llvm_tools reads the
# directory of the pinned LLVM tools, ANTIC_LLVM_DIR. The test refuses the
# target in any other CMakeLists.txt, a rule of tests/ that writes
# runtime/bin, and a target keyed on the path of llvm-mc, which tied the
# directory to an option of antic. Run with cmake -P and ROOT, the root of
# the repository.

cmake_minimum_required(VERSION 3.21)

file(READ "${ROOT}/CMakeLists.txt" root)
string(REGEX MATCH "add_custom_target\\(llvm_tools ALL[^)]*\\)" target "${root}")
if(target STREQUAL "")
    message(FATAL_ERROR "the root CMakeLists.txt defines no target llvm_tools")
endif()
if(NOT target MATCHES "\\\${ANTIC_LLVM_DIR}/bin")
    message(FATAL_ERROR "the target llvm_tools of the root CMakeLists.txt "
                        "does not read ANTIC_LLVM_DIR:\n${target}")
endif()

file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/tests/*CMakeLists.txt" "${ROOT}/tests/*.cmake"
     "${ROOT}/src/*CMakeLists.txt" "${ROOT}/src/*.cmake")
list(SORT files)
set(found "")
foreach(file IN LISTS files)
    file(STRINGS "${ROOT}/${file}" lines
         REGEX "add_custom_target\\(llvm_tools|-DDEST=\\\${ANTIC_RUNTIME_DIR}/bin")
    if(lines)
        string(APPEND found "\n  ${file}")
    endif()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "these files build bin/ of the runtime archive, "
                        "which the root CMakeLists.txt builds:${found}")
endif()
