/* The C half of the tests media_audio_link_<target>. A program that calls
   raylib's audio and miniaudio of its own links both libraries of the
   runtime tree, and the two share one set of the ma_ functions: a second
   copy in raylib would define each of them twice. The functions give plain
   integers, so the Anti half needs no binding. On Linux the file is linked
   by itself, and MEDIA_AUDIO_PROBE_MAIN gives it the main that prints what
   media_audio_link.anti prints. */
#include "../binary_stdio.h"
#include <miniaudio.h>
#include <raylib.h>
#include <stdint.h>

/* The system libraries of raylib on Windows, as raylib_probe.c names
   them. */
#if defined(_WIN32)
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#endif

int32_t media_audio_probe_raylib(void);
int32_t media_audio_probe_miniaudio(void);

/* The frames of eight frames of silence at 8000 Hz once raylib has
   resampled them to 16000 Hz. WaveFormat converts through
   ma_convert_frames, which raylib takes from the miniaudio library. */
int32_t media_audio_probe_raylib(void)
{
    Wave wave;
    int32_t frames;

    wave.frameCount = 8;
    wave.sampleRate = 8000;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = MemAlloc(8 * sizeof(int16_t));
    if (wave.data == NULL) {
        return -1;
    }
    WaveFormat(&wave, 16000, 16, 1);
    frames = (int32_t)wave.frameCount;
    if (wave.sampleRate != 16000) {
        frames = -2;
    }
    UnloadWave(wave);
    return frames;
}

/* The positive samples of the first eight frames of a square wave of
   1000 Hz at 8000 Hz, which is four. raylib leaves the generators out of
   its build of miniaudio, so only the library defines them. */
int32_t media_audio_probe_miniaudio(void)
{
    ma_waveform_config config = ma_waveform_config_init(
        ma_format_f32, 1, 8000, ma_waveform_type_square, 0.5, 1000.0);
    ma_waveform wave;
    float frames[8];
    ma_uint64 read = 0;
    int32_t positive = 0;
    size_t i;

    if (ma_waveform_init(&config, &wave) != MA_SUCCESS) {
        return -1;
    }
    if (ma_waveform_read_pcm_frames(&wave, frames, 8, &read) != MA_SUCCESS ||
        read != 8) {
        ma_waveform_uninit(&wave);
        return -2;
    }
    for (i = 0; i < 8; i++) {
        if (frames[i] > 0.0f) {
            positive++;
        }
    }
    ma_waveform_uninit(&wave);
    return positive;
}

#if defined(MEDIA_AUDIO_PROBE_MAIN)
#include <stdio.h>

int main(void)
{
    printf("%d\n", (int)media_audio_probe_raylib());
    printf("%d\n", (int)media_audio_probe_miniaudio());
    return 0;
}
#endif
