#ifndef ANTIC_DRIVER_PARTS_H
#define ANTIC_DRIVER_PARTS_H

/* The inside of the driver, which its files share. driver.c reads and
   writes the files of a compilation and runs it: the front end, lowering,
   the passes, the back end and the assembler, and the library file, the
   header and the interface it writes. driver_link.c plans and runs the
   link of a program: the facts of the target, Apple's SDK, the Windows
   paths, the exports of a host and the native libraries. driver_search.c
   finds the library files of the imports and loads them in order.
   driver_library.c writes a library for C or a plugin: the joined COFF
   objects, the bundled runtime and the copy of the package header.
   driver_index.c writes the plugin index beside a plugin. No other file
   includes this one. */

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "driver.h"
#include "linker.h"
#include "text.h"

struct interface;

#define ASSEMBLY_SUFFIX ".s"
#define DEF_SUFFIX ".def"

/* What a compilation writes beside the assembly. A library for C has a
   name, a header and a package header copy. A linked binary has the
   licence notice and the names of its export fns. */
struct extras {
    struct text name;
    struct text header;
    struct text package;
    struct text notice;
    struct text exports;
    /* The program can host a plugin, so the link exports its symbols.
       The compilation decides it and the link reads it. */
    bool hosts_plugins;
    /* The `provides` lines of a plugin, one per line as
       `<interface>\t<class>`, which the index file carries. */
    struct text provides;
    /* The names a Windows program that can host a plugin defines, as
       emit_names writes them, which its .def file exports. */
    struct text host_names;
    /* The program holds `anti.regex`, so the link adds PCRE2. */
    bool regex;
    /* The packages of the licence notice: the interface of the compiled
       module and the library files it loaded. own is NULL where nothing
       links. The back end writes the notice once the program's link mode
       is known, since a program of musl names more packages. */
    const struct interface *own;
    const struct interface *const *libraries;
    size_t library_count;
};

/* A list of paths that grows as it is filled. */
struct paths {
    const char **items;
    size_t count;
    size_t capacity;
};

/* The texts behind the facts of a link. */
struct link_facts {
    struct text sdk_path;
    struct text sdk_version;
    struct text crt_dir;
    struct text sysroot;
    struct text lld_dir;
    struct text rpath;
};

/* DESIGN: a Windows link runs in the directory of its output. The object
   files, the PDB and the output are named relative to that directory, and
   so is a runtime archive given by a relative path. lld-link and link.exe
   record the path of every object and the whole command line in the PDB.
   The PDB goes into the symbols archive of a release, and a path of the
   build in it would ship. An absolute runtime archive, such as
   the one of an install, stays as given, and so do its linker and its
   sysroot. */
struct windows_link {
    struct arena arena;
    struct text directory;      /* where the link runs, empty for here */
    struct text base;           /* the absolute form of that directory */
};

struct interface;
struct ir_module;
struct module;
struct types;

/* driver.c */

bool driver_read_bytes(const char *path, struct text *out);
bool driver_write_file(const char *path, const struct text *content);
bool driver_is_plugin(const struct options *o);
bool driver_file_exists(const char *path);
/* Write text, the LLVM IR of the back end, to <base>.ll, and run opt and
   llc of llvm_run.c on it into output: the object, or the assembly under
   -S. The tools come from --opt and --llc, or from bin/ of the runtime
   archive, and nowhere else. <base>.ll and <base>.bc are deleted
   afterwards unless --keep-llvm. */
bool driver_compile_llvm(const struct options *o, const struct text *text,
                         const char *base, const char *output);

/* driver_link.c */

/* The tool name of bin/ of the runtime archive, with the suffix of the
   host, or NULL with a message when the archive lacks it. path holds the
   text. */
const char *driver_archive_tool(const struct options *o, const char *name,
                                struct text *path);
void driver_link_facts_free(struct link_facts *f);
/* Whether the program links the C library of musl, and with it mimalloc:
   a program for Linux that lld links outside the glibc mode. */
bool driver_links_musl(const struct options *o, const struct extras *extras);
bool driver_link_inputs_of(const struct options *o, const struct extras *extras,
                           const char *object, const char *executable,
                           struct link_inputs *in, struct link_facts *f);
bool driver_windows_link_paths(struct windows_link *w, struct link_inputs *in,
                               const char **def_file);
void driver_windows_link_free(struct windows_link *w);
bool driver_run_link(const struct windows_link *w,
                     const struct link_command *c);
bool driver_native_inputs(const struct options *o, const struct extras *extras,
                          struct text *glue, struct text *pcre2,
                          const char ***out, size_t *count);
bool driver_link_program(const struct options *o, const char *object,
                         const char *executable, const struct extras *extras);

/* driver_search.c */

void driver_add_path(struct paths *p, const char *path);
const char **driver_libraries_with_input(const struct options *o);
bool driver_find_libraries(const struct options *o, const struct module *tree,
                           struct arena *arena, struct paths *out);
bool driver_load_libraries(const struct paths *paths, const char *module,
                           struct arena *arena, struct types *types,
                           struct ir_module *program,
                           const struct interface **out);

/* driver_library.c */

bool driver_build_c_library(const struct options *o, const char *object,
                            const char *base, const struct extras *extras);

/* driver_index.c */

bool driver_write_plugin_index(const char *dir, const char *name,
                               const char *library,
                               const struct text *provides);

#endif
