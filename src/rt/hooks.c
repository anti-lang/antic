#include <stddef.h>

#include "atomic.h"
#include "hooks.h"
#include "object.h"
#include "plugin.h"
#include "std.h"

/* DESIGN: the one handler lives in an atomic word of the runtime and not
   in a static field of anti.lang.Trace. The hook sites of the runtime
   read it as well, and no C name reaches an Anti global. Trace.install
   stores it here. */
static void *installed;

void anti_rt_trace_install(struct anti_object *h)
{
    anti_rt_atomic_store(&installed, (int64_t)sizeof installed,
                         (int64_t)(intptr_t)h);
}

/* The installed handler, or NULL. */
static struct anti_object *handler(void)
{
    return (struct anti_object *)(intptr_t)anti_rt_atomic_load(
        &installed, (int64_t)sizeof installed);
}

/* The empty bodies of the root, in the order of enum anti_hook. An entry
   that holds one of them is no hook of the class. The compare is what a
   program without a hook of its own pays. */
static anti_rt_body root_body(int64_t hook)
{
    typedef anti_rt_body body;

    switch (hook) {
    case ANTI_HOOK_CREATED: return (body)anti_lang_Object_created;
    case ANTI_HOOK_DESTROYED: return (body)anti_lang_Object_destroyed;
    case ANTI_HOOK_COPIED: return (body)anti_lang_Object_copied;
    case ANTI_HOOK_DISPATCHED: return (body)anti_lang_Object_dispatched;
    case ANTI_HOOK_JOINED: return (body)anti_lang_Object_joined;
    case ANTI_HOOK_ENTER: return (body)anti_lang_Object_enter;
    case ANTI_HOOK_LEAVE: return (body)anti_lang_Object_leave;
    case ANTI_HOOK_FAILED: return (body)anti_lang_Object_failed;
    default: return (body)anti_lang_Object_changed;
    }
}

/* The hook the object itself declares, or NULL where it kept the
   root's. */
static anti_rt_body own_hook(const struct anti_object *self, int64_t hook)
{
    anti_rt_body body =
        anti_rt_entry_body(self, ANTI_ENTRY_OF_HOOK((int)hook));

    return body == root_body(hook) ? NULL : body;
}

/* The handler's function for the hook, or NULL when none is installed.
   anti.lang.TraceHandler declares the nine after the root's, so the
   entry stands at a place every handler shares. */
static anti_rt_body handler_hook(struct anti_object *h, int64_t hook)
{
    return h == NULL
               ? NULL
               : anti_rt_entry_body(h, ANTI_ENTRY_OF_HANDLER((int)hook));
}

void anti_rt_hook(struct anti_object *self, int64_t hook)
{
    typedef void (*handler_fn)(struct anti_object *, struct anti_object *);
    typedef void (*object_fn)(struct anti_object *);
    struct anti_object *h = handler();
    handler_fn taken = (handler_fn)handler_hook(h, hook);
    object_fn own = (object_fn)own_hook(self, hook);

    /* DESIGN: the objects of a loaded library are counted here, so
       `unload` refuses to take the code of a live object away. A
       program with no library open pays one load and one compare. */
    if (hook == ANTI_HOOK_CREATED) {
        anti_rt_plugin_created(self != NULL ? self->table : NULL);
    } else if (hook == ANTI_HOOK_DESTROYED) {
        anti_rt_plugin_destroyed(self != NULL ? self->table : NULL);
    }
    if (taken != NULL) {
        taken(h, self);
    }
    if (own != NULL) {
        own(self);
    }
}

void anti_rt_hook_copied(struct anti_object *self, struct anti_object *from)
{
    typedef void (*handler_fn)(struct anti_object *, struct anti_object *,
                               struct anti_object *);
    typedef void (*object_fn)(struct anti_object *, struct anti_object *);
    struct anti_object *h = handler();
    handler_fn taken = (handler_fn)handler_hook(h, ANTI_HOOK_COPIED);
    object_fn own = (object_fn)own_hook(self, ANTI_HOOK_COPIED);

    if (taken != NULL) {
        taken(h, self, from);
    }
    if (own != NULL) {
        own(self, from);
    }
}

void anti_rt_hook_call(struct anti_object *self, int64_t hook,
                       const unsigned char *name, int64_t length)
{
    typedef void (*handler_fn)(struct anti_object *, struct anti_object *,
                               struct anti_text);
    typedef void (*object_fn)(struct anti_object *, struct anti_text);
    struct anti_object *h = handler();
    handler_fn taken = (handler_fn)handler_hook(h, hook);
    object_fn own = (object_fn)own_hook(self, hook);
    struct anti_text text;

    text.ptr = name;
    text.len = length;
    /* The handler wraps the object's hook: it comes first on `enter` and
       last on `leave`, so the two nest around the call. */
    if (hook == ANTI_HOOK_LEAVE) {
        if (own != NULL) {
            own(self, text);
        }
        if (taken != NULL) {
            taken(h, self, text);
        }
        return;
    }
    if (taken != NULL) {
        taken(h, self, text);
    }
    if (own != NULL) {
        own(self, text);
    }
}

void anti_rt_hook_failed(struct anti_object *self, const unsigned char *name,
                         int64_t length, struct anti_object *e)
{
    typedef void (*handler_fn)(struct anti_object *, struct anti_object *,
                               struct anti_text, struct anti_object *);
    typedef void (*object_fn)(struct anti_object *, struct anti_text,
                              struct anti_object *);
    struct anti_object *h = handler();
    handler_fn taken = (handler_fn)handler_hook(h, ANTI_HOOK_FAILED);
    object_fn own = (object_fn)own_hook(self, ANTI_HOOK_FAILED);
    struct anti_text text;

    text.ptr = name;
    text.len = length;
    if (taken != NULL) {
        taken(h, self, text, e);
    }
    if (own != NULL) {
        own(self, text, e);
    }
}

void anti_rt_hook_changed(struct anti_object *self,
                          const struct anti_field *field)
{
    typedef void (*handler_fn)(struct anti_object *, struct anti_object *,
                               const struct anti_field *);
    typedef void (*object_fn)(struct anti_object *,
                              const struct anti_field *);
    struct anti_object *h = handler();
    handler_fn taken = (handler_fn)handler_hook(h, ANTI_HOOK_CHANGED);
    object_fn own = (object_fn)own_hook(self, ANTI_HOOK_CHANGED);

    if (taken != NULL) {
        taken(h, self, field);
    }
    if (own != NULL) {
        own(self, field);
    }
}
