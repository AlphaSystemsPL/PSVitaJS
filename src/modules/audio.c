#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <psp2/audioout.h>
#include "../env.h"
#include <tremor/ivorbisfile.h>
#define OUT_RATE 48000
#define GRAIN 1024
#define MAX_CLIPS 64
#define MAX_VOICES 16
#define MAX_STREAMS 4
#define MAX_WAV_BYTES (64u * 1024u * 1024u)
#define STREAM_CACHE 4096

typedef struct
{
	int used;
	int16_t *samples;
	uint32_t frames;
} Clip;
typedef struct
{
	int active, clip;
	uint32_t pos;
	float vol;
	int loop;
} Voice;
typedef struct
{
	uint16_t format, channels;
	uint32_t rate;
	uint16_t bits;
	long data_off;
	uint32_t data_size;
} WavInfo;
typedef enum
{
	STREAM_NONE = 0,
	STREAM_WAV = 1,
	STREAM_OGG = 2
} StreamType;

typedef struct
{
	int used, playing, paused, loop;
	StreamType type;

	FILE *f; /* WAV only. OGG FILE* is owned by ov_open()/ov_clear(). */
	WavInfo info;

	OggVorbis_File ogg;
	int ogg_open;
	ogg_int64_t ogg_decode_frame;

	uint32_t rate;
	uint16_t channels;
	ogg_int64_t frames;

	double pos;
	float vol;

	ogg_int64_t cache_start;
	uint32_t cache_frames;
	int16_t cache[STREAM_CACHE * 2]; /* always interleaved stereo PCM16 */
} Stream;
static Clip clips[MAX_CLIPS];
static Voice voices[MAX_VOICES];
static Stream streams[MAX_STREAMS];
static int16_t ogg_decode_buffer[STREAM_CACHE * 2];
static pthread_mutex_t mutex;
static pthread_t thread;
static int mutex_init, initialized, port = -1;
static volatile int running;
static float master = 1.0f;
static uint16_t u16(FILE *f)
{
	uint8_t b[2];
	if (fread(b, 1, 2, f) != 2)
		return 0;
	return b[0] | ((uint16_t)b[1] << 8);
}
static uint32_t u32(FILE *f)
{
	uint8_t b[4];
	if (fread(b, 1, 4, f) != 4)
		return 0;
	return b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static int16_t clamp16(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768
																		  : (int16_t)v; }
static float clampv(float v) { return v < 0 ? 0 : v > 1 ? 1
														: v; }
static int parse_wav(FILE *f, WavInfo *i, int enforce_max)
{
	char id[4];
	memset(i, 0, sizeof(*i));
	if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4))
		return -1;
	(void)u32(f);
	if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4))
		return -1;
	int ff = 0, fd = 0;
	while (!ff || !fd)
	{
		if (fread(id, 1, 4, f) != 4)
			break;
		uint32_t n = u32(f);
		long p = ftell(f);
		if (!memcmp(id, "fmt ", 4))
		{
			if (n < 16)
				return -1;
			i->format = u16(f);
			i->channels = u16(f);
			i->rate = u32(f);
			(void)u32(f);
			(void)u16(f);
			i->bits = u16(f);
			ff = 1;
		}
		else if (!memcmp(id, "data", 4))
		{
			i->data_off = p;
			i->data_size = n;
			fd = 1;
		}
		if (fseek(f, p + n + (n & 1), SEEK_SET) != 0)
			return -1;
	}
	if (!ff || !fd || i->format != 1 || i->bits != 16 || (i->channels != 1 && i->channels != 2) || !i->rate || !i->data_size)
		return -2;
	if (enforce_max && i->data_size > MAX_WAV_BYTES)
		return -3;
	return 0;
}
static int load_clip(const char *path, int16_t **out, uint32_t *outframes)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return -10;
	WavInfo i;
	int r = parse_wav(f, &i, 1);
	if (r < 0)
	{
		fclose(f);
		return r;
	}
	uint32_t inf = i.data_size / (i.channels * 2);
	if (!inf || fseek(f, i.data_off, SEEK_SET))
	{
		fclose(f);
		return -11;
	}
	size_t ns = (size_t)inf * i.channels;
	int16_t *in = malloc(ns * 2);
	if (!in)
	{
		fclose(f);
		return -12;
	}
	if (fread(in, 2, ns, f) != ns)
	{
		free(in);
		fclose(f);
		return -13;
	}
	fclose(f);
	uint32_t of = (uint32_t)(((uint64_t)inf * OUT_RATE + i.rate - 1) / i.rate);
	int16_t *o = malloc((size_t)of * 4);
	if (!o)
	{
		free(in);
		return -14;
	}
	for (uint32_t x = 0; x < of; x++)
	{
		double sp = (double)x * i.rate / OUT_RATE;
		uint32_t a = (uint32_t)sp;
		if (a >= inf)
			a = inf - 1;
		uint32_t b = a + 1 < inf ? a + 1 : a;
		double q = sp - a;
		int16_t l0, r0, l1, r1;
		if (i.channels == 1)
		{
			l0 = r0 = in[a];
			l1 = r1 = in[b];
		}
		else
		{
			l0 = in[a * 2];
			r0 = in[a * 2 + 1];
			l1 = in[b * 2];
			r1 = in[b * 2 + 1];
		}
		o[x * 2] = clamp16((int32_t)(l0 + (l1 - l0) * q));
		o[x * 2 + 1] = clamp16((int32_t)(r0 + (r1 - r0) * q));
	}
	free(in);
	*out = o;
	*outframes = of;
	return 0;
}
static void stream_close_native(Stream *s)
{
	if (!s)
		return;

	if (s->type == STREAM_OGG && s->ogg_open)
	{
		ov_clear(&s->ogg);
		s->ogg_open = 0;
		s->f = NULL;
	}
	else if (s->f)
	{
		fclose(s->f);
		s->f = NULL;
	}
}

static int stream_fill_wav(Stream *s, ogg_int64_t frame)
{
	if (!s->f || frame < 0 || frame >= s->frames)
		return -1;

	uint32_t want = (uint32_t)(s->frames - frame);
	if (want > STREAM_CACHE)
		want = STREAM_CACHE;

	if (fseek(
			s->f,
			s->info.data_off + (long)((uint64_t)frame * s->channels * 2),
			SEEK_SET) != 0)
		return -1;

	if (s->channels == 2)
	{
		size_t got = fread(
			s->cache,
			sizeof(int16_t),
			(size_t)want * 2,
			s->f
		);
		s->cache_frames = (uint32_t)(got / 2);
	}
	else
	{
		size_t got = fread(
			s->cache,
			sizeof(int16_t),
			want,
			s->f
		);

		s->cache_frames = (uint32_t)got;

		/* Expand backwards so mono input can be converted in-place. */
		for (uint32_t i = s->cache_frames; i > 0; i--)
		{
			int16_t v = s->cache[i - 1];
			s->cache[(i - 1) * 2] = v;
			s->cache[(i - 1) * 2 + 1] = v;
		}
	}

	s->cache_start = frame;
	return s->cache_frames ? 0 : -1;
}

static int stream_fill_ogg(Stream *s, ogg_int64_t frame)
{
	if (!s->ogg_open || frame < 0 || frame >= s->frames)
		return -1;

	uint32_t want = (uint32_t)(s->frames - frame);
	if (want > STREAM_CACHE)
		want = STREAM_CACHE;

	if (s->ogg_decode_frame != frame)
	{
		if (ov_pcm_seek(&s->ogg, frame) < 0)
			return -1;
		s->ogg_decode_frame = frame;
	}

	s->cache_start = frame;
	s->cache_frames = 0;

	int bitstream = 0;

	while (s->cache_frames < want)
	{
		uint32_t remaining = want - s->cache_frames;
		int bytes_requested = (int)((size_t)remaining * s->channels * sizeof(int16_t));

		if (bytes_requested > (int)sizeof(ogg_decode_buffer))
			bytes_requested = (int)sizeof(ogg_decode_buffer);

		long bytes = ov_read(
			&s->ogg,
			(char *)ogg_decode_buffer,
			bytes_requested,
			&bitstream
		);

		if (bytes == 0)
			break;

		if (bytes < 0)
		{
			if (bytes == OV_HOLE)
				continue;
			return -1;
		}

		uint32_t sample_count = (uint32_t)bytes / sizeof(int16_t);
		uint32_t decoded_frames = sample_count / s->channels;

		if (!decoded_frames)
			continue;

		if (decoded_frames > remaining)
			decoded_frames = remaining;

		uint32_t base = s->cache_frames;

		if (s->channels == 1)
		{
			for (uint32_t i = 0; i < decoded_frames; i++)
			{
				int16_t v = ogg_decode_buffer[i];
				s->cache[(base + i) * 2] = v;
				s->cache[(base + i) * 2 + 1] = v;
			}
		}
		else
		{
			memcpy(
				&s->cache[base * 2],
				ogg_decode_buffer,
				(size_t)decoded_frames * 2 * sizeof(int16_t)
			);
		}

		s->cache_frames += decoded_frames;
		s->ogg_decode_frame += decoded_frames;
	}

	return s->cache_frames ? 0 : -1;
}

static int stream_fill(Stream *s, ogg_int64_t frame)
{
	if (frame < 0 || frame >= s->frames)
		return -1;

	if (
		s->cache_frames &&
		frame >= s->cache_start &&
		frame < s->cache_start + s->cache_frames
	)
		return 0;

	if (s->type == STREAM_OGG)
		return stream_fill_ogg(s, frame);

	if (s->type == STREAM_WAV)
		return stream_fill_wav(s, frame);

	return -1;
}

static int stream_sample(Stream *s, ogg_int64_t frame, int16_t *l, int16_t *r)
{
	if (stream_fill(s, frame) < 0)
		return -1;

	ogg_int64_t offset = frame - s->cache_start;
	if (offset < 0 || offset >= s->cache_frames)
		return -1;

	uint32_t idx = (uint32_t)offset;
	*l = s->cache[idx * 2];
	*r = s->cache[idx * 2 + 1];

	return 0;
}

static void mix_stream(Stream *s, int32_t *L, int32_t *R, float mv)
{
	if (!s->used || !s->playing || s->paused)
		return;

	if (s->pos >= (double)s->frames)
	{
		if (s->loop)
		{
			s->pos = 0;
			s->cache_frames = 0;
			if (s->type == STREAM_OGG)
				s->ogg_decode_frame = -1;
		}
		else
		{
			s->playing = 0;
			return;
		}
	}

	ogg_int64_t a = (ogg_int64_t)s->pos;
	ogg_int64_t b = a + 1 < s->frames ? a + 1 : a;

	int16_t l0, r0, l1, r1;

	if (stream_sample(s, a, &l0, &r0) < 0)
	{
		s->playing = 0;
		return;
	}

	if (stream_sample(s, b, &l1, &r1) < 0)
	{
		l1 = l0;
		r1 = r0;
	}

	double q = s->pos - (double)a;
	float g = s->vol * mv;

	*L += (int32_t)((l0 + (l1 - l0) * q) * g);
	*R += (int32_t)((r0 + (r1 - r0) * q) * g);

	s->pos += (double)s->rate / OUT_RATE;
}

static void *mixthread(void *x)
{
	(void)x;
	int16_t b[GRAIN * 2];
	while (running)
	{
		memset(b, 0, sizeof(b));
		pthread_mutex_lock(&mutex);
		float mv = master;
		for (int f = 0; f < GRAIN; f++)
		{
			int32_t L = 0, R = 0;
			for (int v = 0; v < MAX_VOICES; v++)
			{
				Voice *q = &voices[v];
				if (!q->active)
					continue;
				if (q->clip < 0 || q->clip >= MAX_CLIPS || !clips[q->clip].used)
				{
					q->active = 0;
					continue;
				}
				Clip *c = &clips[q->clip];
				if (q->pos >= c->frames)
				{
					if (q->loop && c->frames)
						q->pos = 0;
					else
					{
						q->active = 0;
						continue;
					}
				}
				size_t si = (size_t)q->pos * 2;
				float g = q->vol * mv;
				L += (int32_t)(c->samples[si] * g);
				R += (int32_t)(c->samples[si + 1] * g);
				q->pos++;
			}
			for (int s = 0; s < MAX_STREAMS; s++)
				mix_stream(&streams[s], &L, &R, mv);
			b[f * 2] = clamp16(L);
			b[f * 2 + 1] = clamp16(R);
		}
		pthread_mutex_unlock(&mutex);
		if (sceAudioOutOutput(port, b) < 0)
		{
			running = 0;
			break;
		}
	}
	return NULL;
}
static int ensure(void)
{
	if (initialized)
		return 0;
	if (!mutex_init)
	{
		if (pthread_mutex_init(&mutex, NULL))
			return -1;
		mutex_init = 1;
	}
	port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, GRAIN, OUT_RATE, SCE_AUDIO_OUT_MODE_STEREO);
	if (port < 0)
		return port;
	memset(clips, 0, sizeof(clips));
	memset(voices, 0, sizeof(voices));
	memset(streams, 0, sizeof(streams));
	for (int i = 0; i < MAX_VOICES; i++)
		voices[i].clip = -1;
	master = 1;
	running = 1;
	if (pthread_create(&thread, NULL, mixthread, NULL))
	{
		sceAudioOutReleasePort(port);
		port = -1;
		running = 0;
		return -2;
	}
	initialized = 1;
	return 0;
}
static int freeclip(void)
{
	for (int i = 0; i < MAX_CLIPS; i++)
		if (!clips[i].used)
			return i;
	return -1;
}
static int freevoice(void)
{
	for (int i = 0; i < MAX_VOICES; i++)
		if (!voices[i].active)
			return i;
	return -1;
}
static int freestream(void)
{
	for (int i = 0; i < MAX_STREAMS; i++)
		if (!streams[i].used)
			return i;
	return -1;
}
static JSValue initerr(JSContext *c, int r) { return JS_ThrowInternalError(c, "Unable to initialize PS Vita audio: 0x%08X", (unsigned)r); }
static int getid(JSContext *c, JSValueConst v, int max, const char *n)
{
	int32_t id;
	if (JS_ToInt32(c, &id, v))
		return -2;
	if (id < 0 || id >= max)
	{
		JS_ThrowRangeError(c, "Invalid %s id", n);
		return -2;
	}
	return id;
}
static int getvol(JSContext *c, JSValueConst v, float *out)
{
	double d;
	if (JS_ToFloat64(c, &d, v))
		return -1;
	*out = clampv((float)d);
	return 0;
}
static JSValue loadwav(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "load_wav(path) expects one argument");
	int ir = ensure();
	if (ir < 0)
		return initerr(c, ir);
	const char *p = JS_ToCString(c, v[0]);
	if (!p)
		return JS_EXCEPTION;
	int16_t *s = NULL;
	uint32_t f = 0;
	int r = load_clip(p, &s, &f);
	JS_FreeCString(c, p);
	if (r < 0)
		return JS_ThrowInternalError(c, "Unable to load WAV (error %d)", r);
	pthread_mutex_lock(&mutex);
	int id = freeclip();
	if (id >= 0)
	{
		clips[id].used = 1;
		clips[id].samples = s;
		clips[id].frames = f;
	}
	pthread_mutex_unlock(&mutex);
	if (id < 0)
	{
		free(s);
		return JS_ThrowInternalError(c, "Audio clip limit reached");
	}
	return JS_NewInt32(c, id);
}
static JSValue freewav(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "free(clipId) expects one argument");
	int id = getid(c, v[0], MAX_CLIPS, "clip");
	if (id < 0)
		return JS_EXCEPTION;
	if (!initialized)
		return JS_UNDEFINED;
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < MAX_VOICES; i++)
		if (voices[i].active && voices[i].clip == id)
			voices[i].active = 0;
	if (clips[id].used)
		free(clips[id].samples);
	memset(&clips[id], 0, sizeof(clips[id]));
	pthread_mutex_unlock(&mutex);
	return JS_UNDEFINED;
}
static JSValue play(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a < 1 || a > 3)
		return JS_ThrowSyntaxError(c, "play(clipId[,volume[,loop]]) expects 1-3 arguments");
	int ir = ensure();
	if (ir < 0)
		return initerr(c, ir);
	int id = getid(c, v[0], MAX_CLIPS, "clip");
	if (id < 0)
		return JS_EXCEPTION;
	float vol = 1;
	if (a >= 2 && getvol(c, v[1], &vol) < 0)
		return JS_EXCEPTION;
	int loop = a >= 3 ? JS_ToBool(c, v[2]) : 0;
	if (loop < 0)
		return JS_EXCEPTION;
	pthread_mutex_lock(&mutex);
	if (!clips[id].used)
	{
		pthread_mutex_unlock(&mutex);
		return JS_ThrowRangeError(c, "Invalid or freed clip id");
	}
	int q = freevoice();
	if (q >= 0)
	{
		voices[q] = (Voice){1, id, 0, vol, loop ? 1 : 0};
	}
	pthread_mutex_unlock(&mutex);
	return JS_NewInt32(c, q);
}
static JSValue stop(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "stop(voiceId) expects one argument");
	int id = getid(c, v[0], MAX_VOICES, "voice");
	if (id < 0)
		return JS_EXCEPTION;
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		voices[id].active = 0;
		voices[id].clip = -1;
		voices[id].pos = 0;
		pthread_mutex_unlock(&mutex);
	}
	return JS_UNDEFINED;
}
static JSValue stopall(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	(void)v;
	if (a)
		return JS_ThrowSyntaxError(c, "stop_all() expects no arguments");
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		for (int i = 0; i < MAX_VOICES; i++)
			voices[i].active = 0;
		for (int i = 0; i < MAX_STREAMS; i++)
			streams[i].playing = 0;
		pthread_mutex_unlock(&mutex);
	}
	return JS_UNDEFINED;
}
static JSValue isplaying(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "is_playing(voiceId) expects one argument");
	int id = getid(c, v[0], MAX_VOICES, "voice");
	if (id < 0)
		return JS_FALSE;
	int b = 0;
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		b = voices[id].active;
		pthread_mutex_unlock(&mutex);
	}
	return JS_NewBool(c, b);
}
static JSValue voicevol(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(c, "set_voice_volume(voiceId,volume) expects two arguments");
	int id = getid(c, v[0], MAX_VOICES, "voice");
	if (id < 0)
		return JS_EXCEPTION;
	float q;
	if (getvol(c, v[1], &q) < 0)
		return JS_EXCEPTION;
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		voices[id].vol = q;
		pthread_mutex_unlock(&mutex);
	}
	return JS_UNDEFINED;
}
static JSValue masterm(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "set_master_volume(volume) expects one argument");
	float q;
	if (getvol(c, v[0], &q) < 0)
		return JS_EXCEPTION;
	int r = ensure();
	if (r < 0)
		return initerr(c, r);
	pthread_mutex_lock(&mutex);
	master = q;
	pthread_mutex_unlock(&mutex);
	return JS_UNDEFINED;
}
static JSValue getmaster(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	(void)v;
	if (a)
		return JS_ThrowSyntaxError(c, "get_master_volume() expects no arguments");
	float q = master;
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		q = master;
		pthread_mutex_unlock(&mutex);
	}
	return JS_NewFloat64(c, q);
}
static JSValue openstream(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;

	if (a != 1)
		return JS_ThrowSyntaxError(c, "open_stream(path) expects one argument");

	int ir = ensure();
	if (ir < 0)
		return initerr(c, ir);

	const char *p = JS_ToCString(c, v[0]);
	if (!p)
		return JS_EXCEPTION;

	FILE *f = fopen(p, "rb");

	if (!f)
	{
		JS_FreeCString(c, p);
		return JS_ThrowInternalError(c, "Unable to open audio stream");
	}

	uint8_t magic[4] = {0};

	if (fread(magic, 1, sizeof(magic), f) != sizeof(magic))
	{
		fclose(f);
		JS_FreeCString(c, p);
		return JS_ThrowInternalError(c, "Unable to read audio stream header");
	}

	rewind(f);

	Stream candidate;
	memset(&candidate, 0, sizeof(candidate));

	candidate.used = 1;
	candidate.vol = 1.0f;
	candidate.ogg_decode_frame = -1;

	if (!memcmp(magic, "OggS", 4))
	{
		int r = ov_open(f, &candidate.ogg, NULL, 0);

		if (r < 0)
		{
			fclose(f);
			JS_FreeCString(c, p);
			return JS_ThrowInternalError(c, "Unable to open OGG/Vorbis stream (error %d)", r);
		}

		candidate.type = STREAM_OGG;
		candidate.ogg_open = 1;
		candidate.f = NULL;

		vorbis_info *vi = ov_info(&candidate.ogg, -1);

		if (
			!vi ||
			(vi->channels != 1 && vi->channels != 2) ||
			vi->rate <= 0
		)
		{
			ov_clear(&candidate.ogg);
			JS_FreeCString(c, p);
			return JS_ThrowInternalError(
				c,
				"Unsupported OGG/Vorbis stream; only mono/stereo is supported"
			);
		}

		ogg_int64_t total = ov_pcm_total(&candidate.ogg, -1);

		if (total <= 0)
		{
			ov_clear(&candidate.ogg);
			JS_FreeCString(c, p);
			return JS_ThrowInternalError(c, "Unable to determine OGG/Vorbis stream length");
		}

		candidate.rate = (uint32_t)vi->rate;
		candidate.channels = (uint16_t)vi->channels;
		candidate.frames = total;
	}
	else if (!memcmp(magic, "RIFF", 4))
	{
		WavInfo i;
		int r = parse_wav(f, &i, 0);

		if (r < 0)
		{
			fclose(f);
			JS_FreeCString(c, p);
			return JS_ThrowInternalError(c, "Unsupported WAV stream (error %d)", r);
		}

		candidate.type = STREAM_WAV;
		candidate.f = f;
		candidate.info = i;
		candidate.rate = i.rate;
		candidate.channels = i.channels;
		candidate.frames = (ogg_int64_t)i.data_size / (i.channels * 2);
	}
	else
	{
		fclose(f);
		JS_FreeCString(c, p);
		return JS_ThrowInternalError(
			c,
			"Unsupported stream format; expected PCM WAV or OGG/Vorbis"
		);
	}

	JS_FreeCString(c, p);

	pthread_mutex_lock(&mutex);
	int id = freestream();

	if (id >= 0)
		streams[id] = candidate;

	pthread_mutex_unlock(&mutex);

	if (id < 0)
	{
		stream_close_native(&candidate);
		return JS_ThrowInternalError(c, "Audio stream limit reached (%d)", MAX_STREAMS);
	}

	return JS_NewInt32(c, id);
}

static JSValue closestream(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "close_stream(streamId) expects one argument");
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_EXCEPTION;
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		stream_close_native(&streams[id]);
		memset(&streams[id], 0, sizeof(streams[id]));
		pthread_mutex_unlock(&mutex);
	}
	return JS_UNDEFINED;
}
static JSValue playstream(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a < 1 || a > 3)
		return JS_ThrowSyntaxError(c, "play_stream(streamId[,volume[,loop]]) expects 1-3 arguments");
	if (!initialized)
		return JS_ThrowInternalError(c, "Audio is not initialized; open a stream first");
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_EXCEPTION;
	float vol = 1;
	if (a >= 2 && getvol(c, v[1], &vol) < 0)
		return JS_EXCEPTION;
	int loop = a >= 3 ? JS_ToBool(c, v[2]) : 0;
	if (loop < 0)
		return JS_EXCEPTION;
	pthread_mutex_lock(&mutex);
	if (!streams[id].used)
	{
		pthread_mutex_unlock(&mutex);
		return JS_ThrowRangeError(c, "Invalid stream id");
	}
	streams[id].playing = 1;
	streams[id].paused = 0;
	streams[id].loop = loop ? 1 : 0;
	streams[id].vol = vol;
	pthread_mutex_unlock(&mutex);
	return JS_UNDEFINED;
}
static JSValue streamcmd(JSContext *c, int a, JSValueConst *v, int cmd)
{
	if (a != 1)
		return JS_ThrowSyntaxError(c, "stream command expects one streamId");
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_EXCEPTION;
	if (initialized)
	{
		pthread_mutex_lock(&mutex);
		Stream *s = &streams[id];
		if (cmd == 0)
			s->paused = 1;
		else if (cmd == 1)
			s->paused = 0;
		else if (cmd == 2)
		{
			s->playing = 0;
			s->paused = 0;
			s->pos = 0;
			s->cache_frames = 0;
			if (s->type == STREAM_OGG)
				s->ogg_decode_frame = -1;
		}
		pthread_mutex_unlock(&mutex);
	}
	return JS_UNDEFINED;
}
static JSValue pausestream(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return streamcmd(c, a, v, 0);
}
static JSValue resumestream(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return streamcmd(c, a, v, 1);
}
static JSValue stopstream(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	return streamcmd(c, a, v, 2);
}
static JSValue streamplaying(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "is_stream_playing(streamId) expects one argument");
	if (!initialized)
		return JS_FALSE;
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_FALSE;
	pthread_mutex_lock(&mutex);
	int b = streams[id].used && streams[id].playing && !streams[id].paused;
	pthread_mutex_unlock(&mutex);
	return JS_NewBool(c, b);
}
static JSValue streamvol(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(c, "set_stream_volume(streamId,volume) expects two arguments");
	if (!initialized)
		return JS_ThrowInternalError(c, "Audio is not initialized; open a stream first");
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_EXCEPTION;
	float q;
	if (getvol(c, v[1], &q) < 0)
		return JS_EXCEPTION;
	pthread_mutex_lock(&mutex);
	streams[id].vol = q;
	pthread_mutex_unlock(&mutex);
	return JS_UNDEFINED;
}
static JSValue streamseek(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	if (a != 2)
		return JS_ThrowSyntaxError(c, "seek_stream(streamId,seconds) expects two arguments");
	if (!initialized)
		return JS_ThrowInternalError(c, "Audio is not initialized; open a stream first");
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_EXCEPTION;
	double sec;
	if (JS_ToFloat64(c, &sec, v[1]))
		return JS_EXCEPTION;
	if (sec < 0)
		sec = 0;
	pthread_mutex_lock(&mutex);
	Stream *s = &streams[id];
	double p = sec * s->rate;
	if (p > (double)s->frames)
		p = (double)s->frames;
	s->pos = p;
	s->cache_frames = 0;
	if (s->type == STREAM_OGG)
		s->ogg_decode_frame = -1;
	pthread_mutex_unlock(&mutex);
	return JS_UNDEFINED;
}
static JSValue streamtime(JSContext *c, JSValueConst t, int a, JSValueConst *v, int duration)
{
	(void)t;
	if (a != 1)
		return JS_ThrowSyntaxError(c, "stream getter expects one streamId");
	if (!initialized)
		return JS_NewFloat64(c, 0);
	int id = getid(c, v[0], MAX_STREAMS, "stream");
	if (id < 0)
		return JS_EXCEPTION;
	pthread_mutex_lock(&mutex);
	Stream *s = &streams[id];
	double q = s->rate
		? (duration ? (double)s->frames / s->rate : s->pos / s->rate)
		: 0;
	pthread_mutex_unlock(&mutex);
	return JS_NewFloat64(c, q);
}
static JSValue streampos(JSContext *c, JSValueConst t, int a, JSValueConst *v) { return streamtime(c, t, a, v, 0); }
static JSValue streamdur(JSContext *c, JSValueConst t, int a, JSValueConst *v) { return streamtime(c, t, a, v, 1); }
static void shutdown(void)
{
	if (!initialized)
		return;
	running = 0;
	pthread_join(thread, NULL);
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < MAX_CLIPS; i++)
		if (clips[i].used)
			free(clips[i].samples);
	for (int i = 0; i < MAX_STREAMS; i++)
		if (streams[i].used)
			stream_close_native(&streams[i]);
	memset(clips, 0, sizeof(clips));
	memset(voices, 0, sizeof(voices));
	memset(streams, 0, sizeof(streams));
	pthread_mutex_unlock(&mutex);
	if (port >= 0)
		sceAudioOutReleasePort(port);
	port = -1;
	initialized = 0;
	if (mutex_init)
	{
		pthread_mutex_destroy(&mutex);
		mutex_init = 0;
	}
}
static JSValue term(JSContext *c, JSValueConst t, int a, JSValueConst *v)
{
	(void)t;
	(void)v;
	if (a)
		return JS_ThrowSyntaxError(c, "term() expects no arguments");
	shutdown();
	return JS_UNDEFINED;
}
static const JSCFunctionListEntry funcs[] = {
	JS_CFUNC_DEF("load_wav", 1, loadwav), 
	JS_CFUNC_DEF("free", 1, freewav),
	JS_CFUNC_DEF("play", 3, play),
	JS_CFUNC_DEF("stop", 1, stop),
	JS_CFUNC_DEF("stop_all", 0, stopall),
	JS_CFUNC_DEF("is_playing", 1, isplaying),
	JS_CFUNC_DEF("set_voice_volume", 2, voicevol),
	JS_CFUNC_DEF("set_master_volume", 1, masterm),
	JS_CFUNC_DEF("get_master_volume", 0, getmaster),
	JS_CFUNC_DEF("open_stream", 1, openstream),
	JS_CFUNC_DEF("close_stream", 1, closestream),
	JS_CFUNC_DEF("play_stream", 3, playstream),
	JS_CFUNC_DEF("pause_stream", 1, pausestream),
	JS_CFUNC_DEF("resume_stream", 1, resumestream),
	JS_CFUNC_DEF("stop_stream", 1, stopstream),
	JS_CFUNC_DEF("is_stream_playing", 1, streamplaying),
	JS_CFUNC_DEF("set_stream_volume", 2, streamvol),
	JS_CFUNC_DEF("seek_stream", 2, streamseek),
	JS_CFUNC_DEF("get_stream_position", 1, streampos),
	JS_CFUNC_DEF("get_stream_duration", 1, streamdur),
	JS_CFUNC_DEF("term", 0, term)
};
static int initmod(JSContext *c, JSModuleDef *m) { return JS_SetModuleExportList(c, m, funcs, countof(funcs)); }
JSModuleDef *vitajs_audio_init(JSContext *c) { return vitajs_push_module(c, initmod, funcs, countof(funcs), "Audio"); }
