#ifndef ANTIC_ANTIC_H
#define ANTIC_ANTIC_H

/* What the anti tool may use of antic. */

/* DESIGN: what the anti tool may use of antic. anti compiles through
   driver.h, and everything else it needs of the compiler stands here:
   the names of the files antic writes and of the runtime archive and the
   package record of a library file, from shared.h, and the walks over a
   source, a library file and a checked module that the commands of anti
   ask for. The walks run inside antic and hand back plain records, so a
   change of the syntax tree, the checker or the library reader changes
   this file or nothing of anti. The shared helpers beside it, text.h,
   arena.h, attributes.h, shared.h, target.h, cpu.h, modpath.h,
   platform.h, userdirs.h, sha256.h and applesdk.h, serve every part
   alike. The test anti_interface refuses any other header of src/antic/
   in a file of src/anti/. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "lexer.h"
#include "shared.h"
#include "text.h"

/* The three functions of the names of a link, which linker.c defines.
   They stand here, since anti reads the option --lto and names the PDB
   of a Windows program as the link does. The word of --lto that names
   mode, which names its directory of the
   runtime as bitcode as well. anti reads the option with the same
   words. */
const char *link_lto_name(enum lto mode);

/* Set *mode to the mode that name names, and answer whether one does. */
bool link_lto_from_name(const char *name, enum lto *mode);

/* Append the path of the PDB of a Windows link whose output is
   executable: the output with its suffix replaced by `.pdb`, beside it.
   The link and the symbols archive of anti build both name it so. */
void link_pdb_path(struct text *out, const char *executable);

/* Read the package header and the module path of the library file in
   data. Both lie in the memory pool. Returns false and writes a message
   to error when the bytes are no library file of this version. */
bool antic_library_header(const uint8_t *data, size_t size,
                          struct arena *arena, struct package *package,
                          const char **module, char *error,
                          size_t error_size);

/* Append the lines of the licence notice of the packages to out, as
   notice_text of notice.h writes them, without the two markers of a
   binary. Every form of `anti license` prints these lines. */
void antic_notice_lines(struct text *out,
                        const struct package *const *packages, size_t count);

/* Append the SPDX identifier of the component of licenses/ of the runtime
   archive whose text is <component>.txt, through
   notice_component_license. */
void antic_component_license(const char *component, struct text *out);

/* Whether data starts as a library file does, with its magic. `anti
   license --from-archive` looks for the copy of a package header in a
   member of a static library by it. */
bool antic_library_starts(const uint8_t *data, size_t size);

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

/* Release every record of page that antic_doc_page wrote and leave page
   empty, so a page the call left half built is freed as well. */
void doc_page_free(struct doc_page *page);

#endif
