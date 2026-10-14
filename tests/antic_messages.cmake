# The messages of the driver of antic that name a file, a tool or an option
# and stand at no position of a source. One case each.
# tests/run_anti_messages.cmake states the form of a case. It runs the
# program that ANTI names, which is antic for this table, in the directory
# of the case.

set(program "fn main() -> int\n{\n\treturn 42;\n}\n")
set(library "//! A library.\n\npub fn twice(x: int) -> int\n{\n\treturn x * 2;\n}\n")
set(tools --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
# A library for C, which exports a function.
set(exporting "//! A library for C.\n\nexport fn twice(x: c_int) -> c_int\n{\n\treturn x * 2;\n}\n")

# Run antic in the directory of the case, which must succeed.
function(antic_runs)
    execute_process(COMMAND "${ANTI}" ${tools} ${ARGN}
                    WORKING_DIRECTORY "${PROJECT}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
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
# R149 of "Rules of 2026-10-13": the message names the directory.
anti_message(output_program_no_directory
    FILES main.anti "${program}"
    ARGS ${tools} -o none/main main.anti
    ERR "antic: cannot create none/main, the directory none does not exist")
anti_message(output_library_no_directory
    FILES com/example/lib.anti "${library}"
    ARGS ${tools} -c -I "${PROJECT}" -o none/lib.antl
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot create none/lib.antl, the directory none does not exist")

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
# R147 of "Rules of 2026-10-13": the module path is the path of the source
# under the search root once both are absolute and normalised, so a root
# and a source written in two forms agree. The library each setup writes
# and the command that compiles the module again both name the path.
function(compile_under_the_current_directory)
    antic_runs(-c -I . com/example/lib.anti)
endfunction()
anti_message(module_path_under_the_current_directory
    FILES com/example/lib.anti "${library}"
    SETUP compile_under_the_current_directory
    ARGS ${tools} -c -I . com/example/lib.anti com/example/lib.antl
    ERR "antic: com/example/lib.antl holds module `com.example.lib`, which this command compiles")
function(compile_under_a_root_named_twice)
    antic_runs(-c -I src ./src/com/example/lib.anti)
endfunction()
anti_message(module_path_under_a_root_named_twice
    FILES src/com/example/lib.anti "${library}"
    SETUP compile_under_a_root_named_twice
    ARGS ${tools} -c -I src/../src ./src/com/example/lib.anti
         src/com/example/lib.antl
    ERR "antic: src/com/example/lib.antl holds module `com.example.lib`, which this command compiles")
function(compile_under_an_absolute_root)
    antic_runs(-c -I "${PROJECT}" com/example/lib.anti)
endfunction()
anti_message(module_path_under_an_absolute_root
    FILES com/example/lib.anti "${library}"
    SETUP compile_under_an_absolute_root
    ARGS ${tools} -c -I . "${PROJECT}/com/example/lib.anti"
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

# R131 of "Rules of 2026-10-13": a clause of the whole file silences the
# warning `nothing-exported` of a library for C that exports nothing.
anti_message(library_exports_nothing_allowed STATUS 0 ANY_OUT
    FILES com/example/lib.anti "allow(nothing-exported, \"the library is linked for its module\");\n\n${library}"
    ARGS ${tools} --lib static -I "${PROJECT}" -o lib.a
         "${PROJECT}/com/example/lib.anti")

# R132 of "Rules of 2026-10-13": three options that do not stand beside
# the kind of library are refused, each with its message.
anti_message(library_shared_bundles_the_runtime STATUS 2
    FILES com/example/lib.anti "${exporting}"
    ARGS ${tools} --lib shared --bundle-runtime -I "${PROJECT}" -o lib.so
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: --bundle-runtime puts the runtime into a library of --lib static, and a library of --lib shared links it")
anti_message(library_static_without_the_runtime STATUS 2
    FILES com/example/lib.anti "${exporting}"
    ARGS ${tools} --lib static --no-runtime -I "${PROJECT}" -o lib.a
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: --no-runtime builds a plugin, which is a library of --lib shared")
# R150 of "Rules of 2026-10-13": --soname gives a shared library its major
# version, and an archive has none, so --lib static refuses it.
anti_message(library_static_with_a_soname STATUS 2
    FILES com/example/lib.anti "${exporting}"
    ARGS ${tools} --lib static --soname -I "${PROJECT}" -o lib.a
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: --soname gives a library of --lib shared its major version, and a library of --lib static has none")
anti_message(library_static_in_dev_mode STATUS 2
    FILES com/example/lib.anti "${exporting}"
    ARGS ${tools} --lib static --dev -I "${PROJECT}" -o lib.a
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: --lib static builds in release mode, without --dev")

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
    FILES com/example/lib.anti "${exporting}"
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

# A provider is written as one path, and the module of a function holds
# dots as its name may. `com.example.app.console.get` is the `get` of the
# singleton `console` of `com.example.app`, and the function `get` of the
# module `com.example.app.console` as well.
function(compile_both_providers)
    antic_runs(-c -I "${PROJECT}" -o "${PROJECT}/out/com/example/app.antl"
               "${PROJECT}/com/example/app.anti")
    antic_runs(-c -I "${PROJECT}/out" -I "${PROJECT}"
               -o "${PROJECT}/out/com/example/app/console.antl"
               "${PROJECT}/com/example/app/console.anti")
endfunction()
anti_message(provider_names_two_functions
    FILES com/example/app.anti "/// A logger.\npub abstract class Logger\n{\n\t/// Say it.\n\tpub abstract fn say(self) -> int;\n}\n\n/// The console.\npub singleton class console\n{\n\timplements lg: Logger,\n\n\tconcrete fn say(self) -> int { return 4; }\n}\n"
          com/example/app/console.anti "import com.example.app;\n\nsingleton class Quiet inherits app.Logger\n{\n\tconcrete fn say(self) -> int { return 5; }\n}\n\n/// The other provider.\npub fn get() -> *app.Logger\n{\n\treturn Quiet.get();\n}\n"
          main.anti "import com.example.app;\nimport com.example.app.console;\n\nclass Uses\n{\n\tpub inject log: *app.Logger,\n}\n\nfn main() -> int\n{\n\tlet u = Uses { };\n\treturn u.log.say() + console.get().say();\n}\n"
    DIRECTORIES out/com/example/app
    SETUP compile_both_providers
    ARGS ${tools} -S -I out
         --inject com.example.app.Logger=com.example.app.console.get
         -o main.s main.anti
    ERR "main.anti: error: the provider `com.example.app.console.get` of `com.example.app.Logger` names more than one function of the program")

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
