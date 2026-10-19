# The one reader of tools/cpu-levels, the processor levels of antic and
# the default level of each of its targets. The build, the packer,
# tools/check-cpu.cmake and the tests include this file, and the test
# pin_readers refuses the table read anywhere else. The default rows are
# the one list of the six targets, which the test target_lists holds.
#
# antic_read_cpu_levels(<file>) sets, in the scope of the caller:
#   ANTIC_TARGETS              the targets, in the order of the default rows
#   ANTIC_CPU_LEVEL_<target>   the default level of <target>
#   ANTIC_CPU_LEVELS_<arch>    the levels of x86_64 or arm64, lowest first
#   ANTIC_CPU_MARCH_<level>    the -march= value of <level>
#   ANTIC_CPU_ID_<level>       the number of <level>

macro(antic_read_cpu_levels antic_cpu_file)
    file(STRINGS "${antic_cpu_file}" antic_cpu_rows REGEX "^(level|default) ")
    set(ANTIC_TARGETS "")
    set(ANTIC_CPU_LEVELS_x86_64 "")
    set(ANTIC_CPU_LEVELS_arm64 "")
    foreach(antic_cpu_row IN LISTS antic_cpu_rows)
        string(REPLACE " " ";" antic_cpu_fields "${antic_cpu_row}")
        list(GET antic_cpu_fields 0 antic_cpu_kind)
        list(GET antic_cpu_fields 1 antic_cpu_key)
        if(antic_cpu_kind STREQUAL "level")
            list(GET antic_cpu_fields 2 antic_cpu_arch)
            list(GET antic_cpu_fields 3 "ANTIC_CPU_ID_${antic_cpu_key}")
            list(GET antic_cpu_fields 4 "ANTIC_CPU_MARCH_${antic_cpu_key}")
            list(APPEND "ANTIC_CPU_LEVELS_${antic_cpu_arch}" "${antic_cpu_key}")
        else()
            list(GET antic_cpu_fields 2 "ANTIC_CPU_LEVEL_${antic_cpu_key}")
            list(APPEND ANTIC_TARGETS "${antic_cpu_key}")
        endif()
    endforeach()
    if(ANTIC_TARGETS STREQUAL "")
        message(FATAL_ERROR "${antic_cpu_file} names no target")
    endif()
endmacro()
