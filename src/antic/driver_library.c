/* A library for C and a plugin: the llvm-ar and join commands, the COFF
   objects joined into one, the bundled runtime with the marker member of
   Mach-O, the object of the package header copy, and the header, the
   archive or the shared library each build writes. */

#include "driver_parts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "coff.h"
#include "llvm_emit.h"
#include "platform.h"
#include "say.h"

#define MARKER_SUFFIX ".bundle"
#define EXPORTED_SUFFIX ".exported"

/* DESIGN: a bundled runtime holds every member of the runtime library
   except two. The object of src/rt/start.c has the C main of executables. The
   object of src/rt/license.c reads the notice, which a static library has
   none of. The bundle takes the stub object of the runtime archive
   instead, and anti.license.text then returns an empty text. */
#define RUNTIME_START_MEMBER "start."
#define RUNTIME_LICENSE_MEMBER "license."

static bool run_command(const struct link_command *c, const char *what)
{
    if (platform_run(c->argv) != 0) {
        say_antic("%s failed\n", what);
        return false;
    }
    return true;
}

/* Join the COFF objects into one object at output, which src/antic/coff.c does
   because no linker of COFF writes a relocatable object. */
static bool join_coff(const struct paths *objects, const char *output)
{
    struct coff_input *inputs = alloc_zeroed(objects->count, sizeof *inputs);
    struct text *bytes = alloc_zeroed(objects->count, sizeof *bytes);
    struct text joined = {0};
    struct text error = {0};
    bool ok = true;
    size_t i;

    for (i = 0; ok && i < objects->count; i++) {
        ok = driver_read_bytes(objects->items[i], &bytes[i]);
        inputs[i].name = objects->items[i];
        inputs[i].data = (const unsigned char *)bytes[i].data;
        inputs[i].size = bytes[i].length;
    }
    if (ok && !coff_join(inputs, objects->count, &joined, &error)) {
        say_antic("joining the runtime into the library: %s\n",
                  text_cstr(&error));
        ok = false;
    }
    ok = ok && driver_write_file(output, &joined);
    for (i = 0; i < objects->count; i++) {
        text_free(&bytes[i]);
    }
    free(inputs);
    free(bytes);
    text_free(&joined);
    text_free(&error);
    return ok;
}

void driver_bundle_marker(struct text *out, const char *package)
{
    const char *p;

    text_appendf(out, "%s_", RUNTIME_BUNDLE_MARKER);
    for (p = package; *p != '\0'; p++) {
        bool plain = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                     (*p >= '0' && *p <= '9');
        text_appendf(out, "%c", plain ? *p : '_');
    }
}

/* Compile text, the LLVM IR of an object that antic writes beside the
   library object, at <path>.o with opt and llc, and make path that
   object. */
static bool compile_member(const struct options *o, const struct text *text,
                           struct text *path)
{
    struct text obj_path = {0};
    bool ok;

    text_appendf(&obj_path, "%s%s", text_cstr(path),
                 target_info(o->target)->object_suffix);
    ok = driver_compile_llvm(o, text, text_cstr(path), text_cstr(&obj_path));
    text_free(path);
    text_append(path, text_cstr(&obj_path));
    text_free(&obj_path);
    return ok;
}

/* Copy path into the memory pool and add the copy to paths. */
static void add_pooled(struct arena *arena, struct paths *paths,
                       const struct text *path)
{
    char *copy = arena_alloc(arena, path->length + 1);

    memcpy(copy, text_cstr(path), path->length + 1);
    driver_add_path(paths, copy);
}

/* Compile the marker member of a bundled Mach-O library at
   <base>.bundle.o and add it to members. */
static bool add_marker(const struct options *o, const struct extras *extras,
                       const char *base, struct arena *arena,
                       struct paths *members)
{
    struct text source = {0};
    struct text path = {0};
    bool ok;

    text_appendf(&path, "%s%s", base, MARKER_SUFFIX);
    llvm_emit_bundle_marker(&source, o->target, text_cstr(&extras->marker));
    ok = compile_member(o, &source, &path);
    if (ok) {
        add_pooled(arena, members, &path);
    }
    text_free(&source);
    text_free(&path);
    return ok;
}

/* DESIGN: a bundled runtime of ELF and of COFF is one relocatable object
   of the library object and the members of the runtime, so two bundles
   in one program define every symbol of the runtime twice and the link
   reports a duplicate. ld.lld -r writes the object of ELF and coff_join
   the one of COFF. Mach-O has neither: ld64.lld 23.1.1 writes no
   relocatable object, a build runs no linker of the host, so not Apple's
   ld -r, and a joiner of Mach-O of our own is out of proportion to the
   feature. A bundled runtime of Mach-O is an archive instead: the
   library object, the members of the runtime, and one marker member.
   A link loads a member of an archive only for a symbol it lacks, so
   the runtime members of a second bundle would stay out without a word.
   The marker brings the duplicate back. It defines
   anti_rt_bundle_<package>, which the library object of that package
   refers to, so the link loads the marker of every bundle it takes a
   library object from. It also defines anti_rt_bundle, the same name in
   every bundle, so two markers in one program are a duplicate symbol.
   See the entry on --bundle-runtime in docs/decisions.md. */

/* Add the license stub of the runtime archive to objects, then extract
   every member of the runtime library at library into <base>.rt with
   llvm_ar, apart from the members of start.c and license.c, and add
   their paths. The paths are in the memory pool. */
static bool extract_runtime(const struct options *o, const char *llvm_ar,
                            const char *library, const char *base,
                            struct arena *arena, struct paths *objects)
{
    struct text listing = {0};
    struct text dir = {0};
    struct text output = {0};
    struct text stub = {0};
    const char *p;
    bool ok;

    text_appendf(&dir, "%s.rt", base);
    text_appendf(&output, "--output=%s", text_cstr(&dir));
    {
        const char *argv[] = {llvm_ar, "t", library, NULL};
        ok = platform_capture(argv, &listing) == 0;
    }
    text_appendf(&stub, "%s/%s/%s/%s/%s%s", o->runtime, RUNTIME_LIB_DIR,
                 target_name(o->target), cpu_name(o->cpu),
                 RUNTIME_LICENSE_STUB, target_info(o->target)->object_suffix);
    add_pooled(arena, objects, &stub);
    for (p = text_cstr(&listing); ok && *p != '\0';) {
        size_t n = strcspn(p, "\r\n");
        if (n > 0 &&
            strncmp(p, RUNTIME_START_MEMBER, strlen(RUNTIME_START_MEMBER)) != 0 &&
            strncmp(p, RUNTIME_LICENSE_MEMBER,
                    strlen(RUNTIME_LICENSE_MEMBER)) != 0) {
            struct text member = {0};
            char *path = arena_alloc(arena, dir.length + n + 2);
            const char *argv[] = {llvm_ar, "x", NULL, NULL, NULL, NULL};
            text_appendf(&member, "%.*s", (int)n, p);
            snprintf(path, dir.length + n + 2, "%s/%.*s", text_cstr(&dir),
                     (int)n, p);
            argv[2] = text_cstr(&output);
            argv[3] = library;
            argv[4] = text_cstr(&member);
            ok = platform_run(argv) == 0;
            driver_add_path(objects, path);
            text_free(&member);
        }
        p += n;
        p += strspn(p, "\r\n");
    }
    text_free(&listing);
    text_free(&dir);
    text_free(&output);
    text_free(&stub);
    return ok;
}

/* Add the members of a bundled static library to members: on Mach-O
   the objects and the marker, on ELF and COFF the objects joined into
   <base>.bundled.o. The paths are in the memory pool. */
static bool join_bundle(const struct options *o, const struct extras *extras,
                        const struct paths *objects, const char *base,
                        struct arena *arena, struct paths *members)
{
    enum object_format format = target_info(o->target)->format;
    struct text joined = {0};
    bool ok;

    if (format == FORMAT_MACHO) {
        size_t i;
        for (i = 0; i < objects->count; i++) {
            driver_add_path(members, objects->items[i]);
        }
        return add_marker(o, extras, base, arena, members);
    }
    text_appendf(&joined, "%s.bundled%s", base,
                 target_info(o->target)->object_suffix);
    if (format == FORMAT_COFF) {
        ok = join_coff(objects, text_cstr(&joined));
    } else {
        struct link_command c;
        struct link_inputs in;
        struct link_facts facts;
        ok = driver_link_inputs_of(o, extras, NULL, NULL, &in, &facts);
        if (ok) {
            link_relocatable_command(&c, o->target, &in, text_cstr(&joined),
                                     objects->items, objects->count);
            ok = run_command(&c, "joining the runtime into the library");
            link_command_free(&c);
        }
        driver_link_facts_free(&facts);
    }
    add_pooled(arena, members, &joined);
    text_free(&joined);
    return ok;
}

/* The members of a static library besides the package header. They are
   the compiled object, or with a bundled runtime one relocatable object
   of it and the runtime's members, or for Mach-O the object, those
   members and the marker. The paths are in the memory pool. */
static bool bundle(const struct options *o, const struct extras *extras,
                   const char *object, const char *base, struct arena *arena,
                   struct paths *members)
{
    struct text library = {0};
    struct text found_ar = {0};
    struct paths objects = {0};
    const char *llvm_ar;
    bool ok;

    if (!o->bundle_runtime) {
        driver_add_path(members, object);
        return true;
    }
    llvm_ar = o->llvm_ar != NULL ? o->llvm_ar
                                 : driver_archive_tool(o, "llvm-ar", &found_ar);
    if (llvm_ar == NULL) {
        return false;
    }
    link_runtime_library(&library, o->runtime, o->target, o->cpu);
    driver_add_path(&objects, object);
    ok = extract_runtime(o, llvm_ar, text_cstr(&library), base, arena,
                         &objects) &&
         join_bundle(o, extras, &objects, base, arena, members);
    if (!ok) {
        say_antic("cannot bundle %s\n", text_cstr(&library));
    }
    free((void *)objects.items);
    text_free(&library);
    text_free(&found_ar);
    return ok;
}

/* Compile the object of the package header copy at <path>.o with llc,
   and make path that object. */
static bool assemble_package(const struct options *o, const struct text *bytes,
                             struct text *path)
{
    struct text source = {0};
    bool ok;

    llvm_emit_package(&source, o->target, bytes->data, bytes->length);
    ok = compile_member(o, &source, path);
    text_free(&source);
    return ok;
}

/* Write the static archive at path from object, its bundled runtime and
   the copy of the package header, and print the line that links a C
   program with it. */
static bool build_static(const struct options *o, const char *object,
                         const char *base, const struct extras *extras,
                         const char *dir, const char *path)
{
    struct arena arena = {0};
    struct text package = {0};
    struct text found_ar = {0};
    struct paths members = {0};
    struct link_command c;
    const char *llvm_ar;
    bool ok;

    text_appendf(&package, "%s%s%s", dir, text_cstr(&extras->name),
                 LIBRARY_PACKAGE_SUFFIX);
    llvm_ar = o->llvm_ar != NULL ? o->llvm_ar
                                 : driver_archive_tool(o, "llvm-ar", &found_ar);
    ok = llvm_ar != NULL && assemble_package(o, &extras->package, &package) &&
         bundle(o, extras, object, base, &arena, &members);
    if (ok) {
        driver_add_path(&members, text_cstr(&package));
        platform_remove(path);
        link_archive_command(&c, o->target, llvm_ar, path, members.items,
                             members.count);
        ok = run_command(&c, "llvm-ar");
        link_command_free(&c);
    }
    if (ok) {
        struct text line = {0};
        link_line(&line, o->target, path, o->runtime, o->cpu,
                  o->bundle_runtime);
        printf("%s\n", text_cstr(&line));
        text_free(&line);
    }
    free((void *)members.items);
    arena_free(&arena);
    text_free(&package);
    text_free(&found_ar);
    return ok;
}

/* With --soname, put the major version and the full one of a shared
   library at path in s, and on Linux name the versioned file the link
   writes in versioned. */
static bool soname_parts(const struct options *o, const char *path,
                         struct shared_options *s, struct text *major,
                         struct text *versioned)
{
    const char *version = o->package_version;

    if (!o->soname) {
        return true;
    }
    if (version == NULL) {
        say_antic("--soname needs --package-version\n");
        return false;
    }
    text_appendf(major, "%.*s", (int)strcspn(version, "."), version);
    s->major = text_cstr(major);
    s->version = version;
    /* DESIGN: with --soname a Linux library is lib<name>.so.<major>,
       and lib<name>.so links to it for the C compiler. */
    if (target_info(o->target)->os == OS_LINUX) {
        text_appendf(versioned, "%s.%s", path, text_cstr(major));
    }
    return true;
}

/* Append one line per export function of extras, the name after
   prefix. */
static void append_exports(struct text *out, const struct extras *extras,
                           const char *prefix)
{
    const char *p = text_cstr(&extras->exports);

    while (*p != '\0') {
        size_t n = strcspn(p, "\n");
        text_appendf(out, "%s%.*s\n", prefix, (int)n, p);
        p += n + (p[n] == '\n');
    }
}

/* DESIGN: the exported surface of a shared library for C is the export
   functions and anti_licenses. Windows takes it as a .def file and macOS
   as an -exported_symbols_list. Linux takes --exclude-libs, because the
   runtime is an archive. A Windows plugin exports its table and the list
   of the places the loader fills with the addresses of the host. */

/* Write the file of the exported surface of a shared library in dir, at
   exported on macOS and at def on Windows, and name it in s. */
static bool write_exports(const struct options *o, const struct extras *extras,
                          const char *dir, struct shared_options *s,
                          struct text *exported, struct text *def)
{
    enum target_os os = target_info(o->target)->os;
    const char *name = text_cstr(&extras->name);
    struct text content = {0};
    bool ok = true;

    if (os == OS_MACOS && !s->plugin) {
        text_appendf(exported, "%s%s%s", dir, name, EXPORTED_SUFFIX);
        append_exports(&content, extras, "_");
        text_append(&content, "_anti_licenses\n");
        ok = driver_write_file(text_cstr(exported), &content);
        s->exported_file = text_cstr(exported);
    } else if (os == OS_WINDOWS) {
        text_appendf(def, "%s%s%s", dir, name, DEF_SUFFIX);
        text_appendf(&content, "LIBRARY %s\nEXPORTS\n", name);
        if (s->plugin) {
            text_append(&content, "    anti_rt_provides DATA\n"
                                  "    anti_rt_imports DATA\n");
        } else {
            append_exports(&content, extras, "    ");
            text_append(&content, "    anti_licenses DATA\n");
        }
        ok = driver_write_file(text_cstr(def), &content);
        s->def_file = text_cstr(def);
    }
    text_free(&content);
    return ok;
}

/* Link the shared library or the plugin at path from object, in dir. */
static bool build_shared(const struct options *o, const char *object,
                         const struct extras *extras, const char *dir,
                         const char *path)
{
    struct link_inputs in;
    struct link_command c;
    struct shared_options s = {NULL, NULL, NULL, NULL, driver_is_plugin(o)};
    struct link_facts facts;
    struct windows_link w;
    struct text major = {0};
    struct text versioned = {0};
    struct text def = {0};
    struct text exported = {0};
    struct text glue = {0};
    struct text pcre2 = {0};
    const char *const *extra = o->objects;
    const char **owned = NULL;
    size_t extra_count = o->object_count;
    bool ok = true;

    /* A library for C links PCRE2 as a program does. A plugin links no
       runtime, and the glue it calls is the host's. */
    if (!s.plugin) {
        ok = driver_native_inputs(o, extras, &glue, &pcre2, &owned,
                                  &extra_count);
        extra = owned;
    }
    memset(&in, 0, sizeof in);
    memset(&facts, 0, sizeof facts);
    ok = ok && soname_parts(o, path, &s, &major, &versioned);
    ok = ok && driver_link_inputs_of(o, extras, object,
                                     versioned.length > 0
                                         ? text_cstr(&versioned)
                                         : path,
                                     &in, &facts);
    in.extra = extra;
    in.extra_count = extra_count;
    ok = ok && write_exports(o, extras, dir, &s, &exported, &def);
    memset(&w, 0, sizeof w);
    ok = ok && (target_info(o->target)->os != OS_WINDOWS ||
                driver_windows_link_paths(&w, &in, &s.def_file));
    if (ok) {
        link_shared_command(&c, o->target, &in, &s);
        ok = driver_run_link(&w, &c);
        link_command_free(&c);
    }
    driver_windows_link_free(&w);
    if (ok && versioned.length > 0) {
        ok = platform_symlink(platform_file_name(text_cstr(&versioned)), path);
        if (!ok) {
            say_antic("cannot write %s\n", path);
        }
    }
    if (ok && s.plugin) {
        ok = driver_write_plugin_index(dir, platform_file_name(path), path,
                                       &extras->provides);
    }
    driver_link_facts_free(&facts);
    text_free(&major);
    text_free(&versioned);
    text_free(&def);
    text_free(&exported);
    free(owned);
    text_free(&glue);
    text_free(&pcre2);
    return ok;
}

/* Write a library for C from object: its header, and the static archive
   with the copy of the package header, or the shared library. A static
   library prints the line that links a C program with it. */
bool driver_build_c_library(const struct options *o, const char *object,
                            const char *base, const struct extras *extras)
{
    const struct target_info *info = target_info(o->target);
    const char *name = text_cstr(&extras->name);
    struct text dir = {0};
    struct text path = {0};
    struct text header = {0};
    bool ok;

    text_append_bytes(&dir, base, (size_t)(platform_file_name(base) - base));
    if (o->output != NULL) {
        text_append(&path, o->output);
    } else if (o->lib == LIB_STATIC) {
        text_appendf(&path, info->format == FORMAT_COFF ? "%s%s.lib"
                                                        : "%slib%s.a",
                     text_cstr(&dir), name);
    } else {
        text_appendf(&path,
                     info->os == OS_WINDOWS ? "%s%s.dll"
                     : info->os == OS_MACOS ? "%slib%s.dylib"
                                            : "%slib%s.so",
                     text_cstr(&dir), name);
    }
    text_appendf(&header, "%s%s%s", text_cstr(&dir), name, HEADER_SUFFIX);
    /* A plugin is loaded by an Anti host and never by C, so it carries
       no header of its own. */
    ok = driver_is_plugin(o) ||
         driver_write_file(text_cstr(&header), &extras->header);
    if (ok && o->lib == LIB_STATIC) {
        ok = build_static(o, object, base, extras, text_cstr(&dir),
                          text_cstr(&path));
    } else if (ok) {
        ok = build_shared(o, object, extras, text_cstr(&dir),
                          text_cstr(&path));
    }
    text_free(&dir);
    text_free(&path);
    text_free(&header);
    return ok;
}
