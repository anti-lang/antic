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
# A pop with nothing pushed leaves the packing of the target, so the struct
# and the function that takes it are bound. The pragma before it keeps the
# warning of clang on the empty stack out of the standard error.
anti_message(clang_pack_pop_of_an_empty_stack STATUS 0
    FILES api.h "#pragma clang diagnostic ignored \"-Wignored-pragmas\"\n#pragma pack(pop)\nstruct Plain { char a; int b; };\nvoid take(struct Plain *p);\n"
    OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
    ARGS ${clang_bind})
# An empty struct, a GNU extension of C, has no list of fields in the AST
# of clang at all.
anti_message(clang_struct_without_a_field STATUS 0
    FILES api.h "struct Empty {};\nvoid take(struct Empty *e);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `Empty` has no field and is left out")
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
# No --runtime, and no runtime archive beside the anti of the tree.
anti_message(clang_no_runtime
    FILES api.h "int count(void);\n"
    ARGS bind --clang api.h -o "${WORK}/out" --target linux-x86_64
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
# An enum with the fixed base `__int128`, whose values leave 64 bits. Anti
# has no integer of 128 bits, so the enum and the function that takes it
# are left out (T170).
anti_message(clang_enum_of_128_bits STATUS 0
    FILES api.h "enum Huge : __int128 { BIG = (__int128)1 << 70 };\nvoid take(enum Huge h);\nint count(void);\n"
    OUT "bind: anti.api from api.h, 1 functions, 2 warnings"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `Huge` is left out: Anti has no form for its base `__int128`"
        "anti: api.h: warning: `take` is left out: Anti has no form for the type of a parameter")
# A `va_list` is a pointer as a parameter alone, so a field of one has no
# form.
anti_message(clang_va_list_field STATUS 0
    FILES api.h "#include <stdarg.h>\nstruct Args { va_list ap; };\nvoid take(struct Args *a);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `Args` is left out: Anti has no form for the type of its field `ap`")
# A flexible array member has no length, so Anti has no form for it.
anti_message(clang_flexible_array STATUS 0
    FILES api.h "struct Buf { int n; int data[]; };\nvoid take(struct Buf *b);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `Buf` is left out: Anti has no form for the type of its field `data`")

# A tag that a parameter declares first is known to that function alone.
# A struct of that kind is opaque behind its pointer, and an enum has no
# form. The pragma keeps the warning of clang out of the standard error.
set(visible "#pragma clang diagnostic ignored \"-Wvisibility\"\n")
anti_message(clang_struct_declared_in_a_parameter STATUS 0
    FILES api.h "${visible}void take(struct Nope *p);\n"
    OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
    ARGS ${clang_bind})
anti_message(clang_enum_declared_in_a_parameter STATUS 0
    FILES api.h "${visible}void take(enum Nope e);\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `take` is left out: Anti has no form for the type of a parameter")
# An enum without a tag is constants alone, so a field of that type has no
# type of the binding, also after a field of an anonymous union, which binds
# as `S_u`.
anti_message(clang_field_of_an_anonymous_enum STATUS 0
    FILES api.h "struct S { union { int a; float b; } u; enum { A1, B1 } e; };\nvoid take(struct S *s);\n"
    OUT "bind: anti.api from api.h, 1 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `S` is left out: Anti has no form for the type of its field `e`")
# A function declared through a typedef of a function type, whose spelling
# holds no parenthesis.
anti_message(clang_function_through_a_typedef STATUS 0
    FILES api.h "typedef int F(void);\nF f;\n"
    OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
    ARGS ${clang_bind})
# An enum without a tag whose value leaves int has a base of its own, which
# its constant and a macro that names it take.
anti_message(clang_wide_anonymous_enum_in_a_macro STATUS 0
    FILES api.h "enum { BIG = 0x100000000 };\n#define M BIG\n"
    OUT "bind: anti.api from api.h, 0 functions, 0 warnings"
    ARGS ${clang_bind})
# A packed enum holds an attribute among its enumerators.
anti_message(clang_packed_enum STATUS 0
    FILES api.h "enum __attribute__((packed)) E { A };\nvoid take(enum E e);\n"
    OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
    ARGS ${clang_bind})
# A chain of typedefs deeper than the parser of a type goes has no form.
set(chain "typedef int A0;\n")
foreach(i RANGE 1 299)
    math(EXPR before "${i} - 1")
    string(APPEND chain "typedef A${before} A${i};\n")
endforeach()
anti_message(clang_typedef_chain_too_deep STATUS 0
    FILES api.h "${chain}A299 f(void);\n"
    OUT "bind: anti.api from api.h, 0 functions, 1 warning"
    ARGS ${clang_bind}
    ERR "anti: api.h: warning: `f` is left out: Anti has no form for its result")
# The probe finds no value other than zero in an enum of one value 0.
anti_message(clang_probe_of_an_enum_of_zero STATUS 0
    FILES api.h "enum Zero { ZERO };\nstruct Z { enum Zero z; };\nstruct P { struct Z a; struct Z b; int x; };\nvoid take(struct P *p);\n"
    OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
    ARGS ${clang_bind} --probe)

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
    # A clang that serves the AST of ast.json and the header after the
    # preprocessor of pre.i, where the case writes one.
    function(clang_serving_files)
        write_clang("for a in \"$@\"; do\n    case \"$a\" in\n    --version) echo 'clang version ${CLANG_VERSION}'; exit 0 ;;\n    -print-resource-dir) echo /nowhere; exit 0 ;;\n    -E) if [ -f pre.i ]; then cat pre.i; fi; exit 0 ;;\n    esac\ndone\ncat ast.json\n")
    endfunction()
    # A variable without a name.
    anti_message(clang_ast_variable_without_a_name COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"VarDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 0 functions, 1 warning"
        ARGS ${clang_bind}
        ERR "anti: api.h: warning: the variable `` is skipped")
    # Two typedefs that name each other resolve to no type.
    anti_message(clang_ast_typedef_cycle COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"TypedefDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"A\", \"type\": {\"qualType\": \"B\"}}, {\"kind\": \"TypedefDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"B\", \"type\": {\"qualType\": \"A\"}}, {\"kind\": \"FunctionDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"f\", \"type\": {\"qualType\": \"A (void)\"}}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 0 functions, 1 warning"
        ARGS ${clang_bind}
        ERR "anti: api.h: warning: `f` is left out: Anti has no form for its result")
    # An enum without a tag takes the name of its typedef, which `enum`
    # then names.
    anti_message(clang_ast_enum_named_by_its_typedef COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"EnumDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"id\": \"0x1\", \"inner\": [{\"kind\": \"EnumConstantDecl\", \"name\": \"X\"}]}, {\"kind\": \"TypedefDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"Mode\", \"type\": {\"qualType\": \"Mode\"}, \"inner\": [{\"decl\": {\"id\": \"0x1\", \"name\": \"\"}}]}, {\"kind\": \"FunctionDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"h\", \"type\": {\"qualType\": \"void (enum Mode)\"}}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
        ARGS ${clang_bind})
    # A value that is no number, and one below the least int64_t, count
    # on from the enumerator before.
    anti_message(clang_ast_enumerators_without_a_value COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"EnumDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"E\", \"inner\": [{\"kind\": \"EnumConstantDecl\", \"name\": \"A\", \"inner\": [{\"kind\": \"ConstantExpr\", \"value\": \"seven\"}]}, {\"kind\": \"EnumConstantDecl\", \"name\": \"B\", \"inner\": [{\"kind\": \"ConstantExpr\", \"value\": \"-99999999999999999999\"}]}]}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 0 functions, 0 warnings"
        ARGS ${clang_bind})
    # A part without a kind and a field without a type are passed over, and
    # an anonymous record with no field after it is named by its count.
    anti_message(clang_ast_parts_of_a_record_without_a_type COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"RecordDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"S\", \"tagUsed\": \"struct\", \"completeDefinition\": true, \"inner\": [{\"name\": \"q\"}, {\"kind\": \"FieldDecl\", \"name\": \"t\"}, {\"kind\": \"FieldDecl\", \"name\": \"x\", \"type\": {\"qualType\": \"int\"}}, {\"kind\": \"RecordDecl\", \"tagUsed\": \"struct\", \"completeDefinition\": true, \"inner\": []}]}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 0 functions, 1 warning"
        ARGS ${clang_bind}
        ERR "anti: api.h: warning: `S_anon0` has no field and is left out")
    # A function without a name or a type is passed over, and a parameter
    # without a type is spelled `int` in C.
    anti_message(clang_ast_functions_without_a_type COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"FunctionDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"type\": {\"qualType\": \"void (void)\"}}, {\"kind\": \"FunctionDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"n\"}, {\"kind\": \"FunctionDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"p\", \"type\": {\"qualType\": \"void (int)\"}, \"inner\": [{\"kind\": \"ParmVarDecl\", \"name\": \"a\"}]}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 1 functions, 0 warnings"
        ARGS ${clang_bind})
    # The spelling of an anonymous record that does not close.
    anti_message(clang_ast_anonymous_type_without_its_end COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": [{\"kind\": \"RecordDecl\", \"loc\": {\"file\": \"api.h\", \"line\": 1}, \"name\": \"S\", \"tagUsed\": \"struct\", \"completeDefinition\": true, \"inner\": [{\"kind\": \"RecordDecl\", \"tagUsed\": \"struct\", \"completeDefinition\": true, \"inner\": [{\"kind\": \"FieldDecl\", \"name\": \"a\", \"type\": {\"qualType\": \"int\"}}]}, {\"kind\": \"FieldDecl\", \"name\": \"x\", \"type\": {\"qualType\": \"struct (unnamed at api.h:1:1\"}}]}]}\n"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 0 functions, 1 warning"
        ARGS ${clang_bind}
        ERR "anti: api.h: warning: `S` is left out: Anti has no form for the type of its field `x`")
    # A line marker without a file keeps the file, and the last line of the
    # header after the preprocessor ends without a newline.
    anti_message(clang_ast_line_marker_without_a_file COPY STATUS 0
        FILES api.h "int v;\n" ast.json "{\"kind\": \"TranslationUnitDecl\", \"inner\": []}\n" pre.i "# 1 \"api.h\"\n# 2\n#define X 1\n#define Y"
        SETUP clang_serving_files
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        OUT "bind: anti.api from api.h, 0 functions, 1 warning"
        ARGS ${clang_bind}
        ERR "anti: api.h: warning: the macro `Y` has no value and is skipped")
    # A first line that names no clang version, before the line that does.
    function(clang_version_on_the_second_line)
        write_clang("echo 'Another compiler'\necho 'clang version ${CLANG_VERSION}'\n")
    endfunction()
    anti_message(clang_version_on_the_second_line COPY
        FILES api.h "int count(void);\n"
        SETUP clang_version_on_the_second_line
        ENV "PATH=${WORK}/fake:$ENV{PATH}"
        ARGS ${clang_bind}
        ERR "anti: cannot read the version of clang from `clang --version`, and ${clang_where}")
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
# The shim of a static inline function, written after the module.
anti_message(fault_shim_not_written
    FILES api.h "static inline int twice(int n) { return n * 2; }\n"
    FAULT open:2:EACCES
    ARGS bind --clang api.h -o ../out --runtime "${RUNTIME}"
    ERR "anti: cannot write ../out/shim_api.c")
# T153: the output directory, which anti bind leaves without a message.
anti_message(fault_out_not_made
    FILES api.json "${fault_api}"
    FAULT mkdir:2:EACCES
    ARGS bind api.json -o ../out
    ERR "anti: cannot make ../out")
# The list of the values of an array or an object of the JSON reader,
# list_add of jsontree.c, whose values and names take memory of their own.
# The array of `functions` takes the fifteenth and the sixteenth, and the
# object around it the seventeenth and the eighteenth.
foreach(case "array_values|15|17" "array_names|16|17"
             "object_values|17|18" "object_names|18|18")
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 count)
    list(GET parts 2 column)
    anti_message(fault_json_${name}_memory
        FILES api.json "{\"functions\": [1]}"
        FAULT alloc:${count}:ENOMEM
        ARGS bind api.json -o ../out
        ERR "anti: api.json is not JSON: out of memory at line 1, column ${column}")
endforeach()
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
# The search for the imports of the library file reads it again,
# driver_find_libraries of driver_search.c, and the fifth read fails.
anti_message(fault_header_imports_unread
    FILES anti.toml "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n"
          src/com/example/one.anti "//! The smallest library.\n\n/// One.\nexport fn one() -> c_int\n{\n\treturn 1;\n}\n"
    SETUP build_the_library
    FAULT read:5:EACCES
    ARGS bind --header dist/${HOST}/dev/com/example/one.antl -o ../out
    ERR "antic: cannot read dist/${HOST}/dev/com/example/one.antl")
# The load of the library file the search found, driver_load_libraries
# of driver_search.c, whose first read is the seventh.
anti_message(fault_header_library_unloaded
    FILES anti.toml "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n"
          src/com/example/one.anti "//! The smallest library.\n\n/// One.\nexport fn one() -> c_int\n{\n\treturn 1;\n}\n"
    SETUP build_the_library
    FAULT read:7:EACCES
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
