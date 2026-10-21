# The messages of `anti bind`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

# An API description that is no JSON. Each case is the text of the file
# and what the reader says of it.
string(REPEAT "[" 1100 deep)
foreach(case
        "string|{\"functions\": [\"abc\\q\"]}|a malformed string at line 1, column 16"
        "nul|{\"functions\": [\"a\\u0000b\"]}|a string that holds a NUL at line 1, column 26"
        "key|{\"func\\qtions\": []}|a malformed string at line 1, column 2"
        "colon|{\"functions\" []}|no `:` after a member name at line 1, column 14"
        "brace|{\"functions\": [] |an object without its `}` at line 1, column 18"
        "bracket|{\"functions\": [1 2]}|an array without its `]` at line 1, column 18"
        "deep|${deep}|values nested too deep at line 1, column 1026"
        "end|{\"functions\": |the end of the input where a value belongs at line 1, column 15"
        "value|{\"functions\": ?}|no JSON value at line 1, column 15"
        "after|{\"functions\": []} x|bytes after the value at line 1, column 19")
    string(REGEX MATCH "^([^|]*)[|](.*)[|]([^|]*)$" parts "${case}")
    anti_message(json_${CMAKE_MATCH_1}
        FILES api.json "${CMAKE_MATCH_2}"
        ARGS bind api.json -o "${WORK}/out"
        ERR "anti: api.json is not JSON: ${CMAKE_MATCH_3}")
endforeach()

# An API description of rlparser that the reader refuses.
anti_message(api_no_functions
    FILES api.json "{\"structs\": []}"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json holds no API description of rlparser")
anti_message(api_struct_name
    FILES api.json "{\"functions\": [], \"structs\": [{\"name\": \"2bad\", \"fields\": []}]}"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json names the struct `2bad`, which is no C identifier")
set(field "{\"name\": \"x\", \"type\": \"int\"}")
anti_message(api_struct_twice
    FILES api.json "{\"functions\": [], \"structs\": [{\"name\": \"A\", \"fields\": [${field}]}, {\"name\": \"A\", \"fields\": [${field}]}]}"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json names the struct `A` twice")

# An API description that binds with a warning.
# A part that is `null` is a part the description lacks.
anti_message(api_part_is_null STATUS 0
    FILES api.json "{\"functions\": [], \"structs\": null}"
    OUT "bind: anti.api from api.json, 0 functions, 0 warnings"
    ARGS bind api.json -o "${WORK}/out")
# A description whose file name has no suffix gives the module its whole
# name.
anti_message(api_without_a_suffix STATUS 0
    FILES api "{\"functions\": [{\"name\": \"count\", \"returnType\": \"int\", \"params\": []}]}"
    OUT "bind: anti.api from api, 1 functions, 0 warnings"
    ARGS bind api -o "${WORK}/out")
anti_message(api_enum_value STATUS 0
    FILES api.json "{\"functions\": [], \"enums\": [{\"name\": \"Mode\", \"values\": [{\"name\": \"MODE_A\", \"value\": \"first\"}, {\"name\": \"MODE_B\", \"value\": 1}]}]}"
    OUT "bind: anti.api from api.json, 0 functions, 1 warning"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: a value of the enum `Mode` is no integer")
# R170 of "Rules of 2026-10-13" in docs/decisions.md: a type that does not
# parse gives one warning, which names the type and what is left out.
anti_message(api_type_does_not_parse STATUS 0
    FILES api.json "{\"functions\": [{\"name\": \"f\", \"returnType\": \"void\", \"params\": [{\"name\": \"p\", \"type\": \"int (\"}]}]}"
    OUT "bind: anti.api from api.json, 0 functions, 1 warning"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: `f` is left out: the type `int (` does not parse")
# R171 of "Rules of 2026-10-13" in docs/decisions.md: a field without its
# type gives a warning. By R172 a struct that lost a field is refused,
# since its layout would differ from that of C.
anti_message(api_field_without_a_type
    FILES api.json "{\"functions\": [], \"structs\": [{\"name\": \"Half\", \"fields\": [{\"name\": \"x\"}, {\"name\": \"y\", \"type\": \"int\"}]}]}"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: the field `x` of `Half` has no type"
        "anti: api.json: the struct `Half` lost a field, so its layout would differ from that of C")
anti_message(api_field_without_a_name
    FILES api.json "{\"functions\": [], \"structs\": [{\"name\": \"Half\", \"fields\": [{\"type\": \"int\"}, {\"name\": \"y\", \"type\": \"int\"}]}]}"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: the struct `Half` lost a field, so its layout would differ from that of C")
anti_message(api_field_that_is_no_object
    FILES api.json "{\"functions\": [], \"structs\": [{\"name\": \"Half\", \"fields\": [3, {\"name\": \"y\", \"type\": \"int\"}]}]}"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: the struct `Half` lost a field, so its layout would differ from that of C")
anti_message(api_type_names_itself STATUS 0
    FILES api.json "{\"functions\": [{\"name\": \"f\", \"returnType\": \"void\", \"params\": [{\"name\": \"p\", \"type\": \"Loop\"}]}], \"aliases\": [{\"name\": \"Loop\", \"type\": \"Loop\"}]}"
    OUT "bind: anti.api from api.json, 0 functions, 2 warnings"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: the type `Loop` names itself"
        "anti: api.json: warning: `f` is left out: Anti has no form for the type of a parameter")
anti_message(api_struct_without_a_field STATUS 0
    FILES api.json "{\"functions\": [{\"name\": \"f\", \"returnType\": \"void\", \"params\": [{\"name\": \"p\", \"type\": \"Empty *\"}]}], \"structs\": [{\"name\": \"Empty\", \"fields\": []}]}"
    OUT "bind: anti.api from api.json, 1 functions, 1 warning"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: `Empty` has no field and is left out")
anti_message(api_constant_of_a_struct_left_out STATUS 0
    FILES api.json "{\"functions\": [], \"structs\": [{\"name\": \"Pix\", \"fields\": [{\"name\": \"r\", \"type\": \"unsigned char\"}, {\"name\": \"z\", \"type\": \"__int128\"}]}], \"defines\": [{\"name\": \"BLACK\", \"type\": \"COLOR\", \"value\": \"CLITERAL(Pix){ 0, 0 }\"}]}"
    OUT "bind: anti.api from api.json, 0 functions, 2 warnings"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: `Pix` is left out: Anti has no form for the type of its field `z`"
        "anti: api.json: warning: `BLACK` is left out: its struct is left out")

# A header that clang reads and the reader of its AST refuses.
set(clang_bind bind --clang api.h -o "${WORK}/out" --runtime "${RUNTIME}")
# S333. A name of a header that is no C identifier is refused, as a name
# of a description is. clang takes `$` in an identifier. Each case is its
# name, the header and the kind the message names.
foreach(case
        "struct|struct a$b { int x; };\nvoid take(struct a$b *p);\n|struct"
        "union|union a$b { int x; };\nvoid take(union a$b *p);\n|union"
        "typedef|typedef struct { int x; } a$b;\nvoid take(a$b *p);\n|struct"
        "field|struct S { int a$b; };\nvoid take(struct S *p);\n|field"
        "enum|enum a$b { E0 };\nvoid take(enum a$b e);\n|enum"
        "enum_typedef|typedef enum { E0 } a$b;\nvoid take(a$b e);\n|enum"
        "value|enum E { a$b };\nvoid take(enum E e);\n|value"
        "function|int a$b(void);\n|function"
        "parameter|int f(int a$b);\n|parameter"
        "macro|#define a$b 1\n|macro")
    string(REGEX MATCH "^([^|]*)[|](.*)[|]([^|]*)$" parts "${case}")
    anti_message(clang_name_${CMAKE_MATCH_1}
        FILES api.h "${CMAKE_MATCH_2}"
        ARGS ${clang_bind}
        ERR "anti: api.h names the ${CMAKE_MATCH_3} `a$b`, which is no C identifier")
endforeach()
anti_message(clang_field_aligned
    FILES api.h "struct Wide { int a; _Alignas(16) int b; };\nvoid take(struct Wide *w);\n"
    ARGS ${clang_bind}
    ERR "anti: api.h: `Wide` aligns a field other than its first, which Anti has no form for")
anti_message(clang_packed_to_two
    FILES api.h "#pragma pack(push, 2)\nstruct PackTwo { char a; int b; };\n#pragma pack(pop)\nvoid take(struct PackTwo *p);\n"
    ARGS ${clang_bind}
    ERR "anti: api.h: `PackTwo` is packed to more than one byte, which Anti has no form for")
anti_message(clang_aligned_without_a_value
    FILES api.h "struct __attribute__((aligned)) Most { int a; };\nvoid take(struct Most *m);\n"
    ARGS ${clang_bind}
    ERR "anti: api.h: `Most` is aligned without a value")
anti_message(clang_target
    FILES api.h "int count(void);\n"
    ARGS ${clang_bind} --target plan9-x86_64
    ERR "anti: unknown target plan9-x86_64")
anti_message(clang_no_sysroot
    FILES api.h "int count(void);\n" archive/notes.txt "No sysroot.\n"
    ARGS bind --clang api.h -o "${WORK}/out" --target linux-x86_64 --runtime archive
    ERR "anti: the runtime archive holds no sysroot for linux-x86_64, whose headers clang reads. Pass --runtime.")
# clang writes its own lines on the header before the one of anti.
anti_message(clang_refuses_the_header
    FILES api.h "int count(;\n"
    ARGS ${clang_bind}
    ERR_MATCH "api\\.h:1:11: error: [^\n]*\n(.*\n)*anti: clang could not read the header as C, status 1\n")

# A header that binds with a warning.
anti_message(clang_anonymous_member STATUS 0
    FILES api.h "struct Mixed { int tag; union { int a; float b; }; };\nvoid take(struct Mixed *m);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: an anonymous member of `Mixed` is the field `anon0`")
# _BitInt is C23, which clang reads for every target. A block of Apple
# would do on macOS alone.
anti_message(clang_type_does_not_parse STATUS 0
    FILES api.h "void each(_BitInt(37) n);\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `each` is left out: its type `void (_BitInt(37))` does not parse")
# T098: the function gets a second warning, which names its result.
anti_message(clang_static_without_a_body STATUS 0
    FILES api.h "static int hidden(void);\nint shown(void);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `hidden` is static without a body and is left out")
anti_message(clang_macro_is_a_word STATUS 0
    FILES api.h "#define defer 3\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: the macro `defer` is a word of Anti and is skipped")
anti_message(clang_macro_is_no_constant STATUS 0
    FILES api.h "#define NOW clock()\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: the macro `NOW` is no constant and is skipped")
anti_message(clang_variable STATUS 0
    FILES api.h "extern int counter;\nint count(void);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: the variable `counter` is skipped")
anti_message(clang_field_type STATUS 0
    FILES api.h "struct Odd { _Complex double z; };\nvoid take(struct Odd *o);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `Odd` is left out: Anti has no form for the type of its field `z`")
# A static function whose result is a pointer to a function. The shim
# cannot spell its result, so the function is left out (T167).
anti_message(clang_static_returns_a_function STATUS 0
    FILES api.h "static void (*pick(void))(int) { return 0; }\nint count(int n);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `pick` is left out: a shim cannot write a function that returns a function")
# A flexible array member has no length, so Anti has no form for it.
anti_message(clang_flexible_array STATUS 0
    FILES api.h "struct Buf { int n; int data[]; };\nvoid take(struct Buf *b);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `Buf` is left out: Anti has no form for the type of its field `data`")

# A clang that anti was not tested against. The copy of anti takes the
# first clang on PATH, which a shell script stands in for.
if(NOT CMAKE_HOST_WIN32)
    string(REGEX MATCH "^[0-9]+" clang_major "${CLANG_VERSION}")
    set(clang_where "bind --clang needs clang ${clang_major}. Get clang-${CLANG_TAG}-${HOST}.tar.xz from ${CLANG_PAGE}, or another clang ${clang_major}, and put it first on PATH.")
    function(write_clang text)
        file(WRITE "${WORK}/fake/clang" "#!/bin/sh\n${text}")
        file(CHMOD "${WORK}/fake/clang"
             PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    endfunction()
    function(clang_of_another_version)
        write_clang("echo 'Homebrew clang version 19.1.7'\necho 'Target: x'\n")
    endfunction()
    # --version gives the pinned version, and the calls after it give the
    # text of the case: the resource directory, the header after the
    # preprocessor and the AST.
    function(clang_without_resources)
        write_clang("for a in \"$@\"; do\n    case \"$a\" in\n    --version) echo 'clang version ${CLANG_VERSION}'; exit 0 ;;\n    esac\ndone\nexit 1\n")
    endfunction()
    function(clang_with_a_cut_ast)
        write_clang("for a in \"$@\"; do\n    case \"$a\" in\n    --version) echo 'clang version ${CLANG_VERSION}'; exit 0 ;;\n    -print-resource-dir) echo /nowhere; exit 0 ;;\n    -E) exit 0 ;;\n    esac\ndone\necho '{\"kind\": \"TranslationUnitDecl\", \"inner\": ['\n")
    endfunction()
    # R163 of "Rules of 2026-10-13" in docs/decisions.md: a program anti
    # cannot start is reported with the name anti.
    anti_message(clang_missing COPY
        FILES api.h "int count(void);\n" empty/notes.txt "No clang.\n"
        ENV "PATH=${PROJECT}/empty"
        ARGS ${clang_bind}
        ERR "anti: cannot run clang: No such file or directory"
            "anti: `clang --version` failed. ${clang_where}")
    anti_message(clang_of_another_version COPY
        FILES api.h "int count(void);\n"
        SETUP clang_of_another_version
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        ARGS ${clang_bind}
        ERR "anti: clang is clang 19, and ${clang_where}")
    anti_message(clang_without_resources COPY
        FILES api.h "int count(void);\n"
        SETUP clang_without_resources
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        ARGS ${clang_bind} --target linux-x86_64
        ERR "anti: clang -print-resource-dir failed")
    anti_message(clang_with_a_cut_ast COPY
        FILES api.h "int count(void);\n"
        SETUP clang_with_a_cut_ast
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        ARGS ${clang_bind}
        ERR "anti: the AST of clang is not JSON: the end of the input where a value belongs at line 2, column 1")
endif()

# A macro of one float whose value %g writes with an exponent above zero,
# and a cast of a negative number. Each is a constant expression, so the
# header binds without a warning. T119 and T120 of
# docs/audit/tests-findings.md.
anti_message(clang_float_with_a_large_exponent STATUS 0
    FILES api.h "#define API_H\n#define BIG 1e20\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: the macro `API_H` has no value and is skipped")
anti_message(clang_cast_of_a_negative_number STATUS 0
    FILES api.h "#define API_H\n#define LOW ((int)-3)\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: the macro `API_H` has no value and is skipped")

# `bind --header` reads a library file.
anti_message(header_of_a_damaged_library
    FILES one.antl "A text where the library file belongs.\n"
    ARGS bind --header one.antl --runtime "${RUNTIME}"
    ERR "anti: one.antl: is not a library file")

# Fault injection: each case makes one call of the system fail in
# anti_faults, through the variable ANTI_FAULT of src/rt/fault.h. The
# output goes to a relative directory, so the count of its directories is
# the same on every machine.
set(fault_api "{\"functions\": [], \"structs\": [{\"name\": \"Vec\", \"fields\": [{\"name\": \"x\", \"type\": \"float\"}]}]}")
anti_message(fault_module_not_written
    FILES api.json "${fault_api}"
    FAULT open:2:EACCES
    ARGS bind api.json -o ../out
    ERR "anti: cannot write ../out/api.anti")
anti_message(fault_probe_c_not_written
    FILES api.json "${fault_api}"
    FAULT open:3:EACCES
    ARGS bind api.json --probe -o ../out
    ERR "anti: cannot write ../out/probe_api.c")
anti_message(fault_probe_not_written
    FILES api.json "${fault_api}"
    FAULT open:4:EACCES
    ARGS bind api.json --probe -o ../out
    ERR "anti: cannot write ../out/probe_api.anti")
# T153: the output directory, which anti bind leaves without a message.
anti_message(fault_out_not_made
    FILES api.json "${fault_api}"
    FAULT mkdir:2:EACCES
    ARGS bind api.json -o ../out
    ERR "anti: cannot make ../out")
# T152: the library file of anti bind --header stops part of the way,
# driver_library_header of driver.c. The fourth read meets the end of
# the file. A build of the project writes the library file.
function(build_the_library)
    execute_process(COMMAND "${ANTI}" build --runtime "${RUNTIME}"
                    WORKING_DIRECTORY "${PROJECT}" RESULT_VARIABLE status
                    OUTPUT_QUIET ERROR_QUIET)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build of the library ended with ${status}")
    endif()
endfunction()
anti_message(fault_header_read_partway
    FILES anti.toml "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n"
          src/com/example/one.anti "//! The smallest library.\n\n/// One.\nexport fn one() -> c_int\n{\n\treturn 1;\n}\n"
    SETUP build_the_library
    FAULT read:4:EACCES
    ARGS bind --header dist/${HOST}/dev/com/example/one.antl -o ../out
    ERR "antic: cannot read dist/${HOST}/dev/com/example/one.antl")
# The header of the library file, unit_library_header of units.c, whose
# first read is the one that fails.
anti_message(fault_header_unread
    FILES anti.toml "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n"
          src/com/example/one.anti "//! The smallest library.\n\n/// One.\nexport fn one() -> c_int\n{\n\treturn 1;\n}\n"
    SETUP build_the_library
    FAULT read:1:EACCES
    ARGS bind --header dist/${HOST}/dev/com/example/one.antl -o ../out
    ERR "anti: cannot read dist/${HOST}/dev/com/example/one.antl")
