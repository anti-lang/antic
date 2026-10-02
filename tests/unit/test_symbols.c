#include "../binary_stdio.h"
#include "check.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"
#include "target.h"
#include "text.h"

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

/* The name that anti_rt_symbol_unescape gives for symbol, or "" when it
   gives none. */
static void unescapes(const char *symbol, size_t room, const char *expected)
{
    char out[128];
    size_t length;

    memset(out, 'x', sizeof out);
    length = anti_rt_symbol_unescape(symbol, strlen(symbol), out, room);
    CHECK(length < sizeof out);
    out[length < sizeof out ? length : 0] = 0;
    CHECK_STR(out, expected);
}

/* The symbol mangle writes for name in module on an ELF target, read back
   gives name, a `$` of the name among the bytes it escapes. */
static void round_trip(const char *module, const char *name)
{
    struct text symbol = {0};
    struct text wanted = {0};
    char out[256];
    size_t length;

    mangle(&symbol, TARGET_LINUX_X86_64, module, name);
    text_appendf(&wanted, "%s.%s", module, name);
    length = anti_rt_symbol_unescape(text_cstr(&symbol), symbol.length, out,
                                     sizeof out);
    out[length < sizeof out ? length : 0] = 0;
    CHECK_STR(length > 0 ? out : text_cstr(&symbol), text_cstr(&wanted));
    text_free(&symbol);
    text_free(&wanted);
}

static void symbol_escapes(void)
{
    unescapes("app.List$3cint$3e.push", 128, "app.List<int>.push");
    unescapes("app.List$3c$2ageo.Point$3e.push", 128,
              "app.List<*geo.Point>.push");
    unescapes("app.Pair$3cint$2c$20str$3e.swap", 128,
              "app.Pair<int, str>.swap");
    /* `$` itself is an escape, so a name that holds one reads back. */
    unescapes("app.a$24b", 128, "app.a$b");
    /* A name without an escape, or with a `$` that is none of the form,
       stays as it is. */
    unescapes("app.main", 128, "");
    unescapes("app.a$b", 128, "");
    unescapes("app.a$3C", 128, "");
    unescapes("app.a$3", 128, "");
    unescapes("app.a$", 128, "");
    /* The escape of a byte the escape leaves alone is no escape. */
    unescapes("app.a$41", 128, "");
    unescapes("app.a$2e", 128, "");
    unescapes("app.a$00", 128, "");
    /* A C name holds no `.`, and a byte outside the form is no name. */
    unescapes("foo$3cbar", 128, "");
    unescapes("app.a$3cb@plt", 128, "");
    /* A name that does not fit is none. */
    unescapes("app.List$3cint$3e.push", 17, "");
    unescapes("app.List$3cint$3e.push", 18, "app.List<int>.push");
    round_trip("app", "List<int>.push");
    round_trip("com.example", "Map<str, List<*com.example.Point>>.get");
    round_trip("app", "max<[4]f32>");
    round_trip("app", "a$b<fn(int) -> ?*byte>");
    round_trip("app", "main");
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

/* The rows address at line 1 + delta and address + 0x10. */
static void two_rows_at(struct blob *p, uint64_t address, int64_t delta)
{
    set_address(p, address);
    add_int(p, 3, 1);           /* advance_line */
    add_sleb(p, delta);
    add_int(p, 1, 1);           /* copy */
    add_int(p, 2, 1);           /* advance_pc */
    add_uleb(p, 0x10);
    add_int(p, 1, 1);
    end_sequence(p);
}

/* The rows 0x1000 at line 1 + delta and 0x1010. */
static void two_rows(struct blob *p, int64_t delta)
{
    two_rows_at(p, 0x1000, delta);
}

enum {
    ELF_NULL, ELF_SHSTRTAB, ELF_SYMTAB, ELF_STRTAB, ELF_DEBUG_LINE,
    ELF_SECTIONS
};

/* The type and binding byte of the symbol of elf_image. */
static uint8_t elf_symbol_info = 0x12;      /* global function */

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
    symbols[24 + 4] = elf_symbol_info;
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

static bool count_function(void *context, const char *name, uint64_t vaddr,
                           uint64_t size)
{
    int *count = context;

    (void)size;
    CHECK(strcmp(name, "anti_licenses") == 0 && vaddr == 0x1000);
    *count += 1;
    return false;
}

/* The functions of an ELF program take a symbol without a type in a
   section of code, as the lookup of a trace does. The assembly antic
   writes gives its functions no type, and the map of a symbols archive
   listed none of them on Linux. */
static void untyped_functions(void)
{
    struct blob line = {0};
    uint8_t image[1024];
    size_t size;
    int count = 0;

    elf_symbol_info = 0x10;     /* global, no type */
    size = elf_image(image, sizeof image, ELF_NULL, &line);
    elf_symbol_info = 0x12;
    anti_rt_elf_functions(image, size, count_function, &count);
    CHECK(count == 1);
    elf_symbol_info = 0x11;     /* global object */
    size = elf_image(image, sizeof image, ELF_NULL, &line);
    elf_symbol_info = 0x12;
    count = 0;
    anti_rt_elf_functions(image, size, count_function, &count);
    CHECK(count == 0);
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

/* Look vaddr up in a line table alone. The name of the file in found
   points into the image, which is static so that the caller can read it
   until the next call. A buffer of the frame was gone when the caller
   read the name, which ASan reports on Linux. */
static bool line_of(const struct blob *line, uint64_t vaddr,
                    struct anti_found *found)
{
    static uint8_t image[2048];
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

/* The address of __TEXT of a Mach-O header, which a trace and `anti
   symbols` both read. The header holds a __PAGEZERO segment and a
   __TEXT segment, each of 72 bytes. Every read stays inside the size
   given and inside the commands the header counts. */
static void macho_text(void)
{
    uint8_t h[32 + 2 * 72 + 16];
    uint64_t vmaddr = 0;

    memset(h, 0, sizeof h);
    put(h, 0xfeedfacf, 4);
    put(h + 16, 2, 4);
    put(h + 20, 2 * 72, 4);
    put(h + 32, 0x19, 4);
    put(h + 36, 72, 4);
    memcpy(h + 40, "__PAGEZERO", 10);
    put(h + 104, 0x19, 4);
    put(h + 108, 72, 4);
    memcpy(h + 112, "__TEXT", 6);
    put(h + 104 + 24, 0x100000000, 8);
    CHECK(anti_rt_macho_text(h, 32 + 2 * 72, &vmaddr));
    CHECK(vmaddr == 0x100000000);
    /* The file ends inside the second command. */
    CHECK(!anti_rt_macho_text(h, 32 + 72 + 40, &vmaddr));
    /* The file ends inside the header. */
    CHECK(!anti_rt_macho_text(h, 20, &vmaddr));
    /* Not a 64-bit Mach-O header. */
    put(h, 0xfeedface, 4);
    CHECK(!anti_rt_macho_text(h, sizeof h, &vmaddr));
    put(h, 0xfeedfacf, 4);
    /* The commands run past the size the header gives them. */
    put(h + 20, 72 + 8, 4);
    CHECK(!anti_rt_macho_text(h, sizeof h, &vmaddr));
    put(h + 20, 2 * 72, 4);
    /* A command of size 0 would stand still, and one of 16 is shorter
       than a segment. */
    put(h + 36, 0, 4);
    CHECK(!anti_rt_macho_text(h, sizeof h, &vmaddr));
    put(h + 36, 72, 4);
    put(h + 108, 16, 4);
    CHECK(!anti_rt_macho_text(h, sizeof h, &vmaddr));
    put(h + 108, 72, 4);
    /* A header that counts more commands than it holds. */
    put(h + 16, 1000, 4);
    memcpy(h + 112, "__DATA", 6);
    CHECK(!anti_rt_macho_text(h, sizeof h, &vmaddr));
    /* A mapped image has no size, and the header bounds the walk. */
    put(h + 16, 2, 4);
    memcpy(h + 112, "__TEXT", 6);
    CHECK(anti_rt_macho_text(h, SIZE_MAX, &vmaddr));
    CHECK(vmaddr == 0x100000000);
}

/* Malformed Mach-O symbol tables, debug maps and object line tables.
   Each file is built in a buffer and copied into memory of exactly its
   size, so the sanitizer builds catch a read or a write past it. */

/* One nlist_64 record: the offset of its name, its type, its section and
   its value. */
struct t_nlist {
    uint32_t name;
    uint8_t type;
    uint8_t sect;
    uint64_t value;
};

enum {
    N_OSO = 0x66, N_FUN = 0x24, N_SECT_EXT = 0x0f, N_SECT = 0x0e,
    MACHO_VMADDR = 0x4000
};

/* The names of the symbols below: "" at 0, "/tmp/a.o" at 1, "_f" at 10,
   "_g" at 13 and "/tmp/b.o" at 16. */
static const char macho_names[] = "\0/tmp/a.o\0_f\0_g\0/tmp/b.o";

/* A debug map of a.o with f, and the symbols f at 0x1000 and g at 0x1040. */
static const struct t_nlist macho_one[] = {
    {1, N_OSO, 0, 0}, {10, N_FUN, 1, 0x1000}, {0, N_FUN, 0, 0x20},
    {10, N_SECT_EXT, 1, 0x1000}, {13, N_SECT_EXT, 1, 0x1040}
};

/* A Mach-O file of a header, a __LINKEDIT segment at MACHO_VMADDR and
   offset 0, and a symbol table of the records with macho_names after
   them. The names end the file. Gives its size. */
static size_t macho_image(uint8_t *out, size_t room, const struct t_nlist *n,
                          uint32_t count)
{
    size_t symbols = 32 + 72 + 24;
    size_t strings = symbols + 16 * (size_t)count;
    uint32_t i;

    CHECK(strings + sizeof macho_names <= room);
    memset(out, 0, room);
    put(out, 0xfeedfacf, 4);
    put(out + 16, 2, 4);
    put(out + 20, 72 + 24, 4);
    put(out + 32, 0x19, 4);
    put(out + 36, 72, 4);
    memcpy(out + 40, "__LINKEDIT", 10);
    put(out + 32 + 24, MACHO_VMADDR, 8);
    put(out + 104, 0x2, 4);
    put(out + 108, 24, 4);
    put(out + 112, symbols, 4);
    put(out + 116, count, 4);
    put(out + 120, strings, 4);
    put(out + 124, sizeof macho_names, 4);
    for (i = 0; i < count; i++) {
        uint8_t *r = out + symbols + 16 * (size_t)i;
        put(r, n[i].name, 4);
        r[4] = n[i].type;
        r[5] = n[i].sect;
        put(r + 8, n[i].value, 8);
    }
    memcpy(out + strings, macho_names, sizeof macho_names);
    return strings + sizeof macho_names;
}

/* A copy of the first size bytes of image in memory of its own. */
static uint8_t *exactly(const uint8_t *image, size_t size)
{
    uint8_t *copy = malloc(size > 0 ? size : 1);

    if (copy != NULL && size > 0) {
        memcpy(copy, image, size);
    }
    return copy;
}

static bool count_any(void *context, const char *name, uint64_t vaddr,
                      uint64_t size)
{
    int *count = context;

    (void)name;
    (void)vaddr;
    (void)size;
    *count += 1;
    return false;
}

/* The functions of the table, counted. */
static int macho_count(const struct anti_macho_table *t)
{
    int count = 0;

    anti_rt_macho_functions(t, count_any, &count);
    return count;
}

/* Whether the debug map names object and symbol for vaddr. */
static bool maps_to(const struct anti_macho_table *t, uint64_t vaddr,
                    const char *object, const char *symbol)
{
    const char *o = NULL;
    const char *s = NULL;
    uint64_t start = 0;

    return anti_rt_macho_debug_map(t, vaddr, &o, &s, &start) &&
           strcmp(o, object) == 0 && strcmp(s, symbol) == 0;
}

static void macho_symbols(void)
{
    uint8_t image[512];
    struct anti_macho_table t;
    struct anti_found found;
    const char *object = NULL;
    const char *symbol = NULL;
    uint64_t start = 0;
    uint64_t vaddr = 0;
    size_t size = macho_image(image, sizeof image, macho_one, 5);
    size_t cut;

    /* The whole file, read from its bytes and as a mapped image. */
    CHECK(anti_rt_macho_table(image, size, false, 0, &t));
    CHECK(t.count == 5 && macho_count(&t) == 2);
    memset(&found, 0, sizeof found);
    CHECK(anti_rt_macho_function(&t, 0x1010, &found));
    CHECK(found.function != NULL && strcmp(found.function, "f") == 0);
    CHECK(anti_rt_macho_function(&t, 0x1050, &found));
    CHECK(found.function != NULL && strcmp(found.function, "g") == 0);
    CHECK(!anti_rt_macho_function(&t, 0xfff, &found));
    CHECK(anti_rt_macho_symbol(&t, "g", &vaddr) && vaddr == 0x1040);
    CHECK(anti_rt_macho_debug_map(&t, 0x1010, &object, &symbol, &start));
    CHECK(object != NULL && strcmp(object, "/tmp/a.o") == 0);
    CHECK(symbol != NULL && strcmp(symbol, "_f") == 0 && start == 0x1000);
    CHECK(!anti_rt_macho_debug_map(&t, 0x1020, &object, &symbol, &start));
    CHECK(anti_rt_macho_table(image, SIZE_MAX, true,
                              (intptr_t)((uintptr_t)image - MACHO_VMADDR), &t));
    CHECK(t.count == 5 && macho_count(&t) == 2);

    /* Every file cut short loses the end of its names. */
    for (cut = 0; cut < size; cut++) {
        uint8_t *copy = exactly(image, cut);
        CHECK(!anti_rt_macho_table(copy, cut, false, 0, &t));
        free(copy);
    }

    /* The table or the names past the end of the file, or a count or a
       size that runs past it. */
    put(image + 112, size, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 112, 0xfffffff0u, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 112, 128, 4);
    put(image + 116, 7, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 116, 0xffffffffu, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 116, 5, 4);
    put(image + 124, sizeof macho_names + 1, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 124, 0xffffffffu, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 120, 0xfffffff0u, 4);
    put(image + 124, sizeof macho_names, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));

    /* A symbol table command too short to hold its fields, and none. */
    size = macho_image(image, sizeof image, macho_one, 5);
    put(image + 108, 16, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 108, 24, 4);
    put(image + 104, 0x99, 4);
    CHECK(!anti_rt_macho_table(image, size, false, 0, &t));
    put(image + 104, 0x2, 4);
    /* A mapped image finds its table through __LINKEDIT alone. */
    memcpy(image + 40, "__DATA\0\0\0\0", 10);
    CHECK(anti_rt_macho_table(image, size, false, 0, &t));
    CHECK(!anti_rt_macho_table(image, SIZE_MAX, true, 0, &t));

    /* A name past the names, at their end, and one that runs off it.
       Each symbol is passed over, and the others stay. */
    size = macho_image(image, sizeof image, macho_one, 5);
    put(image + 128 + 16 * 4, 500, 4);
    CHECK(anti_rt_macho_table(image, size, false, 0, &t));
    CHECK(macho_count(&t) == 1 && !anti_rt_macho_symbol(&t, "g", &vaddr));
    put(image + 128 + 16 * 4, sizeof macho_names, 4);
    CHECK(macho_count(&t) == 1 && !anti_rt_macho_symbol(&t, "g", &vaddr));
    put(image + 128 + 16 * 4, 16, 4);
    image[size - 1] = 'x';
    CHECK(macho_count(&t) == 1 && !anti_rt_macho_symbol(&t, "g", &vaddr));
    CHECK(anti_rt_macho_symbol(&t, "f", &vaddr) && vaddr == 0x1000);
}

/* The debug map against damaged entries. A function belongs to the object
   entry before it, and an entry whose name is damaged names no object
   and no function. */
static void macho_debug_map(void)
{
    static const struct t_nlist two[] = {
        {1, N_OSO, 0, 0}, {10, N_FUN, 1, 0x1000}, {0, N_FUN, 0, 0x20},
        {16, N_OSO, 0, 0}, {13, N_FUN, 1, 0x1040}, {0, N_FUN, 0, 0x20}
    };
    static const struct t_nlist open[] = {
        {1, N_OSO, 0, 0}, {10, N_FUN, 1, 0x1000}, {500, N_FUN, 1, 0x1040},
        {0, N_FUN, 0, 0x80}
    };
    static const struct t_nlist stray[] = {
        {1, N_OSO, 0, 0}, {0, N_FUN, 0, 0x20}, {10, N_FUN, 1, 0x1000},
        {0, N_FUN, 0, 0}, {13, N_FUN, 1, 0x1040}, {0, N_FUN, 0, UINT64_MAX}
    };
    uint8_t image[512];
    struct anti_macho_table t;
    size_t size;

    size = macho_image(image, sizeof image, two, 6);
    CHECK(anti_rt_macho_table(image, size, false, 0, &t));
    CHECK(maps_to(&t, 0x1010, "/tmp/a.o", "_f"));
    CHECK(maps_to(&t, 0x1050, "/tmp/b.o", "_g"));
    /* The name of b.o lies past the names. */
    put(image + 128 + 16 * 3, 500, 4);
    CHECK(maps_to(&t, 0x1010, "/tmp/a.o", "_f"));
    CHECK(!maps_to(&t, 0x1050, "/tmp/a.o", "_g"));
    CHECK(!maps_to(&t, 0x1050, "/tmp/b.o", "_g"));

    /* The start of g is damaged, so the end after it closes no function,
       and f, which has no end, takes no address. */
    size = macho_image(image, sizeof image, open, 4);
    CHECK(anti_rt_macho_table(image, size, false, 0, &t));
    CHECK(!maps_to(&t, 0x1010, "/tmp/a.o", "_f"));
    CHECK(!maps_to(&t, 0x1050, "/tmp/a.o", "_f"));

    /* An end before any start, an end of size 0, and one whose size runs
       to the end of the address space. */
    size = macho_image(image, sizeof image, stray, 6);
    CHECK(anti_rt_macho_table(image, size, false, 0, &t));
    CHECK(!maps_to(&t, 0x1000, "/tmp/a.o", "_f"));
    CHECK(!maps_to(&t, 0x0, "/tmp/a.o", ""));
    CHECK(maps_to(&t, 0x1040, "/tmp/a.o", "_g"));
    CHECK(maps_to(&t, UINT64_MAX, "/tmp/a.o", "_g"));
    CHECK(!maps_to(&t, 0x103f, "/tmp/a.o", "_g"));
}

/* An object file with one section, __debug_line of __DWARF, the symbol
   _f at 0x1000 and the line table line. Its relocations follow the
   section, and the symbol table and its names end the file. Gives the
   size. */
static size_t macho_object(uint8_t *out, size_t room, const struct blob *line,
                           const uint8_t *relocs, uint32_t nreloc)
{
    static const struct t_nlist f[] = {{10, N_SECT, 1, 0x1000}};
    size_t section = 32 + 152 + 24;
    size_t reloff = section + line->size;
    size_t symbols = reloff + 8 * (size_t)nreloc;
    size_t strings = symbols + 16;
    uint8_t *s = out + 32 + 72;

    CHECK(strings + sizeof macho_names <= room);
    memset(out, 0, room);
    put(out, 0xfeedfacf, 4);
    put(out + 16, 2, 4);
    put(out + 20, 152 + 24, 4);
    put(out + 32, 0x19, 4);
    put(out + 36, 152, 4);
    put(out + 32 + 64, 1, 4);
    memcpy(s, "__debug_line", 12);
    memcpy(s + 16, "__DWARF", 7);
    put(s + 40, line->size, 8);
    put(s + 48, section, 4);
    put(s + 56, reloff, 4);
    put(s + 60, nreloc, 4);
    put(out + 184, 0x2, 4);
    put(out + 188, 24, 4);
    put(out + 192, symbols, 4);
    put(out + 196, 1, 4);
    put(out + 200, strings, 4);
    put(out + 204, sizeof macho_names, 4);
    memcpy(out + section, line->bytes, line->size);
    if (nreloc > 0) {
        memcpy(out + reloff, relocs, 8 * (size_t)nreloc);
    }
    put(out + symbols, f[0].name, 4);
    out[symbols + 4] = f[0].type;
    out[symbols + 5] = f[0].sect;
    put(out + symbols + 8, f[0].value, 8);
    memcpy(out + strings, macho_names, sizeof macho_names);
    return strings + sizeof macho_names;
}

/* The line of _f + 4 in the object, from a copy of exactly size bytes
   that the relocations resolve first. 0 for none. */
static int64_t object_line(const uint8_t *image, size_t size)
{
    uint8_t *copy = exactly(image, size);
    struct anti_found found;
    int64_t line = 0;

    memset(&found, 0, sizeof found);
    anti_rt_macho_relocate(copy, size);
    if (anti_rt_macho_object_line(copy, size, "_f", 4, &found)) {
        line = found.line;
        CHECK(found.file != NULL && strcmp(found.file, "a.anti") == 0);
    }
    free(copy);
    return line;
}

/* A relocation of the plain unsigned kind at address, against symbol
   index, of 1 << length bytes. */
static void relocation(uint8_t *r, uint32_t address, uint32_t index,
                       uint32_t length)
{
    put(r, address, 4);
    put(r + 4, index | length << 25 | 1u << 27, 4);
}

static void macho_object_lines(void)
{
    struct blob h = {{0}, 0};
    struct blob p = {{0}, 0};
    struct blob line = {{0}, 0};
    struct blob zero = {{0}, 0};
    uint8_t image[1024];
    uint8_t r[16];
    uint32_t operand;
    size_t size;
    size_t cut;

    simple_lines(&line, 6);
    size = macho_object(image, sizeof image, &line, NULL, 0);
    CHECK(object_line(image, size) == 7);
    for (cut = 0; cut < size; cut++) {
        CHECK(object_line(image, cut) == 0);
    }
    /* The section past the end of the file, or running past it. */
    put(image + 32 + 72 + 48, (uint32_t)size, 4);
    CHECK(object_line(image, size) == 0);
    put(image + 32 + 72 + 48, 208, 4);
    put(image + 32 + 72 + 40, size, 8);
    CHECK(object_line(image, size) == 0);
    put(image + 32 + 72 + 40, UINT64_MAX, 8);
    CHECK(object_line(image, size) == 0);
    put(image + 32 + 72 + 40, line.size, 8);
    /* A count of sections the command has no room for reads the one it
       holds. */
    put(image + 32 + 64, 1000, 4);
    CHECK(object_line(image, size) == 7);

    /* The address of the rows as a relocation against _f, which the
       relocation completes. The operand of DW_LNE_set_address follows
       the unit length, the version, the header length, the header and
       the three bytes of the opcode. */
    line_header(&h, 4, 0, 0, 0, 0);
    two_rows_at(&p, 0, 6);
    line_unit(&zero, 4, &h, &p);
    operand = (uint32_t)(4 + 2 + 4 + h.size + 3);
    relocation(r, operand, 0, 3);
    size = macho_object(image, sizeof image, &zero, r, 1);
    CHECK(object_line(image, size) == 7);
    for (cut = 0; cut < size; cut++) {
        CHECK(object_line(image, cut) == 0);
    }
    /* A symbol past the table, an address past the section, a field of 4
       bytes and a scattered relocation are each passed over. */
    relocation(r, operand, 1, 3);
    size = macho_object(image, sizeof image, &zero, r, 1);
    CHECK(object_line(image, size) == 0);
    relocation(r, (uint32_t)zero.size - 7, 0, 3);
    size = macho_object(image, sizeof image, &zero, r, 1);
    CHECK(object_line(image, size) == 0);
    relocation(r, operand, 0, 2);
    size = macho_object(image, sizeof image, &zero, r, 1);
    CHECK(object_line(image, size) == 0);
    relocation(r, operand | 0x80000000u, 0, 3);
    size = macho_object(image, sizeof image, &zero, r, 1);
    CHECK(object_line(image, size) == 0);
    /* Relocations past the end of the file, or more than it holds. */
    relocation(r, operand, 0, 3);
    size = macho_object(image, sizeof image, &zero, r, 1);
    put(image + 32 + 72 + 56, (uint32_t)size, 4);
    CHECK(object_line(image, size) == 0);
    put(image + 32 + 72 + 56, 208 + (uint32_t)zero.size, 4);
    put(image + 32 + 72 + 60, 0xffffffffu, 4);
    CHECK(object_line(image, size) == 0);
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
    symbol_escapes();
    nobits_sections();
    untyped_functions();
    line_overflow();
    empty_entry_formats();
    long_leb();
    loaded_room();
    macho_text();
    macho_symbols();
    macho_debug_map();
    macho_object_lines();
}
