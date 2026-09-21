#include "symbols.h"

#include <string.h>

/* DESIGN: every read goes through these helpers, which take the bytes
   little-endian at any alignment and refuse a read past the end. A file
   of another host or a damaged file then gives no answer, never a crash.
   A mapped image has no known end, and its reads pass SIZE_MAX. */
struct bytes {
    const uint8_t *data;
    size_t size;
};

static bool inside(const struct bytes *b, uint64_t at, uint64_t n)
{
    return at <= b->size && n <= b->size - at;
}

static uint64_t get(const uint8_t *p, int n)
{
    uint64_t v = 0;
    int i;

    for (i = n - 1; i >= 0; i--) {
        v = v << 8 | p[i];
    }
    return v;
}

/* A reader that walks bytes from a start to an end. */
struct cursor {
    const uint8_t *at;
    const uint8_t *end;
    bool bad;
};

static uint64_t take(struct cursor *c, int n)
{
    uint64_t v;

    if (c->bad || c->end - c->at < n) {
        c->bad = true;
        return 0;
    }
    v = get(c->at, n);
    c->at += n;
    return v;
}

static uint64_t uleb(struct cursor *c)
{
    uint64_t v = 0;
    int shift = 0;

    while (!c->bad) {
        uint64_t byte = take(c, 1);
        if (shift < 64) {
            v |= (byte & 0x7f) << shift;
        }
        shift += 7;
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    return v;
}

static int64_t sleb(struct cursor *c)
{
    uint64_t v = 0;
    int shift = 0;
    uint64_t byte = 0;

    while (!c->bad) {
        byte = take(c, 1);
        if (shift < 64) {
            v |= (byte & 0x7f) << shift;
        }
        shift += 7;
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    if (shift < 64 && (byte & 0x40) != 0) {
        v |= ~(uint64_t)0 << shift;
    }
    return (int64_t)v;
}

/* A NUL-terminated string at the cursor, or NULL when it runs off. */
static const char *string_at(struct cursor *c)
{
    const uint8_t *start = c->at;

    while (!c->bad && c->at < c->end && *c->at != 0) {
        c->at++;
    }
    if (c->bad || c->at >= c->end) {
        c->bad = true;
        return NULL;
    }
    c->at++;
    return (const char *)start;
}

/* The NUL-terminated string at offset of a string section. */
static const char *string_in(const uint8_t *section, size_t size,
                             uint64_t offset)
{
    if (section == NULL || offset >= size ||
        memchr(section + offset, 0, size - offset) == NULL) {
        return NULL;
    }
    return (const char *)section + offset;
}

/* The DWARF line table */

/* The sections a line table reads its strings from. */
struct line_sections {
    const uint8_t *line;
    size_t line_size;
    const uint8_t *line_str;        /* .debug_line_str, for DWARF 5 */
    size_t line_str_size;
    const uint8_t *str;             /* .debug_str */
    size_t str_size;
};

enum {
    FORM_BLOCK = 0x09, FORM_DATA1 = 0x0b, FORM_DATA2 = 0x05,
    FORM_DATA4 = 0x06, FORM_DATA8 = 0x07, FORM_DATA16 = 0x1e,
    FORM_STRING = 0x08, FORM_STRP = 0x0e, FORM_UDATA = 0x0f,
    FORM_LINE_STRP = 0x1f
};

/* Read one value of a DWARF 5 entry format. A string gives text, and a
   number gives number. */
static bool read_form(struct cursor *c, uint64_t form, bool wide,
                      const struct line_sections *s, const char **text,
                      uint64_t *number)
{
    *text = NULL;
    *number = 0;
    switch (form) {
    case FORM_STRING:
        *text = string_at(c);
        break;
    case FORM_LINE_STRP:
        *text = string_in(s->line_str, s->line_str_size, take(c, wide ? 8 : 4));
        break;
    case FORM_STRP:
        *text = string_in(s->str, s->str_size, take(c, wide ? 8 : 4));
        break;
    case FORM_UDATA:
        *number = uleb(c);
        break;
    case FORM_DATA1:
        *number = take(c, 1);
        break;
    case FORM_DATA2:
        *number = take(c, 2);
        break;
    case FORM_DATA4:
        *number = take(c, 4);
        break;
    case FORM_DATA8:
        *number = take(c, 8);
        break;
    case FORM_DATA16:
        take(c, 8);
        take(c, 8);
        break;
    case FORM_BLOCK: {
        uint64_t n = uleb(c);
        if ((uint64_t)(c->end - c->at) < n) {
            c->bad = true;
        } else {
            c->at += n;
        }
        break;
    }
    default:
        return false;
    }
    return !c->bad;
}

/* The name of file index in the table of a unit. DWARF 5 counts from 0
   and earlier versions count from 1. The path is the name as the unit
   wrote it, which antic makes the path under the search root. */
static const char *file_name(struct cursor table, int version, uint64_t index,
                             bool wide, const struct line_sections *s)
{
    uint64_t i;

    if (version >= 5) {
        uint8_t formats = (uint8_t)take(&table, 1);
        uint64_t kinds[16];
        uint64_t forms[16];
        uint64_t count;
        uint8_t k;
        if (formats > 16) {
            return NULL;
        }
        for (k = 0; k < formats; k++) {
            kinds[k] = uleb(&table);
            forms[k] = uleb(&table);
        }
        count = uleb(&table);
        for (i = 0; i < count && !table.bad; i++) {
            const char *path = NULL;
            for (k = 0; k < formats; k++) {
                const char *text;
                uint64_t number;
                if (!read_form(&table, forms[k], wide, s, &text, &number)) {
                    return NULL;
                }
                if (kinds[k] == 1) {
                    path = text;
                }
            }
            if (i == index) {
                return path;
            }
        }
        return NULL;
    }
    for (i = 1; !table.bad; i++) {
        const char *name = string_at(&table);
        if (name == NULL || name[0] == 0) {
            return NULL;
        }
        uleb(&table);
        uleb(&table);
        uleb(&table);
        if (i == index) {
            return name;
        }
    }
    return NULL;
}

/* One row of the line program. */
struct row {
    uint64_t address;
    uint64_t file;
    int64_t line;
};

/* Look address up in one unit of the line table. found says whether a row
   covers it, which is the last row at or below it before the next. */
static bool unit_line(struct cursor *c, const struct line_sections *s,
                      uint64_t address, struct anti_found *out)
{
    uint64_t length = take(c, 4);
    bool wide = length == 0xffffffffu;
    const uint8_t *end;
    struct cursor unit;
    struct cursor table;
    uint64_t header_length;
    int version;
    uint8_t min_length;
    int8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;
    uint8_t lengths[256];
    struct row r;
    struct row last;
    bool have_last = false;
    int i;

    if (wide) {
        length = take(c, 8);
    }
    if (c->bad || (uint64_t)(c->end - c->at) < length) {
        c->bad = true;
        return false;
    }
    end = c->at + length;
    unit.at = c->at;
    unit.end = end;
    unit.bad = false;
    c->at = end;
    version = (int)take(&unit, 2);
    if (version < 2 || version > 5) {
        return false;
    }
    /* DWARF 5 names the size of an address and of a segment selector,
       and each set_address carries its own length. */
    if (version >= 5) {
        take(&unit, 2);
    }
    header_length = take(&unit, wide ? 8 : 4);
    if (unit.bad || (uint64_t)(unit.end - unit.at) < header_length) {
        return false;
    }
    table.end = unit.at + header_length;
    min_length = (uint8_t)take(&unit, 1);
    /* The operations per instruction, from DWARF 4, and the default of
       is_stmt, which a lookup does not need. */
    if (version >= 4) {
        take(&unit, 1);
    }
    take(&unit, 1);
    line_base = (int8_t)take(&unit, 1);
    line_range = (uint8_t)take(&unit, 1);
    opcode_base = (uint8_t)take(&unit, 1);
    memset(lengths, 0, sizeof lengths);
    for (i = 1; i < opcode_base; i++) {
        lengths[i] = (uint8_t)take(&unit, 1);
    }
    if (unit.bad || line_range == 0) {
        return false;
    }
    /* The directories stand before the files, and the file table is read
       again for the one name a lookup needs. */
    if (version >= 5) {
        uint8_t formats = (uint8_t)take(&unit, 1);
        uint64_t forms[16];
        uint64_t count;
        uint8_t k;
        uint64_t j;
        if (formats > 16) {
            return false;
        }
        for (k = 0; k < formats; k++) {
            uleb(&unit);
            forms[k] = uleb(&unit);
        }
        count = uleb(&unit);
        for (j = 0; j < count && !unit.bad; j++) {
            for (k = 0; k < formats; k++) {
                const char *text;
                uint64_t number;
                if (!read_form(&unit, forms[k], wide, s, &text, &number)) {
                    return false;
                }
            }
        }
    } else {
        const char *dir;
        while ((dir = string_at(&unit)) != NULL && dir[0] != 0) {
        }
    }
    table.at = unit.at;
    table.bad = unit.bad;
    unit.at = table.end;
    memset(&r, 0, sizeof r);
    r.file = 1;
    r.line = 1;
    memset(&last, 0, sizeof last);
    while (!unit.bad && unit.at < unit.end) {
        uint8_t op = (uint8_t)take(&unit, 1);
        bool emit = false;
        bool ends = false;
        if (op >= opcode_base) {
            uint8_t adjusted = (uint8_t)(op - opcode_base);
            r.address += (uint64_t)(adjusted / line_range) * min_length;
            r.line += line_base + adjusted % line_range;
            emit = true;
        } else if (op == 0) {
            uint64_t n = uleb(&unit);
            const uint8_t *next;
            uint8_t sub;
            if (unit.bad || n == 0 || (uint64_t)(unit.end - unit.at) < n) {
                break;
            }
            next = unit.at + n;
            sub = (uint8_t)take(&unit, 1);
            if (sub == 1) {
                emit = true;
                ends = true;
            } else if (sub == 2) {
                r.address = take(&unit, n - 1 >= 8 ? 8 : (int)(n - 1));
            }
            unit.at = next;
        } else if (op == 1) {
            emit = true;
        } else if (op == 2) {
            r.address += uleb(&unit) * min_length;
        } else if (op == 3) {
            r.line += sleb(&unit);
        } else if (op == 4) {
            r.file = uleb(&unit);
        } else if (op == 8) {
            r.address += (uint64_t)((255 - opcode_base) / line_range) *
                         min_length;
        } else if (op == 9) {
            r.address += take(&unit, 2);
        } else {
            for (i = 0; i < lengths[op]; i++) {
                uleb(&unit);
            }
        }
        if (!emit) {
            continue;
        }
        if (have_last && last.address <= address && address < r.address) {
            const char *name = file_name(table, version, last.file, wide, s);
            out->file = name;
            out->file_length = name != NULL ? strlen(name) : 0;
            out->line = last.line;
            return true;
        }
        last = r;
        have_last = !ends;
        if (ends) {
            memset(&r, 0, sizeof r);
            r.file = 1;
            r.line = 1;
        }
    }
    return false;
}

/* The file and the line of address in the line table of s. */
static bool dwarf_line(const struct line_sections *s, uint64_t address,
                       struct anti_found *out)
{
    struct cursor c;

    if (s->line == NULL) {
        return false;
    }
    c.at = s->line;
    c.end = s->line + s->line_size;
    c.bad = false;
    while (!c.bad && c.at < c.end) {
        if (unit_line(&c, s, address, out)) {
            return true;
        }
    }
    return false;
}

/* ELF */

enum {
    ELF_SHT_SYMTAB = 2, ELF_SHF_EXECINSTR = 4, ELF_STT_FUNC = 2,
    ELF_STT_NOTYPE = 0
};

/* One section of an ELF file. */
struct elf_section {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
};

static bool elf_header(const struct bytes *b, uint64_t *shoff,
                       uint32_t *shnum, uint32_t *shstrndx)
{
    const uint8_t *d = b->data;

    if (!inside(b, 0, 64) || memcmp(d, "\x7f" "ELF", 4) != 0 || d[4] != 2 ||
        d[5] != 1 || get(d + 58, 2) != 64) {
        return false;
    }
    *shoff = get(d + 40, 8);
    *shnum = (uint32_t)get(d + 60, 2);
    *shstrndx = (uint32_t)get(d + 62, 2);
    return inside(b, *shoff, (uint64_t)*shnum * 64) && *shstrndx < *shnum;
}

static bool elf_section_at(const struct bytes *b, uint64_t shoff,
                           uint32_t index, struct elf_section *out)
{
    const uint8_t *p = b->data + shoff + (uint64_t)index * 64;

    out->name = (uint32_t)get(p, 4);
    out->type = (uint32_t)get(p + 4, 4);
    out->flags = get(p + 8, 8);
    out->offset = get(p + 24, 8);
    out->size = get(p + 32, 8);
    out->link = (uint32_t)get(p + 40, 4);
    return inside(b, out->offset, out->type == 8 ? 0 : out->size);
}

/* The section of an ELF file with the name, by index. */
static bool elf_find(const struct bytes *b, const char *name,
                     struct elf_section *out, uint32_t *index)
{
    uint64_t shoff;
    uint32_t shnum;
    uint32_t shstrndx;
    struct elf_section names;
    uint32_t i;

    if (!elf_header(b, &shoff, &shnum, &shstrndx) ||
        !elf_section_at(b, shoff, shstrndx, &names)) {
        return false;
    }
    for (i = 0; i < shnum; i++) {
        const char *s;
        if (!elf_section_at(b, shoff, i, out)) {
            continue;
        }
        s = string_in(b->data + names.offset, names.size, out->name);
        if (s != NULL && strcmp(s, name) == 0) {
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }
    return false;
}

/* Walk the symbols of an ELF file. Each call of visit gets the name, the
   value, the size, the type and the flags of the section the symbol is
   in. A true result stops the walk. */
typedef bool elf_visit(void *context, const char *name, uint64_t value,
                       uint64_t size, unsigned type, uint64_t flags);

static bool elf_symbols(const struct bytes *b, elf_visit *visit, void *context)
{
    uint64_t shoff;
    uint32_t shnum;
    uint32_t shstrndx;
    struct elf_section symtab;
    struct elf_section strtab;
    uint64_t i;

    if (!elf_header(b, &shoff, &shnum, &shstrndx) ||
        !elf_find(b, ".symtab", &symtab, NULL) ||
        symtab.type != ELF_SHT_SYMTAB || symtab.link >= shnum ||
        !elf_section_at(b, shoff, symtab.link, &strtab)) {
        return false;
    }
    for (i = 0; i + 24 <= symtab.size; i += 24) {
        const uint8_t *p = b->data + symtab.offset + i;
        uint32_t shndx = (uint32_t)get(p + 6, 2);
        struct elf_section home;
        const char *name =
            string_in(b->data + strtab.offset, strtab.size, get(p, 4));
        if (name == NULL || name[0] == 0 || shndx == 0 || shndx >= shnum ||
            !elf_section_at(b, shoff, shndx, &home)) {
            continue;
        }
        if (visit(context, name, get(p + 8, 8), get(p + 16, 8), p[4] & 0xf,
                  home.flags)) {
            return true;
        }
    }
    return false;
}

/* The best function symbol so far for an address. */
struct nearest {
    uint64_t address;
    const char *name;
    uint64_t value;
};

/* DESIGN: the entry of an Anti program has two names at one address,
   `module.main` and `anti.rt.main`. The name of the program is the one a
   frame gives, so a name under `anti.rt.` loses a tie. */
static bool better(const struct nearest *n, const char *name, uint64_t value)
{
    if (n->name == NULL || value > n->value) {
        return true;
    }
    return value == n->value && strncmp(n->name, "anti.rt.", 8) == 0 &&
           strncmp(name, "anti.rt.", 8) != 0;
}

static bool elf_nearest(void *context, const char *name, uint64_t value,
                        uint64_t size, unsigned type, uint64_t flags)
{
    struct nearest *n = context;

    if ((type != ELF_STT_FUNC && type != ELF_STT_NOTYPE) ||
        (flags & ELF_SHF_EXECINSTR) == 0 || value > n->address ||
        (size > 0 && n->address >= value + size)) {
        return false;
    }
    if (better(n, name, value)) {
        n->name = name;
        n->value = value;
    }
    return false;
}

bool anti_elf_function(const uint8_t *file, size_t size, uint64_t vaddr,
                       struct anti_found *out)
{
    struct bytes b = {file, size};
    struct nearest n = {vaddr, NULL, 0};

    elf_symbols(&b, elf_nearest, &n);
    if (n.name == NULL) {
        return false;
    }
    out->function = n.name;
    out->function_length = strlen(n.name);
    return true;
}

/* The symbol named in a walk, and its value once found. */
struct named {
    const char *name;
    uint64_t value;
};

static bool elf_named(void *context, const char *name, uint64_t value,
                      uint64_t size, unsigned type, uint64_t flags)
{
    struct named *n = context;

    (void)size;
    (void)type;
    (void)flags;
    if (strcmp(name, n->name) != 0) {
        return false;
    }
    n->value = value;
    return true;
}

bool anti_elf_symbol(const uint8_t *file, size_t size, const char *name,
                     uint64_t *vaddr)
{
    struct bytes b = {file, size};
    struct named n = {name, 0};

    if (!elf_symbols(&b, elf_named, &n)) {
        return false;
    }
    *vaddr = n.value;
    return true;
}

bool anti_elf_line(const uint8_t *file, size_t size, uint64_t vaddr,
                   struct anti_found *out)
{
    struct bytes b = {file, size};
    struct elf_section section;
    struct line_sections s;

    memset(&s, 0, sizeof s);
    if (!elf_find(&b, ".debug_line", &section, NULL)) {
        return false;
    }
    s.line = file + section.offset;
    s.line_size = section.size;
    if (elf_find(&b, ".debug_line_str", &section, NULL)) {
        s.line_str = file + section.offset;
        s.line_str_size = section.size;
    }
    if (elf_find(&b, ".debug_str", &section, NULL)) {
        s.str = file + section.offset;
        s.str_size = section.size;
    }
    return dwarf_line(&s, vaddr, out);
}

/* Mach-O */

#define MACHO_MAGIC_64 0xfeedfacfu

enum {
    MACHO_SEGMENT_64 = 0x19, MACHO_SYMTAB = 0x2,
    MACHO_N_STAB = 0xe0, MACHO_N_TYPE = 0x0e, MACHO_N_SECT = 0x0e,
    MACHO_N_FUN = 0x24, MACHO_N_OSO = 0x66, MACHO_NLIST = 16
};

/* Call visit on each load command, with its kind and its bytes. */
typedef bool macho_visit(void *context, uint32_t kind, const uint8_t *command,
                         uint32_t size);

static bool macho_commands(const struct bytes *b, macho_visit *visit,
                           void *context)
{
    uint32_t count;
    uint32_t total;
    uint64_t at = 32;
    uint32_t i;

    if (!inside(b, 0, 32) || get(b->data, 4) != MACHO_MAGIC_64) {
        return false;
    }
    count = (uint32_t)get(b->data + 16, 4);
    total = (uint32_t)get(b->data + 20, 4);
    if (!inside(b, 32, total)) {
        return false;
    }
    for (i = 0; i < count; i++) {
        uint32_t kind;
        uint32_t size;
        if (at + 8 > 32 + (uint64_t)total) {
            return false;
        }
        kind = (uint32_t)get(b->data + at, 4);
        size = (uint32_t)get(b->data + at + 4, 4);
        if (size < 8 || at + size > 32 + (uint64_t)total) {
            return false;
        }
        if (visit(context, kind, b->data + at, size)) {
            return true;
        }
        at += size;
    }
    return false;
}

/* What the walk over the commands of a header collects for its symbol
   table. */
struct macho_layout {
    uint64_t linkedit_vmaddr;
    uint64_t linkedit_fileoff;
    bool has_linkedit;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
    bool has_symtab;
};

static bool macho_layout_visit(void *context, uint32_t kind,
                               const uint8_t *command, uint32_t size)
{
    struct macho_layout *l = context;

    if (kind == MACHO_SEGMENT_64 && size >= 72 &&
        strncmp((const char *)command + 8, "__LINKEDIT", 16) == 0) {
        l->linkedit_vmaddr = get(command + 24, 8);
        l->linkedit_fileoff = get(command + 40, 8);
        l->has_linkedit = true;
    } else if (kind == MACHO_SYMTAB && size >= 24) {
        l->symoff = (uint32_t)get(command + 8, 4);
        l->nsyms = (uint32_t)get(command + 12, 4);
        l->stroff = (uint32_t)get(command + 16, 4);
        l->strsize = (uint32_t)get(command + 20, 4);
        l->has_symtab = true;
    }
    return false;
}

bool anti_macho_table(const uint8_t *header, size_t size, bool mapped,
                      intptr_t slide, struct anti_macho_table *out)
{
    struct bytes b = {header, size};
    struct macho_layout l;
    const uint8_t *base = header;

    memset(&l, 0, sizeof l);
    macho_commands(&b, macho_layout_visit, &l);
    if (!l.has_symtab) {
        return false;
    }
    if (mapped) {
        if (!l.has_linkedit) {
            return false;
        }
        base = (const uint8_t *)(uintptr_t)(l.linkedit_vmaddr + (uint64_t)slide -
                                            l.linkedit_fileoff);
    } else if (!inside(&b, l.symoff, (uint64_t)l.nsyms * MACHO_NLIST) ||
               !inside(&b, l.stroff, l.strsize)) {
        return false;
    }
    out->symbols = base + l.symoff;
    out->count = l.nsyms;
    out->strings = (const char *)base + l.stroff;
    out->strings_size = l.strsize;
    return true;
}

/* The fields of symbol i of a table, and its name, or NULL. */
static const char *macho_symbol(const struct anti_macho_table *t, uint32_t i,
                                uint8_t *type, uint8_t *sect, uint64_t *value)
{
    const uint8_t *p = t->symbols + (uint64_t)i * MACHO_NLIST;

    *type = p[4];
    *sect = p[5];
    *value = get(p + 8, 8);
    return string_in((const uint8_t *)t->strings, t->strings_size, get(p, 4));
}

/* The name of a Mach-O symbol without the `_` that C puts before it. */
static const char *unprefixed(const char *name)
{
    return name[0] == '_' ? name + 1 : name;
}

bool anti_macho_function(const struct anti_macho_table *t, uint64_t vaddr,
                         struct anti_found *out)
{
    struct nearest n = {vaddr, NULL, 0};
    uint32_t i;

    for (i = 0; i < t->count; i++) {
        uint8_t type;
        uint8_t sect;
        uint64_t value;
        const char *name = macho_symbol(t, i, &type, &sect, &value);
        if (name == NULL || name[0] == 0 || (type & MACHO_N_STAB) != 0 ||
            (type & MACHO_N_TYPE) != MACHO_N_SECT || sect != 1 ||
            value > vaddr) {
            continue;
        }
        if (better(&n, unprefixed(name), value)) {
            n.name = unprefixed(name);
            n.value = value;
        }
    }
    if (n.name == NULL) {
        return false;
    }
    out->function = n.name;
    out->function_length = strlen(n.name);
    return true;
}

bool anti_macho_symbol(const struct anti_macho_table *t, const char *name,
                       uint64_t *vaddr)
{
    uint32_t i;

    for (i = 0; i < t->count; i++) {
        uint8_t type;
        uint8_t sect;
        uint64_t value;
        const char *s = macho_symbol(t, i, &type, &sect, &value);
        if (s != NULL && (type & MACHO_N_STAB) == 0 &&
            (type & MACHO_N_TYPE) == MACHO_N_SECT &&
            strcmp(unprefixed(s), name) == 0) {
            *vaddr = value;
            return true;
        }
    }
    return false;
}

/* DESIGN: a Mach-O link keeps the debug information in the objects and
   names them in its symbol table, the debug map. An object path stands
   before the functions it holds, and each function has two entries, its
   start and its size. A link can give two names to one function and the
   size to one of them, so an entry of size 0 matches no address. */
bool anti_macho_debug_map(const struct anti_macho_table *t, uint64_t vaddr,
                          const char **object, const char **symbol,
                          uint64_t *start)
{
    const char *current = NULL;
    const char *name = NULL;
    uint64_t begin = 0;
    uint32_t i;

    for (i = 0; i < t->count; i++) {
        uint8_t type;
        uint8_t sect;
        uint64_t value;
        const char *s = macho_symbol(t, i, &type, &sect, &value);
        if (s == NULL) {
            continue;
        }
        if (type == MACHO_N_OSO) {
            current = s;
        } else if (type == MACHO_N_FUN && s[0] != 0) {
            name = s;
            begin = value;
        } else if (type == MACHO_N_FUN && name != NULL) {
            if (current != NULL && begin <= vaddr && vaddr - begin < value) {
                *object = current;
                *symbol = name;
                *start = begin;
                return true;
            }
            name = NULL;
        }
    }
    return false;
}

/* The sections of the one segment of a Mach-O object that the line
   lookup reads. */
struct macho_sections {
    struct bytes file;
    uint64_t line_offset;
    uint64_t line_size;
    uint32_t line_reloff;
    uint32_t line_nreloc;
    uint64_t line_str_offset;
    uint64_t line_str_size;
    uint64_t str_offset;
    uint64_t str_size;
};

static bool macho_sections_visit(void *context, uint32_t kind,
                                 const uint8_t *command, uint32_t size)
{
    struct macho_sections *m = context;
    uint32_t count;
    uint32_t i;

    if (kind != MACHO_SEGMENT_64 || size < 72) {
        return false;
    }
    count = (uint32_t)get(command + 64, 4);
    for (i = 0; i < count && 72 + (uint64_t)(i + 1) * 80 <= size; i++) {
        const uint8_t *s = command + 72 + (uint64_t)i * 80;
        uint64_t sectsize = get(s + 40, 8);
        uint64_t offset = get(s + 48, 4);
        if (strncmp((const char *)s + 16, "__DWARF", 16) != 0) {
            continue;
        }
        if (strncmp((const char *)s, "__debug_line", 16) == 0) {
            m->line_offset = offset;
            m->line_size = sectsize;
            m->line_reloff = (uint32_t)get(s + 56, 4);
            m->line_nreloc = (uint32_t)get(s + 60, 4);
        } else if (strncmp((const char *)s, "__debug_line_str", 16) == 0) {
            m->line_str_offset = offset;
            m->line_str_size = sectsize;
        } else if (strncmp((const char *)s, "__debug_str", 16) == 0) {
            m->str_offset = offset;
            m->str_size = sectsize;
        }
    }
    return false;
}

static bool macho_object_sections(const uint8_t *file, size_t size,
                                  struct macho_sections *m)
{
    memset(m, 0, sizeof *m);
    m->file.data = file;
    m->file.size = size;
    macho_commands(&m->file, macho_sections_visit, m);
    return m->line_size > 0 &&
           inside(&m->file, m->line_offset, m->line_size) &&
           inside(&m->file, m->line_str_offset, m->line_str_size) &&
           inside(&m->file, m->str_offset, m->str_size);
}

/* DESIGN: the assembler leaves the address of a line sequence as a
   relocation. One against a section holds the address already, and one
   against a symbol holds the addend, which the address of the symbol
   completes. Only the plain unsigned kind, 0 on both architectures, is
   resolved, since a line table writes no other. */
void anti_macho_relocate(uint8_t *file, size_t size)
{
    struct macho_sections m;
    struct anti_macho_table t;
    uint32_t i;

    if (!macho_object_sections(file, size, &m) ||
        !anti_macho_table(file, size, false, 0, &t) ||
        !inside(&m.file, m.line_reloff, (uint64_t)m.line_nreloc * 8)) {
        return;
    }
    for (i = 0; i < m.line_nreloc; i++) {
        const uint8_t *r = file + m.line_reloff + (uint64_t)i * 8;
        uint32_t address = (uint32_t)get(r, 4);
        uint32_t info = (uint32_t)get(r + 4, 4);
        uint32_t index = info & 0xffffff;
        int length = 1 << (info >> 25 & 3);
        bool external = (info >> 27 & 1) != 0;
        uint32_t type = info >> 28;
        uint8_t kind;
        uint8_t sect;
        uint64_t value;
        uint8_t *field;
        uint64_t sum;
        int k;
        if ((address & 0x80000000u) != 0 || !external || type != 0 ||
            index >= t.count || length != 8 ||
            (uint64_t)address + 8 > m.line_size) {
            continue;
        }
        macho_symbol(&t, index, &kind, &sect, &value);
        field = file + m.line_offset + address;
        sum = get(field, 8) + value;
        for (k = 0; k < 8; k++) {
            field[k] = (uint8_t)(sum >> (8 * k));
        }
    }
}

bool anti_macho_object_line(const uint8_t *file, size_t size,
                            const char *symbol, uint64_t offset,
                            struct anti_found *out)
{
    struct macho_sections m;
    struct anti_macho_table t;
    struct line_sections s;
    uint32_t i;

    if (!macho_object_sections(file, size, &m) ||
        !anti_macho_table(file, size, false, 0, &t)) {
        return false;
    }
    for (i = 0; i < t.count; i++) {
        uint8_t type;
        uint8_t sect;
        uint64_t value;
        const char *name = macho_symbol(&t, i, &type, &sect, &value);
        if (name == NULL || (type & MACHO_N_STAB) != 0 ||
            (type & MACHO_N_TYPE) != MACHO_N_SECT ||
            strcmp(name, symbol) != 0) {
            continue;
        }
        memset(&s, 0, sizeof s);
        s.line = file + m.line_offset;
        s.line_size = m.line_size;
        s.line_str = m.line_str_size > 0 ? file + m.line_str_offset : NULL;
        s.line_str_size = m.line_str_size;
        s.str = m.str_size > 0 ? file + m.str_offset : NULL;
        s.str_size = m.str_size;
        return dwarf_line(&s, value + offset, out);
    }
    return false;
}
