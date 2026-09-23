/* The platform layer of the runtime, src/rt/platform.h, on the host. It
   checks the steps of a long sleep and the environment in full length
   and in UTF-8. It also checks the path rules every system shares and a
   library opened from a path outside ASCII. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#elif !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/platform.h"
#include "check.h"

#if defined(_WIN32)
#include <windows.h>
#endif

int check_failures;

#define PLUGIN_UTF8 PLUGIN_DIR "/platform-\xc3\xbc/" PLUGIN_NAME

/* The nanoseconds a wait of total sleeps in its steps, which is at least
   total and less than a millisecond more. Each step waits at most a day
   and none is INFINITE. The count of steps goes to steps. */
static int64_t slept(int64_t total, int64_t *steps)
{
    int64_t left = total;
    int64_t done = 0;

    *steps = 0;
    while (left > 0) {
        uint32_t step = anti_rt_sleep_step(left);

        CHECK(step > 0);
        CHECK(step <= ANTI_RT_SLEEP_STEP_MAX);
        CHECK(step != 0xFFFFFFFFu);
        left -= (int64_t)step * 1000000;
        done += (int64_t)step * 1000000;
        (*steps)++;
    }
    return done;
}

static void sleep_steps(void)
{
    int64_t steps;
    /* 2^32 milliseconds, which a Sleep of 32 bits takes as 0. */
    int64_t wrapped = (int64_t)4294967296 * 1000000;
    /* 4294967295 milliseconds, which a Sleep takes as INFINITE. */
    int64_t infinite = (int64_t)4294967295 * 1000000;

    CHECK(anti_rt_sleep_step(1) == 1);
    CHECK(anti_rt_sleep_step(999999) == 1);
    CHECK(anti_rt_sleep_step(1000000) == 1);
    CHECK(anti_rt_sleep_step(1000001) == 2);
    CHECK(anti_rt_sleep_step(wrapped) == ANTI_RT_SLEEP_STEP_MAX);
    CHECK(anti_rt_sleep_step(INT64_MAX) == ANTI_RT_SLEEP_STEP_MAX);
    CHECK(slept(1, &steps) == 1000000 && steps == 1);
    CHECK(slept(wrapped, &steps) == wrapped);
    CHECK(steps == 50);
    CHECK(slept(infinite, &steps) == infinite);
    CHECK(slept(infinite + 1, &steps) == infinite + 1000000);
}

/* Set the variable name to value on the host, in UTF-8 bytes. */
static void put(const char *name, const char *value)
{
#if defined(_WIN32)
    wchar_t wide_name[64];
    wchar_t *wide = malloc((strlen(value) + 1) * sizeof *wide);

    MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, 64);
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wide, (int)strlen(value) + 1);
    SetEnvironmentVariableW(wide_name, wide);
    free(wide);
#else
    setenv(name, value, 1);
#endif
}

static void environment(void)
{
    enum { LENGTH = 5000 };
    char *long_value = malloc(LENGTH + 1);
    char *value = NULL;

    /* A value past the 1024 bytes of the ANSI reader of old. */
    memset(long_value, 'a', LENGTH);
    long_value[LENGTH] = '\0';
    put("ANTI_RT_PLATFORM_TEST", long_value);
    CHECK(anti_rt_getenv("ANTI_RT_PLATFORM_TEST", &value) == 0);
    CHECK(value != NULL && strcmp(value, long_value) == 0);
    free(value);
    free(long_value);

    /* A value outside ASCII, which the ANSI code page does not hold. */
    put("ANTI_RT_PLATFORM_TEST", "/tmp/\xc3\xbc\xe2\x82\xac/anti.toml");
    value = NULL;
    CHECK(anti_rt_getenv("ANTI_RT_PLATFORM_TEST", &value) == 0);
    CHECK(value != NULL &&
          strcmp(value, "/tmp/\xc3\xbc\xe2\x82\xac/anti.toml") == 0);
    free(value);

    value = NULL;
    CHECK(anti_rt_getenv("ANTI_RT_PLATFORM_UNSET", &value) == 0);
    CHECK(value == NULL);
}

static void paths(void)
{
    const char *path = "a/b/c.toml";

    CHECK(anti_rt_path_is_absolute("/etc/anti.toml"));
    CHECK(!anti_rt_path_is_absolute("anti.toml"));
    CHECK(!anti_rt_path_is_absolute("a/anti.toml"));
    CHECK(anti_rt_path_last_separator(path) == path + 3);
    CHECK(anti_rt_path_last_separator("anti.toml") == NULL);
}

/* The library of an empty plugin table, copied by the build into a
   directory whose name is outside ASCII. */
static void library_outside_ascii(void)
{
    char text[64];
    void *handle = anti_rt_library_open(PLUGIN_UTF8);

    if (handle == NULL) {
        fprintf(stderr, "%s: %s\n", PLUGIN_UTF8,
                anti_rt_library_error(text, sizeof text));
    }
    CHECK(handle != NULL);
    if (handle != NULL) {
        CHECK(anti_rt_library_symbol(handle, "anti_rt_provides") != NULL);
        anti_rt_library_close(handle);
    }
    CHECK(anti_rt_library_open(PLUGIN_UTF8 ".missing") == NULL);
    CHECK(strlen(anti_rt_library_error(text, sizeof text)) > 0);
}

int main(void)
{
    sleep_steps();
    environment();
    paths();
    library_outside_ascii();
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}
