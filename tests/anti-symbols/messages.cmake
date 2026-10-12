# The messages of `anti symbols`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

anti_message(configuration_no_toml
    FILES conf.toml "[runtime\n"
    ARGS symbols inventory --conf conf.toml
    ERR "anti: conf.toml is no runtime configuration")

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
