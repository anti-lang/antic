#ifndef ANTIC_ANTIC_H
#define ANTIC_ANTIC_H

/* DESIGN: what the anti tool may use of antic. anti compiles through
   driver.h, and everything else it needs of the compiler stands here:
   the names of the files antic writes and of the runtime archive, the
   package record of a library file, and the walks over a source, a
   library file and a checked module that the commands of anti ask for.
   The walks run inside antic and hand back plain records, so a change of
   the syntax tree, the checker or the library reader changes this file or
   nothing of anti. The shared helpers beside it, text.h, arena.h,
   target.h, cpu.h, modpath.h, platform.h, userdirs.h, sha256.h and
   applesdk.h, serve every part alike. The test anti_interface refuses
   any other header of src/antic/ in a file of src/anti/. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "lexer.h"
#include "text.h"

/* The suffix of a library file. */
#define ANTL_SUFFIX ".antl"

/* The suffix of the C header that a library for C carries. antic writes
   the file and `anti build` copies it into `dist/`, so the name stands
   here once. */
#define HEADER_SUFFIX ".h"

/* The runtime archive keeps the runtime of each target and processor
   level in <runtime>/RUNTIME_LIB_DIR/<target>/<level>/. A program built
   with --cpu below the target's default then links a runtime of its own
   level. The native libraries stay in <runtime>/RUNTIME_LIB_DIR/<target>/
   and are built for the default level alone. The sysroot that lld links
   against is <runtime>/RUNTIME_SYSROOT_DIR/<target>/, and the pinned LLVM
   tools are in <runtime>/RUNTIME_BIN_DIR/. A macOS sysroot names the version of
   its stubs in the file SYSROOT_SDK_VERSION. The stubs of Apple's SDK for
   a program that names a framework lie in SYSROOT_APPLE_SDK_DIR of it. */
#define RUNTIME_LIB_DIR "lib"
#define RUNTIME_SYSROOT_DIR "sysroot"
#define RUNTIME_BIN_DIR "bin"
/* The library files of the standard library, a search root of antic. */
#define RUNTIME_STD_DIR "std"
/* The object beside the runtime library that a bundled archive carries
   instead of the licence text of src/rt/license.c. */
#define RUNTIME_LICENSE_STUB "anti_rt_license_stub"
/* DESIGN: the Linux link mode against glibc takes the sysroot and the
   runtime of the target name with this suffix, linux-arm64-glibc beside
   linux-arm64. tools/get-sysroot.cmake and CMakeLists.txt spell the same
   names. */
#define LINUX_GLIBC_SUFFIX "-glibc"
#define SYSROOT_SDK_VERSION "sdk-version"
#define SYSROOT_APPLE_SDK_DIR "sdk"

/* DESIGN: lld of the pinned LLVM release links for every target, and
   --linker platform selects the linker of the host's own toolchain. */
enum linker { LINKER_LLD, LINKER_PLATFORM };

/* Append the path of the PDB of a Windows link whose output is
   executable: the output with its suffix replaced by `.pdb`, beside it.
   The link and the symbols archive of anti build both name it so. */
void link_pdb_path(struct text *out, const char *executable);

/* A dependency in the package header of a library file. */
struct package_dependency {
    const char *name;               /* a module path */
    const char *constraint;         /* a version constraint, such as ^1.2 */
    const char *url;                /* the repository */
};

/* The version of a package whose build names none. It stands in the
   package header of a library file and in every class descriptor. */
#define PACKAGE_VERSION_DEFAULT "0.0.0"

/* The package header of a library file. antic alone writes the module
   path as the name, version 0.0.0 and empty licence fields. */
struct package {
    const char *name;
    const char *version;
    const struct package_dependency *dependencies;
    size_t dependency_count;
    const char *license;            /* an SPDX identifier */
    const char *license_text;       /* the full text */
    const char *const *attribution; /* lines copied verbatim */
    size_t attribution_count;
};

/* Read the package header and the module path of the library file in
   data. Both lie in the memory pool. Returns false and writes a message
   to error when the bytes are no library file of this version. */
bool antic_library_header(const uint8_t *data, size_t size,
                          struct arena *arena, struct package *package,
                          const char **module, char *error,
                          size_t error_size);

/* Split source into the tokens of the language, ending with TOKEN_EOF,
   as antic reads them. `anti fmt` works on the token list, which is the
   one definition of the words and symbols of Anti. Returns false when the
   source does not lex. lexer_token_list_free releases the array of out, and
   the texts of the tokens lie in the memory pool. */
bool antic_tokens(const char *source, size_t length, struct arena *arena,
                  struct token_list *out);

/* Whether name is one identifier, which no keyword and no reserved word
   is. */
bool antic_is_identifier(const char *name);

/* One doc comment of a source: a `///` or `//!` comment, or a `//#` or
   `//#!` comment of the developer. */
struct antic_doc_comment {
    const char *text;
    size_t length;
    int line;                       /* the line of the comment */
    bool dev;                       /* a `//#` or `//#!` comment */
    const char *owner;              /* the item, NULL for the module */
};

/* What the commands of anti read of a source without checking it. */
struct antic_outline {
    const char **imports;           /* the module path of each import */
    size_t import_count;
    const char **tests;             /* the names the `tests` block declares */
    size_t test_count;
    bool setup;                     /* the `fixtures` block declares setup */
    bool teardown;                  /* and teardown */
    bool has_main;                  /* the module declares `fn main` */
    /* Every doc comment: those of the module, then those of each item in
       the order of the source, each before the comments of its
       parameters, its cases and its members. */
    struct antic_doc_comment *comments;
    size_t comment_count;
};

/* Lex and parse source, the bytes of the file at path, into out. What out
   points at lies in the memory pool and in source, which the caller keeps
   while it reads out. With report set, each error of the
   lexer and the parser is printed as path:line:column. Returns false when
   the source does not lex or parse. */
bool antic_outline(const char *path, const char *source, size_t length,
                   bool report, struct arena *arena,
                   struct antic_outline *out);

/* DESIGN: `anti doc` renders a page from these records and nothing of the
   checker. antic builds them from the public interface of a module, or
   from its syntax tree for dev docs and `--private`, and keeps the
   tables of the interface that a backtick name resolves against. Every
   text is a copy, so the page outlives the module it was built from. */

/* One documented item, or one field or function of a body. */
struct doc_entry {
    struct text signature;          /* the declaration, without its body */
    struct text name;               /* the anchor and the heading */
    struct text doc;                /* the `///` text */
    struct text note;               /* the `//#` text, dev docs alone */
    struct doc_entry *fields;       /* the fields or the values of a body */
    size_t field_count;
    size_t field_capacity;
    struct doc_entry *members;      /* the functions of a class body */
    size_t member_count;
    size_t member_capacity;
    /* A function of a synchronized class that runs under the lock of its
       object, which the page says at the function. */
    bool locked;
    /* A name that `type` gives a copy of a generic: the module and the
       name of that generic, which the page links to. Empty otherwise. */
    struct text copy_of_module;
    struct text copy_of;
};

/* One page: a module with its items. */
struct doc_page {
    struct text module;
    struct text doc;                /* the `//!` text */
    struct text note;               /* the `//#!` text */
    struct doc_entry *items;
    size_t item_count;
    size_t item_capacity;
    /* The tables a backtick name reads: the name of every item of the
       interface and the module path of every import. */
    struct text *names;
    size_t name_count;
    struct text *imports;
    size_t import_count;
};

/* What a page shows. */
enum doc_items {
    DOC_ITEMS_PUBLIC,               /* the public interface */
    DOC_ITEMS_PRIVATE,              /* every item of the source */
    DOC_ITEMS_DEV                   /* every item and the `//#` notes */
};

struct options;

/* Build the page of options->input, a source or a library file, which
   the front end checks as --front-end does. DOC_ITEMS_PRIVATE and
   DOC_ITEMS_DEV need a source. Returns false when the module cannot be
   read or does not check, and the reason is printed. The caller frees out
   with doc_page_free, whether the call succeeded or not. */
bool antic_doc_page(const struct options *options, enum doc_items items,
                    struct doc_page *out);

void doc_page_free(struct doc_page *page);

#endif
