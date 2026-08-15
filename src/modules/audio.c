#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <psp2/audioout.h>

#include "../env.h"

/*
 * VitaJS Audio module
 * -------------------
 *
 * Small game-audio mixer for sound effects.
 *
 * Input:
 *   RIFF/WAVE, PCM, signed 16-bit, mono or stereo.
 *   Common source sample rates are accepted and resampled to 48 kHz.
 *
 * Output:
 *   48 kHz, signed 16-bit stereo through SCE_AUDIO_OUT_PORT_TYPE_MAIN.
 *
 * Features:
 *   - up to 64 loaded clips
 *   - up to 16 simultaneous voices
 *   - play/stop/loop
 *   - per-voice volume
 *   - master volume
 *   - playback runs on a native audio thread, never blocks QuickJS
 *
 * JavaScript API:
 *
 *   const laser = Audio.load_wav("app0:/assets/laser.wav");
 *
 *   const voice = Audio.play(laser);
 *   Audio.play(laser, 0.5);
 *   Audio.play(laser, 1.0, true); // loop
 *
 *   Audio.stop(voice);
 *   Audio.stop_all();
 *   Audio.is_playing(voice);
 *   Audio.set_voice_volume(voice, 0.5);
 *   Audio.set_master_volume(0.8);
 *
 *   Audio.free(laser);
 *   Audio.term();
 *
 * Clip and voice values are integer handles. This deliberately avoids
 * unnecessary QuickJS native object lifetimes for audio resources.
 */

#define AUDIO_OUTPUT_RATE       48000
#define AUDIO_GRAIN_FRAMES      1024
#define AUDIO_CHANNELS          2

#define AUDIO_MAX_CLIPS         64
#define AUDIO_MAX_VOICES        16

#define AUDIO_MAX_WAV_BYTES     (64u * 1024u * 1024u)

typedef struct AudioClip
{
    int used;
    int16_t *samples;     /* interleaved stereo, always 48 kHz */
    uint32_t frames;
} AudioClip;

typedef struct AudioVoice
{
    int active;
    int clip_id;
    uint32_t position;
    float volume;
    int loop;
} AudioVoice;

typedef struct WavInfo
{
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;

    long data_offset;
    uint32_t data_size;
} WavInfo;

static AudioClip clips[AUDIO_MAX_CLIPS];
static AudioVoice voices[AUDIO_MAX_VOICES];

static pthread_mutex_t audio_mutex;
static pthread_t audio_thread;

static int audio_mutex_initialized = 0;
static volatile int audio_running = 0;
static int audio_initialized = 0;
static int audio_port = -1;

static float master_volume = 1.0f;

static uint16_t read_u16_le(FILE *f)
{
    uint8_t b[2];

    if (fread(b, 1, 2, f) != 2)
        return 0;

    return (uint16_t)(
        ((uint16_t)b[0]) |
        ((uint16_t)b[1] << 8));
}

static uint32_t read_u32_le(FILE *f)
{
    uint8_t b[4];

    if (fread(b, 1, 4, f) != 4)
        return 0;

    return
        ((uint32_t)b[0]) |
        ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) |
        ((uint32_t)b[3] << 24);
}

static int16_t clamp_s16(int32_t value)
{
    if (value > 32767)
        return 32767;

    if (value < -32768)
        return -32768;

    return (int16_t)value;
}

static float clamp_volume(float volume)
{
    if (volume < 0.0f)
        return 0.0f;

    if (volume > 1.0f)
        return 1.0f;

    return volume;
}

static int parse_wav(FILE *f, WavInfo *info)
{
    char id[4];

    memset(info, 0, sizeof(*info));

    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4) != 0)
        return -1;

    (void)read_u32_le(f);

    if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4) != 0)
        return -1;

    int found_fmt = 0;
    int found_data = 0;

    while (!found_fmt || !found_data)
    {
        if (fread(id, 1, 4, f) != 4)
            break;

        uint32_t chunk_size = read_u32_le(f);
        long chunk_data_pos = ftell(f);

        if (memcmp(id, "fmt ", 4) == 0)
        {
            if (chunk_size < 16)
                return -1;

            info->audio_format = read_u16_le(f);
            info->channels = read_u16_le(f);
            info->sample_rate = read_u32_le(f);

            (void)read_u32_le(f); /* byte rate */
            (void)read_u16_le(f); /* block align */

            info->bits_per_sample = read_u16_le(f);

            found_fmt = 1;
        }
        else if (memcmp(id, "data", 4) == 0)
        {
            info->data_offset = chunk_data_pos;
            info->data_size = chunk_size;
            found_data = 1;
        }

        long skip_to =
            chunk_data_pos +
            (long)chunk_size +
            (long)(chunk_size & 1u);

        if (fseek(f, skip_to, SEEK_SET) != 0)
            return -1;
    }

    if (!found_fmt || !found_data)
        return -1;

    if (info->audio_format != 1)
        return -2; /* PCM only */

    if (info->bits_per_sample != 16)
        return -3;

    if (info->channels != 1 && info->channels != 2)
        return -4;

    if (info->sample_rate == 0)
        return -5;

    if (info->data_size == 0 || info->data_size > AUDIO_MAX_WAV_BYTES)
        return -6;

    return 0;
}

static int load_wav_as_48k_stereo(
    const char *path,
    int16_t **out_samples,
    uint32_t *out_frames)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return -10;

    WavInfo info;

    int parse_result = parse_wav(f, &info);

    if (parse_result < 0)
    {
        fclose(f);
        return parse_result;
    }

    uint32_t bytes_per_frame =
        (uint32_t)info.channels * sizeof(int16_t);

    if (bytes_per_frame == 0)
    {
        fclose(f);
        return -11;
    }

    uint32_t input_frames =
        info.data_size / bytes_per_frame;

    if (input_frames == 0)
    {
        fclose(f);
        return -12;
    }

    if (fseek(f, info.data_offset, SEEK_SET) != 0)
    {
        fclose(f);
        return -13;
    }

    size_t input_sample_count =
        (size_t)input_frames * info.channels;

    int16_t *input_samples =
        malloc(input_sample_count * sizeof(int16_t));

    if (!input_samples)
    {
        fclose(f);
        return -14;
    }

    size_t expected_bytes =
        input_sample_count * sizeof(int16_t);

    if (fread(input_samples, 1, expected_bytes, f) != expected_bytes)
    {
        free(input_samples);
        fclose(f);
        return -15;
    }

    fclose(f);

    uint64_t output_frames_64 =
        ((uint64_t)input_frames * AUDIO_OUTPUT_RATE +
         info.sample_rate - 1) /
        info.sample_rate;

    if (output_frames_64 == 0 || output_frames_64 > UINT32_MAX)
    {
        free(input_samples);
        return -16;
    }

    uint32_t output_frames =
        (uint32_t)output_frames_64;

    if ((size_t)output_frames > SIZE_MAX / (AUDIO_CHANNELS * sizeof(int16_t)))
    {
        free(input_samples);
        return -17;
    }

    int16_t *output =
        malloc(
            (size_t)output_frames *
            AUDIO_CHANNELS *
            sizeof(int16_t));

    if (!output)
    {
        free(input_samples);
        return -18;
    }

    /*
     * Linear resampling to 48 kHz.
     * This is plenty for game SFX and avoids forcing a specific WAV rate
     * in the asset pipeline.
     */
    for (uint32_t i = 0; i < output_frames; i++)
    {
        double source_position =
            ((double)i * (double)info.sample_rate) /
            (double)AUDIO_OUTPUT_RATE;

        uint32_t index0 = (uint32_t)source_position;

        if (index0 >= input_frames)
            index0 = input_frames - 1;

        uint32_t index1 =
            index0 + 1 < input_frames
                ? index0 + 1
                : index0;

        double fraction =
            source_position - (double)index0;

        int16_t l0;
        int16_t r0;
        int16_t l1;
        int16_t r1;

        if (info.channels == 1)
        {
            l0 = r0 = input_samples[index0];
            l1 = r1 = input_samples[index1];
        }
        else
        {
            l0 = input_samples[(size_t)index0 * 2 + 0];
            r0 = input_samples[(size_t)index0 * 2 + 1];
            l1 = input_samples[(size_t)index1 * 2 + 0];
            r1 = input_samples[(size_t)index1 * 2 + 1];
        }

        double left =
            (double)l0 +
            ((double)l1 - (double)l0) * fraction;

        double right =
            (double)r0 +
            ((double)r1 - (double)r0) * fraction;

        output[(size_t)i * 2 + 0] =
            clamp_s16((int32_t)left);

        output[(size_t)i * 2 + 1] =
            clamp_s16((int32_t)right);
    }

    free(input_samples);

    *out_samples = output;
    *out_frames = output_frames;

    return 0;
}

static void *audio_mixer_thread(void *arg)
{
    (void)arg;

    int16_t mix_buffer[
        AUDIO_GRAIN_FRAMES *
        AUDIO_CHANNELS
    ];

    while (audio_running)
    {
        memset(
            mix_buffer,
            0,
            sizeof(mix_buffer));

        pthread_mutex_lock(&audio_mutex);

        float current_master = master_volume;

        for (int frame = 0; frame < AUDIO_GRAIN_FRAMES; frame++)
        {
            int32_t mixed_left = 0;
            int32_t mixed_right = 0;

            for (int v = 0; v < AUDIO_MAX_VOICES; v++)
            {
                AudioVoice *voice = &voices[v];

                if (!voice->active)
                    continue;

                if (
                    voice->clip_id < 0 ||
                    voice->clip_id >= AUDIO_MAX_CLIPS ||
                    !clips[voice->clip_id].used)
                {
                    voice->active = 0;
                    continue;
                }

                AudioClip *clip =
                    &clips[voice->clip_id];

                if (voice->position >= clip->frames)
                {
                    if (voice->loop && clip->frames > 0)
                    {
                        voice->position = 0;
                    }
                    else
                    {
                        voice->active = 0;
                        continue;
                    }
                }

                size_t sample_index =
                    (size_t)voice->position * 2;

                float gain =
                    voice->volume *
                    current_master;

                mixed_left +=
                    (int32_t)(
                        (float)clip->samples[sample_index + 0] *
                        gain);

                mixed_right +=
                    (int32_t)(
                        (float)clip->samples[sample_index + 1] *
                        gain);

                voice->position++;
            }

            mix_buffer[(size_t)frame * 2 + 0] =
                clamp_s16(mixed_left);

            mix_buffer[(size_t)frame * 2 + 1] =
                clamp_s16(mixed_right);
        }

        pthread_mutex_unlock(&audio_mutex);

        /*
         * Blocking by design, but only on the audio thread.
         * One call outputs exactly AUDIO_GRAIN_FRAMES stereo frames.
         */
        int result =
            sceAudioOutOutput(
                audio_port,
                mix_buffer);

        if (result < 0)
        {
            /*
             * Avoid a hot loop if the output port becomes invalid.
             * The process can still terminate cleanly via Audio.term().
             */
            audio_running = 0;
            break;
        }
    }

    return NULL;
}

static int audio_ensure_initialized(void)
{
    if (audio_initialized)
        return 0;

    if (!audio_mutex_initialized)
    {
        if (pthread_mutex_init(&audio_mutex, NULL) != 0)
            return -1;

        audio_mutex_initialized = 1;
    }

    audio_port =
        sceAudioOutOpenPort(
            SCE_AUDIO_OUT_PORT_TYPE_MAIN,
            AUDIO_GRAIN_FRAMES,
            AUDIO_OUTPUT_RATE,
            SCE_AUDIO_OUT_MODE_STEREO);

    if (audio_port < 0)
        return audio_port;

    memset(clips, 0, sizeof(clips));
    memset(voices, 0, sizeof(voices));

    for (int i = 0; i < AUDIO_MAX_VOICES; i++)
        voices[i].clip_id = -1;

    master_volume = 1.0f;
    audio_running = 1;

    int thread_result =
        pthread_create(
            &audio_thread,
            NULL,
            audio_mixer_thread,
            NULL);

    if (thread_result != 0)
    {
        audio_running = 0;

        sceAudioOutReleasePort(audio_port);
        audio_port = -1;

        return -2;
    }

    audio_initialized = 1;

    return 0;
}

static int find_free_clip(void)
{
    for (int i = 0; i < AUDIO_MAX_CLIPS; i++)
    {
        if (!clips[i].used)
            return i;
    }

    return -1;
}

static int find_free_voice(void)
{
    for (int i = 0; i < AUDIO_MAX_VOICES; i++)
    {
        if (!voices[i].active)
            return i;
    }

    return -1;
}

static JSValue vitajs_audio_load_wav(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "load_wav(path: string) expects one argument");

    int init_result = audio_ensure_initialized();

    if (init_result < 0)
        return JS_ThrowInternalError(
            ctx,
            "Unable to initialize PS Vita audio: 0x%08X",
            (unsigned int)init_result);

    const char *path =
        JS_ToCString(ctx, argv[0]);

    if (!path)
        return JS_EXCEPTION;

    int16_t *samples = NULL;
    uint32_t frames = 0;

    int result =
        load_wav_as_48k_stereo(
            path,
            &samples,
            &frames);

    JS_FreeCString(ctx, path);

    if (result < 0)
    {
        return JS_ThrowInternalError(
            ctx,
            "Unable to load WAV file (error %d). "
            "Expected RIFF/WAVE PCM 16-bit mono or stereo.",
            result);
    }

    pthread_mutex_lock(&audio_mutex);

    int clip_id = find_free_clip();

    if (clip_id >= 0)
    {
        clips[clip_id].used = 1;
        clips[clip_id].samples = samples;
        clips[clip_id].frames = frames;
    }

    pthread_mutex_unlock(&audio_mutex);

    if (clip_id < 0)
    {
        free(samples);

        return JS_ThrowInternalError(
            ctx,
            "Audio clip limit reached (%d)",
            AUDIO_MAX_CLIPS);
    }

    return JS_NewInt32(ctx, clip_id);
}

static JSValue vitajs_audio_free(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "free(clipId) expects one argument");

    int32_t clip_id;

    if (JS_ToInt32(ctx, &clip_id, argv[0]))
        return JS_EXCEPTION;

    if (
        clip_id < 0 ||
        clip_id >= AUDIO_MAX_CLIPS)
    {
        return JS_ThrowRangeError(
            ctx,
            "Invalid audio clip id");
    }

    if (!audio_initialized)
        return JS_UNDEFINED;

    pthread_mutex_lock(&audio_mutex);

    if (clips[clip_id].used)
    {
        for (int i = 0; i < AUDIO_MAX_VOICES; i++)
        {
            if (
                voices[i].active &&
                voices[i].clip_id == clip_id)
            {
                voices[i].active = 0;
                voices[i].clip_id = -1;
                voices[i].position = 0;
            }
        }

        free(clips[clip_id].samples);

        clips[clip_id].samples = NULL;
        clips[clip_id].frames = 0;
        clips[clip_id].used = 0;
    }

    pthread_mutex_unlock(&audio_mutex);

    return JS_UNDEFINED;
}

static JSValue vitajs_audio_play(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc < 1 || argc > 3)
    {
        return JS_ThrowSyntaxError(
            ctx,
            "play(clipId[, volume[, loop]]) expects 1-3 arguments");
    }

    int init_result = audio_ensure_initialized();

    if (init_result < 0)
        return JS_ThrowInternalError(
            ctx,
            "Unable to initialize PS Vita audio: 0x%08X",
            (unsigned int)init_result);

    int32_t clip_id;

    if (JS_ToInt32(ctx, &clip_id, argv[0]))
        return JS_EXCEPTION;

    float volume = 1.0f;
    int loop = 0;

    if (argc >= 2)
    {
        if (JS_ToFloat32(ctx, &volume, argv[1]))
            return JS_EXCEPTION;

        volume = clamp_volume(volume);
    }

    if (argc >= 3)
    {
        loop = JS_ToBool(ctx, argv[2]);

        if (loop < 0)
            return JS_EXCEPTION;
    }

    pthread_mutex_lock(&audio_mutex);

    if (
        clip_id < 0 ||
        clip_id >= AUDIO_MAX_CLIPS ||
        !clips[clip_id].used)
    {
        pthread_mutex_unlock(&audio_mutex);

        return JS_ThrowRangeError(
            ctx,
            "Invalid or freed audio clip id");
    }

    int voice_id = find_free_voice();

    if (voice_id >= 0)
    {
        voices[voice_id].active = 1;
        voices[voice_id].clip_id = clip_id;
        voices[voice_id].position = 0;
        voices[voice_id].volume = volume;
        voices[voice_id].loop = loop ? 1 : 0;
    }

    pthread_mutex_unlock(&audio_mutex);

    /*
     * -1 means all 16 mixer voices are currently in use.
     * This is intentionally non-throwing so a busy sound effect does
     * not crash normal game logic.
     */
    return JS_NewInt32(ctx, voice_id);
}

static JSValue vitajs_audio_stop(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "stop(voiceId) expects one argument");

    int32_t voice_id;

    if (JS_ToInt32(ctx, &voice_id, argv[0]))
        return JS_EXCEPTION;

    if (
        voice_id < 0 ||
        voice_id >= AUDIO_MAX_VOICES)
    {
        return JS_ThrowRangeError(
            ctx,
            "Invalid audio voice id");
    }

    if (!audio_initialized)
        return JS_UNDEFINED;

    pthread_mutex_lock(&audio_mutex);

    voices[voice_id].active = 0;
    voices[voice_id].clip_id = -1;
    voices[voice_id].position = 0;

    pthread_mutex_unlock(&audio_mutex);

    return JS_UNDEFINED;
}

static JSValue vitajs_audio_stop_all(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(
            ctx,
            "stop_all() expects no arguments");

    if (!audio_initialized)
        return JS_UNDEFINED;

    pthread_mutex_lock(&audio_mutex);

    for (int i = 0; i < AUDIO_MAX_VOICES; i++)
    {
        voices[i].active = 0;
        voices[i].clip_id = -1;
        voices[i].position = 0;
    }

    pthread_mutex_unlock(&audio_mutex);

    return JS_UNDEFINED;
}

static JSValue vitajs_audio_is_playing(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "is_playing(voiceId) expects one argument");

    int32_t voice_id;

    if (JS_ToInt32(ctx, &voice_id, argv[0]))
        return JS_EXCEPTION;

    if (
        voice_id < 0 ||
        voice_id >= AUDIO_MAX_VOICES)
    {
        return JS_FALSE;
    }

    if (!audio_initialized)
        return JS_FALSE;

    pthread_mutex_lock(&audio_mutex);

    int active =
        voices[voice_id].active;

    pthread_mutex_unlock(&audio_mutex);

    return JS_NewBool(ctx, active);
}

static JSValue vitajs_audio_set_voice_volume(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 2)
        return JS_ThrowSyntaxError(
            ctx,
            "set_voice_volume(voiceId, volume) expects two arguments");

    int32_t voice_id;
    float volume;

    if (JS_ToInt32(ctx, &voice_id, argv[0]))
        return JS_EXCEPTION;

    if (JS_ToFloat32(ctx, &volume, argv[1]))
        return JS_EXCEPTION;

    if (
        voice_id < 0 ||
        voice_id >= AUDIO_MAX_VOICES)
    {
        return JS_ThrowRangeError(
            ctx,
            "Invalid audio voice id");
    }

    if (!audio_initialized)
        return JS_UNDEFINED;

    pthread_mutex_lock(&audio_mutex);

    voices[voice_id].volume =
        clamp_volume(volume);

    pthread_mutex_unlock(&audio_mutex);

    return JS_UNDEFINED;
}

static JSValue vitajs_audio_set_master_volume(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(
            ctx,
            "set_master_volume(volume) expects one argument");

    float volume;

    if (JS_ToFloat32(ctx, &volume, argv[0]))
        return JS_EXCEPTION;

    if (!audio_initialized)
    {
        /*
         * Initialize so the value belongs to a live mixer lifecycle.
         */
        int init_result = audio_ensure_initialized();

        if (init_result < 0)
            return JS_ThrowInternalError(
                ctx,
                "Unable to initialize PS Vita audio: 0x%08X",
                (unsigned int)init_result);
    }

    pthread_mutex_lock(&audio_mutex);

    master_volume =
        clamp_volume(volume);

    pthread_mutex_unlock(&audio_mutex);

    return JS_UNDEFINED;
}

static JSValue vitajs_audio_get_master_volume(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(
            ctx,
            "get_master_volume() expects no arguments");

    float volume = master_volume;

    if (audio_initialized)
    {
        pthread_mutex_lock(&audio_mutex);
        volume = master_volume;
        pthread_mutex_unlock(&audio_mutex);
    }

    return JS_NewFloat32(ctx, volume);
}

static void audio_shutdown_native(void)
{
    if (!audio_initialized)
        return;

    audio_running = 0;

    /*
     * sceAudioOutOutput blocks for roughly one grain, so join waits only
     * for the current audio buffer to finish.
     */
    pthread_join(audio_thread, NULL);

    pthread_mutex_lock(&audio_mutex);

    for (int i = 0; i < AUDIO_MAX_VOICES; i++)
    {
        voices[i].active = 0;
        voices[i].clip_id = -1;
        voices[i].position = 0;
    }

    for (int i = 0; i < AUDIO_MAX_CLIPS; i++)
    {
        if (clips[i].used)
        {
            free(clips[i].samples);

            clips[i].samples = NULL;
            clips[i].frames = 0;
            clips[i].used = 0;
        }
    }

    pthread_mutex_unlock(&audio_mutex);

    if (audio_port >= 0)
    {
        sceAudioOutReleasePort(audio_port);
        audio_port = -1;
    }

    audio_initialized = 0;

    if (audio_mutex_initialized)
    {
        pthread_mutex_destroy(&audio_mutex);
        audio_mutex_initialized = 0;
    }
}

static JSValue vitajs_audio_term(
    JSContext *ctx,
    JSValue this_val,
    int argc,
    JSValueConst *argv)
{
    (void)ctx;
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(
            ctx,
            "term() expects no arguments");

    audio_shutdown_native();

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("load_wav", 1, vitajs_audio_load_wav),
    JS_CFUNC_DEF("free", 1, vitajs_audio_free),
    JS_CFUNC_DEF("play", 3, vitajs_audio_play),
    JS_CFUNC_DEF("stop", 1, vitajs_audio_stop),
    JS_CFUNC_DEF("stop_all", 0, vitajs_audio_stop_all),
    JS_CFUNC_DEF("is_playing", 1, vitajs_audio_is_playing),
    JS_CFUNC_DEF("set_voice_volume", 2, vitajs_audio_set_voice_volume),
    JS_CFUNC_DEF("set_master_volume", 1, vitajs_audio_set_master_volume),
    JS_CFUNC_DEF("get_master_volume", 0, vitajs_audio_get_master_volume),
    JS_CFUNC_DEF("term", 0, vitajs_audio_term),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(
        ctx,
        m,
        module_funcs,
        countof(module_funcs));
}

JSModuleDef *vitajs_audio_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        module_init,
        module_funcs,
        countof(module_funcs),
        "Audio");
}
