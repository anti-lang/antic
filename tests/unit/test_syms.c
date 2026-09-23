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

void test_syms(void)
{
    unit_without_id();
    program_without_functions();
}
