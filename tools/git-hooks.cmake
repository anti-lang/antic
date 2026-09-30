# The git hooks of the repository stand in ANTI_HOOKS_DIR, relative to the
# top of the checkout. The configure step points core.hooksPath of the
# checkout at it, so every clone runs them without a step of its own.
#
# DESIGN: the path is relative. git resolves a relative core.hooksPath
# against the top of the work tree it commits in, so one setting serves
# the checkout and every work tree of it wherever they stand. A tree that
# is no git checkout, such as the export the suite of ./r runs in, is left
# alone.

set(ANTI_HOOKS_DIR tools/hooks)

find_program(ANTI_GIT git)
if(ANTI_GIT)
    execute_process(COMMAND "${ANTI_GIT}" -C "${PROJECT_SOURCE_DIR}"
                        rev-parse --show-toplevel
                    OUTPUT_VARIABLE anti_git_top OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE anti_not_git ERROR_QUIET)
    if(NOT anti_not_git)
        get_filename_component(anti_git_top "${anti_git_top}" REALPATH)
        get_filename_component(anti_source_top "${PROJECT_SOURCE_DIR}" REALPATH)
    endif()
    if(NOT anti_not_git AND anti_git_top STREQUAL anti_source_top)
        execute_process(COMMAND "${ANTI_GIT}" -C "${PROJECT_SOURCE_DIR}"
                            config core.hooksPath
                        OUTPUT_VARIABLE anti_hooks_path
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT anti_hooks_path STREQUAL ANTI_HOOKS_DIR)
            execute_process(COMMAND "${ANTI_GIT}" -C "${PROJECT_SOURCE_DIR}"
                                config core.hooksPath "${ANTI_HOOKS_DIR}"
                            RESULT_VARIABLE anti_hooks_set)
            if(NOT anti_hooks_set EQUAL 0)
                message(FATAL_ERROR "git config core.hooksPath ${ANTI_HOOKS_DIR} failed")
            endif()
            message(STATUS "core.hooksPath: ${ANTI_HOOKS_DIR}, was '${anti_hooks_path}'")
        endif()
    endif()
endif()
