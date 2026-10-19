/* The start of the injections. The lines of the configuration that name
   a library for an interface, and the providers the loader takes from
   those libraries before main. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fail.h"
#include "inject.h"
#include "plugin.h"
#include "str.h"

/* A startup error: the message through the failure routine and status
   70, the status of every refusal at start. The format is a literal. */
#define startup_error(...) anti_rt_fail_exit(70, "anti: " __VA_ARGS__)

/* DESIGN: a line that names an interface to take from a library. The
   interface must be one the program injects, and it must not be
   `inject final`. The line is kept, and the library is opened once the
   configuration has been read, because the search directories come
   from it. The command line wins over the file, and both win over the
   provider the build named. */
enum { INJECTION_MAX = 16 };

struct injection {
    const struct anti_injectable *in;
    char *library;
    int layer;
    char *where;
};

/* DESIGN: the lines of the command line and of the file layer, which the
   start reads before main. rt.configure refuses a file that holds the
   table, so no line comes after the start, and no reader needs a
   lock. */
static struct injection injections[INJECTION_MAX];
static size_t injection_count;

/* The length bytes at text and a NUL, in memory of the start. A slot
   keeps its copies until the program ends, and the caller frees any
   other with free. */
static char *copy(const char *text, size_t length)
{
    char *out = anti_rt_start_alloc(length + 1);

    memcpy(out, text, length);
    out[length] = '\0';
    return out;
}

/* The entry of the interface the text names, or NULL. */
static const struct anti_injectable *injectable_of(const char *name,
                                                   size_t length)
{
    int64_t i;

    for (i = 0; i < anti_rt_injectable.count; i++) {
        const unsigned char *text = anti_rt_injectable.interfaces[i].name;
        if (strlen((const char *)text) == length &&
            memcmp(text, name, length) == 0) {
            return &anti_rt_injectable.interfaces[i];
        }
    }
    return NULL;
}

/* The text between entry i and the one before it, of count entries. */
static const char *separator(int64_t i, int64_t count)
{
    if (i == 0) {
        return "";
    }
    return i + 1 == count ? " and " : ", ";
}

/* The interfaces the program does carry, for the message of a line that
   names one it has not, in memory of its own. */
static char *list_injectable(void)
{
    int64_t count = anti_rt_injectable.count;
    size_t length = 0;
    size_t at = 0;
    char *out;
    int64_t i;

    if (count == 0) {
        return copy("none", 4);
    }
    for (i = 0; i < count; i++) {
        length += strlen(separator(i, count)) +
                  strlen((const char *)anti_rt_injectable.interfaces[i].name);
    }
    out = anti_rt_start_alloc(length + 1);
    for (i = 0; i < count; i++) {
        const char *before = separator(i, count);
        const char *name = (const char *)anti_rt_injectable.interfaces[i].name;
        memcpy(out + at, before, strlen(before));
        at += strlen(before);
        memcpy(out + at, name, strlen(name));
        at += strlen(name);
    }
    out[at] = '\0';
    return out;
}

void anti_rt_inject_line(const char *name, size_t length, const char *library,
                         const char *where, int layer)
{
    const struct anti_injectable *in = injectable_of(name, length);
    struct injection *slot = NULL;
    size_t i;

    if (in == NULL) {
        startup_error("%s: %.*s is no injectable interface of this program, "
                      "which has %s",
                      where, (int)length, name, list_injectable());
    }
    if (in->final != 0) {
        startup_error("%s: %s is `inject final` in %s and cannot be replaced",
                      where, (const char *)in->field,
                      (const char *)in->owner);
    }
    if (in->holder == NULL || in->thunk == NULL) {
        startup_error("%s: %.*s has no place for a provider of a library",
                      where, (int)length, name);
    }
    for (i = 0; i < injection_count; i++) {
        if (injections[i].in == in) {
            slot = &injections[i];
        }
    }
    if (slot == NULL && injection_count == INJECTION_MAX) {
        startup_error("%s: at most %d interfaces come from a library",
                      where, INJECTION_MAX);
    }
    if (slot == NULL) {
        slot = &injections[injection_count++];
    } else if (slot->layer > layer) {
        return;
    }
    free(slot->library);
    free(slot->where);
    slot->in = in;
    slot->library = copy(library, strlen(library));
    slot->layer = layer;
    slot->where = copy(where, strlen(where));
}

/* DESIGN: an interface whose provider is a library gets its object
   before `main`. The build's `plugin:` and `discover` are the lowest
   layer. The configuration file stands above them and the command line
   above both. The runtime stores the object in the holder of the
   interface and puts the thunk in its slot. Every site of the program
   then calls through one function, as it did before. */
void anti_rt_inject_start(const char *dirs)
{
    int64_t i;

    for (i = 0; i < anti_rt_injectable.count; i++) {
        const struct anti_injectable *in = &anti_rt_injectable.interfaces[i];
        const char *library = "";
        const char *where = "the build";
        size_t length = 0;
        int named = 0;
        void *provider;
        size_t k;
        /* An empty path is `discover`, which searches the directories
           of the `plugins` key. */
        if (in->discover != 0) {
            library = in->library != NULL ? (const char *)in->library : "";
            length = (size_t)in->library_length;
            named = 1;
        }
        for (k = 0; k < injection_count; k++) {
            if (injections[k].in == in) {
                library = injections[k].library;
                length = strlen(library);
                where = injections[k].where;
                named = 1;
            }
        }
        if (!named) {
            continue;
        }
        provider = anti_rt_plugin_provider(in->name,
                                           (int64_t)strlen((const char *)
                                                           in->name),
                                           (const unsigned char *)library,
                                           (int64_t)length, dirs);
        if (provider == NULL) {
            struct anti_text why = anti_rt_plugin_message();
            startup_error("%s: %s comes from a library: %.*s", where,
                          (const char *)in->name, (int)why.len, why.ptr);
        }
        *in->holder = provider;
        in->slot->provider = in->thunk;
    }
}

void anti_rt_inject_inspect(void)
{
    int64_t k;
    size_t i;

    if (anti_rt_injectable.count == 0) {
        printf("injectable interfaces: none\n");
    } else {
        printf("injectable interfaces:\n");
    }
    for (k = 0; k < anti_rt_injectable.count; k++) {
        const struct anti_injectable *in = &anti_rt_injectable.interfaces[k];
        const struct anti_descriptor *d = in->descriptor;
        const struct anti_slots *reached =
            d != NULL ? anti_rt_plugin_slots(d) : NULL;
        int64_t used = 0;
        int64_t slot;
        for (slot = 0; reached != NULL && slot < reached->slot_count;
             slot++) {
            used += (reached->bits[slot / 8] >> (slot % 8)) & 1;
        }
        printf("  %s (%s.%s%s) version %.*s, %lld used slot%s\n",
               (const char *)in->name, (const char *)in->owner,
               (const char *)in->field, in->final != 0 ? ", final" : "",
               d != NULL ? (int)d->version_length : 0,
               d != NULL ? d->version : (const unsigned char *)"",
               (long long)used, used == 1 ? "" : "s");
    }
    if (injection_count == 0) {
        printf("loaded plugins: none\n");
    } else {
        printf("loaded plugins:\n");
    }
    for (i = 0; i < injection_count; i++) {
        printf("  %s for %s\n", injections[i].library,
               (const char *)injections[i].in->name);
    }
}
