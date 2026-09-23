#ifndef ANTIC_COFF_H
#define ANTIC_COFF_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* A COFF object to join: the name a message gives it and its bytes. */
struct coff_input {
    const char *name;
    const unsigned char *data;
    size_t size;
};

/* Join the objects into one COFF object in out, as `ld -r` joins them on
   ELF and Mach-O. A reference of one object to a symbol that another
   defines takes that symbol. Returns false and appends the reason to
   error when the joined object would link to another program than the
   objects do. */
bool coff_join(const struct coff_input *inputs, size_t count,
               struct text *out, struct text *error);

#endif
