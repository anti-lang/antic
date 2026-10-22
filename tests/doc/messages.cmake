# The messages of `anti doc`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

# Without a file the command takes the source directory, which holds no
# module here. The sources of a project come from the one function that
# `anti check` and `anti fmt` take them from, M339 of the audit.
anti_message(no_source STATUS 2
    FILES src/notes.txt "No module.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}"
    ERR "anti: no .anti file under src")
foreach(items dev private)
    anti_message(${items}_of_a_library
        FILES one.antl "A library file, which holds the public interface alone.\n"
        ARGS doc --${items} -o "${WORK}/pages" --runtime "${RUNTIME}" one.antl
        ERR "anti: one.antl: --${items} needs the source")
endforeach()
# A library file is read as the driver of antic reads one.
anti_message(library_damaged
    FILES one.antl "A text where the library file belongs.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.antl
    ERR "antic: one.antl is not a library file")
# A source goes through the front end, and its first mistake ends the
# command. The second case shows T123 of docs/audit/tests-findings.md.
anti_message(source_name_no_identifier
    FILES Bad-Name.anti "//! A module whose file has no name of a module.\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" Bad-Name.anti
    ERR "anti: `Bad-Name` in Bad-Name.anti is not a lowercase identifier")
anti_message(source_does_not_parse
    FILES one.anti "fn main( -> int\n{\n\treturn 0;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "one.anti:1:10: error: expected identifier")
# Two modules that import each other have no order, by R125 of "Rules of
# 2026-10-13" in docs/decisions.md.
anti_message(modules_import_each_other
    FILES src/com/example/a.anti "//! The first.\n\nimport com.example.b;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.a;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}"
    ERR "anti: the imports form a cycle: com.example.a -> com.example.b -> com.example.a")
# R179 of "Rules of 2026-10-13" in docs/decisions.md: a fence that nothing
# closes gives a warning, and the page is written.
anti_message(fence_never_closed STATUS 0
    FILES one.anti "//! A module.\n\n/// One.\n/// ```anti\n/// let n = one();\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "doc: 1 module into ${WORK}/pages"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "anti: one.anti: warning: the doc comment of `one` holds a fence that nothing closes")
anti_message(module_fence_never_closed STATUS 0
    FILES one.anti "//! A module.\n//! ```\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "doc: 1 module into ${WORK}/pages"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "anti: one.anti: warning: the doc comment of the module holds a fence that nothing closes")
# A library file beside a source. The source gets its interface file in
# the work directory, and the library file is read as it stands.
anti_message(library_beside_a_source STATUS 0
    FILES one.anti "//! A module.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "doc: 2 modules into ${WORK}/pages"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}"
         "${RUNTIME}/std/anti/simd.antl" one.anti)
anti_message(source_imports_no_module
    FILES one.anti "//! A module.\n\nimport com.example.gone;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    ARGS doc -o "${WORK}/pages" --runtime "${RUNTIME}" one.anti
    ERR "one.anti:3:8: error: cannot find module `com.example.gone`")

# Fault injection: each case makes one call of the system fail in
# anti_faults, through the variable ANTI_FAULT of src/rt/fault.h. The
# pages go to a relative directory, so the count of its directories is the
# same on every machine.
set(fault_manifest "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n")
set(fault_library "//! The smallest library.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
anti_message(fault_page_not_written
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT open:9:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "anti: cannot write pages/com.example.one.html")
anti_message(fault_index_not_written
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT open:10:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "anti: cannot write pages/index.html")
# T153: the work directory beside the pages, which anti doc leaves without
# a message.
anti_message(fault_work_not_made
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT mkdir:2:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "anti: cannot make build/doc-work")
# T152: a library file that stops part of the way, library_interface of
# driver.c. The manifest takes the first two reads, and the fourth is the
# one that meets the end of the file. A build of the project writes the
# library file.
function(build_the_library)
    execute_process(COMMAND "${ANTI}" build --runtime "${RUNTIME}"
                    WORKING_DIRECTORY "${PROJECT}" RESULT_VARIABLE status
                    OUTPUT_QUIET ERROR_QUIET)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build of the library ended with ${status}")
    endif()
endfunction()
anti_message(fault_library_read_partway
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    SETUP build_the_library
    FAULT read:4:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}" dist/${HOST}/dev/com/example/one.antl
    ERR "antic: cannot read dist/${HOST}/dev/com/example/one.antl")
# The search for the imports of the library file reads it again,
# driver_find_libraries of driver_search.c, and the fifth read fails.
anti_message(fault_library_imports_unread
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    SETUP build_the_library
    FAULT read:5:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}" dist/${HOST}/dev/com/example/one.antl
    ERR "antic: cannot read dist/${HOST}/dev/com/example/one.antl")
# The load of the library file the search found, driver_load_libraries
# of driver_search.c, whose first read is the seventh.
anti_message(fault_library_unloaded
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    SETUP build_the_library
    FAULT read:7:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}" dist/${HOST}/dev/com/example/one.antl
    ERR "antic: cannot read dist/${HOST}/dev/com/example/one.antl")
# The listing of the source directory fails after it found the module,
# project_sources of main.c.
anti_message(fault_listing_unread
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT read:6:EIO
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "anti: cannot read src")
# T153: the directory of the library file of the module under the work
# directory, unit_file of units.c, which anti doc leaves without a
# message.
anti_message(fault_library_directory_not_made
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT mkdir:6:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "anti: cannot make build/doc-work/com/example")
# The source read by the compile of the library file, read_file of
# driver.c.
anti_message(fault_source_unopened
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT open:8:EACCES
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "antic: cannot open src/com/example/one.anti")
# The page of a module that imports another reads the library file of the
# import that the compile wrote before, interface_of of driver.c. The
# search for it reads it with the thirty-second read, and the load with
# the thirty-fourth.
set(fault_importer "//! Two.\n\nimport com.example.one;\n\n/// Two.\npub fn two() -> int\n{\n\treturn one.one() + 1;\n}\n")
anti_message(fault_page_import_unfound
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
          src/com/example/two.anti "${fault_importer}"
    FAULT read:32:EIO
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "antic: cannot read build/doc-work/com/example/one.antl")
anti_message(fault_page_import_unloaded
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
          src/com/example/two.anti "${fault_importer}"
    FAULT read:34:EIO
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "antic: cannot read build/doc-work/com/example/one.antl")

# The manifest, which names the source directory.
anti_message(manifest_no_toml
    FILES anti.toml "name = \n" src/com/example/one.anti "${fault_library}"
    ARGS doc -o pages --runtime "${RUNTIME}"
    ERR "anti: anti.toml is no TOML that anti reads")
# Without --runtime the pages take the runtime archive of the user's data
# directory, whose lib/ tells it from any other directory.
anti_message(runtime_of_the_data_directory STATUS 0
    FILES anti.toml "${fault_manifest}" src/com/example/one.anti "${fault_library}"
    DIRECTORIES data/anti/lib
    ENV "XDG_DATA_HOME=${PROJECT}/data" "LOCALAPPDATA=${PROJECT}/data"
    OUT "doc: 1 module into pages"
    ARGS doc -o pages)
