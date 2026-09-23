# Build tests/anti-build/clock with `anti build` in both modes. Its module
# com.example.cf carries `link framework "CoreFoundation";`, and the
# program names no framework. The build passes the framework to antic,
# so the program names CoreFoundation among the libraries it loads. Run
# with cmake -P and these values:
#   ANTI     the anti executable
#   OBJDUMP  llvm-objdump of the pinned release
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  tests/anti-build
#   HOST     the target name of this host, a macOS target
#   WORK     a directory this run writes into

set(project "${WORK}/clock")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/clock" DESTINATION "${WORK}")

foreach(mode dev release)
    set(flag "")
    if(mode STREQUAL "release")
        set(flag --release)
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" build ${flag} --runtime "${RUNTIME}"
                --llvm-mc "${LLVM_MC}"
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build ${flag} failed with ${status}\n${out}${err}")
    endif()
    set(exe "${project}/dist/${HOST}/${mode}/clock")
    execute_process(COMMAND "${OBJDUMP}" --macho --dylibs-used "${exe}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0 OR
       NOT out MATCHES "CoreFoundation\\.framework/Versions/A/CoreFoundation")
        message(FATAL_ERROR "${exe} names no CoreFoundation\n${out}${err}")
    endif()
    execute_process(COMMAND "${exe}" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${exe} exited with ${status}")
    endif()
endforeach()
