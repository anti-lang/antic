/* The file helpers of anti, the one reader and writer and the walk over
   a directory tree. A read that fails part of the way now fails. So do a
   write the disk refuses and a directory that cannot be read. A link
   back up the tree now ends the walk. */
#if !defined(_WIN32)
/* chmod, symlink, mkfifo and geteuid are POSIX, outside the C11
   library. */
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/anti/platform.h"
#include "../binary_stdio.h"
#include "check.h"
#include "files.h"
#include "text.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define FILE_NAME "unit-files.bin"
#define TREE "unit-files-tree"

/* A name outside ASCII and outside every ANSI code page: u with
   diaeresis and the CJK ideograph for middle, in UTF-8 and in UTF-16. */
#define UTF8_TREE "unit-files-\xc3\xbc\xe4\xb8\xad"
#define UTF8_DIR UTF8_TREE "/src/\xc3\xbc"
#define UTF8_FILE UTF8_DIR "/\xe4\xb8\xad.anti"
#define WIDE_TREE L"unit-files-\u00fc\u4e2d"

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

/* M31. Every helper takes a path as UTF-8, and on Windows the names on
   the disk are the UTF-16 of that text. The helpers called the ANSI
   entry points of Windows, which read the bytes in the code page of the
   machine and wrote a directory of another name. */
static void utf8_names(void)
{
    struct files_list found = {0};
    struct text bytes = {0};
    struct text back = {0};

    files_remove_tree(UTF8_TREE);
    CHECK(files_make_dirs(UTF8_DIR));
    CHECK(files_exists(UTF8_DIR));
#if defined(_WIN32)
    CHECK(GetFileAttributesW(WIDE_TREE L"\\src\\\u00fc") !=
          INVALID_FILE_ATTRIBUTES);
#endif
    text_append(&bytes, "anti");
    CHECK(files_write(UTF8_FILE, &bytes));
    CHECK(files_copy_program(UTF8_FILE, UTF8_TREE "/src/copy.anti"));
    CHECK(files_list_tree(UTF8_TREE, ".anti", &found));
    CHECK(found.count == 2);
    if (found.count == 2) {
        CHECK_STR(text_cstr(&found.items[0]), UTF8_TREE "/src/copy.anti");
        CHECK_STR(text_cstr(&found.items[1]), UTF8_FILE);
        CHECK(files_read(text_cstr(&found.items[1]), &back));
        CHECK_STR(text_cstr(&back), "anti");
    }
    files_list_free(&found);
    CHECK(files_remove_tree(UTF8_TREE));
    CHECK(!files_exists(UTF8_TREE));
#if defined(_WIN32)
    CHECK(GetFileAttributesW(WIDE_TREE) == INVALID_FILE_ATTRIBUTES);
#endif
    text_free(&bytes);
    text_free(&back);
}

/* The calls of the platform layer of anti that the helpers stand on. */
static void anti_layer(void)
{
    struct text bytes = {0};

    files_remove_tree(TREE);
    CHECK(platform_kind(TREE, false) == PLATFORM_MISSING);
    CHECK(platform_kind(TREE, true) == PLATFORM_MISSING);
    CHECK(platform_make_dir(TREE));
    CHECK(platform_make_dir(TREE));
    CHECK(platform_kind(TREE, false) == PLATFORM_DIRECTORY);
    text_append(&bytes, "anti");
    CHECK(files_write(TREE "/a.bin", &bytes));
    CHECK(platform_kind(TREE "/a.bin", true) == PLATFORM_FILE);
    CHECK(platform_rename(TREE "/a.bin", TREE "/b.bin"));
    CHECK(platform_kind(TREE "/a.bin", true) == PLATFORM_MISSING);
    CHECK(platform_kind(TREE "/b.bin", true) == PLATFORM_FILE);
    CHECK(!platform_rename(TREE "/a.bin", TREE "/c.bin"));
    CHECK(platform_make_dir(TREE "/empty"));
    CHECK(platform_remove_entry(TREE "/empty"));
    CHECK(platform_kind(TREE "/empty", true) == PLATFORM_MISSING);
    CHECK(platform_remove_entry(TREE "/b.bin"));
    CHECK(!platform_remove_entry(TREE "/b.bin"));
#if !defined(_WIN32)
    CHECK(files_write(TREE "/a.bin", &bytes));
    CHECK(chmod(TREE "/a.bin", 0750) == 0);
    CHECK(files_write(TREE "/b.bin", &bytes));
    CHECK(platform_copy_permissions(TREE "/a.bin", TREE "/b.bin"));
    {
        struct stat st;
        CHECK(stat(TREE "/b.bin", &st) == 0 && (st.st_mode & 07777) == 0750);
    }
    /* A link to a directory is a link, and removing it leaves the
       directory. One that leads nowhere is missing once followed. */
    CHECK(platform_make_dir(TREE "/dir"));
    CHECK(symlink("dir", TREE "/to-dir") == 0);
    CHECK(symlink("absent", TREE "/to-nothing") == 0);
    CHECK(platform_kind(TREE "/to-dir", false) == PLATFORM_LINK);
    CHECK(platform_kind(TREE "/to-dir", true) == PLATFORM_DIRECTORY);
    CHECK(platform_kind(TREE "/to-nothing", false) == PLATFORM_LINK);
    CHECK(platform_kind(TREE "/to-nothing", true) == PLATFORM_MISSING);
    CHECK(platform_remove_entry(TREE "/to-dir"));
    CHECK(platform_kind(TREE "/dir", false) == PLATFORM_DIRECTORY);
#endif
    CHECK(files_remove_tree(TREE));
    CHECK(platform_kind(TREE, false) == PLATFORM_MISSING);
    text_free(&bytes);
}

/* M42: files_read_file reads a regular file, also through a link, and
   refuses a directory, a device and a FIFO, which it neither reads
   without end nor waits on. */
static void regular_files(void)
{
    struct text bytes = {0};
    struct text back = {0};

    files_remove_tree(TREE);
    CHECK(platform_make_dir(TREE));
    text_append(&bytes, "anti");
    CHECK(files_write(TREE "/a.bin", &bytes));
    CHECK(files_read_file(TREE "/a.bin", &back));
    CHECK_STR(text_cstr(&back), "anti");
    CHECK(!files_read_file(TREE, &back));
    CHECK(!files_read_file(TREE "/absent", &back));
    CHECK_STR(text_cstr(&back), "anti");
#if defined(_WIN32)
    CHECK(!files_read_file("NUL", &back));
#else
    CHECK(symlink("a.bin", TREE "/link") == 0);
    CHECK(files_read_file(TREE "/link", &back));
    CHECK_STR(text_cstr(&back), "antianti");
    CHECK(!files_read_file("/dev/zero", &back));
    CHECK(!files_read_file("/dev/null", &back));
    CHECK(mkfifo(TREE "/fifo", 0600) == 0);
    CHECK(!files_read_file(TREE "/fifo", &back));
#endif
    CHECK(files_remove_tree(TREE));
    text_free(&bytes);
    text_free(&back);
}

#if defined(_WIN32)
/* Map the file at wide as a reader that closed its handle and kept the
   section, which Windows' Application Identity service does with a
   program that just ran, as data or with image set as an image. Returns
   the view, and the section in section. */
static const void *map_file(const wchar_t *wide, bool image, HANDLE *section)
{
    DWORD protect = image ? PAGE_READONLY | SEC_IMAGE : PAGE_READONLY;
    HANDLE file = CreateFileW(wide, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, 0, NULL);
    const void *view = NULL;

    *section = NULL;
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    *section = CreateFileMappingW(file, NULL, protect, 0, 0, NULL);
    if (*section != NULL) {
        view = MapViewOfFile(*section, FILE_MAP_READ, 0, 0, 0);
    }
    CloseHandle(file);
    return view;
}

/* A program in dist/ that just ran is mapped by the Application Identity
   service of Windows for up to 1.6 s. Windows refuses to truncate such a
   file with ERROR_USER_MAPPED_FILE, and `anti build` could not copy the
   program again. The copy replaces the mapped file at once, and the
   reader keeps the old bytes until it lets go. */
static void copy_over_mapped(void)
{
    struct text old = {0};
    struct text fresh = {0};
    struct text back = {0};
    HANDLE section;
    const void *view;

    files_remove_tree(TREE);
    CHECK(platform_make_dir(TREE));
    text_append(&old, "old program");
    text_append(&fresh, "new program");
    CHECK(files_write(TREE "/app.exe", &old));
    CHECK(files_write(TREE "/build.exe", &fresh));
    view = map_file(L"" TREE L"\\app.exe", false, &section);
    CHECK(view != NULL);
    CHECK(files_copy_program(TREE "/build.exe", TREE "/app.exe"));
    CHECK(files_read(TREE "/app.exe", &back));
    CHECK_STR(text_cstr(&back), "new program");
    CHECK(view != NULL && memcmp(view, "old program", 11) == 0);
    CHECK(!files_exists(TREE "/app.exe" FILES_NEW_SUFFIX));
    if (view != NULL) {
        UnmapViewOfFile(view);
    }
    if (section != NULL) {
        CloseHandle(section);
    }
    CHECK(files_remove_tree(TREE));
    text_free(&old);
    text_free(&fresh);
    text_free(&back);
}

/* The Application Identity service also holds a program as an image for
   a moment, and Windows then refuses to rename another file over it with
   ERROR_ACCESS_DENIED. It still lets the program be renamed, so the copy
   moves it aside first. The old program stays under its name with
   PLATFORM_OLD_SUFFIX while it is mapped, and the next copy removes it.
   The program here is the unit test itself, since an image section
   needs a real executable. */
static void copy_over_image(void)
{
    wchar_t self[MAX_PATH];
    struct text fresh = {0};
    struct text back = {0};
    HANDLE section;
    const void *view;
    DWORD length = GetModuleFileNameW(NULL, self, MAX_PATH);

    CHECK(length > 0 && length < MAX_PATH);
    files_remove_tree(TREE);
    CHECK(platform_make_dir(TREE));
    CHECK(CopyFileW(self, L"" TREE L"\\app.exe", FALSE));
    text_append(&fresh, "new program");
    CHECK(files_write(TREE "/build.exe", &fresh));
    view = map_file(L"" TREE L"\\app.exe", true, &section);
    CHECK(view != NULL);
    CHECK(files_copy_program(TREE "/build.exe", TREE "/app.exe"));
    CHECK(files_read(TREE "/app.exe", &back));
    CHECK_STR(text_cstr(&back), "new program");
    CHECK(!files_exists(TREE "/app.exe" FILES_NEW_SUFFIX));
    if (view != NULL) {
        UnmapViewOfFile(view);
    }
    if (section != NULL) {
        CloseHandle(section);
    }
    CHECK(files_copy_program(TREE "/build.exe", TREE "/app.exe"));
    CHECK(!files_exists(TREE "/app.exe" PLATFORM_OLD_SUFFIX));
    CHECK(files_remove_tree(TREE));
    text_free(&fresh);
    text_free(&back);
}

/* A program that a reader holds without sharing it for deletion cannot
   be replaced. The copy then fails, the old program stays whole and no
   file of the copy is left beside it. */
static void copy_over_held(void)
{
    struct text old = {0};
    struct text fresh = {0};
    struct text back = {0};
    HANDLE held;

    files_remove_tree(TREE);
    CHECK(platform_make_dir(TREE));
    text_append(&old, "old program");
    text_append(&fresh, "new program");
    CHECK(files_write(TREE "/app.exe", &old));
    CHECK(files_write(TREE "/build.exe", &fresh));
    held = CreateFileW(L"" TREE L"\\app.exe", GENERIC_READ, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING, 0, NULL);
    CHECK(held != INVALID_HANDLE_VALUE);
    CHECK(!files_copy_program(TREE "/build.exe", TREE "/app.exe"));
    if (held != INVALID_HANDLE_VALUE) {
        CloseHandle(held);
    }
    CHECK(files_read(TREE "/app.exe", &back));
    CHECK_STR(text_cstr(&back), "old program");
    CHECK(!files_exists(TREE "/app.exe" FILES_NEW_SUFFIX));
    CHECK(files_remove_tree(TREE));
    text_free(&old);
    text_free(&fresh);
    text_free(&back);
}
#endif

/* files_grow keeps the elements, zeroes the new room and doubles it. */
static void grow_array(void)
{
    size_t room = 0;
    size_t *items = NULL;
    size_t *zeroed = files_array(3, sizeof *zeroed);
    size_t i;
    bool kept = true;
    bool clear = true;

    CHECK(zeroed[0] == 0 && zeroed[2] == 0);
    free(zeroed);
    for (i = 0; i < 40; i++) {
        if (i == room) {
            items = files_grow(items, &room, sizeof *items);
        }
        items[i] = i + 1;
    }
    CHECK(room == 64);
    for (i = 0; i < 40; i++) {
        kept = kept && items[i] == i + 1;
    }
    for (i = 40; i < room; i++) {
        clear = clear && items[i] == 0;
    }
    CHECK(kept);
    CHECK(clear);
    free(items);
}

/* Both separators end a directory on every host. */
static void base_names(void)
{
    CHECK_STR(files_base_name("a/b/c.anti"), "c.anti");
    CHECK_STR(files_base_name("a\\b\\c.anti"), "c.anti");
    CHECK_STR(files_base_name("a\\b/c.anti"), "c.anti");
    CHECK_STR(files_base_name("c.anti"), "c.anti");
    CHECK_STR(files_base_name("a/"), "");
}

void test_files(void)
{
    grow_array();
    base_names();
    read_whole();
    read_error();
    write_errors();
    utf8_names();
    anti_layer();
    regular_files();
#if defined(_WIN32)
    copy_over_mapped();
    copy_over_image();
    copy_over_held();
#else
    walk_unreadable();
    walk_links();
#endif
}
