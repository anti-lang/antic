/* anti symbols over archives and programs that name nothing: a unit of a
   deployment index with no id, and a program with no function. Each
   used to hand a null pointer to memcmp or qsort with a count of zero,
   which the sanitizer builds catch. The paths a configuration and a
   report of --memory-checks name are read as regular files alone. An
   entry whose data no longer matches its CRC refuses its archive. */

#if !defined(_WIN32)
/* mkfifo is POSIX, outside the C11 library. */
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../src/anti/platform.h"
#include "../binary_stdio.h"
#include "check.h"
#include "files.h"
#include "memreport.h"
#include "symmap.h"
#include "syms.h"
#include "target.h"
#include "text.h"
#include "toml.h"
#include "zip.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#define ARCHIVE "test_syms-symbols.zip"
#define TRACE "test_syms-trace.txt"
#define PROGRAM "test_syms-program"
#define MAP "test_syms.map"
#define FIFO "test_syms-fifo"
#define TREE_DIR "test_syms-dir"
#define CONF_DIR "test_syms-conf"
#define QUOTE_DIR "test_syms-quote'd"

static void write_file(const char *path, const char *text)
{
    FILE *f = platform_open(path, true);

    if (f != NULL) {
        fwrite(text, 1, strlen(text), f);
        fclose(f);
    }
}

/* S42 of the first audit: an index unit with no id beside an entry whose name
   starts with `/`. The id and the part before the slash are then both empty. */
static void unit_without_id(void)
{
    static const char index[] = "[module.a]\nmodule = \"prog\"\n";
    static const char map[] = "# build -\n";
    struct zip_entry entries[2];
    const char *symbols[1];

    memset(entries, 0, sizeof entries);
    entries[0].name = "index.toml";
    entries[0].bytes = index;
    entries[0].size = sizeof index - 1;
    entries[1].name = "/prog.map";
    entries[1].bytes = map;
    entries[1].size = sizeof map - 1;
    CHECK(zip_write(ARCHIVE, entries, 2));
    write_file(TRACE, "");
    symbols[0] = ARCHIVE;
    CHECK(syms_resolve(TRACE, symbols, 1) == 0);
    remove(ARCHIVE);
    remove(TRACE);
}

/* S42 of the first audit: a program the ELF reader finds no function in still
   gets a map, with its header lines alone. */
static void program_without_functions(void)
{
    struct text map = {0};
    FILE *f;
    char buffer[256];
    size_t n;

    write_file(PROGRAM, "no program");
    CHECK(symmap_write(PROGRAM, NULL, TARGET_LINUX_X86_64, "-", MAP));
    f = platform_open(MAP, false);
    if (f != NULL) {
        while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
            text_append_bytes(&map, buffer, n);
        }
        fclose(f);
    }
    CHECK_STR(text_cstr(&map), "# The map of a symbols archive of Anti.\n"
                               "# build -\n"
                               "# target linux-x86_64\n");
    text_free(&map);
    remove(PROGRAM);
    remove(MAP);
}

/* A line of a map names a function as a person reads it, and the name
   may hold a blank. The line ends with its location where the map gives
   one. A name the map holds escaped reads back as well. */
static void map_names(void)
{
    static const char map[] =
        "# build -\n"
        "0000000000001000-0000000000001010 app.Pair<int, str>.swap "
        "app.anti:12\n"
        "0000000000001010-0000000000001020 app.List<int>.push\n"
        "0000000000001020-0000000000001030 app.List$3cbyte$3e.push "
        "src/app.anti:7\n";
    struct text function = {0};
    struct text where = {0};

    CHECK(syms_map_lookup(map, 0x1004, &function, &where));
    CHECK_STR(text_cstr(&function), "app.Pair<int, str>.swap");
    CHECK_STR(text_cstr(&where), "app.anti:12");
    text_free(&function);
    text_free(&where);
    CHECK(syms_map_lookup(map, 0x1010, &function, &where));
    CHECK_STR(text_cstr(&function), "app.List<int>.push");
    CHECK_STR(text_cstr(&where), "");
    text_free(&function);
    CHECK(syms_map_lookup(map, 0x102f, &function, &where));
    CHECK_STR(text_cstr(&function), "app.List<byte>.push");
    CHECK_STR(text_cstr(&where), "src/app.anti:7");
    CHECK(!syms_map_lookup(map, 0x1030, &function, &where));
    text_free(&function);
    text_free(&where);
}

/* The last line of a map may end without a newline. An address it does
   not hold is in no line, and the lookup ends at the end of the map. */
static void map_last_line_open(void)
{
    static const char map[] =
        "# build -\n"
        "0000000000001000-0000000000001010 app.main app.anti:3";
    struct text function = {0};
    struct text where = {0};

    CHECK(syms_map_lookup(map, 0x1004, &function, &where));
    CHECK_STR(text_cstr(&function), "app.main");
    CHECK_STR(text_cstr(&where), "app.anti:3");
    text_free(&function);
    text_free(&where);
    CHECK(!syms_map_lookup(map, 0x1010, &function, &where));
    text_free(&function);
    text_free(&where);
}

/* M24 of the second audit: the build id of a program is the line after the
   begin marker of its notice. A line of the same form that the data of the
   program holds before the notice is not its id. */
static void build_id_of_notice(void)
{
    static const char program[] =
        "data build "
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
        "ANTI_LICENSES_BEGIN\nbuild "
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "package app 1.0.0 MIT\nANTI_LICENSES_END\n";
    struct text id = {0};

    write_file(PROGRAM, program);
    CHECK(symmap_build_id(PROGRAM, &id));
    CHECK_STR(text_cstr(&id), "0123456789abcdef0123456789abcdef"
                              "0123456789abcdef0123456789abcdef");
    text_free(&id);
    /* A line of the form with no notice is no build id. */
    write_file(PROGRAM, "data build "
               "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
    CHECK(!symmap_build_id(PROGRAM, &id));
    text_free(&id);
    remove(PROGRAM);
}

static void put_le(struct text *out, uint64_t value, int bytes)
{
    int i;

    for (i = 0; i < bytes; i++) {
        char byte = (char)((value >> (8 * i)) & 0xff);
        text_append_bytes(out, &byte, 1);
    }
}

static void put_zeros(struct text *out, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        text_append_bytes(out, "", 1);
    }
}

/* A Mach-O debug twin whose debug map names object for the function
   from 0x1000 to 0x1100: a header, a `__TEXT` segment at 0, a symbol
   table of an N_OSO, an N_FUN and the N_FUN that ends it, and its
   strings. */
static void twin_naming(const char *object, struct text *out)
{
    size_t length = strlen(object);

    put_le(out, 0xfeedfacfu, 4);
    put_le(out, 0x0100000c, 4);
    put_le(out, 0, 4);
    put_le(out, 0xa, 4);
    put_le(out, 2, 4);
    put_le(out, 72 + 24, 4);
    put_le(out, 0, 8);
    put_le(out, 0x19, 4);
    put_le(out, 72, 4);
    text_append_bytes(out, "__TEXT\0\0\0\0\0\0\0\0\0\0", 16);
    put_zeros(out, 8 * 4 + 4 * 4);
    put_le(out, 0x2, 4);
    put_le(out, 24, 4);
    put_le(out, 128, 4);
    put_le(out, 3, 4);
    put_le(out, 128 + 3 * 16, 4);
    put_le(out, 1 + length + 1 + 3, 4);
    put_le(out, 1, 4);
    put_le(out, 0x66, 1);
    put_zeros(out, 1 + 2 + 8);
    put_le(out, 1 + length + 1, 4);
    put_le(out, 0x24, 1);
    put_le(out, 1, 1);
    put_zeros(out, 2);
    put_le(out, 0x1000, 8);
    put_le(out, 0, 4);
    put_le(out, 0x24, 1);
    put_zeros(out, 1 + 2);
    put_le(out, 0x100, 8);
    put_zeros(out, 1);
    text_append_bytes(out, object, length + 1);
    text_append_bytes(out, "_f", 3);
}

/* M42 of the second audit: the object path of a debug map comes from the
   archive, which need not be the user's. A path to what is no regular file is
   not read: a directory, and on POSIX a FIFO, which waited for a writer
   without end, and /dev/zero, which grew the buffer until anti ended. The
   frame then resolves as far as the twin and the map go. */
static void object_no_file(void)
{
    static const char map[] =
        "# The map of a symbols archive of Anti.\n"
        "# build "
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "0000000000001000-0000000000001100 f\n";
    static const char trace[] =
        "module 0 "
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        " 0x0 prog\n"
        "0x0000000000001001 0+0x1001\n";
    static const char *const objects[] = {
        TREE_DIR,
#if !defined(_WIN32)
        FIFO,
        "/dev/zero",
#endif
    };
    size_t i;

#if !defined(_WIN32)
    remove(FIFO);
    CHECK(mkfifo(FIFO, 0600) == 0);
#endif
    CHECK(platform_make_dir(TREE_DIR));
    write_file(TRACE, trace);
    for (i = 0; i < sizeof objects / sizeof objects[0]; i++) {
        struct text twin = {0};
        struct zip_entry entries[2];
        const char *symbols[1];
        twin_naming(objects[i], &twin);
        memset(entries, 0, sizeof entries);
        entries[0].name = "prog.debug";
        entries[0].bytes = twin.data;
        entries[0].size = twin.length;
        entries[1].name = "prog.map";
        entries[1].bytes = map;
        entries[1].size = sizeof map - 1;
        CHECK(zip_write(ARCHIVE, entries, 2));
        symbols[0] = ARCHIVE;
        CHECK(syms_resolve(TRACE, symbols, 1) == 0);
        text_free(&twin);
    }
    remove(ARCHIVE);
    remove(TRACE);
    platform_remove_entry(TREE_DIR);
#if !defined(_WIN32)
    remove(FIFO);
#endif
}

/* A path the configuration names, or one that stands beside it, is read
   when it is a regular file alone. A FIFO without a writer is refused,
   never waited on: as an include, beside the configuration and as the
   index of a directory of plugins. Windows makes no FIFO, so there the
   paths are missing and the messages the same. */
static void configuration_fifo(void)
{
    static const char *const confs[] = {
        "include = [\"fifo\"]\n",
        "[runtime]\nthreads = 2\n",
        "[runtime]\nplugins = \"plugins\"\n",
    };
    size_t i;

    CHECK(files_remove_tree(CONF_DIR));
    CHECK(platform_make_dir(CONF_DIR));
    CHECK(platform_make_dir(CONF_DIR "/plugins"));
#if !defined(_WIN32)
    CHECK(mkfifo(CONF_DIR "/fifo", 0600) == 0);
    CHECK(mkfifo(CONF_DIR "/plugins/anti-plugins.toml", 0600) == 0);
#endif
    for (i = 0; i < sizeof confs / sizeof confs[0]; i++) {
        write_file(CONF_DIR "/conf.toml", confs[i]);
        CHECK(syms_check(CONF_DIR "/conf.toml", NULL, 0) == 1);
    }
    CHECK(files_remove_tree(CONF_DIR));
}

/* A frame of a report of --memory-checks whose module is a FIFO. The
   line comes out as it went in, and the run does not wait on the FIFO.
   Windows keeps the symbolizing of AddressSanitizer and runs no shell of
   POSIX, so the case runs elsewhere alone, and Windows writes the line
   itself for the expected standard error of the group. */
#define REPORT_FRAME "    #0 0x1 (" FIFO "+0x10)"
static void report_fifo(void)
{
#if !defined(_WIN32)
    static const char *const argv[] = {
        "/bin/sh", "-c", "echo '" REPORT_FRAME "' >&2", NULL,
    };

    remove(FIFO);
    CHECK(mkfifo(FIFO, 0600) == 0);
    CHECK(memreport_run(argv) == 0);
    remove(FIFO);
#else
    fputs(REPORT_FRAME "\n", stderr);
#endif
}

/* A frame of a report whose module is a universal file of macOS that
   holds no file of the processor the frame names: one arm64 file, and a
   frame of x86_64. The line comes out as it went in. Windows writes the
   line itself, as for report_fifo. */
#define UNIVERSAL "test_syms-universal"
#define UNIVERSAL_FRAME "    #0 0x1 (" UNIVERSAL ":x86_64+0x10)"
static void report_universal(void)
{
#if !defined(_WIN32)
    static const unsigned char header[] = {
        0xca, 0xfe, 0xba, 0xbe, 0, 0, 0, 1,    0x01, 0, 0, 0x0c,
        0,    0,    0,    0,    0, 0, 0x10, 0, 0,    0, 0, 0x10,
        0,    0,    0,    0x0e,
    };
    static const char *const argv[] = {
        "/bin/sh", "-c", "echo '" UNIVERSAL_FRAME "' >&2", NULL,
    };
    FILE *f = platform_open(UNIVERSAL, true);

    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fwrite(header, 1, sizeof header, f) == sizeof header);
        fclose(f);
    }
    CHECK(memreport_run(argv) == 0);
    remove(UNIVERSAL);
#else
    fputs(UNIVERSAL_FRAME "\n", stderr);
#endif
}

/* Write entries to the archive with the first byte of the bytes of the
   last entry changed, so its data no longer matches its CRC. */
static void write_broken(struct zip_entry *entries, size_t count)
{
    const struct zip_entry *last = &entries[count - 1];
    struct text bytes = {0};
    char *at = NULL;
    size_t i;
    FILE *f;

    CHECK(zip_write(ARCHIVE, entries, count));
    CHECK(files_read(ARCHIVE, &bytes));
    for (i = 0; at == NULL && i + last->size <= bytes.length; i++) {
        if (memcmp(bytes.data + i, last->bytes, last->size) == 0) {
            at = bytes.data + i;
        }
    }
    CHECK(at != NULL);
    if (at != NULL) {
        *at = (char)(*at ^ 1);
        f = platform_open(ARCHIVE, true);
        CHECK(f != NULL);
        if (f != NULL) {
            CHECK(fwrite(bytes.data, 1, bytes.length, f) == bytes.length);
            fclose(f);
        }
    }
    text_free(&bytes);
}

/* An entry whose data no longer matches its CRC refuses the archive: the
   index of a deployment archive, an entry in the directory of one of its
   ids and an entry at the top of the archive of one build. */
static void broken_entries(void)
{
    static const char index[] =
        "[module.a]\nmodule = \"prog\"\nid = \"0123456789abcdef\"\n";
    static const char map[] = "# The map of a symbols archive of Anti.\n"
                              "# build 0123456789abcdef\n";
    struct zip_entry entries[2];
    const char *symbols[1];

    write_file(TRACE, "");
    symbols[0] = ARCHIVE;
    memset(entries, 0, sizeof entries);
    entries[0].name = "index.toml";
    entries[0].bytes = index;
    entries[0].size = sizeof index - 1;
    entries[1].name = "0123456789abcdef/prog.map";
    entries[1].bytes = map;
    entries[1].size = sizeof map - 1;
    write_broken(entries, 1);
    CHECK(syms_resolve(TRACE, symbols, 1) == 1);
    write_broken(entries, 2);
    CHECK(syms_resolve(TRACE, symbols, 1) == 1);
    entries[1].name = "prog.map";
    write_broken(&entries[1], 1);
    CHECK(syms_resolve(TRACE, symbols, 1) == 1);
    remove(ARCHIVE);
    remove(TRACE);
}

/* The value of the key of the index of a deployment archive that ends in
   field, read by the reader of TOML that `anti symbols` reads it with. */
static void index_value(const char *archive, const char *field,
                        struct text *out)
{
    struct zip_archive z;
    struct text index = {0};
    struct anti_toml *doc = NULL;
    size_t i;
    int64_t n;

    CHECK(zip_read(archive, &z));
    for (i = 0; i < z.count; i++) {
        if (strcmp(text_cstr(&z.items[i].name), "index.toml") == 0) {
            CHECK(zip_unpack(&z, i, &index));
        }
    }
    zip_archive_free(&z);
    doc = anti_rt_toml_read((const unsigned char *)index.data,
                            (int64_t)index.length);
    CHECK(doc != NULL);
    for (n = 0; doc != NULL && n < anti_rt_toml_count(doc); n++) {
        const char *key = (const char *)anti_rt_toml_key(doc, n).ptr;
        if (files_ends_with(key, field)) {
            text_append(out, (const char *)anti_rt_toml_value(doc, n).ptr);
        }
    }
    if (doc != NULL) {
        anti_rt_toml_free(doc);
    }
    text_free(&index);
}

/* The index of a deployment archive names a program and the archive it
   came from as they stand, with a quote in the name of each. The module
   is the stem `check` compares, and the source is a path of the disk. */
static void inventory_quoted_names(void)
{
    static const char map[] =
        "# The map of a symbols archive of Anti.\n# build "
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
    struct zip_entry entry;
    struct text module = {0};
    struct text source = {0};

    CHECK(files_remove_tree(QUOTE_DIR));
    CHECK(platform_make_dir(QUOTE_DIR));
    write_file(QUOTE_DIR "/conf.toml", "[runtime]\nthreads = 2\n");
    write_file(QUOTE_DIR "/tra'cer",
               "MZ\nANTI_LICENSES_BEGIN\nbuild "
               "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
               "\npackage com.example.app 1.0 MIT\n");
    memset(&entry, 0, sizeof entry);
    entry.name = "tra'cer.map";
    entry.bytes = map;
    entry.size = sizeof map - 1;
    CHECK(zip_write(QUOTE_DIR "/tra'cer-symbols.zip", &entry, 1));
    CHECK(syms_inventory(QUOTE_DIR "/conf.toml", NULL,
                         QUOTE_DIR "/all.zip") == 0);
    index_value(QUOTE_DIR "/all.zip", ".module", &module);
    index_value(QUOTE_DIR "/all.zip", ".source", &source);
    CHECK_STR(text_cstr(&module), "tra'cer");
    CHECK_STR(text_cstr(&source), QUOTE_DIR "/tra'cer-symbols.zip");
    text_free(&module);
    text_free(&source);
    CHECK(files_remove_tree(QUOTE_DIR));
}

void test_syms(void)
{
    unit_without_id();
    program_without_functions();
    map_names();
    map_last_line_open();
    build_id_of_notice();
    object_no_file();
    configuration_fifo();
    report_fifo();
    report_universal();
    broken_entries();
    inventory_quoted_names();
}
