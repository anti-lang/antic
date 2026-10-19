/* S312 and M301. The text builder, the copy of an object and the copy of
   an owned buffer stop the program through anti_rt_out_of_memory when
   the C library refuses their memory, as decision 4 of "Decisions of
   2026-10-11" says, and never come back with a literal or NULL. The
   listing of anti.fs does the same, which T139 holds, and the block of
   a snapshot on the heap stops with the same form of line. The memory of the
   start of a program ends it with status 70, as a refusal at start does.

   The files of the runtime this program links are compiled with malloc
   and realloc named refused_malloc and refused_realloc, which give NULL
   once refusing is set and call the C library before. The argument names
   one case, and tests/runners/run_rt_oom.cmake checks the stop. A case that comes
   back prints what it got and ends with status 0. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "fail.h"
#include "object.h"
#include "plugin.h"
#include "snapshot.h"
#include "std.h"

void *refused_malloc(size_t size);
void *refused_realloc(void *p, size_t size);

static int refusing;

void *refused_malloc(size_t size)
{
    return refusing ? NULL : malloc(size);
}

void *refused_realloc(void *p, size_t size)
{
    return refusing ? NULL : realloc(p, size);
}

/* The stand-ins of what src/rt/object.c calls in the loader of plugins,
   which counts the objects of a library. No table here is a library's. */
void anti_rt_plugin_created(const void *table)
{
    (void)table;
}

void anti_rt_plugin_destroyed(const void *table)
{
    (void)table;
}

/* The copy entry of the class below, which copies the table alone. */
static void copy_one(struct anti_object *from, struct anti_object *to)
{
    to->table = from->table;
}

int main(int argc, char **argv)
{
    const char *name;
    struct anti_builder b = {0};
    struct anti_text text = {NULL, 0};
    void *made = NULL;

    if (argc != 2) {
        fputs("usage: rt_oom_tests take|append|fill|append_count|"
              "fill_count|copy_buffer|dup|fs_list\n", stderr);
        return 2;
    }
    name = argv[1];
    if (strcmp(name, "take") == 0) {
        /* An empty builder holds no memory, and its text takes a byte. */
        refusing = 1;
        text = anti_rt_builder_take(&b);
    } else if (strcmp(name, "append") == 0) {
        refusing = 1;
        anti_rt_builder_append(&b, (const unsigned char *)"four", 4);
    } else if (strcmp(name, "fill") == 0) {
        refusing = 1;
        anti_rt_builder_fill(&b, 0, ' ', 4);
    } else if (strcmp(name, "append_count") == 0) {
        /* The bytes would fit in an int64_t, and their NUL does not. */
        anti_rt_builder_append(&b, (const unsigned char *)"four", 4);
        anti_rt_builder_append(&b, (const unsigned char *)"four",
                               INT64_MAX - 4);
    } else if (strcmp(name, "fill_count") == 0) {
        anti_rt_builder_append(&b, (const unsigned char *)"four", 4);
        anti_rt_builder_fill(&b, 0, ' ', INT64_MAX);
    } else if (strcmp(name, "start") == 0) {
        /* The start of a program ends with status 70 and one line. */
        refusing = 1;
        made = anti_rt_start_alloc(16);
    } else if (strcmp(name, "copy_buffer") == 0) {
        refusing = 1;
        made = anti_rt_copy_buffer("three", 3);
    } else if (strcmp(name, "dup") == 0) {
        static const struct anti_descriptor one = {
            (const unsigned char *)"One", 3, NULL, 24, 0, NULL, 0, NULL,
            NULL, 0, 0, NULL, (const unsigned char *)"0.0.0", 5, NULL, 0,
            NULL
        };
        const void *table[ANTI_ENTRY_HOOK] = {0};
        struct anti_object object;

        table[ANTI_ENTRY_DESCRIPTOR] = &one;
        table[ANTI_ENTRY_COPY] = anti_rt_body_entry((anti_rt_body)copy_one);
        object.table = (const struct anti_descriptor *const *)table;
        refusing = 1;
        made = anti_rt_dup(&object, &one);
    } else if (strcmp(name, "snapshot") == 0) {
        /* The block of a snapshot on the heap, whose line has the form of
           every failure of memory of the runtime. */
        refusing = 1;
        made = anti_rt_snapshot_new(16);
    } else if (strcmp(name, "fs_list") == 0) {
        /* T139. The listing of anti.fs fails with ENOMEM, and fs.list
           passes the failure on. */
        int64_t count = 0;

        refusing = 1;
        made = anti_rt_fs_list((const unsigned char *)".", 1, &count);
    } else {
        fprintf(stderr, "no case %s\n", name);
        return 2;
    }
    refusing = 0;
    printf("came back: text %p of %lld bytes, builder of %lld, block %p\n",
           (const void *)text.ptr, (long long)text.len, (long long)b.length,
           made);
    return 0;
}
