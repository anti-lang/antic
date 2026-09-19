/* DESIGN: the raw-bytes rule holds for the C side of a comparison too. The
   bytes a program writes are the bytes the test scripts read. The C streams
   of Windows start in text mode, which writes CRLF for each LF. Every C test
   file includes this header, which puts stdout and stderr into binary mode
   before main. Elsewhere it holds nothing. */
#ifndef ANTIC_TESTS_BINARY_STDIO_H
#define ANTIC_TESTS_BINARY_STDIO_H

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

#if defined(_MSC_VER) && !defined(__clang__)
static void __cdecl binary_stdio(void);
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) static void(__cdecl *binary_stdio_entry)(void) =
    binary_stdio;
#else
__attribute__((constructor))
#endif
static void binary_stdio(void)
{
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
}
#endif

#endif
