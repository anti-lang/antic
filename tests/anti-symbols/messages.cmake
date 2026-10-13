# The messages of `anti symbols`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

anti_message(configuration_no_toml
    FILES conf.toml "[runtime\n"
    ARGS symbols inventory --conf conf.toml
    ERR "anti: conf.toml is no runtime configuration")

anti_message(configuration_not_there
    ARGS symbols inventory --conf none.toml
    ERR "anti: cannot read none.toml")
anti_message(configuration_include_not_there
    FILES conf.toml "include = [\"more.toml\"]\n"
    ARGS symbols inventory --conf conf.toml
    ERR "anti: cannot read ./more.toml")

# The lines of `inventory` and `check` for a configuration that names what
# is not there. They go to the standard output, and `inventory` writes its
# archive all the same.
anti_message(inventory_no_program
    STATUS 0
    FILES conf.toml "[runtime]\nthreads = 2\n"
    ARGS symbols inventory --conf conf.toml
    OUT "missing the program: . holds no program of Anti"
        "wrote symbols.zip with 0 of 1 modules")
anti_message(check_no_program
    FILES conf.toml "[runtime]\nthreads = 2\n"
    ARGS symbols check --conf conf.toml
    OUT "missing the program: . holds no program of Anti")
# A text of `plugins` holds its directories as `--anti.plugins` takes
# them, with `:` between two, and an empty one names nothing.
anti_message(inventory_plugins_as_a_text
    STATUS 0
    FILES conf.toml "[runtime]\nplugins = \"one:two::three\"\n"
    ARGS symbols inventory --conf conf.toml
    OUT "missing the program: . holds no program of Anti"
        "missing ./one/anti-plugins.toml: no index of plugins"
        "missing ./two/anti-plugins.toml: no index of plugins"
        "missing ./three/anti-plugins.toml: no index of plugins"
        "wrote symbols.zip with 0 of 4 modules")
# The file that includes wins per interface, so `app.Logger` names one
# library, and the line of the include alone names the other.
anti_message(inventory_injection_of_an_include
    STATUS 0
    FILES conf.toml
          "include = [\"more.toml\"]\n[injections]\n\"app.Logger\" = \"first.plugin\"\n"
          more.toml
          "[injections]\n\"app.Logger\" = \"second.plugin\"\n\"app.Clock\" = \"third.plugin\"\n"
    ARGS symbols inventory --conf conf.toml
    OUT "missing the program: . holds no program of Anti"
        "missing ./first.plugin: no such file"
        "missing ./third.plugin: no such file"
        "wrote symbols.zip with 0 of 3 modules")

# A symbols archive. The files of a case under archive/ are its entries.
function(pack_archive)
    file(GLOB entries RELATIVE "${PROJECT}/archive" "${PROJECT}/archive/*")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf
                            "${PROJECT}/symbols.zip" --format=zip ${entries}
                    WORKING_DIRECTORY "${PROJECT}/archive"
                    RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the archive was not written")
    endif()
endfunction()
set(resolve symbols resolve trace.txt --symbols symbols.zip)
anti_message(archive_no_zip
    FILES trace.txt "" symbols.zip "A text where the archive belongs.\n"
    ARGS ${resolve}
    ERR "anti: symbols.zip is no zip archive")
anti_message(archive_index_no_toml
    FILES trace.txt "" archive/index.toml "[unit\n"
    SETUP pack_archive
    ARGS ${resolve}
    ERR "anti: symbols.zip holds a broken index.toml")
anti_message(archive_no_build_id
    FILES trace.txt "" archive/notes.txt "No twin and no map.\n"
    SETUP pack_archive
    ARGS ${resolve}
    ERR "anti: symbols.zip holds no build id")

# An archive of one build whose map names the id. It holds no debug link.
set(id "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
set(map "# The map of a symbols archive of Anti.\n# build ${id}\n0000000000001000-0000000000002000 good src/good.anti:7\n")
set(module "module 0 ${id} 0x0000000000000000 prog")
set(frame "0x0000000000001001 0+0x1001")
anti_message(trace_not_there
    FILES archive/prog.map "${map}"
    SETUP pack_archive
    ARGS symbols resolve none.txt --symbols symbols.zip
    ERR "anti: cannot read none.txt")
anti_message(archive_map_no_build_line
    FILES trace.txt "" archive/prog.map
          "0000000000001000-0000000000002000 good src/good.anti:7\n"
    SETUP pack_archive
    ARGS ${resolve}
    ERR "anti: symbols.zip holds no build id")
# Each line leaves as it came in, without the CR of a line of Windows, and
# the last line gains the newline it lacks.
anti_message(trace_lines_of_windows
    STATUS 0
    FILES trace.txt "first\r\nsecond\r\nlast" archive/prog.map "${map}"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "first" "second" "last")
# The map fills what the debug link leaves open, so an archive without
# the link answers no frame.
anti_message(archive_map_alone
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${map}"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame}")
# An index holds one table per module. A key outside them and a key of a
# table before the first `module` line name nothing.
anti_message(archive_index_names_no_module
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n"
          archive/index.toml "other = 1\n[module.a]\nid = \"${id}\"\n"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame}")
