#ifndef ANTI_CURSOR_H
#define ANTI_CURSOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* DESIGN: the lines of a map, of a trace and of a report of
   AddressSanitizer come from other machines. sscanf leaves a number that
   does not fit its object undefined, so a cursor reads them instead. It
   takes digits alone and refuses a sign and a value above 64 bits. It
   names each token by its start and its length, so no line is copied
   into a buffer of fixed size. */
struct cursor {
    const char *at;
    const char *end;
};

/* Whether c is a blank between the tokens of a line. */
bool cursor_is_blank(char c);

/* Pass over blanks. Returns whether there was one. */
bool cursor_blank(struct cursor *c);

/* The digits in base at the cursor as one value. Returns false for no
   digit and for a value above UINT64_MAX. */
bool cursor_number(struct cursor *c, unsigned base, uint64_t *out);

/* A hex number, with or without `0x`. */
bool cursor_hex(struct cursor *c, uint64_t *out);

/* Pass over want. Returns false when the cursor stands on another byte
   or at the end. */
bool cursor_char(struct cursor *c, char want);

/* The bytes up to the next blank or the end. Returns false for none. */
bool cursor_token(struct cursor *c, const char **token, size_t *length);

#endif
