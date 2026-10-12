# The messages of `anti bind`, one case each.
# tests/run_anti_messages.cmake states the form of a case.

# An API description that is no JSON. Each case is the text of the file
# and what the reader says of it.
string(REPEAT "[" 1100 deep)
foreach(case
        "string|{\"functions\": [\"abc\\q\"]}|a malformed string at line 1, column 16"
        "nul|{\"functions\": [\"a\\u0000b\"]}|a string that holds a NUL at line 1, column 26"
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
anti_message(api_enum_value STATUS 0
    FILES api.json "{\"functions\": [], \"enums\": [{\"name\": \"Mode\", \"values\": [{\"name\": \"MODE_A\", \"value\": \"first\"}, {\"name\": \"MODE_B\", \"value\": 1}]}]}"
    OUT "bind: anti.api from api.json, 0 functions, 1 warning"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: a value of the enum `Mode` is no integer")
anti_message(api_type_does_not_parse STATUS 0
    FILES api.json "{\"functions\": [{\"name\": \"f\", \"returnType\": \"void\", \"params\": [{\"name\": \"p\", \"type\": \"int (\"}]}]}"
    OUT "bind: anti.api from api.json, 0 functions, 2 warnings"
    ARGS bind api.json -o "${WORK}/out"
    ERR "anti: api.json: warning: the type `int (` does not parse"
        "anti: api.json: warning: `f` is left out: a type does not parse")
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
anti_message(clang_type_does_not_parse STATUS 0
    FILES api.h "void each(void (^block)(void));\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `each` is left out: its type `void (void (^)(void))` does not parse")
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
    anti_message(clang_missing COPY
        FILES api.h "int count(void);\n" empty/notes.txt "No clang.\n"
        ENV "PATH=${PROJECT}/empty"
        ARGS ${clang_bind}
        ERR "antic: cannot run clang: No such file or directory"
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
