/* A C++ API. anti bind reads a header as C, so clang refuses the
   namespace and the binding is refused with it. */
namespace audio
{
int volume(void);
}
