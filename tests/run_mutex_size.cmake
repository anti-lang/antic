# The size of a Mutex on each target. Run with cmake -P and these values:
#   ANTIC    the antic executable
#   SOURCE   tests/dump/mutex_size.anti
#   WORK     a directory for the listings
#
# main returns size_of(Mutex) times 1000, which the back end folds into
# one constant: 4000 on Linux and macOS, where the word is a futex word
# or an os_unfair_lock, and 8000 on Windows, where it is an SRWLOCK.

file(MAKE_DIRECTORY "${WORK}")
foreach(case linux-x86_64=4000 linux-arm64=4000 macos-arm64=4000
        macos-x86_64=4000 windows-x86_64=8000 windows-arm64=8000)
    string(REPLACE "=" ";" parts "${case}")
    list(GET parts 0 target)
    list(GET parts 1 wanted)
    set(listing "${WORK}/mutex_size.${target}.s")
    execute_process(COMMAND "${ANTIC}" -S --target "${target}" -o "${listing}"
                            "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic -S failed for ${target}\n${err}")
    endif()
    file(READ "${listing}" text)
    if(NOT text MATCHES "[#$]${wanted}[,\n]")
        message(FATAL_ERROR "the listing of ${target} does not return "
                            "${wanted}:\n${text}")
    endif()
endforeach()
