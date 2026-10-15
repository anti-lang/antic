/* The nine hooks of anti.lang.Object and the one anti.lang.TraceHandler
   that anti.lang.Trace.install stores. */
#ifndef ANTI_HOOKS_H
#define ANTI_HOOKS_H

#include <stdint.h>

#include "object.h"

/* DESIGN: a hook site does two things. It calls the installed handler
   when there is one, and then it dispatches the object's own hook. On
   `leave` the order is reversed, so the handler and the object's hook
   nest around the call. Each site is one call of the runtime, so the
   order stands in one place and the compiler writes no branch. */

/* Store h as the handler of every hook, or NULL for none. */
void anti_rt_trace_install(struct anti_object *h);

/* The hook, which is one of enum anti_hook that takes the object alone:
   created, destroyed, dispatched or joined. */
void anti_rt_hook(struct anti_object *self, int64_t hook);

/* The copied hook, with the object the copy was made from. */
void anti_rt_hook_copied(struct anti_object *self, struct anti_object *from);

/* The enter or the leave hook of the function named by bytes and
   length. */
void anti_rt_hook_call(struct anti_object *self, int64_t hook,
                       const unsigned char *name, int64_t length);

/* The failed hook of the function named by bytes and length, with the
   error it gave. */
void anti_rt_hook_failed(struct anti_object *self, const unsigned char *name,
                         int64_t length, struct anti_object *e);

/* The changed hook of one field of the object, which is a record of the
   field list its descriptor carries. */
void anti_rt_hook_changed(struct anti_object *self,
                          const struct anti_field *field);

#endif
