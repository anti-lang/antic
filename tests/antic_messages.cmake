# The messages of the driver of antic that name a file, a tool or an option
# and stand at no position of a source. One case each.
# tests/runners/run_anti_messages.cmake states the form of a case. It runs the
# program that ANTI names, which is antic for this table, in the directory
# of the case.

set(program "fn main() -> int\n{\n\treturn 42;\n}\n")
set(library "//! A library.\n\npub fn twice(x: int) -> int\n{\n\treturn x * 2;\n}\n")
set(tools --runtime "${RUNTIME}")
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
    ERR "antic: cannot create none/main.ll, the directory none does not exist")
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
# A root in the top directory of the file system that does not exist holds
# no source, and the next root gives the module path.
anti_message(module_path_beside_a_root_that_does_not_exist
    FILES com/example/lib.anti "${library}"
    SETUP compile_under_the_current_directory
    ARGS ${tools} -c -I /no-such-root-of-anti -I . com/example/lib.anti
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

# Fault injection: each case makes one call of the system fail in
# antic_faults, through the variable ANTI_FAULT of src/rt/fault.h. A count
# names the call among those of its kind in the run, so a case names the
# run it counts in.

# text as a regular expression that matches the text alone.
function(literal_pattern out text)
    string(REGEX REPLACE "([][+.*()^$?|\\\\])" "\\\\\\1" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# Memory that runs out ends antic with status 70, alloc.c.
anti_message(fault_out_of_memory
    FILES main.anti "${program}"
    FAULT alloc:1:ENOMEM
    STATUS 70
    ARGS ${tools} main.anti
    ERR "antic: out of memory")
# A source that opens and does not read, read_file of driver.c.
anti_message(fault_source_unread
    FILES main.anti "${program}"
    FAULT read:1:EIO
    ARGS ${tools} main.anti
    ERR "antic: cannot read main.anti")
# The library file of -c, whose open is the second, after the source, and
# whose write and close are the first of the run, driver_write_file of
# driver.c.
anti_message(fault_output_not_created
    FILES com/example/lib.anti "${library}"
    FAULT open:2:EACCES
    ARGS ${tools} -c -I "${PROJECT}" -o lib.antl
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot create lib.antl")
anti_message(fault_output_not_written
    FILES com/example/lib.anti "${library}"
    FAULT write:1:ENOSPC
    ARGS ${tools} -c -I "${PROJECT}" -o lib.antl
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot write lib.antl")
anti_message(fault_output_not_closed
    FILES com/example/lib.anti "${library}"
    FAULT close:2:ENOSPC
    ARGS ${tools} -c -I "${PROJECT}" -o lib.antl
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot write lib.antl")
# The thread of the work, platform_run_on_stack of platform.c. Its join
# is the first wait of a run, and the thread is not waited for, so the
# case compiles a library, which starts no other program.
anti_message(fault_work_not_started
    FILES main.anti "${program}"
    FAULT thread:1:EACCES
    ARGS ${tools} main.anti
    ERR "antic: cannot start the thread of the work: Permission denied")
anti_message(fault_work_not_waited
    FILES com/example/lib.anti "${library}"
    FAULT wait:1:EINVAL
    ARGS ${tools} -c -I "${PROJECT}" -o lib.antl
         "${PROJECT}/com/example/lib.anti"
    ERR "antic: cannot wait for the thread of the work: Invalid argument")
# The run of opt, the first program a release build starts, spawn and
# wait_for of platform.c and run_opt of llvm_run.c. A wait that fails
# leaves opt running, and opt then writes a message of its own once antic
# removed its input, so the case matches the two lines of antic first.
anti_message(fault_opt_not_started
    FILES main.anti "${program}"
    FAULT run:1:EACCES
    ARGS ${tools} main.anti
    ERR "antic: cannot run ${RUNTIME}/bin/opt: Permission denied"
        "antic: opt failed")
literal_pattern(opt_pattern "${RUNTIME}/bin/opt")
anti_message(fault_opt_not_waited
    FILES main.anti "${program}"
    FAULT wait:2:ECHILD
    ARGS ${tools} main.anti
    ERR_MATCH "antic: waiting for ${opt_pattern}: No child processes\nantic: opt failed\n.*")
# -S runs opt and then llc for the assembly alone, llvm_run of
# llvm_run.c, and ends with 1 when either does not start, driver_run of
# driver.c.
anti_message(fault_assembly_opt_not_started
    FILES main.anti "${program}"
    FAULT run:1:EACCES
    ARGS ${tools} -S main.anti
    ERR "antic: cannot run ${RUNTIME}/bin/opt: Permission denied"
        "antic: opt failed")
anti_message(fault_assembly_llc_not_started
    FILES main.anti "${program}"
    FAULT run:2:EACCES
    ARGS ${tools} -S main.anti
    ERR "antic: cannot run ${RUNTIME}/bin/llc: Permission denied"
        "antic: llc failed")
# llc of --lto none writes into a directory of its own, so it runs in that
# directory by absolute paths, run_llc of llvm_run.c and platform_run_in
# of platform.c. The second program of the run is llc. The third and
# fourth stat make the absolute path of llc, realpath of the file and of
# its directory, and the fourth and fifth that of its input once the
# third gave the first.
file(REAL_PATH "${RUNTIME}/bin/llc" real_llc)
anti_message(fault_llc_not_forked
    FILES main.anti "${program}"
    DIRECTORIES out
    FAULT run:2:EACCES
    ARGS ${tools} --lto none -o out/main main.anti
    ERR "antic: cannot run ${real_llc}: Permission denied"
        "antic: llc failed")
# The child of the fork enters the directory and runs llc, the seventh
# open and the third program of the run, counted on in the child.
anti_message(fault_llc_not_entered
    FILES main.anti "${program}"
    DIRECTORIES out
    FAULT open:7:EACCES
    ARGS ${tools} --lto none -o out/main main.anti
    ERR "antic: cannot enter out/: Permission denied"
        "antic: llc failed")
anti_message(fault_llc_not_executed
    FILES main.anti "${program}"
    DIRECTORIES out
    FAULT run:3:EACCES
    ARGS ${tools} --lto none -o out/main main.anti
    ERR "antic: cannot run ${real_llc}: Permission denied"
        "antic: llc failed")
anti_message(fault_llc_path
    FILES main.anti "${program}"
    DIRECTORIES out
    FAULT stat:3:EACCES,stat:4:EACCES
    ARGS ${tools} --lto none -o out/main main.anti
    ERR "antic: llc failed")
anti_message(fault_llc_input_path
    FILES main.anti "${program}"
    DIRECTORIES out
    FAULT stat:4:EACCES,stat:5:EACCES
    ARGS ${tools} --lto none -o out/main main.anti
    ERR "antic: llc failed")
# Without --runtime antic takes the archive above the directory that holds
# it, platform_self_directory of platform.c: a copy in WORK/bin beside
# WORK/lib. The first stat asks the system for the path of antic and the
# second makes it absolute. Either one refused leaves antic without an
# archive.
function(lib_beside_the_copy)
    file(MAKE_DIRECTORY "${WORK}/lib")
endfunction()
anti_message(fault_self_path STATUS 2
    FILES main.anti "${program}"
    SETUP lib_beside_the_copy
    COPY
    ENV "XDG_DATA_HOME=${WORK}/data"
    FAULT stat:1:EACCES
    ARGS main.anti
    ERR "antic: linking needs --runtime <dir>, the directory that holds lib/<target>/<level>/")
anti_message(fault_self_directory STATUS 2
    FILES main.anti "${program}"
    SETUP lib_beside_the_copy
    COPY
    ENV "XDG_DATA_HOME=${WORK}/data"
    FAULT stat:2:EACCES
    ARGS main.anti
    ERR "antic: linking needs --runtime <dir>, the directory that holds lib/<target>/<level>/")
# The soname of a library of Linux is a link beside it, platform_symlink of
# platform.c. Its unlink is the third remove of the run and the link the
# twelfth open.
anti_message(fault_soname_not_unlinked
    FILES exp.anti "${exporting}"
    FAULT remove:3:EACCES
    ARGS ${tools} --target linux-x86_64 --lib shared --soname
         --package-version 1.2.3 -o libexp.so exp.anti
    ERR "antic: cannot write libexp.so")
anti_message(fault_soname_not_linked
    FILES exp.anti "${exporting}"
    FAULT open:12:EACCES
    ARGS ${tools} --target linux-x86_64 --lib shared --soname
         --package-version 1.2.3 -o libexp.so exp.anti
    ERR "antic: cannot write libexp.so")
# The digest of a plugin for its index, driver_write_plugin_index of
# driver_index.c, which opens the library as the sixteenth file.
if(NOT CMAKE_HOST_WIN32)
    anti_message(fault_plugin_unread
        DIRECTORIES net/example
        SETUP compile_the_interface
        FAULT open:16:EACCES
        ARGS ${tools} --lib shared --no-runtime -I "${plugins}"
             -I "${PROJECT}" -o libfancy.dylib
             "${plugins}/net/example/fancy.anti"
        ERR "antic: cannot read libfancy.dylib")
endif()
# The linker of the Mac, which xcrun names, platform_capture of
# platform.c and xcrun of driver_link.c. opt and llc run first.
if(CMAKE_HOST_APPLE)
    anti_message(fault_xcrun_no_pipe
        FILES main.anti "${program}"
        FAULT run:3:EACCES
        ARGS ${tools} --linker platform main.anti
        ERR "antic: pipe: Permission denied"
            "antic: xcrun --show-sdk-path failed")
    anti_message(fault_xcrun_not_started
        FILES main.anti "${program}"
        FAULT run:4:EACCES
        ARGS ${tools} --linker platform main.anti
        ERR "antic: cannot run xcrun: Permission denied"
            "antic: xcrun --show-sdk-path failed")
endif()
# A program that names frameworks links against Apple's SDK. The sysroot
# of the package holds none, and the listing of the SDKs of the Command
# Line Tools is the fifteenth open, apple_sdk of driver_link.c and
# apple_clt_sdk of applesdk.c. When it fails, the message names every
# framework and where the SDK comes from.
if(CMAKE_HOST_APPLE)
    anti_message(fault_frameworks_without_an_sdk
        FILES main.anti "${program}"
        FAULT open:15:EACCES
        ARGS ${tools} --framework CoreFoundation --framework Foundation
             main.anti
        ERR "antic: the frameworks CoreFoundation, Foundation link for ${HOST} against Apple's SDK, which no package holds and ${RUNTIME}/sysroot/${HOST}/sdk lacks. A Mac keeps the SDK in /Library/Developer/CommandLineTools/SDKs. Run anti sdk export on a Mac and anti sdk import here.")
endif()
# --bundle-runtime of a static library of the Mac lists the members of
# the runtime with llvm-ar, platform_capture of platform.c and
# extract_runtime of driver_library.c. opt and llc run twice before it.
if(CMAKE_HOST_APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/../tools/cpu-levels.cmake")
    antic_read_cpu_levels("${CMAKE_CURRENT_LIST_DIR}/../tools/cpu-levels")
    set(bundled "${RUNTIME}/lib/${HOST}/${ANTIC_CPU_LEVEL_${HOST}}/libanti_rt.a")
    anti_message(fault_bundle_no_pipe
        FILES exp.anti "${exporting}"
        FAULT run:5:EACCES
        ARGS ${tools} --lib static --bundle-runtime -o libexp.a exp.anti
        ERR "antic: pipe: Permission denied"
            "antic: cannot bundle ${bundled}")
    anti_message(fault_bundle_not_started
        FILES exp.anti "${exporting}"
        FAULT run:6:EACCES
        ARGS ${tools} --lib static --bundle-runtime -o libexp.a exp.anti
        ERR "antic: cannot run ${RUNTIME}/bin/llvm-ar: Permission denied"
            "antic: cannot bundle ${bundled}")
endif()
# The build id of a program digests the runtime library it links,
# build_id and digest_file of driver.c, with the fifth open and read of a
# release build. A library that does not open adds nothing to the digest,
# and the link that needs it reports it, so a link that then opens it
# succeeds. A library that opens and does not read stops the build.
if(CMAKE_HOST_APPLE)
    anti_message(fault_runtime_unopened_for_id STATUS 0
        FILES main.anti "${program}"
        FAULT open:5:EACCES
        ARGS ${tools} main.anti)
    anti_message(fault_runtime_unread_for_id
        FILES main.anti "${program}"
        FAULT read:5:EIO
        ARGS ${tools} main.anti
        ERR "antic: cannot read ${bundled}")
endif()
# The licence text of the runtime, which the notice of every program
# holds, archive_package of driver.c. The open of the text is the fourth
# of a release build. T151: antic says it cannot open the text and links
# the program all the same, with a notice that lacks it.
anti_message(fault_runtime_license_unread
    FILES main.anti "${program}"
    FAULT open:4:EACCES
    ARGS ${tools} main.anti
    ERR "antic: cannot open ${RUNTIME}/licenses/anti_rt.txt")
