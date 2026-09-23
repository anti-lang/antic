/* The runtime configuration.

   DESIGN: a program reads a configuration file only when --anti.conf or
   ANTI_CONF names one, or when main calls rt.configure. The runtime
   never searches for a file. Without one the program runs as it was
   built. The layers are the command line, then the file with its
   includes, then the build. A key takes the value of the highest layer
   that named it. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "plugin.h"

#include "rt.h"
#include "toml.h"

#if defined(_WIN32)
#include <windows.h>
#endif

/* Where a value came from. A higher layer keeps its value. */
enum layer { LAYER_BUILD, LAYER_FILE, LAYER_COMMAND };

struct key {
    const char *name;
    char *value;        /* what the layer set, NULL while no layer did */
    const char *source; /* the file of a value of the file layer */
    enum layer layer;
};

/* The keys of [runtime], in the order --anti.inspect prints them. */
static struct key keys[] = {
    {"backtrace", NULL, NULL, LAYER_BUILD},
    {"logger", NULL, NULL, LAYER_BUILD},
    {"plugins", NULL, NULL, LAYER_BUILD},
    {"threads", NULL, NULL, LAYER_BUILD},
    {"trace", NULL, NULL, LAYER_BUILD}
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
   once the pass over the arguments is done. */
static const char *conf_path;
static int inspect_asked;
static int help_asked;
static int file_read;

/* A startup error: the message on standard error and status 70, the
   status of every refusal at start. */
static void startup_error(const char *format, ...)
{
    va_list rest;

    fputs("anti: ", stderr);
    va_start(rest, format);
    vfprintf(stderr, format, rest);
    va_end(rest);
    fputs("\n", stderr);
    exit(70);
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
static void print_keys(FILE *stream)
{
    size_t i;

    for (i = 0; i < KEY_COUNT; i++) {
        const char *before = i == 0 ? "" : (i + 1 == KEY_COUNT ? " and" : ",");
        fprintf(stream, "%s %s", before, keys[i].name);
    }
}

/* Take the value into the key, unless a higher layer named it. `where`
   names the option or the line of the file, for a message about a value
   the key does not take. */
static void set_key(struct key *k, const char *value, enum layer layer,
                    const char *source, const char *where)
{
    if (strcmp(k->name, "backtrace") == 0) {
        if (strcmp(value, "true") != 0 && strcmp(value, "false") != 0) {
            startup_error("%s takes true or false, found %s", where, value);
        }
    } else if (strcmp(k->name, "threads") == 0) {
        const char *digit = value;
        while (*digit >= '0' && *digit <= '9') {
            digit++;
        }
        if (*digit != '\0' || atoi(value) <= 0) {
            startup_error("%s takes a count above zero, found %s", where,
                          value);
        }
    }
    if (k->layer > layer) {
        return;
    }
    free(k->value);
    k->value = copy(value, strlen(value));
    k->source = source;
    k->layer = layer;
    if (strcmp(k->name, "backtrace") == 0) {
        anti_rt_option_backtrace = strcmp(value, "true") == 0;
    }
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
static void list_injectable(char *out, size_t size)
{
    size_t at = 0;
    int64_t i;

    if (anti_rt_injectable.count == 0) {
        snprintf(out, size, "none");
        return;
    }
    for (i = 0; i < anti_rt_injectable.count && at + 1 < size; i++) {
        const char *before = i == 0 ? ""
                             : i + 1 == anti_rt_injectable.count ? " and "
                                                                 : ", ";
        at += (size_t)snprintf(out + at, size - at, "%s%s", before,
                               (const char *)
                                   anti_rt_injectable.interfaces[i].name);
    }
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
    char where[320];
};

static struct injection injections[INJECTION_MAX];
static size_t injection_count;

static void injection_named(const char *name, size_t length,
                            const char *library, const char *where, int layer)
{
    const struct anti_injectable *in = injectable_of(name, length);
    struct injection *slot = NULL;
    char list[512];
    size_t i;

    if (in == NULL) {
        list_injectable(list, sizeof list);
        startup_error("%s: %.*s is no injectable interface of this program, "
                      "which has %s",
                      where, (int)length, name, list);
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
    slot->in = in;
    slot->library = copy(library, strlen(library));
    slot->layer = layer;
    snprintf(slot->where, sizeof slot->where, "%s", where);
}

static void unknown_option(const char *name, size_t length)
{
    size_t i;

    fprintf(stderr,
            "anti: --anti.%.*s is no option of the runtime, which takes",
            (int)length, name);
    for (i = 0; i < OPTION_COUNT; i++) {
        const char *before =
            i == 0 ? "" : (i + 1 == OPTION_COUNT ? " and" : ",");
        fprintf(stderr, "%s --anti.%s", before, options[i].name);
    }
    fputs("\n", stderr);
    exit(70);
}

void anti_rt_conf_option(const char *name, int64_t length, const char *value)
{
    size_t n = (size_t)length;
    char where[64];
    struct key *k;

    if (n >= sizeof where - sizeof "--anti.") {
        unknown_option(name, n);
    }
    snprintf(where, sizeof where, "--anti.%.*s", (int)n, name);
    if (n == 4 && memcmp(name, "conf", n) == 0) {
        if (value == NULL) {
            startup_error("%s takes the path of a file", where);
        }
        conf_path = value;
        return;
    }
    if (n == 7 && memcmp(name, "inspect", n) == 0) {
        inspect_asked = 1;
        return;
    }
    if (n == 4 && memcmp(name, "help", n) == 0) {
        help_asked = 1;
        return;
    }
    if (n == 6 && memcmp(name, "inject", n) == 0) {
        const char *at = value == NULL ? NULL : strchr(value, '=');
        if (at == NULL) {
            startup_error("%s takes an interface and the path of a library, "
                          "as --anti.inject=Interface=path",
                          where);
        }
        injection_named(value, (size_t)(at - value), at + 1, where,
                        LAYER_COMMAND);
        return;
    }
    k = key_of(name, n);
    if (k == NULL) {
        unknown_option(name, n);
    }
    if (value == NULL) {
        if (strcmp(k->name, "backtrace") != 0) {
            startup_error("%s takes a value", where);
        }
        value = "true";
    }
    set_key(k, value, LAYER_COMMAND, NULL, where);
}

/* The bytes of the file, NUL after them, or NULL when it cannot be
   read. It opens the file through anti.fs, which takes the path of
   Windows as UTF-16 and every other as its bytes. */
static unsigned char *file_bytes(const char *path, int64_t *length)
{
    FILE *f = anti_rt_fs_open((const unsigned char *)path,
                              (int64_t)strlen(path), 0);
    unsigned char *bytes;
    int64_t size;

    if (f == NULL) {
        return NULL;
    }
    size = anti_rt_fs_size(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    bytes = malloc((size_t)size + 1);
    if (bytes == NULL) {
        fclose(f);
        startup_error("out of memory at program start");
    }
    if (size > 0 && fread(bytes, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(bytes);
        return NULL;
    }
    fclose(f);
    bytes[size] = '\0';
    *length = size;
    return bytes;
}

static int is_absolute(const char *path)
{
#if defined(_WIN32)
    if (path[0] == '\\' ||
        ((path[0] | 32) >= 'a' && (path[0] | 32) <= 'z' && path[1] == ':')) {
        return 1;
    }
#endif
    return path[0] == '/';
}

/* The path of an include, which is relative to the file that names
   it. */
static char *resolve(const char *base, const char *path)
{
    const char *last = strrchr(base, '/');
    size_t directory;
    char *out;

#if defined(_WIN32)
    const char *back = strrchr(base, '\\');
    if (back != NULL && (last == NULL || back > last)) {
        last = back;
    }
#endif
    if (is_absolute(path) || last == NULL) {
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

/* The includes of the document, in order, each relative to the file
   that names it. */
static void read_includes(const struct anti_toml *doc, const char *path,
                          const struct including *from)
{
    int64_t count = anti_rt_toml_count(doc);
    int64_t i;

    for (i = 0; i < count; i++) {
        struct anti_text key = anti_rt_toml_key(doc, i);
        struct anti_text value = anti_rt_toml_value(doc, i);
        const char *name = (const char *)key.ptr;
        if (strcmp(name, "include") != 0 &&
            strncmp(name, "include.", 8) != 0) {
            continue;
        }
        read_file(resolve(path, (const char *)value.ptr), from);
    }
}

/* Whether every byte of text is a digit, and there is one. */
static int digits_only(const char *text)
{
    const char *at = text;

    while (*at >= '0' && *at <= '9') {
        at++;
    }
    return at != text && *at == '\0';
}

/* The elements of an array value, joined with `:`, the separator that
   --anti.plugins=dir:dir writes. The reader numbers the elements, so
   they stand in order under the keys <prefix>.0 and further. */
static char *join_elements(const struct anti_toml *doc, int64_t from,
                           const char *prefix)
{
    size_t length = strlen(prefix);
    int64_t count = anti_rt_toml_count(doc);
    char *out = copy("", 0);
    int64_t i;

    for (i = from; i < count; i++) {
        struct anti_text key = anti_rt_toml_key(doc, i);
        struct anti_text value = anti_rt_toml_value(doc, i);
        const char *name = (const char *)key.ptr;
        size_t held = strlen(out);
        char *grown;
        if (strncmp(name, prefix, length) != 0 || name[length] != '.' ||
            !digits_only(name + length + 1)) {
            break;
        }
        grown = malloc(held + (size_t)value.len + 2);
        if (grown == NULL) {
            startup_error("out of memory at program start");
        }
        memcpy(grown, out, held);
        if (held > 0) {
            grown[held++] = ':';
        }
        memcpy(grown + held, value.ptr, (size_t)value.len);
        grown[held + (size_t)value.len] = '\0';
        free(out);
        out = grown;
    }
    return out;
}

/* The keys of the document, after its includes. */
static void read_keys(const struct anti_toml *doc, const char *path)
{
    int64_t count = anti_rt_toml_count(doc);
    int64_t i;

    for (i = 0; i < count; i++) {
        struct anti_text key = anti_rt_toml_key(doc, i);
        struct anti_text value = anti_rt_toml_value(doc, i);
        const char *name = (const char *)key.ptr;
        const char *element;
        char position[280];
        char where[320];
        char prefix[64];
        char *elements;
        size_t length;
        struct key *k;
        snprintf(position, sizeof position, "%s:%lld", path,
                 (long long)anti_rt_toml_line(doc, i));
        if (strcmp(name, "include") == 0 ||
            strncmp(name, "include.", 8) == 0) {
            continue;
        }
        if (strncmp(name, "injections.", 11) == 0) {
            injection_named(name + 11, strlen(name + 11),
                            (const char *)value.ptr, position, LAYER_FILE);
            continue;
        }
        if (strncmp(name, "runtime.", 8) != 0) {
            fprintf(stderr, "anti: %s: %s is no key of the runtime "
                            "configuration, which takes include, the table "
                            "injections and the keys",
                    position, name);
            print_keys(stderr);
            fputs(" of the table runtime\n", stderr);
            exit(70);
        }
        /* An array value stands as <key>.0, <key>.1 and further. The
           first element takes the whole array, and the rest of them
           are then done. */
        name += 8;
        element = strchr(name, '.');
        length = strlen(name);
        if (element != NULL && !digits_only(element + 1)) {
            element = NULL;
        }
        if (element != NULL) {
            length = (size_t)(element - name);
        }
        k = key_of(name, length);
        if (k == NULL) {
            fprintf(stderr,
                    "anti: %s: %.*s is no key of the table runtime, which "
                    "takes",
                    position, (int)length, name);
            print_keys(stderr);
            fputs("\n", stderr);
            exit(70);
        }
        snprintf(where, sizeof where, "%s: %s", position, k->name);
        if (element == NULL) {
            set_key(k, (const char *)value.ptr, LAYER_FILE, path, where);
            continue;
        }
        if (strcmp(element + 1, "0") != 0) {
            continue;
        }
        snprintf(prefix, sizeof prefix, "runtime.%s", k->name);
        elements = join_elements(doc, i, prefix);
        set_key(k, elements, LAYER_FILE, path, where);
        free(elements);
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
    bytes = file_bytes(path, &length);
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
    file_read = 1;
}

/* The path of ANTI_CONF, or NULL. It is the one variable the runtime
   reads. */
static const char *environment_path(void)
{
#if defined(_WIN32)
    static char text[1024];
    DWORD length = GetEnvironmentVariableA("ANTI_CONF", text, sizeof text);

    return length > 0 && length < sizeof text ? text : NULL;
#else
    const char *text = getenv("ANTI_CONF");

    return text != NULL && text[0] != '\0' ? text : NULL;
#endif
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
        char form[64];
        snprintf(form, sizeof form, "--anti.%s%s", options[i].name,
                 options[i].value);
        printf("  %-34s %s\n", form, options[i].what);
    }
}

void anti_rt_conf_start(void)
{
    const char *path = conf_path;

    if (help_asked) {
        help();
        exit(0);
    }
    if (path == NULL) {
        path = environment_path();
    }
    if (path != NULL) {
        read_file(copy(path, strlen(path)), NULL);
    }
    fill_injections();
    if (inspect_asked) {
        inspect();
        exit(0);
    }
}

void anti_rt_conf_configure(const unsigned char *path, int64_t length)
{
    if (file_read) {
        return;
    }
    read_file(copy((const char *)path, (size_t)length), NULL);
}

struct anti_text anti_rt_conf_get(const unsigned char *key, int64_t length)
{
    struct key *k = key_of((const char *)key, (size_t)length);
    struct anti_text text;

    text.ptr = (const unsigned char *)"";
    text.len = 0;
    if (k != NULL && k->value != NULL) {
        text.ptr = (const unsigned char *)k->value;
        text.len = (int64_t)strlen(k->value);
    }
    return text;
}
