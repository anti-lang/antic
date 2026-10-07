# The LLVM text of the atomic operations for each of the six targets. Each
# operation is the LLVM atomic instruction of its width, sequentially
# consistent, and no call of the runtime's anti_rt_atomic_* remains. Run
# with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/dump/atomics.anti
#   WORK      a directory for the text

file(MAKE_DIRECTORY "${WORK}")
set(wanted
    "load atomic i8, ptr [^\n]* seq_cst, align 1"
    "store atomic i16 [^\n]* seq_cst, align 2"
    "atomicrmw xchg ptr [^\n]*, i32 [^\n]* seq_cst, align 4"
    "atomicrmw add ptr [^\n]*, i64 [^\n]* seq_cst, align 8"
    "atomicrmw sub ptr [^\n]*, i8 "
    "atomicrmw and ptr [^\n]*, i16 "
    "atomicrmw or ptr [^\n]*, i32 "
    "cmpxchg ptr [^\n]*, i64 [^\n]*, i64 [^\n]* seq_cst seq_cst, align 8"
    "sext i8 [^\n]* to i64"
    "inttoptr i64 ")
foreach(target linux-x86_64 linux-arm64 macos-x86_64 macos-arm64
        windows-x86_64 windows-arm64)
    execute_process(
        COMMAND "${ANTIC}" --runtime "${RUNTIME}" --dev --dump-llvm
                --target ${target} "${SOURCE}"
        RESULT_VARIABLE status OUTPUT_FILE "${WORK}/atomics.${target}.ll"
        ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${target}\n${err}")
    endif()
    file(READ "${WORK}/atomics.${target}.ll" text)
    foreach(pattern IN LISTS wanted)
        if(NOT text MATCHES "${pattern}")
            message(FATAL_ERROR "the text of ${target} holds no '${pattern}'")
        endif()
    endforeach()
    if(text MATCHES "call [^\n]*@[\"_]*anti_rt_atomic_")
        message(FATAL_ERROR "the text of ${target} calls anti_rt_atomic_*")
    endif()
endforeach()
