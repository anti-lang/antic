/* The error of the C library and the Win32 error that the next
   SystemError reads, set to a code of the test's choice for
   tests/abi/abi_system_error.anti. Every system but Windows has no Win32
   error to set. */
#include <errno.h>

#include "../binary_stdio.h"
#ifdef _WIN32
#include <windows.h>
#endif

void abi_set_errno(int code);
void abi_set_last_error(int code);

void abi_set_errno(int code)
{
    errno = code;
}

void abi_set_last_error(int code)
{
#ifdef _WIN32
    SetLastError((DWORD)code);
#else
    (void)code;
#endif
}
