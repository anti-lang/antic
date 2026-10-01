#include "diagnostic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

static void add(struct diagnostics *d, enum diag_name name, int line,
                int column, bool warning, bool doc, const char *format,
                va_list args)
{
    struct diagnostic *item;

    d->items = alloc_grow(d->items, &d->capacity, d->count, sizeof *d->items);
    item = &d->items[d->count++];
    item->line = line;
    item->column = column;
    item->warning = warning;
    item->doc = doc;
    item->promoted = false;
    item->name = name;
    vsnprintf(item->message, sizeof item->message, format, args);
}

void diagnostics_add(struct diagnostics *d, int line, int column,
                     const char *format, ...)
{
    va_list args;

    va_start(args, format);
    add(d, NAME_NONE, line, column, false, false, format, args);
    va_end(args);
}

void diagnostics_warn(struct diagnostics *d, enum diag_name name, int line,
                      int column, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    add(d, name, line, column, true, false, format, args);
    va_end(args);
}

void diagnostics_check(struct diagnostics *d, enum diag_name name, int line,
                       int column, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    add(d, name, line, column, false, false, format, args);
    va_end(args);
}

void diagnostics_doc(struct diagnostics *d, enum diag_name name, int line,
                     int column, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    add(d, name, line, column, true, true, format, args);
    va_end(args);
}

void diagnostics_free(struct diagnostics *d)
{
    free(d->items);
    d->items = NULL;
    d->count = 0;
    d->capacity = 0;
}
