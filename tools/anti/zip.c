/* The zip writer of the symbols archive.

   DESIGN: the entries are stored and never compressed. The archive
   holds an executable and a text map, which a reader unpacks once. The
   bytes saved are worth neither a compressor nor a library. Every field
   that would carry a clock or a machine is fixed, so one input gives one
   archive on every host. */
#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text.h"

/* The four signatures of the format, and the version that a stored entry
   needs. */
enum {
    ZIP_LOCAL = 0x04034b50,
    ZIP_CENTRAL = 0x02014b50,
    ZIP_END = 0x06054b50,
    ZIP_VERSION = 20
};

/* 1980-01-01 in the date form of MS-DOS, which is the first date the
   format holds. The time is midnight. */
enum { ZIP_DATE = 0x0021, ZIP_TIME = 0 };

/* The mode of a file of the archive, in the high half of the external
   attributes, as a Unix writer puts it there. */
enum { ZIP_MODE_FILE = 0100644, ZIP_MODE_PROGRAM = 0100755 };

static void put16(struct text *out, unsigned value)
{
    char bytes[2];

    bytes[0] = (char)(value & 0xff);
    bytes[1] = (char)((value >> 8) & 0xff);
    text_append_bytes(out, bytes, sizeof bytes);
}

static void put32(struct text *out, unsigned long value)
{
    char bytes[4];

    bytes[0] = (char)(value & 0xff);
    bytes[1] = (char)((value >> 8) & 0xff);
    bytes[2] = (char)((value >> 16) & 0xff);
    bytes[3] = (char)((value >> 24) & 0xff);
    text_append_bytes(out, bytes, sizeof bytes);
}

/* The CRC-32 of the zip format, which is the one of IEEE 802.3. */
static unsigned long crc32_of(const char *bytes, size_t length)
{
    unsigned long crc = 0xffffffffUL;
    size_t i;
    int bit;

    for (i = 0; i < length; i++) {
        crc ^= (unsigned char)bytes[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xedb88320UL & (unsigned long)(-(long)(crc & 1)));
        }
    }
    return crc ^ 0xffffffffUL;
}

static bool read_file(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[65536];
    size_t n;

    if (f == NULL) {
        fprintf(stderr, "anti: cannot read %s\n", path);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
    return true;
}

bool zip_write(const char *path, const struct zip_entry *entries, size_t count)
{
    struct text out = {0};
    struct text directory = {0};
    unsigned long *offsets = calloc(count + 1, sizeof *offsets);
    FILE *f;
    size_t i;
    bool ok = true;

    if (offsets == NULL) {
        fputs("anti: out of memory\n", stderr);
        exit(70);
    }
    for (i = 0; ok && i < count; i++) {
        struct text bytes = {0};
        unsigned long crc;
        size_t name_length = strlen(entries[i].name);
        ok = read_file(entries[i].file, &bytes);
        if (!ok) {
            text_free(&bytes);
            break;
        }
        crc = crc32_of(bytes.data, bytes.length);
        offsets[i] = (unsigned long)out.length;
        put32(&out, ZIP_LOCAL);
        put16(&out, ZIP_VERSION);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, ZIP_TIME);
        put16(&out, ZIP_DATE);
        put32(&out, crc);
        put32(&out, (unsigned long)bytes.length);
        put32(&out, (unsigned long)bytes.length);
        put16(&out, (unsigned)name_length);
        put16(&out, 0);
        text_append_bytes(&out, entries[i].name, name_length);
        text_append_bytes(&out, bytes.data, bytes.length);
        put32(&directory, ZIP_CENTRAL);
        put16(&directory, ZIP_VERSION | (3u << 8));   /* written on Unix */
        put16(&directory, ZIP_VERSION);
        put16(&directory, 0);
        put16(&directory, 0);
        put16(&directory, ZIP_TIME);
        put16(&directory, ZIP_DATE);
        put32(&directory, crc);
        put32(&directory, (unsigned long)bytes.length);
        put32(&directory, (unsigned long)bytes.length);
        put16(&directory, (unsigned)name_length);
        put16(&directory, 0);
        put16(&directory, 0);
        put16(&directory, 0);
        put16(&directory, 0);
        put32(&directory, (unsigned long)(entries[i].executable
                                              ? ZIP_MODE_PROGRAM
                                              : ZIP_MODE_FILE)
                              << 16);
        put32(&directory, offsets[i]);
        text_append_bytes(&directory, entries[i].name, name_length);
        text_free(&bytes);
    }
    if (ok) {
        unsigned long start = (unsigned long)out.length;
        text_append_bytes(&out, directory.data, directory.length);
        put32(&out, ZIP_END);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, (unsigned)count);
        put16(&out, (unsigned)count);
        put32(&out, (unsigned long)directory.length);
        put32(&out, start);
        put16(&out, 0);
        f = fopen(path, "wb");
        if (f == NULL) {
            fprintf(stderr, "anti: cannot write %s\n", path);
            ok = false;
        } else {
            ok = out.length == 0 ||
                 fwrite(out.data, 1, out.length, f) == out.length;
            if (fclose(f) != 0 || !ok) {
                fprintf(stderr, "anti: cannot write %s\n", path);
                ok = false;
            }
        }
    }
    free(offsets);
    text_free(&out);
    text_free(&directory);
    return ok;
}
