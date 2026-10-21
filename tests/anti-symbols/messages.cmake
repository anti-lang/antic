# The messages of `anti symbols`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

anti_message(configuration_no_toml
    FILES conf.toml "[runtime\n"
    ARGS symbols inventory --conf conf.toml
    ERR "anti: conf.toml is no runtime configuration")

anti_message(configuration_not_there
    ARGS symbols inventory --conf none.toml
    ERR "anti: cannot read none.toml")
# The directory of a configuration in the root directory is `/`.
anti_message(configuration_in_the_root_not_there
    ARGS symbols check --conf /nowhere.toml
    ERR "anti: cannot read /nowhere.toml")
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
# A program is a file with the header of an executable and a notice that
# names its build. The notice of the first ends in the line of its
# package, without a newline, and that of the second in a line of an
# attribution. The build id of the third holds a letter that is no digit
# of base sixteen, so the file is no program of Anti.
set(notice_id "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
set(notice_head "MZ\nANTI_LICENSES_BEGIN\nbuild ")
anti_message(check_notice_ends_in_its_package_line
    FILES conf.toml "[runtime]\nthreads = 2\n"
          prog "${notice_head}${notice_id}\npackage com.example.app 1.0"
    ARGS symbols check --conf conf.toml
    OUT "missing ./prog ${notice_id}")
anti_message(check_notice_ends_in_an_attribution_line
    FILES conf.toml "[runtime]\nthreads = 2\n"
          prog "${notice_head}${notice_id}\npackage com.example.app 1.0\nattribution Example"
    ARGS symbols check --conf conf.toml
    OUT "missing ./prog ${notice_id}")
anti_message(check_build_id_with_another_letter
    FILES conf.toml "[runtime]\nthreads = 2\n"
          prog "${notice_head}0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg\npackage com.example.app 1.0 MIT\n"
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
# R183 of "Rules of 2026-10-13" in docs/decisions.md: an archive without
# the debug link answers a frame through its map. The offset of a frame of
# Linux counts from address 0.
anti_message(archive_map_alone
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${map}"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame} good src/good.anti:7")
# The hexadecimal digits of a frame read in either case.
set(capital_frame "0x0000000000001A0F 0+0x1A0F")
anti_message(archive_map_alone_with_capital_digits
    STATUS 0
    FILES trace.txt "${module}\n${capital_frame}\n" archive/prog.map "${map}"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${capital_frame} good src/good.anti:7")
# The offset of a frame of macOS counts from the text, whose address the
# map of a Mach-O program names.
set(macho_map "# The map of a symbols archive of Anti.\n# build ${id}\n# target macos-arm64\n# text 0000000100000000\n0000000100001000-0000000100002000 good src/good.anti:7\n")
anti_message(archive_map_alone_of_macos
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${macho_map}"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame} good src/good.anti:7")
# An index holds one table per module. By R184 of "Rules of 2026-10-13"
# in docs/decisions.md a key outside them refuses the index. A key of a
# table before the first `module` line names nothing.
anti_message(archive_index_key_outside_the_modules
    FILES trace.txt "${module}\n${frame}\n"
          archive/index.toml "other = 1\n[module.a]\nmodule = \"a\"\nid = \"${id}\"\n"
    SETUP pack_archive
    ARGS ${resolve}
    ERR "anti: symbols.zip holds the key other in index.toml outside a module table")
anti_message(archive_index_names_no_module
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n"
          archive/index.toml "[module.a]\nid = \"${id}\"\n"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame}")

# The address of the text holds the letters of base sixteen, and a frame
# of macOS counts from it, map_text of syms.c.
anti_message(archive_map_text_of_letters
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map
          "# The map of a symbols archive of Anti.\n# build ${id}\n# target macos-arm64\n# text 00000001000a0000\n00000001000a1000-00000001000a2000 good src/good.anti:7\n"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame} good src/good.anti:7")
# A text line whose address holds a byte that is no digit of base sixteen
# names no text, and the offset of a frame counts from address 0.
anti_message(archive_map_text_not_hexadecimal
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map
          "# The map of a symbols archive of Anti.\n# build ${id}\n# target macos-arm64\n# text 000000010000000g\n0000000000001000-0000000000002000 good src/good.anti:7\n"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame} good src/good.anti:7")
# The last line of a map lacks its newline and ends in blanks after the
# location, syms_map_lookup of syms.c.
anti_message(archive_map_last_line_unended
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map
          "# The map of a symbols archive of Anti.\n# build ${id}\n0000000000001000-0000000000002000 good src/good.anti:7  "
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame} good src/good.anti:7")
# A frame outside every line of the map stays as it came in.
anti_message(archive_map_misses_the_frame
    STATUS 0
    FILES trace.txt "${module}\n0x0000000000005001 0+0x5001\n"
          archive/prog.map "${map}"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "0x0000000000005001 0+0x5001")
# A debug link that is no program answers no frame, resolve_frame of
# syms.c, so the frame stays as it came in.
anti_message(archive_debug_link_no_program
    STATUS 0
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${map}"
          archive/prog.debug "Not a program.\n"
    SETUP pack_archive
    ARGS ${resolve}
    OUT "${module}" "${frame}")
# `check` with an archive that holds another build reports the program
# missing, state_of of syms.c.
anti_message(check_archive_of_another_build
    FILES conf.toml "[runtime]\nthreads = 2\n"
          prog "${notice_head}${notice_id}\npackage com.example.app 1.0\n"
          archive/other.map "# The map of a symbols archive of Anti.\n# build 1123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n0000000000001000-0000000000002000 good src/good.anti:7\n"
    SETUP pack_archive
    ARGS symbols check --conf conf.toml --symbols symbols.zip
    OUT "missing ./prog ${notice_id}")
anti_message(check_archive_no_zip
    FILES conf.toml "[runtime]\nthreads = 2\n" symbols.zip "A text where the archive belongs.\n"
    ARGS symbols check --conf conf.toml --symbols symbols.zip
    ERR "anti: symbols.zip is no zip archive")
# A library of `[injections]` named by an absolute path, and one that is
# no binary of Anti.
anti_message(check_injection_absolute_and_no_binary
    FILES conf.toml "[injections]\n\"app.Logger\" = \"${PROJECT}/none.plugin\"\n\"app.Clock\" = \"first.plugin\"\n"
          first.plugin "A text where a library belongs.\n"
    ARGS symbols check --conf conf.toml
    OUT "missing the program: . holds no program of Anti"
        "missing ${PROJECT}/none.plugin: no such file"
        "missing ./first.plugin: it carries no build id of Anti")
# Two files that include each other, named by the same path.
anti_message(configuration_include_cycle
    FILES conf.toml "include = [\"more.toml\"]\n" more.toml "include = [\"conf.toml\"]\n"
    ARGS symbols inventory --conf ./conf.toml
    ERR "anti: the configuration files include one another at ./conf.toml")

# Fault injection: each case makes one call of the system fail in
# anti_faults, through the variable ANTI_FAULT of src/rt/fault.h. A count
# names the call among those of its kind in the run. `inventory` and
# `check` open the configuration, then the directory beside it, and
# `resolve` reads the archive and then the trace.
anti_message(fault_inventory_directory_unread
    STATUS 0
    FILES conf.toml "[runtime]\nthreads = 2\n"
    FAULT open:2:EACCES
    ARGS symbols inventory --conf conf.toml
    ERR "anti: cannot read ."
    OUT "missing the program: . holds no program of Anti"
        "wrote symbols.zip with 0 of 1 modules")
anti_message(fault_check_directory_unread
    FILES conf.toml "[runtime]\nthreads = 2\n"
    FAULT open:2:EACCES
    ARGS symbols check --conf conf.toml
    ERR "anti: cannot read ."
    OUT "missing the program: . holds no program of Anti")
anti_message(fault_inventory_archive_not_written
    FILES conf.toml "[runtime]\nthreads = 2\n"
    FAULT write:1:ENOSPC
    ARGS symbols inventory --conf conf.toml
    ERR "anti: cannot write symbols.zip"
    OUT "missing the program: . holds no program of Anti")
anti_message(fault_resolve_archive_unread
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${map}"
    SETUP pack_archive
    FAULT open:1:EACCES
    ARGS ${resolve}
    ERR "anti: cannot read symbols.zip")
anti_message(fault_resolve_archive_read_partway
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${map}"
    SETUP pack_archive
    FAULT read:2:EIO
    ARGS ${resolve}
    ERR "anti: cannot read symbols.zip")
anti_message(fault_resolve_trace_read_partway
    FILES trace.txt "${module}\n${frame}\n" archive/prog.map "${map}"
    SETUP pack_archive
    FAULT read:3:EIO
    ARGS ${resolve}
    ERR "anti: cannot read trace.txt")
