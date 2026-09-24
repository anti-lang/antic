/* The patterns of a program: the literals it compiles before main, and
   the messages of the errors of a pattern compiled at run time. */
#include "atomic.h"
#include "cpu_level.h"
#include "regex.h"
#include "std.h"

#include <stdlib.h>
#include <string.h>

/* DESIGN: every pattern literal is compiled once, before main, by the
   constructor antic writes for each module that holds one. antic checked
   the pattern already. A literal that does not compile here means the
   memory ran out, and that ends the program as a refusal at start does.
   The constructor may run before main checks the processor. PCRE2 is
   built for the default level of its target, so the check runs here
   first. */
void *anti_rt_regex_literal(const unsigned char *bytes, int64_t length)
{
    unsigned char message[ANTI_RT_REGEX_MESSAGE_ROOM];
    int32_t code = 0;
    int64_t offset = 0;
    void *compiled;

    anti_rt_cpu_check();
    compiled = anti_rt_regex_compile(bytes, length, &code, &offset);
    if (compiled == NULL) {
        anti_rt_regex_message_into(code, message, sizeof message);
        anti_rt_fail_exit(70, "anti: the pattern `%.*s` does not compile at "
                          "start: %s", (int)length, (const char *)bytes,
                          (const char *)message);
    }
    return compiled;
}

/* The error numbers of PCRE2 lie between -256 and 511: the failures of a
   match below zero and those of a compile from 100 up. */
#define MESSAGE_LOW (-256)
#define MESSAGE_COUNT 768

/* DESIGN: PCRE2 writes a message into memory of the caller, and a `str`
   owns nothing. The text of each error number is therefore made once and
   kept for the life of the program. Two threads that ask at once for the
   same number race to one slot, and the one that loses frees its copy. */
static unsigned char *messages[MESSAGE_COUNT];

struct anti_text anti_rt_regex_message(int32_t code)
{
    struct anti_text text;
    unsigned char *have;
    unsigned char *made;
    void *slot;

    text.ptr = (const unsigned char *)"";
    text.len = 0;
    if (code < MESSAGE_LOW || code >= MESSAGE_LOW + MESSAGE_COUNT) {
        return text;
    }
    slot = &messages[code - MESSAGE_LOW];
    have = (unsigned char *)(intptr_t)anti_rt_atomic_load(
        slot, (int64_t)sizeof(void *));
    if (have == NULL) {
        made = malloc(ANTI_RT_REGEX_MESSAGE_ROOM);
        if (made == NULL) {
            return text;
        }
        anti_rt_regex_message_into(code, made, ANTI_RT_REGEX_MESSAGE_ROOM);
        if (anti_rt_atomic_compare_swap(slot, (int64_t)sizeof(void *), 0,
                                        (int64_t)(intptr_t)made)) {
            have = made;
        } else {
            free(made);
            have = (unsigned char *)(intptr_t)anti_rt_atomic_load(
                slot, (int64_t)sizeof(void *));
        }
    }
    text.ptr = have;
    text.len = (int64_t)strlen((const char *)have);
    return text;
}
