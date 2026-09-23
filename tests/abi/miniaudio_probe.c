/* The C half of the tests miniaudio_link_<target>. It calls the
   miniaudio of the runtime tree without an audio device. The functions
   give plain integers, so the Anti half needs no binding of miniaudio. On
   Linux the file is linked by itself, and MINIAUDIO_PROBE_MAIN gives it
   the main that prints what miniaudio_link.anti prints. */
#include "../binary_stdio.h"
#include <miniaudio.h>
#include <stdint.h>
#include <string.h>

int32_t miniaudio_probe_version(void);
int32_t miniaudio_probe_square(void);
int32_t miniaudio_probe_null_devices(void);
int32_t miniaudio_probe_linked(void);

/* 1 when the library reports the version of its header. */
int32_t miniaudio_probe_version(void)
{
    return strcmp(ma_version_string(), MA_VERSION_STRING) == 0;
}

/* The positive samples of the first eight frames of a square wave of
   1000 Hz at 8000 Hz. The phase advances by 1/8, which a float holds
   exactly, so the first half of the period is positive. */
int32_t miniaudio_probe_square(void)
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

/* The playback devices of the null back end, which is one on every
   target. The context starts and stops without an audio device. */
int32_t miniaudio_probe_null_devices(void)
{
    ma_backend backends[1] = {ma_backend_null};
    ma_context context;
    ma_device_info *playback;
    ma_device_info *capture;
    ma_uint32 playback_count = 0;
    ma_uint32 capture_count = 0;

    if (ma_context_init(backends, 1, NULL, &context) != MA_SUCCESS) {
        return -1;
    }
    if (ma_context_get_devices(&context, &playback, &playback_count, &capture,
                               &capture_count) != MA_SUCCESS) {
        ma_context_uninit(&context);
        return -2;
    }
    ma_context_uninit(&context);
    return (int32_t)playback_count;
}

/* 1 when the device and the decoder functions resolved at link, so the
   parts that open a device and read a file are in the program. */
int32_t miniaudio_probe_linked(void)
{
    ma_result (*volatile device)(ma_context *, const ma_device_config *,
                                 ma_device *) = ma_device_init;
    ma_result (*volatile decoder)(const char *, const ma_decoder_config *,
                                  ma_decoder *) = ma_decoder_init_file;

    return device != NULL && decoder != NULL;
}

#if defined(MINIAUDIO_PROBE_MAIN)
#include <stdio.h>

int main(void)
{
    printf("%d\n", (int)miniaudio_probe_version());
    printf("%d\n", (int)miniaudio_probe_square());
    printf("%d\n", (int)miniaudio_probe_null_devices());
    printf("%d\n", (int)miniaudio_probe_linked());
    return 0;
}
#endif
