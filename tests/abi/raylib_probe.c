/* The C half of the tests raylib_link_<target>. It calls the raylib of
   the runtime tree without a window. The functions give plain integers, so
   the Anti half needs no binding of raylib. On Linux the file is linked by
   itself, and RAYLIB_PROBE_MAIN gives it the main that prints what
   raylib_link.anti prints. */
#include "../binary_stdio.h"
#include <raylib.h>
#include <stddef.h>
#include <stdint.h>

/* The system libraries raylib's desktop back end calls on Windows. The
   directives reach lld-link through the object, as they do for a C
   program built with the tools of Microsoft. GLFW loads opengl32.dll at
   run time, and kernel32.lib comes with the C runtime. */
#if defined(_WIN32)
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#endif

int32_t raylib_probe_resize(void);
int32_t raylib_probe_text(void);
int32_t raylib_probe_color(void);
int32_t raylib_probe_linked(void);

/* 1 when a solid red image stays solid red once resized. The resampler of
   stb_image_resize2.h takes its SIMD path at the level of every target. */
int32_t raylib_probe_resize(void)
{
    Image image = GenImageColor(64, 64, RED);
    Color centre;
    Color corner;
    int32_t same;

    ImageResize(&image, 16, 16);
    centre = GetImageColor(image, 8, 8);
    corner = GetImageColor(image, 15, 15);
    same = image.width == 16 && image.height == 16 && centre.r == RED.r &&
           centre.g == RED.g && centre.b == RED.b && centre.a == RED.a &&
           corner.r == RED.r && corner.g == RED.g && corner.b == RED.b &&
           corner.a == RED.a;
    UnloadImage(image);
    return same;
}

/* The length of a formatted text plus ten times an integer read from one,
   5 + 10 * -17. */
int32_t raylib_probe_text(void)
{
    return (int32_t)TextLength(TextFormat("%d-%s", 42, "ab")) +
           10 * TextToInteger("-17");
}

/* RED at half its alpha, packed as raylib packs a colour. */
int32_t raylib_probe_color(void)
{
    return ColorToInt(Fade(RED, 0.5f));
}

/* 1 when the window, drawing, model and audio functions resolved at link,
   so GLFW, rlgl and raylib's own miniaudio are in the program. */
int32_t raylib_probe_linked(void)
{
    void (*volatile window)(int, int, const char *) = InitWindow;
    void (*volatile audio)(void) = InitAudioDevice;
    void (*volatile text)(const char *, int, int, int, Color) = DrawText;
    Model (*volatile model)(const char *) = LoadModel;

    return window != NULL && audio != NULL && text != NULL && model != NULL;
}

#if defined(RAYLIB_PROBE_MAIN)
#include <stdio.h>

int main(void)
{
    printf("%d\n", (int)raylib_probe_resize());
    printf("%d\n", (int)raylib_probe_text());
    printf("%d\n", (int)raylib_probe_color());
    printf("%d\n", (int)raylib_probe_linked());
    return 0;
}
#endif
