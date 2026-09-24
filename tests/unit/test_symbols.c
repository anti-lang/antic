#include "../binary_stdio.h"
#include "check.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"

/* The name that anti_rt_coff_demangle gives for symbol, or "" when it gives
   none. */
static void demangles(const char *symbol, size_t room, const char *expected)
{
    char out[128];
    size_t length;

    memset(out, 'x', sizeof out);
    length = anti_rt_coff_demangle(symbol, strlen(symbol), out, room);
    CHECK(length < sizeof out);
    out[length < sizeof out ? length : 0] = 0;
    CHECK_STR(out, expected);
}

/* Malformed ELF and DWARF input. Each image is built by hand in a buffer
   larger than the size the reader is given. A section that escapes the
   size check finds its bytes in the tail past that size. */

static void put(uint8_t *p, uint64_t v, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

/* A byte string that a test builds a section or a line program in. */
struct blob {
    uint8_t bytes[512];
    size_t size;
};

static void add(struct blob *b, const void *bytes, size_t n)
{
    CHECK(b->size + n <= sizeof b->bytes);
    if (b->size + n <= sizeof b->bytes) {
        memcpy(b->bytes + b->size, bytes, n);
        b->size += n;
    }
}

static void add_int(struct blob *b, uint64_t v, int n)
{
    uint8_t bytes[8];

    put(bytes, v, n);
    add(b, bytes, (size_t)n);
}

static void add_uleb(struct blob *b, uint64_t v)
{
    do {
        uint8_t byte = (uint8_t)(v & 0x7f);
        v >>= 7;
        add_int(b, v != 0 ? byte | 0x80u : byte, 1);
    } while (v != 0);
}

static void add_sleb(struct blob *b, int64_t v)
{
    bool more = true;

    while (more) {
        uint8_t byte = (uint8_t)((uint64_t)v & 0x7f);
        /* An arithmetic shift, written so it does not depend on one. */
        v = v < 0 ? -1 - (-1 - v) / 128 : v / 128;
        more = !((v == 0 && (byte & 0x40) == 0) ||
                 (v == -1 && (byte & 0x40) != 0));
        add_int(b, more ? byte | 0x80u : byte, 1);
    }
}

/* The part of a DWARF 4 or 5 line unit after its header length, up to
   the program: one file, `a.anti`, and no directory. A DWARF 5 table
   takes the counts and the formats given. */
static void line_header(struct blob *h, int version, uint8_t dir_formats,
                        uint64_t dir_count, uint8_t file_formats,
                        uint64_t file_count)
{
    static const uint8_t lengths[12] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};
    uint8_t k;
    uint64_t i;

    add_int(h, 1, 1);           /* minimum instruction length */
    add_int(h, 1, 1);           /* operations per instruction */
    add_int(h, 1, 1);           /* default is_stmt */
    add_int(h, (uint8_t)-5, 1); /* line base */
    add_int(h, 14, 1);          /* line range */
    add_int(h, 13, 1);          /* opcode base */
    add(h, lengths, sizeof lengths);
    if (version < 5) {
        add_int(h, 0, 1);
        add(h, "a.anti", 7);
        add_uleb(h, 0);
        add_uleb(h, 0);
        add_uleb(h, 0);
        add_int(h, 0, 1);
        return;
    }
    add_int(h, dir_formats, 1);
    for (k = 0; k < dir_formats; k++) {
        add_uleb(h, 1);         /* DW_LNCT_path */
        add_uleb(h, 0x08);      /* DW_FORM_string */
    }
    add_uleb(h, dir_count);
    for (i = 0; i < dir_count && dir_formats > 0; i++) {
        add(h, "d", 2);
    }
    add_int(h, file_formats, 1);
    for (k = 0; k < file_formats; k++) {
        add_uleb(h, 1);
        add_uleb(h, 0x08);
    }
    add_uleb(h, file_count);
    for (i = 0; i < file_count && file_formats > 0; i++) {
        add(h, "a.anti", 7);
    }
}

/* A whole line unit of the version with the header h and the program. */
static void line_unit(struct blob *out, int version, const struct blob *h,
                      const struct blob *program)
{
    size_t after_length = 2 + (version >= 5 ? 2u : 0u) + 4 + h->size +
                          program->size;

    add_int(out, after_length, 4);
    add_int(out, (uint64_t)version, 2);
    if (version >= 5) {
        add_int(out, 8, 1);     /* address size */
        add_int(out, 0, 1);     /* segment selector size */
    }
    add_int(out, h->size, 4);
    add(out, h->bytes, h->size);
    add(out, program->bytes, program->size);
}

static void set_address(struct blob *p, uint64_t address)
{
    add_int(p, 0, 1);
    add_uleb(p, 9);
    add_int(p, 2, 1);
    add_int(p, address, 8);
}

static void end_sequence(struct blob *p)
{
    add_int(p, 0, 1);
    add_uleb(p, 1);
    add_int(p, 1, 1);
}

/* The rows 0x1000 at line 1 + delta and 0x1010. */
static void two_rows(struct blob *p, int64_t delta)
{
    set_address(p, 0x1000);
    add_int(p, 3, 1);           /* advance_line */
    add_sleb(p, delta);
    add_int(p, 1, 1);           /* copy */
    add_int(p, 2, 1);           /* advance_pc */
    add_uleb(p, 0x10);
    add_int(p, 1, 1);
    end_sequence(p);
}

enum {
    ELF_NULL, ELF_SHSTRTAB, ELF_SYMTAB, ELF_STRTAB, ELF_DEBUG_LINE,
    ELF_SECTIONS
};

/* An ELF file with one function symbol, `anti_licenses` at 0x1000, and
   the line table line. The section nobits is of type NOBITS with a size
   of 1 GiB. Its bytes stand past the size the file reports, which the
   function returns. ELF_NULL makes every section an ordinary one. */
static size_t elf_image(uint8_t *out, size_t room, int nobits,
                        const struct blob *line)
{
    static const char names[] =
        "\0.shstrtab\0.symtab\0.strtab\0.debug_line";
    static const uint32_t name_at[ELF_SECTIONS] = {0, 1, 11, 19, 27};
    static const uint32_t types[ELF_SECTIONS] = {0, 3, 2, 3, 1};
    static const char strings[] = "\0anti_licenses";
    uint8_t symbols[48];
    const uint8_t *contents[ELF_SECTIONS];
    size_t sizes[ELF_SECTIONS];
    size_t offsets[ELF_SECTIONS];
    size_t at = 64 + 64 * ELF_SECTIONS;
    size_t end = 0;
    int i;

    memset(out, 0, room);
    memset(symbols, 0, sizeof symbols);
    put(symbols + 24, 1, 4);
    symbols[24 + 4] = 0x12;     /* global function */
    put(symbols + 24 + 6, ELF_SYMTAB, 2);
    put(symbols + 24 + 8, 0x1000, 8);
    put(symbols + 24 + 16, 0x10, 8);
    contents[ELF_NULL] = NULL;
    sizes[ELF_NULL] = 0;
    contents[ELF_SHSTRTAB] = (const uint8_t *)names;
    sizes[ELF_SHSTRTAB] = sizeof names;
    contents[ELF_SYMTAB] = symbols;
    sizes[ELF_SYMTAB] = sizeof symbols;
    contents[ELF_STRTAB] = (const uint8_t *)strings;
    sizes[ELF_STRTAB] = sizeof strings;
    contents[ELF_DEBUG_LINE] = line->bytes;
    sizes[ELF_DEBUG_LINE] = line->size;
    /* The NOBITS section goes last, so the file ends where it starts. */
    for (i = 1; i < ELF_SECTIONS; i++) {
        if (i != nobits) {
            offsets[i] = at;
            at += sizes[i];
        }
    }
    offsets[ELF_NULL] = 0;
    end = at;
    if (nobits != ELF_NULL) {
        offsets[nobits] = at;
        at += sizes[nobits];
    }
    CHECK(at <= room);
    memcpy(out, "\x7f" "ELF", 4);
    out[4] = 2;
    out[5] = 1;
    put(out + 40, 64, 8);
    put(out + 58, 64, 2);
    put(out + 60, ELF_SECTIONS, 2);
    put(out + 62, ELF_SHSTRTAB, 2);
    for (i = 1; i < ELF_SECTIONS; i++) {
        uint8_t *h = out + 64 + 64 * i;
        put(h, name_at[i], 4);
        put(h + 4, i == nobits ? 8 : types[i], 4);
        put(h + 8, i == ELF_SYMTAB ? 4 : 0, 8);
        put(h + 24, offsets[i], 8);
        put(h + 32, i == nobits ? (uint64_t)1 << 30 : sizes[i], 8);
        put(h + 40, i == ELF_SYMTAB ? ELF_STRTAB : 0, 4);
        memcpy(out + offsets[i], contents[i], sizes[i]);
    }
    return end;
}

/* A line table of one DWARF 4 unit with the two rows of two_rows. */
static void simple_lines(struct blob *line, int64_t delta)
{
    struct blob h = {{0}, 0};
    struct blob p = {{0}, 0};

    line_header(&h, 4, 0, 0, 0, 0);
    two_rows(&p, delta);
    line_unit(line, 4, &h, &p);
}

/* Look vaddr up in a line table alone. */
static bool line_of(const struct blob *line, uint64_t vaddr,
                    struct anti_found *found)
{
    uint8_t image[2048];
    size_t size = elf_image(image, sizeof image, ELF_NULL, line);

    memset(found, 0, sizeof *found);
    return anti_rt_elf_line(image, size, vaddr, found);
}

/* S28: a section of type NOBITS holds no bytes of the file. A reader of
   names, strings or lines refuses it whatever its size says. */
static void nobits_sections(void)
{
    struct blob line = {{0}, 0};
    uint8_t image[2048];
    struct anti_found found;
    uint64_t vaddr = 0;
    size_t size;

    simple_lines(&line, 6);
    size = elf_image(image, sizeof image, ELF_NULL, &line);
    CHECK(anti_rt_elf_symbol(image, size, "anti_licenses", &vaddr));
    CHECK(vaddr == 0x1000);
    memset(&found, 0, sizeof found);
    CHECK(anti_rt_elf_line(image, size, 0x1004, &found));
    CHECK(found.line == 7);
    CHECK(found.file != NULL && strcmp(found.file, "a.anti") == 0);

    size = elf_image(image, sizeof image, ELF_SHSTRTAB, &line);
    CHECK(!anti_rt_elf_symbol(image, size, "anti_licenses", &vaddr));
    CHECK(!anti_rt_elf_line(image, size, 0x1004, &found));
    size = elf_image(image, sizeof image, ELF_STRTAB, &line);
    CHECK(!anti_rt_elf_symbol(image, size, "anti_licenses", &vaddr));
    size = elf_image(image, sizeof image, ELF_DEBUG_LINE, &line);
    CHECK(anti_rt_elf_symbol(image, size, "anti_licenses", &vaddr));
    CHECK(!anti_rt_elf_line(image, size, 0x1004, &found));
}

/* S29: the line register refuses a step that leaves int64_t, from
   advance_line and from a special opcode, and keeps one that reaches
   the edge. */
static void line_overflow(void)
{
    struct blob line = {{0}, 0};
    struct blob h = {{0}, 0};
    struct blob p = {{0}, 0};
    struct anti_found found;

    simple_lines(&line, INT64_MAX - 1);
    CHECK(line_of(&line, 0x1004, &found));
    CHECK(found.line == INT64_MAX);

    line.size = 0;
    simple_lines(&line, INT64_MAX);
    CHECK(!line_of(&line, 0x1004, &found));

    line.size = 0;
    simple_lines(&line, INT64_MIN);
    CHECK(line_of(&line, 0x1004, &found));
    CHECK(found.line == INT64_MIN + 1);

    /* Special opcode 19 adds one to the line and nothing to the address. */
    line.size = 0;
    line_header(&h, 4, 0, 0, 0, 0);
    set_address(&p, 0x1000);
    add_int(&p, 3, 1);
    add_sleb(&p, INT64_MAX - 1);
    add_int(&p, 2, 1);
    add_uleb(&p, 0x10);
    add_int(&p, 19, 1);
    add_int(&p, 2, 1);
    add_uleb(&p, 0x10);
    add_int(&p, 1, 1);
    end_sequence(&p);
    line_unit(&line, 4, &h, &p);
    CHECK(!line_of(&line, 0x1014, &found));
}

/* M9: an entry table of DWARF 5 with a count and no formats has entries
   of no bytes, which a count of 2^64 - 1 walks for ever. The reader
   refuses such a table. */
static void empty_entry_formats(void)
{
    struct blob line = {{0}, 0};
    struct blob h = {{0}, 0};
    struct blob p = {{0}, 0};
    struct anti_found found;

    /* Two files, since the file register starts at 1 and DWARF 5 counts
       from 0. */
    line_header(&h, 5, 1, 1, 1, 2);
    two_rows(&p, 6);
    line_unit(&line, 5, &h, &p);
    CHECK(line_of(&line, 0x1004, &found));
    CHECK(found.line == 7);
    CHECK(found.file != NULL && strcmp(found.file, "a.anti") == 0);

    /* The directories. */
    line.size = 0;
    h.size = 0;
    line_header(&h, 5, 0, UINT64_MAX, 1, 2);
    line_unit(&line, 5, &h, &p);
    CHECK(!line_of(&line, 0x1004, &found));

    /* The files, walked for the name of file UINT64_MAX - 1. */
    line.size = 0;
    h.size = 0;
    p.size = 0;
    line_header(&h, 5, 1, 1, 0, UINT64_MAX);
    add_int(&p, 4, 1);          /* set_file */
    add_uleb(&p, UINT64_MAX - 1);
    two_rows(&p, 6);
    line_unit(&line, 5, &h, &p);
    CHECK(line_of(&line, 0x1004, &found));
    CHECK(found.line == 7);
    CHECK(found.file == NULL);
}

/* S11: a LEB128 number of 310 million bytes counts its shift past the
   range of int. Its value is 0, so an extended opcode of that length
   ends the unit and an advance_line of it moves nothing. The unit is the
   last section of the file, which grows to hold it. */
static void long_leb(void)
{
    enum { CONTINUED = 310000000 };
    struct blob none = {{0}, 0};
    struct blob h = {{0}, 0};
    uint8_t image[2048];
    size_t start = elf_image(image, sizeof image, ELF_NULL, &none);
    size_t after_length;
    size_t length;
    uint8_t *file;
    uint8_t *unit;
    uint8_t *op;
    struct anti_found found;

    line_header(&h, 4, 0, 0, 0, 0);
    after_length = 2 + 4 + h.size + 1 + CONTINUED + 1;
    length = 4 + after_length;
    file = malloc(start + length);
    CHECK(file != NULL);
    if (file == NULL) {
        return;
    }
    memcpy(file, image, start);
    put(file + 64 + 64 * ELF_DEBUG_LINE + 32, length, 8);
    unit = file + start;
    put(unit, after_length, 4);
    put(unit + 4, 4, 2);
    put(unit + 6, h.size, 4);
    memcpy(unit + 10, h.bytes, h.size);
    op = unit + 10 + h.size;
    memset(op + 1, 0x80, CONTINUED);
    op[1 + CONTINUED] = 0;
    op[0] = 0;
    memset(&found, 0, sizeof found);
    CHECK(!anti_rt_elf_line(file, start + length, 0x1000, &found));
    op[0] = 3;
    CHECK(!anti_rt_elf_line(file, start + length, 0x1000, &found));
    free(file);
}

/* S37: a build id is read only where a readable PT_LOAD of the mapped
   module holds it. The read ends at the end of that segment. */
static void loaded_room(void)
{
    uint8_t headers[3 * 56];

    memset(headers, 0, sizeof headers);
    /* PT_LOAD, readable, 0x1000 bytes in memory at 0x10000. */
    put(headers, 1, 4);
    put(headers + 4, 4, 4);
    put(headers + 16, 0x10000, 8);
    put(headers + 40, 0x1000, 8);
    /* PT_LOAD with no read permission at 0x20000. */
    put(headers + 56, 1, 4);
    put(headers + 56 + 16, 0x20000, 8);
    put(headers + 56 + 40, 0x1000, 8);
    /* PT_NOTE at 0x30000, and a segment that wraps the address space. */
    put(headers + 112, 4, 4);
    put(headers + 112 + 4, 4, 4);
    put(headers + 112 + 16, 0x30000, 8);
    put(headers + 112 + 40, 0x1000, 8);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0x10000) == 0x1000);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0x10ff0) == 0x10);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0x11000) == 0);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0xffff) == 0);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0x20010) == 0);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0x30010) == 0);
    CHECK(anti_rt_elf_loaded_room(headers, 1, 0x10000) == 0x1000);
    CHECK(anti_rt_elf_loaded_room(headers, 0, 0x10000) == 0);
    put(headers + 112, 1, 4);
    put(headers + 112 + 16, UINT64_MAX - 0xf, 8);
    CHECK(anti_rt_elf_loaded_room(headers, 3, UINT64_MAX) == 0);
    CHECK(anti_rt_elf_loaded_room(headers, 3, 0x5) == 0);
}

void test_symbols(void)
{
    /* The forms of "Symbols" in docs/decisions.md. */
    demangles("_A3com7example8geometry3vec_push", 128,
              "com.example.geometry.vec.push");
    demangles("_A8geometry_length", 128, "geometry.length");
    /* A segment may hold `_`, and a method keeps its `.`. */
    demangles("_A11stack_trace_inner", 128, "stack_trace.inner");
    demangles("_A4anti4lang_StackTrace.capture", 128,
              "anti.lang.StackTrace.capture");
    demangles("_A4anti2rt_main", 128, "anti.rt.main");
    /* A name after the segments may start with a digit. */
    demangles("_A11stack_trace_0", 128, "stack_trace.0");
    /* Anything else is not a mangled name. */
    demangles("main", 128, "");
    demangles("_A", 128, "");
    demangles("_Ax_main", 128, "");
    demangles("_A4anti", 128, "");
    demangles("_A9anti_main", 128, "");
    demangles("_A4anti2rt", 128, "");
    demangles("_A4anti2rt_", 128, "");
    demangles("_A0_main", 128, "");
    /* A name that does not fit is none. */
    demangles("_A8geometry_length", 14, "");
    demangles("_A8geometry_length", 15, "geometry.length");
    demangles("_A8geometry_length", 8, "");
    nobits_sections();
    line_overflow();
    empty_entry_formats();
    long_leb();
    loaded_room();
}
