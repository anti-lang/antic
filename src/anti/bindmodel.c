/* The memory of a binding and its warnings, the words of Anti a name of
   C cannot take, and the libraries each bundled library links. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "antic.h"
#include "bindmodel.h"
#include "files.h"

void bind_list_add(struct bind_list *list, void *item)
{
    if (list->count == list->room) {
        list->items = files_grow(list->items, &list->room, sizeof *list->items);
    }
    list->items[list->count++] = item;
}

const char *bind_strndup(struct bind_module *b, const char *s, size_t n)
{
    char *copy = arena_alloc(&b->arena, n + 1);

    memcpy(copy, s, n);
    return copy;
}

const char *bind_strdup(struct bind_module *b, const char *s)
{
    return bind_strndup(b, s, strlen(s));
}

void bind_warn(struct bind_module *b, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "anti: %s: warning: ", b->source);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    b->warnings++;
}

struct bind_type *bind_type_new(struct bind_module *b, enum bind_kind kind)
{
    struct bind_type *t = arena_alloc(&b->arena, sizeof *t);

    t->kind = kind;
    return t;
}

bool bind_is_keyword(const char *name)
{
    /* A name is free when the lexer reads it as one identifier. That
       covers the keywords, the reserved words and `null`. */
    return !antic_is_identifier(name);
}

/* DESIGN: the frameworks of Apple's SDK that each bundled library needs
   on macOS, which the binding names with `link framework`. raylib opens
   its window through GLFW over Cocoa and draws with OpenGL. miniaudio
   plays through Core Audio. A library outside the table names none. */
static const char *const raylib_frameworks[] = {
    "Cocoa", "CoreVideo", "IOKit", "OpenGL"
};
static const char *const miniaudio_frameworks[] = {
    "AudioToolbox", "CoreAudio", "CoreFoundation"
};

/* DESIGN: the libraries of the glibc sysroot that each bundled library
   needs on Linux, which the binding names with `link linux`. They are
   the link lines that the two projects give for Linux. A program that
   reaches one links dynamically against glibc. */
static const char *const raylib_linux[] = {
    "GL", "m", "pthread", "dl", "rt", "X11"
};
static const char *const miniaudio_linux[] = {"dl", "pthread", "m"};

size_t bind_linux_libraries(const char *library, const char *const **names)
{
    if (strcmp(library, "raylib") == 0) {
        *names = raylib_linux;
        return sizeof raylib_linux / sizeof raylib_linux[0];
    }
    if (strcmp(library, "miniaudio") == 0) {
        *names = miniaudio_linux;
        return sizeof miniaudio_linux / sizeof miniaudio_linux[0];
    }
    *names = NULL;
    return 0;
}

size_t bind_frameworks(const char *library, const char *const **names)
{
    if (strcmp(library, "raylib") == 0) {
        *names = raylib_frameworks;
        return sizeof raylib_frameworks / sizeof raylib_frameworks[0];
    }
    if (strcmp(library, "miniaudio") == 0) {
        *names = miniaudio_frameworks;
        return sizeof miniaudio_frameworks / sizeof miniaudio_frameworks[0];
    }
    *names = NULL;
    return 0;
}
