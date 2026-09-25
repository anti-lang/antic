/* anti symbols over archives and programs that name nothing: a unit of a
   deployment index with no id, and a program with no function. Each
   used to hand a null pointer to memcmp or qsort with a count of zero,
   which the sanitizer builds catch. */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "symmap.h"
#include "syms.h"
#include "target.h"
#include "text.h"
#include "zip.h"

#define ARCHIVE "test_syms-symbols.zip"
#define TRACE "test_syms-trace.txt"
#define PROGRAM "test_syms-program"
#define MAP "test_syms.map"

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");

    if (f != NULL) {
        fwrite(text, 1, strlen(text), f);
        fclose(f);
    }
}

/* S42: an index unit with no id beside an entry whose name starts with
   `/`. The id and the part before the slash are then both empty. */
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

/* S42: a program the ELF reader finds no function in still gets a map,
   with its header lines alone. */
static void program_without_functions(void)
{
    struct text map = {0};
    FILE *f;
    char buffer[256];
    size_t n;

    write_file(PROGRAM, "no program");
    CHECK(symmap_write(PROGRAM, TARGET_LINUX_X86_64, "-", MAP));
    f = fopen(MAP, "rb");
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

void test_syms(void)
{
    unit_without_id();
    program_without_functions();
    map_names();
}
