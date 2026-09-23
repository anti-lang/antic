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
        if (entries[i].file != NULL) {
            ok = read_file(entries[i].file, &bytes);
        } else {
            text_append_bytes(&bytes, entries[i].bytes, entries[i].size);
        }
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

static unsigned get16(const unsigned char *p)
{
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static unsigned long get32(const unsigned char *p)
{
    return (unsigned long)p[0] | (unsigned long)p[1] << 8 |
           (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

/* The end record of the central directory, found from the end of the
   file back over the longest comment the format allows. */
static bool find_end(const struct text *bytes, size_t *at)
{
    const unsigned char *p = (const unsigned char *)bytes->data;
    size_t i;

    if (bytes->length < 22) {
        return false;
    }
    for (i = bytes->length - 22;; i--) {
        if (get32(p + i) == ZIP_END) {
            *at = i;
            return true;
        }
        if (i == 0 || bytes->length - i > 22 + 65535) {
            return false;
        }
    }
}

bool zip_read(const char *path, struct zip_archive *out)
{
    const unsigned char *p;
    size_t end;
    size_t at;
    size_t count;
    size_t i;

    memset(out, 0, sizeof *out);
    if (!read_file(path, &out->bytes)) {
        return false;
    }
    p = (const unsigned char *)out->bytes.data;
    if (!find_end(&out->bytes, &end)) {
        fprintf(stderr, "anti: %s is no zip archive\n", path);
        return false;
    }
    count = get16(p + end + 10);
    at = get32(p + end + 16);
    out->items = calloc(count + 1, sizeof *out->items);
    if (out->items == NULL) {
        fputs("anti: out of memory\n", stderr);
        exit(70);
    }
    for (i = 0; i < count; i++) {
        struct zip_item *item = &out->items[i];
        size_t name_length;
        size_t local;
        if (at + 46 > end || get32(p + at) != ZIP_CENTRAL) {
            fprintf(stderr, "anti: %s has a broken central directory\n",
                    path);
            return false;
        }
        name_length = get16(p + at + 28);
        item->method = get16(p + at + 10);
        item->crc = get32(p + at + 16);
        item->packed = get32(p + at + 20);
        item->size = get32(p + at + 24);
        local = get32(p + at + 42);
        if (at + 46 + name_length > end || local + 30 > at ||
            get32(p + local) != ZIP_LOCAL) {
            fprintf(stderr, "anti: %s has a broken entry\n", path);
            return false;
        }
        text_append_bytes(&item->name, p + at + 46, name_length);
        item->offset = local + 30 + get16(p + local + 26) +
                       get16(p + local + 28);
        if (item->offset + item->packed > at) {
            fprintf(stderr, "anti: %s: the data of %s runs past its end\n",
                    path, text_cstr(&item->name));
            return false;
        }
        out->count++;
        at += 46 + name_length + get16(p + at + 30) + get16(p + at + 32);
    }
    return true;
}

void zip_archive_free(struct zip_archive *a)
{
    size_t i;

    for (i = 0; i < a->count; i++) {
        text_free(&a->items[i].name);
    }
    free(a->items);
    text_free(&a->bytes);
    memset(a, 0, sizeof *a);
}

/* DESIGN: deflate as RFC 1951 gives it, decoded the way zlib's puff
   does: a table of the count of codes per length and the symbols in
   order of their code, walked one bit at a time. It is the slow form
   and the short one, and an archive of symbols is unpacked once. */
struct inflate {
    const unsigned char *in;
    size_t length;
    size_t at;
    unsigned long bits;
    int held;
    struct text *out;
};

struct huffman {
    short count[16];
    short symbol[288];
};

static int need(struct inflate *s, int n, unsigned long *value)
{
    while (s->held < n) {
        if (s->at == s->length) {
            return 0;
        }
        s->bits |= (unsigned long)s->in[s->at++] << s->held;
        s->held += 8;
    }
    *value = s->bits & ((1UL << n) - 1);
    s->bits >>= n;
    s->held -= n;
    return 1;
}

static int decode(struct inflate *s, const struct huffman *h)
{
    int code = 0;
    int first = 0;
    int index = 0;
    int length;

    for (length = 1; length < 16; length++) {
        unsigned long bit;
        int count;
        if (!need(s, 1, &bit)) {
            return -1;
        }
        code |= (int)bit;
        count = h->count[length];
        if (code - count < first) {
            return h->symbol[index + (code - first)];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

/* Build the table from the length of each symbol's code. Returns false
   for a set of lengths that is more than a complete code. */
static bool construct(struct huffman *h, const short *lengths, int n)
{
    short offsets[16];
    int left = 1;
    int i;

    memset(h->count, 0, sizeof h->count);
    for (i = 0; i < n; i++) {
        h->count[lengths[i]]++;
    }
    for (i = 1; i < 16; i++) {
        left = (left << 1) - h->count[i];
        if (left < 0) {
            return false;
        }
    }
    offsets[1] = 0;
    for (i = 1; i < 15; i++) {
        offsets[i + 1] = (short)(offsets[i] + h->count[i]);
    }
    for (i = 0; i < n; i++) {
        if (lengths[i] != 0) {
            h->symbol[offsets[lengths[i]]++] = (short)i;
        }
    }
    return true;
}

static bool codes(struct inflate *s, const struct huffman *lengths,
                  const struct huffman *distances)
{
    static const short base[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                   15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                   67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                    4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dbase[30] = {
        1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
        33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const short dextra[30] = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,
                                     4, 4, 5, 5, 6, 6, 7,  7,  8,  8,
                                     9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int symbol = decode(s, lengths);
        unsigned long more;
        size_t length;
        size_t distance;
        size_t i;
        if (symbol < 0) {
            return false;
        }
        if (symbol < 256) {
            char byte = (char)symbol;
            text_append_bytes(s->out, &byte, 1);
            continue;
        }
        if (symbol == 256) {
            return true;
        }
        symbol -= 257;
        if (symbol >= 29 || !need(s, extra[symbol], &more)) {
            return false;
        }
        length = (size_t)base[symbol] + more;
        symbol = decode(s, distances);
        if (symbol < 0 || symbol >= 30 || !need(s, dextra[symbol], &more)) {
            return false;
        }
        distance = (size_t)dbase[symbol] + more;
        if (distance > s->out->length) {
            return false;
        }
        for (i = 0; i < length; i++) {
            char byte = s->out->data[s->out->length - distance];
            text_append_bytes(s->out, &byte, 1);
        }
    }
}

static bool fixed(struct inflate *s)
{
    struct huffman lengths;
    struct huffman distances;
    short sizes[288];
    int i;

    for (i = 0; i < 144; i++) {
        sizes[i] = 8;
    }
    for (; i < 256; i++) {
        sizes[i] = 9;
    }
    for (; i < 280; i++) {
        sizes[i] = 7;
    }
    for (; i < 288; i++) {
        sizes[i] = 8;
    }
    construct(&lengths, sizes, 288);
    for (i = 0; i < 30; i++) {
        sizes[i] = 5;
    }
    construct(&distances, sizes, 30);
    return codes(s, &lengths, &distances);
}

static bool dynamic(struct inflate *s)
{
    static const short order[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                    11, 4,  12, 3, 13, 2, 14, 1, 15};
    struct huffman lengths;
    struct huffman distances;
    short sizes[320];
    unsigned long nlen;
    unsigned long ndist;
    unsigned long ncode;
    unsigned long value;
    int index;

    if (!need(s, 5, &nlen) || !need(s, 5, &ndist) || !need(s, 4, &ncode)) {
        return false;
    }
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if (nlen > 286 || ndist > 30) {
        return false;
    }
    memset(sizes, 0, sizeof sizes);
    for (index = 0; index < (int)ncode; index++) {
        if (!need(s, 3, &value)) {
            return false;
        }
        sizes[order[index]] = (short)value;
    }
    if (!construct(&lengths, sizes, 19)) {
        return false;
    }
    index = 0;
    while (index < (int)(nlen + ndist)) {
        int symbol = decode(s, &lengths);
        short repeat = 0;
        unsigned long times;
        if (symbol < 0) {
            return false;
        }
        if (symbol < 16) {
            sizes[index++] = (short)symbol;
            continue;
        }
        if (symbol == 16) {
            if (index == 0 || !need(s, 2, &times)) {
                return false;
            }
            repeat = sizes[index - 1];
            times += 3;
        } else if (symbol == 17) {
            if (!need(s, 3, &times)) {
                return false;
            }
            times += 3;
        } else {
            if (!need(s, 7, &times)) {
                return false;
            }
            times += 11;
        }
        if (index + (int)times > (int)(nlen + ndist)) {
            return false;
        }
        while (times-- > 0) {
            sizes[index++] = repeat;
        }
    }
    if (!construct(&lengths, sizes, (int)nlen) ||
        !construct(&distances, sizes + nlen, (int)ndist)) {
        return false;
    }
    return codes(s, &lengths, &distances);
}

static bool stored(struct inflate *s)
{
    size_t length;

    s->bits = 0;
    s->held = 0;
    if (s->at + 4 > s->length) {
        return false;
    }
    length = get16(s->in + s->at);
    if (length != (~get16(s->in + s->at + 2) & 0xffffu)) {
        return false;
    }
    s->at += 4;
    if (s->at + length > s->length) {
        return false;
    }
    text_append_bytes(s->out, s->in + s->at, length);
    s->at += length;
    return true;
}

static bool inflate(const unsigned char *in, size_t length, struct text *out)
{
    struct inflate s;
    unsigned long last;
    unsigned long type;
    bool ok = true;

    s.in = in;
    s.length = length;
    s.at = 0;
    s.bits = 0;
    s.held = 0;
    s.out = out;
    do {
        if (!need(&s, 1, &last) || !need(&s, 2, &type)) {
            return false;
        }
        ok = type == 0   ? stored(&s)
             : type == 1 ? fixed(&s)
             : type == 2 ? dynamic(&s)
                         : false;
    } while (ok && !last);
    return ok;
}

bool zip_unpack(const struct zip_archive *a, size_t index, struct text *out)
{
    const struct zip_item *item = &a->items[index];
    const unsigned char *data =
        (const unsigned char *)a->bytes.data + item->offset;
    size_t from = out->length;
    bool ok;

    if (item->method == 0) {
        ok = item->packed == item->size;
        if (ok) {
            text_append_bytes(out, data, item->size);
        }
    } else if (item->method == 8) {
        ok = inflate(data, item->packed, out) &&
             out->length - from == item->size;
    } else {
        fprintf(stderr, "anti: %s is packed with method %u, and the reader "
                        "takes stored and deflate\n",
                text_cstr(&item->name), item->method);
        return false;
    }
    if (ok && crc32_of(out->data + from, out->length - from) != item->crc) {
        ok = false;
    }
    if (!ok) {
        fprintf(stderr, "anti: the data of %s is broken\n",
                text_cstr(&item->name));
    }
    return ok;
}
