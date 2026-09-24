/* The zip reader of anti symbols. The archives are cut short or point
   outside the file, and the deflate streams are broken or unpack past
   the size their entry declares. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "text.h"
#include "zip.h"

#define ARCHIVE "test_zip.zip"

static void put16(struct text *out, unsigned long value)
{
    unsigned char bytes[2];

    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
    text_append_bytes(out, bytes, sizeof bytes);
}

static void put32(struct text *out, unsigned long value)
{
    put16(out, value & 0xffff);
    put16(out, (value >> 16) & 0xffff);
}

static unsigned long crc_of(const unsigned char *bytes, size_t length)
{
    unsigned long crc = 0xffffffffUL;
    size_t i;
    int bit;

    for (i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xedb88320UL : crc >> 1;
        }
    }
    return crc ^ 0xffffffffUL;
}

/* One entry as the archive states it. The fields name what the central
   directory claims, which a test may set apart from the data. */
struct entry {
    const char *name;
    const unsigned char *data;
    size_t packed;
    unsigned long size;
    unsigned long crc;
    unsigned method;
};

/* An archive of count entries at path. */
static void write_archive(const struct entry *entries, size_t count)
{
    struct text out = {0};
    struct text directory = {0};
    FILE *f;
    size_t i;

    for (i = 0; i < count; i++) {
        const struct entry *e = &entries[i];
        size_t name_length = strlen(e->name);
        unsigned long local = (unsigned long)out.length;
        put32(&out, 0x04034b50UL);
        put16(&out, 20);
        put16(&out, 0);
        put16(&out, e->method);
        put32(&out, 0);
        put32(&out, e->crc);
        put32(&out, (unsigned long)e->packed);
        put32(&out, e->size);
        put16(&out, name_length);
        put16(&out, 0);
        text_append_bytes(&out, e->name, name_length);
        text_append_bytes(&out, e->data, e->packed);
        put32(&directory, 0x02014b50UL);
        put16(&directory, 20);
        put16(&directory, 20);
        put16(&directory, 0);
        put16(&directory, e->method);
        put32(&directory, 0);
        put32(&directory, e->crc);
        put32(&directory, (unsigned long)e->packed);
        put32(&directory, e->size);
        put16(&directory, name_length);
        put16(&directory, 0);
        put16(&directory, 0);
        put16(&directory, 0);
        put16(&directory, 0);
        put32(&directory, 0);
        put32(&directory, local);
        text_append_bytes(&directory, e->name, name_length);
    }
    {
        unsigned long at = (unsigned long)out.length;
        text_append_bytes(&out, directory.data, directory.length);
        put32(&out, 0x06054b50UL);
        put16(&out, 0);
        put16(&out, 0);
        put16(&out, count);
        put16(&out, count);
        put32(&out, (unsigned long)directory.length);
        put32(&out, at);
        put16(&out, 0);
    }
    f = fopen(ARCHIVE, "wb");
    if (f != NULL) {
        fwrite(out.data, 1, out.length, f);
        fclose(f);
    }
    text_free(&out);
    text_free(&directory);
}

/* The bytes of the archive, for a test that breaks them. */
static void read_archive(struct text *out)
{
    FILE *f = fopen(ARCHIVE, "rb");
    char buffer[4096];
    size_t n;

    if (f == NULL) {
        return;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
}

static void write_bytes(const char *bytes, size_t length)
{
    FILE *f = fopen(ARCHIVE, "wb");

    if (f != NULL) {
        fwrite(bytes, 1, length, f);
        fclose(f);
    }
}

/* A reader that fails leaves out empty, so a caller frees nothing. */
static bool read_fails_empty(void)
{
    struct zip_archive z;
    bool ok = zip_read(ARCHIVE, &z);
    bool empty = z.items == NULL && z.count == 0 && z.bytes.data == NULL;

    zip_archive_free(&z);
    return !ok && empty;
}

/* A deflate stream written bit by bit, the way RFC 1951 packs it. */
struct bits {
    unsigned char data[1 << 14];
    size_t length;
    unsigned long held;
    int count;
};

static void bits_put(struct bits *b, unsigned long value, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        b->held |= ((value >> i) & 1) << b->count;
        if (++b->count == 8) {
            b->data[b->length++] = (unsigned char)b->held;
            b->held = 0;
            b->count = 0;
        }
    }
}

/* A Huffman code goes out from its high bit. */
static void bits_code(struct bits *b, unsigned long code, int n)
{
    int i;

    for (i = n - 1; i >= 0; i--) {
        bits_put(b, (code >> i) & 1, 1);
    }
}

static void bits_flush(struct bits *b)
{
    if (b->count > 0) {
        b->data[b->length++] = (unsigned char)b->held;
        b->held = 0;
        b->count = 0;
    }
}

/* The fixed code of a literal below 144, and of the end of a block. */
static void fixed_literal(struct bits *b, unsigned char c)
{
    bits_code(b, 0x30u + c, 8);
}

static void fixed_end(struct bits *b)
{
    bits_code(b, 0, 7);
}

/* A copy of 258 bytes from one byte back: length symbol 285 and
   distance code 0, neither with extra bits. */
static void fixed_copy_258(struct bits *b)
{
    bits_code(b, 0xc5, 8);
    bits_code(b, 0, 5);
}

static struct entry deflated(const char *name, const struct bits *b,
                             unsigned long size, unsigned long crc)
{
    struct entry e;

    e.name = name;
    e.data = b->data;
    e.packed = b->length;
    e.size = size;
    e.crc = crc;
    e.method = 8;
    return e;
}

/* The one entry of the archive unpacked. Gives whether it unpacked, and
   the length the output reached. */
static bool unpack_one(struct text *out)
{
    struct zip_archive z;
    bool ok = false;

    if (zip_read(ARCHIVE, &z) && z.count == 1) {
        ok = zip_unpack(&z, 0, out);
    }
    zip_archive_free(&z);
    return ok;
}

static void stored_and_deflate(void)
{
    static const unsigned char hello[] = "hello";
    struct entry e;
    struct bits b;
    struct text out = {0};

    e.name = "a.txt";
    e.data = hello;
    e.packed = 5;
    e.size = 5;
    e.crc = crc_of(hello, 5);
    e.method = 0;
    write_archive(&e, 1);
    CHECK(unpack_one(&out));
    CHECK_STR(text_cstr(&out), "hello");
    text_free(&out);

    /* One fixed block: `a`, then a copy of 258 from one byte back. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 1, 2);
    fixed_literal(&b, 'a');
    fixed_copy_258(&b);
    fixed_end(&b);
    bits_flush(&b);
    {
        unsigned char expected[259];
        memset(expected, 'a', sizeof expected);
        e = deflated("b.txt", &b, 259, crc_of(expected, sizeof expected));
        write_archive(&e, 1);
        CHECK(unpack_one(&out));
        CHECK(out.length == 259);
        text_free(&out);
    }
}

/* M11: a stream that unpacks to far more than its entry declares stops
   at the declared size, rather than growing until memory runs out. */
static void deflate_bound(void)
{
    struct bits b;
    struct text out = {0};
    struct entry e;
    int i;

    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 1, 2);
    fixed_literal(&b, 'a');
    for (i = 0; i < 8000; i++) {
        fixed_copy_258(&b);
    }
    fixed_end(&b);
    bits_flush(&b);
    e = deflated("bomb.txt", &b, 10, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    CHECK(out.length <= 10);
    text_free(&out);

    /* A stored block inside deflate is held to the size too. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 0, 2);
    bits_flush(&b);
    b.data[b.length++] = 0x00;
    b.data[b.length++] = 0x10;
    b.data[b.length++] = 0xff;
    b.data[b.length++] = 0xef;
    memset(b.data + b.length, 'x', 0x1000);
    b.length += 0x1000;
    e = deflated("stored.txt", &b, 16, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    CHECK(out.length <= 16);
    text_free(&out);
}

static void broken_deflate(void)
{
    struct bits b;
    struct text out = {0};
    struct entry e;
    int i;

    /* Block type 3 is reserved. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 3, 2);
    bits_flush(&b);
    e = deflated("reserved", &b, 1, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);

    /* A stream cut before its end code. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 1, 2);
    fixed_literal(&b, 'a');
    bits_flush(&b);
    e = deflated("cut", &b, 1, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);

    /* A copy before any output. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 1, 2);
    fixed_copy_258(&b);
    fixed_end(&b);
    bits_flush(&b);
    e = deflated("early", &b, 258, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);

    /* The same copy does not reach back into what out held before the
       entry, even where the bytes it would copy match the CRC. */
    {
        unsigned char copied[258];
        memset(copied, 'z', sizeof copied);
        e = deflated("early", &b, 258, crc_of(copied, sizeof copied));
        write_archive(&e, 1);
        text_append(&out, "xyz");
        CHECK(!unpack_one(&out));
        text_free(&out);
    }

    /* A stored block whose length and its complement disagree. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 0, 2);
    bits_flush(&b);
    b.data[b.length++] = 0x02;
    b.data[b.length++] = 0x00;
    b.data[b.length++] = 0x00;
    b.data[b.length++] = 0x00;
    b.data[b.length++] = 'h';
    b.data[b.length++] = 'i';
    e = deflated("complement", &b, 2, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);

    /* A dynamic block whose code of code lengths is oversubscribed: all
       nineteen lengths are 1. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 2, 2);
    bits_put(&b, 0, 5);
    bits_put(&b, 0, 5);
    bits_put(&b, 15, 4);
    for (i = 0; i < 19; i++) {
        bits_put(&b, 1, 3);
    }
    bits_flush(&b);
    e = deflated("oversubscribed", &b, 1, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);

    /* A stream shorter than the size its entry declares. */
    memset(&b, 0, sizeof b);
    bits_put(&b, 1, 1);
    bits_put(&b, 1, 2);
    fixed_literal(&b, 'a');
    fixed_end(&b);
    bits_flush(&b);
    e = deflated("short", &b, 100, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);

    /* The right bytes under the wrong CRC. */
    e = deflated("crc", &b, 1, 0);
    write_archive(&e, 1);
    CHECK(!unpack_one(&out));
    text_free(&out);
}

/* Offsets of the central entry of a one-entry archive whose name is
   a.txt and whose data is five bytes: 30 + 5 + 5 = 40. */
enum { CENTRAL = 40, END = CENTRAL + 46 + 5 };

static void broken_archives(void)
{
    static const unsigned char hello[] = "hello";
    struct text good = {0};
    struct text bad = {0};
    struct entry e;
    size_t cut;

    e.name = "a.txt";
    e.data = hello;
    e.packed = 5;
    e.size = 5;
    e.crc = crc_of(hello, 5);
    e.method = 0;
    write_archive(&e, 1);
    read_archive(&good);
    CHECK(good.length == END + 22);

    /* Too short to hold an end record, and every cut of the archive. */
    write_bytes("PK", 2);
    CHECK(read_fails_empty());
    for (cut = 1; cut < good.length; cut++) {
        write_bytes(good.data, good.length - cut);
        CHECK(read_fails_empty());
    }

    /* M17: the data of the entry runs past the central directory. The
       reader fails after it has read the name of the entry, and leaves
       nothing behind. */
    text_append_bytes(&bad, good.data, good.length);
    bad.data[CENTRAL + 20] = (char)0xff;
    bad.data[CENTRAL + 21] = (char)0xff;
    write_bytes(bad.data, bad.length);
    CHECK(read_fails_empty());
    text_free(&bad);

    /* The end record counts two entries and the directory holds one. */
    text_append_bytes(&bad, good.data, good.length);
    bad.data[END + 10] = 2;
    write_bytes(bad.data, bad.length);
    CHECK(read_fails_empty());
    text_free(&bad);

    /* The directory stands past the end record. */
    text_append_bytes(&bad, good.data, good.length);
    bad.data[END + 19] = 0x7f;
    write_bytes(bad.data, bad.length);
    CHECK(read_fails_empty());
    text_free(&bad);

    /* A name that runs past the directory. */
    text_append_bytes(&bad, good.data, good.length);
    bad.data[CENTRAL + 28] = (char)0xff;
    bad.data[CENTRAL + 29] = (char)0xff;
    write_bytes(bad.data, bad.length);
    CHECK(read_fails_empty());
    text_free(&bad);

    /* A local header outside the file. */
    text_append_bytes(&bad, good.data, good.length);
    bad.data[CENTRAL + 45] = 0x7f;
    write_bytes(bad.data, bad.length);
    CHECK(read_fails_empty());
    text_free(&bad);

    /* A local header whose name and extra field run past the data. */
    text_append_bytes(&bad, good.data, good.length);
    bad.data[28] = (char)0xff;
    bad.data[29] = (char)0xff;
    write_bytes(bad.data, bad.length);
    CHECK(read_fails_empty());
    text_free(&bad);

    text_free(&good);
    remove(ARCHIVE);
}

/* A name longer than 65535 bytes and more than 65535 entries have no
   field of the format that holds them, so the writer refuses both
   rather than cut them. */
static void write_limits(void)
{
    struct zip_entry *many = calloc(65536, sizeof *many);
    char *name = malloc(70000 + 1);
    struct zip_entry one;
    FILE *f;
    size_t i;

    CHECK(many != NULL && name != NULL);
    if (many == NULL || name == NULL) {
        free(many);
        free(name);
        return;
    }
    remove(ARCHIVE);
    memset(name, 'n', 70000);
    name[70000] = '\0';
    memset(&one, 0, sizeof one);
    one.name = name;
    one.bytes = "x";
    one.size = 1;
    CHECK(!zip_write(ARCHIVE, &one, 1));
    f = fopen(ARCHIVE, "rb");
    CHECK(f == NULL);
    if (f != NULL) {
        fclose(f);
    }
    name[8] = '\0';
    for (i = 0; i < 65536; i++) {
        many[i].name = name;
        many[i].bytes = "x";
        many[i].size = 1;
    }
    CHECK(!zip_write(ARCHIVE, many, 65536));
    CHECK(zip_write(ARCHIVE, many, 65535));
    remove(ARCHIVE);
    free(many);
    free(name);
}

void test_zip(void)
{
    fputs("anti test: the messages below are expected\n", stderr);
    stored_and_deflate();
    deflate_bound();
    broken_deflate();
    broken_archives();
    write_limits();
}
