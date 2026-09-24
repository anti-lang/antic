/* The file helpers of anti, the one reader and writer and the walk over
   a directory tree. A read that fails part of the way now fails. So do a
   write the disk refuses and a directory that cannot be read. A link
   back up the tree now ends the walk. */
#if !defined(_WIN32)
/* chmod, symlink and geteuid are POSIX, outside the C11 library. */
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "files.h"
#include "text.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

#define FILE_NAME "unit-files.bin"
#define TREE "unit-files-tree"

/* A file larger than any buffer of the reader comes back whole. */
static void read_whole(void)
{
    struct text bytes = {0};
    struct text back = {0};
    size_t i;

    for (i = 0; i < 200000; i++) {
        char c = (char)('a' + (char)(i % 26));
        text_append_bytes(&bytes, &c, 1);
    }
    CHECK(files_write(FILE_NAME, &bytes));
    CHECK(files_read(FILE_NAME, &back));
    CHECK(back.length == bytes.length);
    CHECK(back.length == bytes.length &&
          memcmp(back.data, bytes.data, bytes.length) == 0);
    remove(FILE_NAME);
    text_free(&bytes);
    text_free(&back);
}

/* M1. A directory opens for reading on POSIX systems and then fails its
   first read. The reader took that for the end of an empty file. It
   fails now and leaves the text as it was. */
static void read_error(void)
{
    struct text back = {0};

    files_remove_tree(TREE);
    CHECK(files_make_dirs(TREE));
    text_append(&back, "kept");
    CHECK(!files_read(TREE, &back));
    CHECK_STR(text_cstr(&back), "kept");
    fputs("anti test: the message below is expected\n", stderr);
    CHECK(!files_read_reported(TREE, &back));
    CHECK_STR(text_cstr(&back), "kept");
    remove("unit-files-absent.bin");
    CHECK(!files_read("unit-files-absent.bin", &back));
    files_remove_tree(TREE);
    text_free(&back);
}

/* Rule 26. The writer fails where fopen fails, and where the disk refuses
   the bytes when the buffer is flushed at fclose. /dev/full refuses
   every write, and only Linux has it. */
static void write_errors(void)
{
    struct text bytes = {0};

    text_append(&bytes, "anti");
    files_remove_tree(TREE);
    fputs("anti test: the message below is expected\n", stderr);
    CHECK(!files_write(TREE "/absent/file.bin", &bytes));
#if defined(__linux__)
    fputs("anti test: the message below is expected\n", stderr);
    CHECK(!files_write("/dev/full", &bytes));
#endif
    text_free(&bytes);
}

#if !defined(_WIN32)
static void touch(const char *path)
{
    struct text empty = {0};

    CHECK(files_write(path, &empty));
}

/* M15. A directory without permission to read failed nothing and added
   nothing. A superuser reads it anyway, so the case is passed over. */
static void walk_unreadable(void)
{
    struct files_list found = {0};

    files_remove_tree(TREE);
    CHECK(files_make_dirs(TREE "/closed"));
    touch(TREE "/closed/a.anti");
    CHECK(chmod(TREE "/closed", 0) == 0);
    if (geteuid() != 0) {
        fputs("anti test: the message below is expected\n", stderr);
        CHECK(!files_list_tree(TREE, ".anti", &found));
    }
    CHECK(chmod(TREE "/closed", 0755) == 0);
    files_list_free(&found);
    /* A directory that does not exist adds nothing and is no error. */
    CHECK(files_list_tree(TREE "/absent", ".anti", &found));
    CHECK(found.count == 0);
    files_list_free(&found);
    files_remove_tree(TREE);
}

/* M15. A link to a parent directory led the walk round until opendir ran
   out of descriptors. A link to a directory is not followed now, and a
   link to a file is listed as the file. */
static void walk_links(void)
{
    struct files_list found = {0};

    files_remove_tree(TREE);
    CHECK(files_make_dirs(TREE "/src"));
    touch(TREE "/src/a.anti");
    CHECK(symlink("..", TREE "/src/up") == 0);
    CHECK(symlink("a.anti", TREE "/src/b.anti") == 0);
    CHECK(files_list_tree(TREE, ".anti", &found));
    CHECK(found.count == 2);
    if (found.count == 2) {
        CHECK_STR(text_cstr(&found.items[0]), TREE "/src/a.anti");
        CHECK_STR(text_cstr(&found.items[1]), TREE "/src/b.anti");
    }
    files_list_free(&found);
    files_remove_tree(TREE);
}
#endif

void test_files(void)
{
    read_whole();
    read_error();
    write_errors();
#if !defined(_WIN32)
    walk_unreadable();
    walk_links();
#endif
}
