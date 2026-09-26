# The files and the flags of the PCRE2 library, which the build of the
# runtime tree and tools/pack-anti.cmake read, so that antic and the
# library of each target compile the same sources the same way. It sets
# variables alone, so a script run with cmake -P includes it as well.

# The sources of the 8-bit library, the list of NON-AUTOTOOLS-BUILD.
# pcre2_jit_compile.c is one of them with JIT off, when it holds the stubs
# of the JIT functions.
set(ANTIC_PCRE2_SOURCES auto_possess chkdint compile compile_cgroup
    compile_class config context convert dfa_match error extuni find_bracket
    jit_compile maketables match match_data match_next newline ord2utf
    pattern_info script_run serialize string_utils study substitute
    substring tables ucd valid_utf xclass)

# DESIGN: the generic config.h with four definitions and nothing else.
# PCRE2_CODE_UNIT_WIDTH=8 is the 8-bit library, SUPPORT_UNICODE the UTF
# support, and PCRE2_STATIC drops the dllimport of pcre2.h on Windows. JIT
# is off because SUPPORT_JIT is not defined. The limits stay the defaults
# of the release.
set(ANTIC_PCRE2_DEFINES -DHAVE_CONFIG_H -DPCRE2_CODE_UNIT_WIDTH=8
    -DSUPPORT_UNICODE -DPCRE2_STATIC)

# The warnings of PCRE2's own build, which src/native/warnings.cmake holds
# beside those of the other native libraries.
include("${CMAKE_CURRENT_LIST_DIR}/warnings.cmake")
