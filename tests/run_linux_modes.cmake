# Build a program in each Linux link mode with `anti build`, for both
# Linux targets. tests/anti-build/app imports nothing that names a
# library, so it links statically against musl. The module
# com.example.xlib of tests/anti-build/window carries `link linux "X11";`
# and `link linux "GL";`, and the program names neither. The build passes
# both to antic, which links it dynamically against glibc 2.35 with the
# two libraries of the sysroot. A Linux host runs the program of its own
# target. Run with cmake -P and these values:
#   ANTI     the anti executable
#   READOBJ  llvm-readobj of the pinned release
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  tests/anti-build
#   HOST     the target name of this host
#   WORK     a directory this run writes into

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" "${FIXTURE}/window" DESTINATION "${WORK}")

# The dynamic section and the program headers of exe.
function(read_elf exe out)
    execute_process(COMMAND "${READOBJ}" --needed-libs --program-headers
                            "${exe}"
        RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${READOBJ} failed on ${exe}\n${text}${err}")
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

foreach(target linux-arm64 linux-x86_64)
    foreach(mode dev release)
        set(flag "")
        if(mode STREQUAL "release")
            set(flag --release)
        endif()
        foreach(project app window)
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E env
                        "XDG_CACHE_HOME=${WORK}/cache"
                        "LOCALAPPDATA=${WORK}/cache"
                        "${ANTI}" build ${flag} --target "${target}"
                        --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                WORKING_DIRECTORY "${WORK}/${project}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
            if(NOT status EQUAL 0)
                message(FATAL_ERROR "anti build ${flag} --target ${target} of "
                                    "${project} failed with ${status}\n"
                                    "${out}${err}")
            endif()
        endforeach()

        # The static program has no dynamic linker and loads nothing.
        set(app "${WORK}/app/dist/${target}/${mode}/app")
        read_elf("${app}" text)
        if(text MATCHES "PT_INTERP" OR text MATCHES "\\.so")
            message(FATAL_ERROR "${app} is not static\n${text}")
        endif()

        # The dynamic one names the dynamic linker of glibc and loads the
        # two libraries of the binding with libm and libc.
        set(window "${WORK}/window/dist/${target}/${mode}/window")
        read_elf("${window}" text)
        foreach(needed PT_INTERP libX11\\.so\\.6 libGL\\.so\\.1 libm\\.so\\.6
                       libc\\.so\\.6)
            if(NOT text MATCHES "${needed}")
                message(FATAL_ERROR "${window} lacks ${needed}\n${text}")
            endif()
        endforeach()

        if(NOT target STREQUAL HOST)
            continue()
        endif()
        execute_process(COMMAND "${app}" RESULT_VARIABLE status
                        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 7 OR NOT out STREQUAL "one\n")
            message(FATAL_ERROR "${app} gave ${status}\n${out}${err}")
        endif()
        execute_process(COMMAND "${window}" RESULT_VARIABLE status
                        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 0 OR NOT out STREQUAL "no display\nno context\n")
            message(FATAL_ERROR "${window} gave ${status}\n${out}${err}")
        endif()
    endforeach()
endforeach()
