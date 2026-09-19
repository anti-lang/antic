# DESIGN: the raw-bytes rule. The harness reads the standard output of a
# program as the bytes of a file, on every host. An OUTPUT_VARIABLE loses
# the CR of each CRLF and every NUL, and a file keeps them.

# Run the command after <file> with its standard output in <file>. Set
# <hex> to those bytes in hexadecimal and <status> to its exit status.
function(program_output hex status file)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_FILE "${file}")
    file(READ "${file}" bytes HEX)
    set(${hex} "${bytes}" PARENT_SCOPE)
    set(${status} "${code}" PARENT_SCOPE)
endfunction()
