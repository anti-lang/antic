/* The runtime configuration.

   DESIGN: a program reads a configuration file only when --anti.conf or
   ANTI_CONF names one, or when main calls rt.configure. The runtime
   never searches for a file. Without one the program runs as it was
   built. The layers are the command line, then the file with its
   includes, then the build. A key takes the value of the highest layer
   that named it. */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "platform.h"
#include "plugin.h"

#include "rt.h"
#include "toml.h"

#include "atomic.h"


/* Where a value came from. A higher layer keeps its value. */
enum layer { LAYER_BUILD, LAYER_FILE, LAYER_COMMAND };

struct key {
    const char *name;
    char *value;        /* what the layer set, NULL while no layer did */
    size_t length;      /* the bytes of value */
    const char *source; /* the file of a value of the file layer */
    enum layer layer;
};

/* The keys of [runtime], in the order --anti.inspect prints them. */
static struct key keys[] = {
    {"backtrace", NULL, 0, NULL, LAYER_BUILD},
    {"hash_seed", NULL, 0, NULL, LAYER_BUILD},
    {"logger", NULL, 0, NULL, LAYER_BUILD},
    {"plugins", NULL, 0, NULL, LAYER_BUILD},
    {"threads", NULL, 0, NULL, LAYER_BUILD},
    {"trace", NULL, 0, NULL, LAYER_BUILD}
};

/* One option under --anti., what follows its name and what it does.
   DESIGN: the list is written once. --anti.help prints it, and the
   message of an unknown option names every entry of it. */
struct option {
    const char *name;
    const char *value;
    const char *what;
};

static const struct option options[] = {
    {"backtrace", "[=true|false]", "capture the frames of an error"},
    {"conf", "=<path>", "read the configuration file"},
    {"hash_seed", "=<number>", "the seed of the hashing collections"},
    {"help", "", "print these options and end"},
    {"inject", "=<interface>=<path>", "take an interface from a library"},
    {"inspect", "", "print the effective configuration and end"},
    {"logger", "=<path>", "the configuration file of anti.log"},
    {"plugins", "=<dir:dir>", "where a plugin is loaded from"},
    {"threads", "=<count>", "the threads of the worker pool"},
    {"trace", "=<what>", "what the runtime traces"}
};

#define KEY_COUNT (sizeof keys / sizeof keys[0])
#define OPTION_COUNT (sizeof options / sizeof options[0])

/* The file --anti.conf named, and the two options that end the program
   once the pass over the arguments is done. DESIGN: the pass over the
   arguments writes the three before main, on the one thread there is,
   and anti_rt_conf_start reads them before main as well. Nothing touches
   them once threads run, so they need no lock. */
static const char *conf_path;
static int inspect_asked;
static int help_asked;
/* 1 once a file was read or rt.configure began to read one. It is
   atomic, so two calls of rt.configure read one file between them. */
static int64_t file_read;

/* DESIGN: the options and the file named at start are read before main,
   on one thread. rt.configure of anti.runtime runs later, while any
   thread may read a key through anti_rt_conf_get. The lock guards the
   value, the source and the layer of every key. A reader on another
   thread may still hold the bytes of a value that a later layer or
   include replaces. So the list below keeps every such value until the
   program ends. A program holds few values, and they are short. */
static void hold(void) { anti_rt_lock_hold(ANTI_RT_LOCK_CONF); }
static void release(void) { anti_rt_lock_release(ANTI_RT_LOCK_CONF); }

struct retired {
    struct retired *next;
    char *value;
};

static struct retired *retired;

/* A startup error: the message through the failure routine and status
   70, the status of every refusal at start. The format is a literal. */
#define startup_error(...) anti_rt_fail_exit(70, "anti: " __VA_ARGS__)

/* The text the format and its values give, in memory of its own that
   the caller frees. */
static char *format_text(const char *format, ...)
{
    va_list rest;
    char *out;
    int n;

    va_start(rest, format);
    n = vsnprintf(NULL, 0, format, rest);
    va_end(rest);
    if (n < 0) {
        startup_error("a message of the configuration cannot be written");
    }
    out = malloc((size_t)n + 1);
    if (out == NULL) {
        startup_error("out of memory at program start");
    }
    va_start(rest, format);
    if (vsnprintf(out, (size_t)n + 1, format, rest) != n) {
        startup_error("a message of the configuration cannot be written");
    }
    va_end(rest);
    return out;
}

/* The text of a list, with its last entry after `and`. before gives
   what stands before each entry, as ` ` or ` --anti.`. */
static char *list_text(const char *const *names, size_t count,
                       const char *before)
{
    char *out = format_text("%s", "");
    size_t i;

    for (i = 0; i < count; i++) {
        const char *comma = i == 0 ? "" : (i + 1 == count ? " and" : ",");
        char *longer = format_text("%s%s%s%s", out, comma, before, names[i]);
        free(out);
        out = longer;
    }
    return out;
}

static char *copy(const char *text, size_t length)
{
    char *out = malloc(length + 1);

    if (out == NULL) {
        startup_error("out of memory at program start");
    }
    memcpy(out, text, length);
    out[length] = '\0';
    return out;
}

/* The key of that name, or NULL. */
static struct key *key_of(const char *name, size_t length)
{
    size_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        if (strlen(keys[i].name) == length &&
            memcmp(keys[i].name, name, length) == 0) {
            return &keys[i];
        }
    }
    return NULL;
}

/* The keys of [runtime], for the message of one that is not there. */
static char *key_names(void)
{
    const char *names[KEY_COUNT];
    size_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        names[i] = keys[i].name;
    }
    return list_text(names, KEY_COUNT, " ");
}

/* Whether the length bytes at text are the word. */
static int same_word(const char *text, size_t length, const char *word)
{
    return strlen(word) == length && memcmp(text, word, length) == 0;
}

/* The seed that value, of length bytes, names in decimal digits, into
   *out. Returns 0 when it holds anything else or passes UINT64_MAX. */
static int seed_of(const char *value, size_t length, uint64_t *out)
{
    uint64_t n = 0;
    size_t i;

    if (length == 0) {
        return 0;
    }
    for (i = 0; i < length; i++) {
        unsigned digit = (unsigned)(unsigned char)value[i] - '0';
        if (digit > 9 || n > (UINT64_MAX - digit) / 10) {
            return 0;
        }
        n = n * 10 + digit;
    }
    *out = n;
    return 1;
}

/* Take the value, of length bytes, into the key, unless a higher layer
   named it. `where` names the option or the line of the file, for a
   message about a value the key does not take. */
static void set_key(struct key *k, const char *value, size_t length,
                    enum layer layer, const char *source, const char *where)
{
    int on = same_word(value, length, "true");
    uint64_t seed = 0;
    struct retired *old;
    char *kept;

    if (length > INT_MAX) {
        startup_error("%s takes no value this long", where);
    }
    if (strcmp(k->name, "backtrace") == 0) {
        if (!on && !same_word(value, length, "false")) {
            startup_error("%s takes true or false, found %.*s", where,
                          (int)length, value);
        }
    } else if (strcmp(k->name, "hash_seed") == 0) {
        if (!seed_of(value, length, &seed)) {
            startup_error("%s takes a number from 0 to %llu, found %.*s",
                          where, (unsigned long long)UINT64_MAX, (int)length,
                          value);
        }
    } else if (strcmp(k->name, "threads") == 0) {
        struct anti_text text;
        text.ptr = (const unsigned char *)value;
        text.len = (int64_t)length;
        if (anti_rt_conf_threads(text) == 0) {
            startup_error("%s takes a count from 1 to %ld, found %.*s", where,
                          (long)ANTI_RT_THREADS_MAX, (int)length, value);
        }
    }
    kept = copy(value, length);
    old = malloc(sizeof *old);
    if (old == NULL) {
        startup_error("out of memory at program start");
    }
    /* A value this call does not take, and a node it does not use, are
       freed after the lock. */
    hold();
    if (k->layer <= layer) {
        if (k->value != NULL) {
            old->value = k->value;
            old->next = retired;
            retired = old;
            old = NULL;
        }
        k->value = kept;
        k->length = length;
        kept = NULL;
        k->source = source;
        k->layer = layer;
        if (strcmp(k->name, "backtrace") == 0) {
            anti_rt_atomic_store(&anti_rt_option_backtrace,
                                 (int64_t)sizeof anti_rt_option_backtrace,
                                 on);
        } else if (strcmp(k->name, "hash_seed") == 0) {
            anti_rt_hash_seed_set(seed);
        }
    }
    release();
    free(kept);
    free(old);
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

/* The interfaces the program does carry, for the message of a line that
   names one it has not. */
static char *list_injectable(void)
{
    char *out = format_text("%s", "");
    int64_t i;

    if (anti_rt_injectable.count == 0) {
        free(out);
        return format_text("%s", "none");
    }
    for (i = 0; i < anti_rt_injectable.count; i++) {
        const char *before = i == 0 ? ""
                             : i + 1 == anti_rt_injectable.count ? " and "
                                                                 : ", ";
        char *longer = format_text(
            "%s%s%s", out, before,
            (const char *)anti_rt_injectable.interfaces[i].name);
        free(out);
        out = longer;
    }
    return out;
}

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
   start reads before main. rt.configure may add to them later from one
   thread alone, since file_read lets one call read a file. No reader
   looks at them after the start, so they need no lock. */
static struct injection injections[INJECTION_MAX];
static size_t injection_count;

static void injection_named(const char *name, size_t length,
                            const char *library, const char *where, int layer)
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
        slot->layer = LAYER_BUILD;
    }
    if (slot->layer > layer) {
        return;
    }
    free(slot->library);
    free(slot->where);
    slot->in = in;
    slot->library = copy(library, strlen(library));
    slot->layer = layer;
    slot->where = copy(where, strlen(where));
}

static _Noreturn void unknown_option(const char *name, size_t length)
{
    const char *names[OPTION_COUNT];
    size_t i;

    for (i = 0; i < OPTION_COUNT; i++) {
        names[i] = options[i].name;
    }
    startup_error("--anti.%.*s is no option of the runtime, which takes%s",
                  (int)length, name,
                  list_text(names, OPTION_COUNT, " --anti."));
}

void anti_rt_conf_option(const char *name, int64_t length, const char *value)
{
    size_t n = (size_t)length;
    char *where;
    struct key *k;

    /* No option is as long as this, and the bound keeps the length
       within the int that %.*s takes. */
    if (n >= 64) {
        unknown_option(name, n);
    }
    where = format_text("--anti.%.*s", (int)n, name);
    if (n == 4 && memcmp(name, "conf", n) == 0) {
        if (value == NULL) {
            startup_error("%s takes the path of a file", where);
        }
        conf_path = value;
    } else if (n == 7 && memcmp(name, "inspect", n) == 0) {
        inspect_asked = 1;
    } else if (n == 4 && memcmp(name, "help", n) == 0) {
        help_asked = 1;
    } else if (n == 6 && memcmp(name, "inject", n) == 0) {
        const char *at = value == NULL ? NULL : strchr(value, '=');
        if (at == NULL) {
            startup_error("%s takes an interface and the path of a library, "
                          "as --anti.inject=Interface=path",
                          where);
        }
        injection_named(value, (size_t)(at - value), at + 1, where,
                        LAYER_COMMAND);
    } else {
        k = key_of(name, n);
        if (k == NULL) {
            unknown_option(name, n);
        }
        if (value == NULL && strcmp(k->name, "backtrace") != 0) {
            startup_error("%s takes a value", where);
        }
        if (value == NULL) {
            value = "true";
        }
        set_key(k, value, strlen(value), LAYER_COMMAND, NULL, where);
    }
    free(where);
}

/* The path of an include, which is relative to the file that names
   it. */
static char *resolve(const char *base, const char *path)
{
    const char *last = anti_rt_path_last_separator(base);
    size_t directory;
    char *out;

    if (anti_rt_path_is_absolute(path) || last == NULL) {
        return copy(path, strlen(path));
    }
    directory = (size_t)(last - base) + 1;
    out = malloc(directory + strlen(path) + 1);
    if (out == NULL) {
        startup_error("out of memory at program start");
    }
    memcpy(out, base, directory);
    memcpy(out + directory, path, strlen(path) + 1);
    return out;
}

/* One file of the file layer, with the files that include it, so that a
   cycle names the path it closes on. */
struct including {
    const char *path;
    const struct including *from;
};

static void read_file(const char *path, const struct including *from);

/* Whether the key is the word, or starts with it and a dot. */
static int key_under(struct anti_text key, const char *word)
{
    size_t length = strlen(word);

    return (size_t)key.len >= length &&
           memcmp(key.ptr, word, length) == 0 &&
           ((size_t)key.len == length || key.ptr[length] == '.');
}

/* The value of the key at index of the document, in memory of its own
   that the caller frees. DESIGN: every value of the file layer ends up
   in a C string: a path, or a key that anti_rt_conf_get hands on. A NUL
   byte in it would cut it short without a word, so the file is refused
   with the position of the value. */
static char *value_text(const struct anti_toml *doc, int64_t index,
                        const char *path)
{
    struct anti_text value = anti_rt_toml_value(doc, index);

    if (value.len > 0 && memchr(value.ptr, 0, (size_t)value.len) != NULL) {
        startup_error("%s:%lld: the value holds a NUL byte", path,
                      (long long)anti_rt_toml_line(doc, index));
    }
    return copy((const char *)value.ptr, (size_t)value.len);
}

/* The includes of the document, in order, each relative to the file
   that names it. */
static void read_includes(const struct anti_toml *doc, const char *path,
                          const struct including *from)
{
    int64_t count = anti_rt_toml_count(doc);
    int64_t i;

    for (i = 0; i < count; i++) {
        char *value;
        if (!key_under(anti_rt_toml_key(doc, i), "include")) {
            continue;
        }
        value = value_text(doc, i, path);
        read_file(resolve(path, value), from);
        free(value);
    }
}

/* Whether every one of the length bytes at text is a digit, and there is
   one. */
static int digits_only(const unsigned char *text, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
    }
    return length > 0;
}

/* The element number of the key, which is the name of a key of
   [runtime] and `.<digits>`, and name_length the bytes of that name.
   Gives 0 for a key without a number. */
static int element_of(struct anti_text name, size_t *name_length)
{
    const unsigned char *dot = memchr(name.ptr, '.', (size_t)name.len);
    size_t before;

    *name_length = (size_t)name.len;
    if (dot == NULL) {
        return 0;
    }
    before = (size_t)(dot - name.ptr);
    if (!digits_only(dot + 1, (size_t)name.len - before - 1)) {
        return 0;
    }
    *name_length = before;
    return 1;
}

/* The elements of an array value, joined with `:`, the separator that
   --anti.plugins=dir:dir writes. The reader numbers the elements, so
   they stand in order under the keys runtime.<name>.0 and further. The
   length of the result goes to length. */
static char *join_elements(const struct anti_toml *doc, int64_t from,
                           const char *name, const char *path,
                           size_t *length)
{
    size_t name_length = strlen(name);
    int64_t count = anti_rt_toml_count(doc);
    char *out = copy("", 0);
    size_t held = 0;
    int64_t i;

    for (i = from; i < count; i++) {
        struct anti_text key = anti_rt_toml_key(doc, i);
        char *value;
        size_t value_length;
        size_t element;
        char *grown;
        if (!key_under(key, "runtime") || key.len <= 8) {
            break;
        }
        key.ptr += 8;
        key.len -= 8;
        if (!element_of(key, &element) || element != name_length ||
            memcmp(key.ptr, name, name_length) != 0) {
            break;
        }
        value = value_text(doc, i, path);
        value_length = strlen(value);
        grown = malloc(held + value_length + 2);
        if (grown == NULL) {
            startup_error("out of memory at program start");
        }
        memcpy(grown, out, held);
        if (held > 0) {
            grown[held++] = ':';
        }
        memcpy(grown + held, value, value_length);
        held += value_length;
        grown[held] = '\0';
        free(value);
        free(out);
        out = grown;
    }
    *length = held;
    return out;
}

/* The key at index of [runtime], or [injections], of the document. It
   ends the program on any other. */
static void read_key(const struct anti_toml *doc, int64_t i,
                     const char *path, const char *position)
{
    struct anti_text name = anti_rt_toml_key(doc, i);
    size_t length;
    char *value;
    char *where;
    struct key *k;

    if (name.len > INT_MAX) {
        startup_error("%s: the key is too long", position);
    }
    if (key_under(name, "injections") && name.len > 11) {
        value = value_text(doc, i, path);
        injection_named((const char *)name.ptr + 11, (size_t)name.len - 11,
                        value, position, LAYER_FILE);
        free(value);
        return;
    }
    if (!key_under(name, "runtime") || name.len <= 8) {
        startup_error("%s: %.*s is no key of the runtime configuration, "
                      "which takes include, the table injections and "
                      "the keys%s of the table runtime",
                      position, (int)name.len, (const char *)name.ptr,
                      key_names());
    }
    /* An array value stands as <key>.0, <key>.1 and further. The first
       element takes the whole array, and the rest of them are then
       done. */
    name.ptr += 8;
    name.len -= 8;
    element_of(name, &length);
    k = key_of((const char *)name.ptr, length);
    if (k == NULL) {
        startup_error("%s: %.*s is no key of the table runtime, which "
                      "takes%s",
                      position, (int)length, (const char *)name.ptr,
                      key_names());
    }
    where = format_text("%s: %s", position, k->name);
    if (length == (size_t)name.len) {
        value = value_text(doc, i, path);
        set_key(k, value, strlen(value), LAYER_FILE, path, where);
        free(value);
    } else if (same_word((const char *)name.ptr + length + 1,
                         (size_t)name.len - length - 1, "0")) {
        size_t joined;
        value = join_elements(doc, i, k->name, path, &joined);
        set_key(k, value, joined, LAYER_FILE, path, where);
        free(value);
    }
    free(where);
}

/* The keys of the document, after its includes. */
static void read_keys(const struct anti_toml *doc, const char *path)
{
    int64_t count = anti_rt_toml_count(doc);
    int64_t i;

    for (i = 0; i < count; i++) {
        char *position;
        if (key_under(anti_rt_toml_key(doc, i), "include")) {
            continue;
        }
        position = format_text("%s:%lld", path,
                               (long long)anti_rt_toml_line(doc, i));
        read_key(doc, i, path, position);
        free(position);
    }
}

/* Read one file of the file layer and apply it: its includes first, in
   order, depth first, then its own keys. The including file therefore
   wins per key. The path is the one the caller resolved, and the reader
   keeps it. */
static void read_file(const char *path, const struct including *from)
{
    struct including here;
    const struct including *up;
    struct anti_toml *doc;
    unsigned char *bytes;
    int64_t length = 0;
    int depth = 0;

    for (up = from; up != NULL; up = up->from) {
        if (strcmp(up->path, path) == 0) {
            startup_error("the configuration files include one another at %s",
                          path);
        }
        depth++;
    }
    if (depth > 32) {
        startup_error("the configuration files include one another at %s",
                      path);
    }
    errno = 0;
    bytes = anti_rt_fs_read(path, &length);
    if (bytes == NULL && errno == ENOMEM) {
        startup_error("out of memory at program start");
    }
    if (bytes == NULL) {
        startup_error("cannot read the configuration file %s", path);
    }
    doc = anti_rt_toml_read(bytes, length);
    if (doc == NULL) {
        startup_error("%s is not the TOML subset", path);
    }
    here.path = path;
    here.from = from;
    read_includes(doc, path, &here);
    read_keys(doc, path);
    anti_rt_toml_free(doc);
    free(bytes);
    anti_rt_atomic_store(&file_read, (int64_t)sizeof file_read, 1);
}

/* The path of ANTI_CONF in memory of its own, or NULL. It is the one
   variable the runtime reads. */
static char *environment_path(void)
{
    char *text;

    if (anti_rt_getenv("ANTI_CONF", &text) != 0) {
        startup_error("out of memory at program start");
    }
    return text;
}

/* DESIGN: an interface whose provider is a library gets its object
   before `main`. The build's `plugin:` and `discover` are the lowest
   layer. The configuration file stands above them and the command line
   above both. The runtime stores the object in the holder of the
   interface and puts the thunk in its slot. Every site of the program
   then calls through one function, as it did before. */
static void fill_injections(void)
{
    struct anti_text dirs = anti_rt_conf_get((const unsigned char *)"plugins",
                                             7);
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
                                           (int64_t)length,
                                           (const char *)dirs.ptr);
        if (provider == NULL) {
            struct anti_text why = anti_rt_plugin_message();
            startup_error("%s: %s comes from a library: %.*s", where,
                          (const char *)in->name, (int)why.len, why.ptr);
        }
        *in->holder = provider;
        *in->slot = in->thunk;
    }
}

static void inspect(void)
{
    struct anti_text version = anti_rt_runtime_version();
    size_t i;

    printf("anti runtime %.*s\n", (int)version.len, version.ptr);
    for (i = 0; i < KEY_COUNT; i++) {
        const char *layer = "build";
        if (keys[i].layer == LAYER_COMMAND) {
            layer = "command line";
        } else if (keys[i].layer == LAYER_FILE) {
            layer = keys[i].source;
        }
        printf("%s = %s (%s)\n", keys[i].name,
               keys[i].value == NULL ? "" : keys[i].value, layer);
    }
    if (anti_rt_injectable.count == 0) {
        printf("injectable interfaces: none\n");
    } else {
        int64_t k;
        printf("injectable interfaces:\n");
        for (k = 0; k < anti_rt_injectable.count; k++) {
            const struct anti_injectable *in =
                &anti_rt_injectable.interfaces[k];
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
    }
    if (injection_count == 0) {
        printf("loaded plugins: none\n");
    } else {
        size_t k;
        printf("loaded plugins:\n");
        for (k = 0; k < injection_count; k++) {
            printf("  %s for %s\n", injections[k].library,
                   (const char *)injections[k].in->name);
        }
    }
}

static void help(void)
{
    size_t i;

    printf("anti: the options of the runtime, which it takes before the "
           "program sees its arguments\n");
    for (i = 0; i < OPTION_COUNT; i++) {
        size_t used = sizeof "--anti." - 1 + strlen(options[i].name) +
                      strlen(options[i].value);
        int pad = used < 34 ? 34 - (int)used : 0;
        printf("  --anti.%s%s%*s %s\n", options[i].name, options[i].value,
               pad, "", options[i].what);
    }
}

void anti_rt_conf_start(void)
{
    if (help_asked) {
        help();
        exit(0);
    }
    if (conf_path != NULL) {
        read_file(copy(conf_path, strlen(conf_path)), NULL);
    } else {
        char *path = environment_path();

        if (path != NULL) {
            read_file(path, NULL);
        }
    }
    fill_injections();
    if (inspect_asked) {
        inspect();
        exit(0);
    }
}

void anti_rt_conf_configure(const unsigned char *path, int64_t length)
{
    if (!anti_rt_atomic_compare_swap(&file_read, (int64_t)sizeof file_read,
                                     0, 1)) {
        return;
    }
    /* The path goes on as a C string, which would end at a NUL inside
       it and name another file. */
    if (length > 0 && memchr(path, 0, (size_t)length) != NULL) {
        startup_error("the path of the configuration file %s holds a NUL "
                      "byte",
                      (const char *)path);
    }
    read_file(copy((const char *)path, (size_t)length), NULL);
}

/* DESIGN: the count is read digit by digit against its bound, so every
   target gives the same pool for the same text. atoi leaves a number out
   of range undefined, and the C libraries of the targets differ on it. */
int32_t anti_rt_conf_threads(struct anti_text text)
{
    int64_t count = 0;
    int64_t i;

    if (text.len <= 0) {
        return 0;
    }
    for (i = 0; i < text.len; i++) {
        int digit = text.ptr[i] - '0';
        if (digit < 0 || digit > 9 ||
            count > (ANTI_RT_THREADS_MAX - digit) / 10) {
            return 0;
        }
        count = count * 10 + digit;
    }
    return (int32_t)count;
}

struct anti_text anti_rt_conf_get(const unsigned char *key, int64_t length)
{
    struct key *k = key_of((const char *)key, (size_t)length);
    struct anti_text text;

    text.ptr = (const unsigned char *)"";
    text.len = 0;
    hold();
    if (k != NULL && k->value != NULL) {
        text.ptr = (const unsigned char *)k->value;
        text.len = (int64_t)k->length;
    }
    release();
    return text;
}
