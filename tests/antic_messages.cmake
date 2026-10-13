# The messages of the driver of antic that name a file, a tool or an option
# and stand at no position of a source. One case each.
# tests/run_anti_messages.cmake states the form of a case. It runs the
# program that ANTI names, which is antic for this table, in the directory
# of the case.

set(program "fn main() -> int\n{\n\treturn 42;\n}\n")
set(library "//! A library.\n\npub fn twice(x: int) -> int\n{\n\treturn x * 2;\n}\n")
set(tools --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")

# Run antic in the directory of the case, which must succeed.
function(antic_runs)
    execute_process(COMMAND "${ANTI}" ${tools} ${ARGN}
                    WORKING_DIRECTORY "${PROJECT}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err)
    if(NOT status EQUAL 0)
        string(REPLACE ";" " " line "${ARGN}")
        message(FATAL_ERROR "antic ${line} ended with ${status}\n${out}${err}")
    endif()
endfunction()

# The package header of a library.
anti_message(dependency_form
    FILES main.anti "${program}"
    ARGS ${tools} --dependency nocommas main.anti
    ERR "antic: --dependency nocommas is not <name>,<constraint>,<url>")
anti_message(license_text_missing
    FILES com/example/lib.anti "${library}"
    ARGS ${tools} -c -I "${PROJECT}" --license-text none.txt
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot open none.txt")
# A program holds a NUL byte, and antic itself is the one this case has.
anti_message(license_text_with_a_nul_byte
    FILES com/example/lib.anti "${library}"
    ARGS ${tools} -c -I "${PROJECT}" --license-text "${ANTI}"
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: ${ANTI} contains a NUL byte")

# An output in a directory that is not there.
anti_message(output_program_no_directory
    FILES main.anti "${program}"
    ARGS ${tools} -o none/main main.anti
    ERR "antic: cannot create none/main.ll")
anti_message(output_library_no_directory
    FILES com/example/lib.anti "${library}"
    ARGS ${tools} -c -I "${PROJECT}" -o none/lib.antl
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot create none/lib.antl")

# A library file of the command line and of a search root.
anti_message(library_missing
    FILES main.anti "${program}"
    ARGS ${tools} main.anti none.antl
    ERR "antic: cannot open none.antl")
anti_message(library_no_library_file
    FILES main.anti "${program}"
          junk.antl "A text where the library file belongs.\n"
    ARGS ${tools} main.anti junk.antl
    ERR "antic: junk.antl is not a library file")
anti_message(library_of_a_root_no_library_file
    FILES main.anti "import com.example.bad;\n\n${program}"
          com/example/bad.antl "A text where the library file belongs.\n"
    ARGS ${tools} -I "${PROJECT}" main.anti
    ERR_MATCH "antic: [^\n]*bad\\.antl is not a library file\n")
function(compile_the_library)
    antic_runs(-c -I "${PROJECT}" "${PROJECT}/com/example/lib.anti")
endfunction()
anti_message(library_of_the_module_compiled
    FILES com/example/lib.anti "${library}"
    SETUP compile_the_library
    ARGS ${tools} -c -I "${PROJECT}" "${PROJECT}/com/example/lib.anti"
         com/example/lib.antl
    ERR "antic: com/example/lib.antl holds module `com.example.lib`, which this command compiles")
# DESIGN: antic refuses the library file of the module it compiles, so no
# two builds in one tree write a cycle. Each of the two trees below holds
# one module that imports nothing and one that imports it, and `both`
# gets the importing library file of each.
function(compile_a_cycle)
    foreach(pair "first|a|b" "second|b|a")
        string(REPLACE "|" ";" pair "${pair}")
        list(GET pair 0 tree)
        list(GET pair 1 plain)
        list(GET pair 2 importing)
        antic_runs(-c -I "${PROJECT}/${tree}"
                   "${PROJECT}/${tree}/com/example/${plain}.anti")
        antic_runs(-c -I "${PROJECT}/${tree}"
                   -o "${PROJECT}/both/com/example/${importing}.antl"
                   "${PROJECT}/${tree}/com/example/${importing}.anti")
    endforeach()
endfunction()
anti_message(library_cycle
    FILES first/com/example/a.anti "//! a.\n\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          first/com/example/b.anti "//! b.\n\nimport com.example.a;\n\npub fn two() -> int\n{\n\treturn a.one() + 1;\n}\n"
          second/com/example/b.anti "//! b.\n\npub fn two() -> int\n{\n\treturn 2;\n}\n"
          second/com/example/a.anti "//! a.\n\nimport com.example.b;\n\npub fn one() -> int\n{\n\treturn b.two() - 1;\n}\n"
          main.anti "import com.example.a;\n\nfn main() -> int\n{\n\treturn a.one();\n}\n"
    DIRECTORIES both/com/example
    SETUP compile_a_cycle
    ARGS ${tools} -I both main.anti
    ERR "antic: the library files import each other in a cycle")

# The name of a source is the last segment of its module path.
anti_message(source_name_starts_with_a_digit
    FILES 1abc.anti "${program}"
    ARGS ${tools} -c 1abc.anti
    ERR "antic: `1abc` in 1abc.anti is not a lowercase identifier")

# A build that links, without the runtime archive.
anti_message(lib_without_runtime
    STATUS 2
    FILES com/example/lib.anti "${library}"
    ARGS --lib static -I "${PROJECT}" "${PROJECT}/com/example/lib.anti"
    ERR "antic: --lib needs --runtime <dir>, the directory that holds lib/<target>/<level>/")
anti_message(dev_without_runtime
    FILES main.anti "${program}"
    ARGS --dev main.anti
    ERR "antic: linking needs --runtime <dir>, the directory that holds lib/<target>/")

# A library file as the input of --dev.
anti_message(dev_library_missing
    ARGS ${tools} --dev none.antl
    ERR "antic: cannot open none.antl")
anti_message(dev_library_no_library_file
    FILES junk.antl "A text where the library file belongs.\n"
    ARGS ${tools} --dev junk.antl
    ERR "antic: junk.antl is not a library file")

# A tool that does not start. The system words the reason, so the cases
# hold a pattern of that line.
anti_message(archiver_does_not_start
    FILES com/example/lib.anti "${library}"
    ARGS ${tools} --lib static --llvm-ar ./none -I "${PROJECT}" -o lib.a
         "${PROJECT}/com/example/lib.anti"
    ERR_MATCH "antic: cannot run \\./none: [^\n]*\nantic: llvm-ar failed\n")
anti_message(opt_does_not_start
    FILES main.anti "${program}"
    ARGS --runtime "${RUNTIME}" --opt ./none -o main main.anti
    ERR_MATCH "antic: cannot run \\./none: [^\n]*\nantic: opt failed\n")
anti_message(llc_does_not_start
    FILES main.anti "${program}"
    ARGS --runtime "${RUNTIME}" --lto none --llc ./none -o main main.anti
    ERR_MATCH "antic: cannot run \\./none: [^\n]*\nantic: llc failed\n")

# The index beside a plugin holds the name of the library as a TOML string
# of one line, which takes a name with one kind of quote. Windows has no
# file name with a `"`.
if(NOT CMAKE_HOST_WIN32)
    set(plugins "${CMAKE_CURRENT_LIST_DIR}/plugin")
    function(compile_the_interface)
        antic_runs(-c -I "${plugins}" -o "${PROJECT}/net/example/greet.antl"
                   "${plugins}/net/example/greet.anti")
    endfunction()
    anti_message(plugin_name_with_both_quotes
        DIRECTORIES net/example
        SETUP compile_the_interface
        ARGS ${tools} --lib shared --no-runtime -I "${plugins}"
             -I "${PROJECT}" -o "we\"ir'd.so"
             "${plugins}/net/example/fancy.anti"
        ERR "antic: anti-plugins.toml cannot hold a name with both quotes or a line break: we\"ir'd.so")
endif()
