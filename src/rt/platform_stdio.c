/* The printf and scanf families and atexit of a Windows target, which
   the headers of the UCRT declare and ucrtbase.dll does not export. See
   the static part of the C runtime in platform_windows.c.

   DESIGN: the definitions are weak, and the file is a member of the
   runtime as an object alone, never as bitcode. An object compiled with
   the headers of Microsoft, as the profile runtime of compiler-rt and a C
   object of a user may be, carries the inline functions of those headers
   under the same names, and a weak definition gives way to them where a
   strong one would be a duplicate. The LTO of lld takes a weak
   definition of bitcode as a strong one, so the object joins the bitcode
   archives as it is. This file holds a #if on the system outside the
   files named platform_<system>.c, as rule 22 of docs/c-guidelines.md
   says. */
#if defined(_WIN32)

#include <corecrt_startup.h>
#include <corecrt_stdio_config.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define ANTI_RT_WEAK __attribute__((weak))

/* atexit, which the UCRT exports as _crt_atexit alone. */
int atexit(void (*function)(void));
ANTI_RT_WEAK int atexit(void (*function)(void))
{
    return _crt_atexit(function);
}

/* The printf and scanf families, each one call of the common function
   of its kind with the options of the module, as the static library of
   Microsoft and the C runtime of mingw-w64 define them. The options are
   the flags a program could set, never set here, so each stays zero. The
   family is the one the headers of mingw-w64 declare as functions of the
   program. The seven they declare as imports of the DLL, _vscprintf,
   _swprintf, _vswprintf, _snwprintf, _vsnwprintf, _scwprintf and
   _vscwprintf, and _scprintf, whose v form is one of them, stay out,
   since the DLL exports none and no code of ours names them. */
ANTI_RT_WEAK unsigned long long *__local_stdio_printf_options(void)
{
    static unsigned long long options;

    return &options;
}

ANTI_RT_WEAK unsigned long long *__local_stdio_scanf_options(void)
{
    static unsigned long long options;

    return &options;
}

ANTI_RT_WEAK int vprintf(const char *format, va_list arguments)
{
    return __stdio_common_vfprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, stdout,
                                   format, NULL, arguments);
}

ANTI_RT_WEAK int printf(const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vprintf(format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vfprintf(FILE *file, const char *format, va_list arguments)
{
    return __stdio_common_vfprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, file,
                                   format, NULL, arguments);
}

ANTI_RT_WEAK int fprintf(FILE *file, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vfprintf(file, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vsprintf(char *buffer, const char *format, va_list arguments)
{
    return __stdio_common_vsprintf(
        _CRT_INTERNAL_LOCAL_PRINTF_OPTIONS |
            _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR,
        buffer, (size_t)-1, format, NULL, arguments);
}

ANTI_RT_WEAK int sprintf(char *buffer, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vsprintf(buffer, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vsnprintf(char *buffer, size_t size, const char *format,
              va_list arguments)
{
    return __stdio_common_vsprintf(
        _CRT_INTERNAL_LOCAL_PRINTF_OPTIONS |
            _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR,
        buffer, size, format, NULL, arguments);
}

ANTI_RT_WEAK int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return count;
}

/* The two of the older form, which write no terminator into a buffer
   the text fills. */
ANTI_RT_WEAK int _vsnprintf(char *buffer, size_t size, const char *format,
               va_list arguments)
{
    return __stdio_common_vsprintf(
        _CRT_INTERNAL_LOCAL_PRINTF_OPTIONS |
            _CRT_INTERNAL_PRINTF_LEGACY_VSPRINTF_NULL_TERMINATION,
        buffer, size, format, NULL, arguments);
}

ANTI_RT_WEAK int _snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = _vsnprintf(buffer, size, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vscanf(const char *format, va_list arguments)
{
    return __stdio_common_vfscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, stdin,
                                  format, NULL, arguments);
}

ANTI_RT_WEAK int scanf(const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vscanf(format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vfscanf(FILE *file, const char *format, va_list arguments)
{
    return __stdio_common_vfscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, file,
                                  format, NULL, arguments);
}

ANTI_RT_WEAK int fscanf(FILE *file, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vfscanf(file, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vsscanf(const char *text, const char *format, va_list arguments)
{
    return __stdio_common_vsscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, text,
                                  (size_t)-1, format, NULL, arguments);
}

ANTI_RT_WEAK int sscanf(const char *text, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vsscanf(text, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int _snscanf(const char *text, size_t size, const char *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = __stdio_common_vsscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, text,
                                   size, format, NULL, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vwprintf(const wchar_t *format, va_list arguments)
{
    return __stdio_common_vfwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS,
                                    stdout, format, NULL, arguments);
}

ANTI_RT_WEAK int wprintf(const wchar_t *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vwprintf(format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vfwprintf(FILE *file, const wchar_t *format, va_list arguments)
{
    return __stdio_common_vfwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, file,
                                    format, NULL, arguments);
}

ANTI_RT_WEAK int fwprintf(FILE *file, const wchar_t *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vfwprintf(file, format, arguments);
    va_end(arguments);
    return count;
}

/* swprintf of C99 takes a size and gives -1 for a text that does not
   fit, which the common function gives without the option of snprintf,
   except for no buffer and no size, where it gives the length the text
   would have and C99 asks for -1. The library of mingw-w64 does the
   same. */
ANTI_RT_WEAK int vswprintf(wchar_t *buffer, size_t size, const wchar_t *format,
              va_list arguments)
{
    int count;

    if (buffer == NULL && size == 0) {
        return -1;
    }
    count = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer,
                                     size, format, NULL, arguments);
    return count < 0 ? -1 : count;
}

ANTI_RT_WEAK int swprintf(wchar_t *buffer, size_t size, const wchar_t *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vswprintf(buffer, size, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vwscanf(const wchar_t *format, va_list arguments)
{
    return __stdio_common_vfwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, stdin,
                                   format, NULL, arguments);
}

ANTI_RT_WEAK int wscanf(const wchar_t *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vwscanf(format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vfwscanf(FILE *file, const wchar_t *format, va_list arguments)
{
    return __stdio_common_vfwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, file,
                                   format, NULL, arguments);
}

ANTI_RT_WEAK int fwscanf(FILE *file, const wchar_t *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vfwscanf(file, format, arguments);
    va_end(arguments);
    return count;
}

ANTI_RT_WEAK int vswscanf(const wchar_t *text, const wchar_t *format, va_list arguments)
{
    return __stdio_common_vswscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, text,
                                   (size_t)-1, format, NULL, arguments);
}

ANTI_RT_WEAK int swscanf(const wchar_t *text, const wchar_t *format, ...)
{
    va_list arguments;
    int count;

    va_start(arguments, format);
    count = vswscanf(text, format, arguments);
    va_end(arguments);
    return count;
}

#else

/* ISO C wants a declaration in every file, and elsewhere this one holds
   no other. */
typedef int anti_rt_platform_stdio_unused;

#endif
