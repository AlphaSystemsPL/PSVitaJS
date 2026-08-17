# Audio API

`Audio` has two complementary playback paths:

1. **loaded WAV clips** for short sound effects and overlapping mixer voices,
2. **streamed WAV / OGG Vorbis audio** for music and longer assets without loading the complete file into RAM.

The mixer outputs 48 kHz stereo.

## WAV sound effects

`Audio.load_wav()` accepts RIFF/WAVE PCM:

- signed 16-bit PCM
- mono or stereo

```js
const sound = Audio.load_wav(
    "app0:/assets/jump.wav"
);

const voice = Audio.play(sound);
```

Volume range:

```text
0.0 = silent
1.0 = full volume
```

Loop a loaded clip:

```js
const voice = Audio.play(
    sound,
    0.4,
    true
);
```

Voice controls:

```js
Audio.is_playing(voice);
Audio.set_voice_volume(voice, 0.2);
Audio.stop(voice);
Audio.stop_all();
```

Free the loaded clip:

```js
Audio.free(sound);
```

Freeing a clip stops voices that use it.

## Streamed music / long audio

`Audio.open_stream()` auto-detects:

- PCM 16-bit WAV (`RIFF`)
- OGG Vorbis (`OggS`)

Example with OGG:

```js
const music = Audio.open_stream(
    "app0:/assets/music.ogg"
);

Audio.play_stream(
    music,
    0.7,
    true
);
```

The complete OGG file is not loaded into RAM. OGG decoding uses Tremor and feeds the existing mixer through a small PCM cache.

WAV streams are also read incrementally rather than loading the complete asset as a clip.

## Stream controls

Pause and resume:

```js
Audio.pause_stream(music);
Audio.resume_stream(music);
```

Change stream volume:

```js
Audio.set_stream_volume(
    music,
    0.5
);
```

Seek in seconds:

```js
Audio.seek_stream(
    music,
    30
);
```

Read current position and duration:

```js
console.log(
    Audio.get_stream_position(music),
    Audio.get_stream_duration(music)
);
```

Check playback:

```js
if (Audio.is_stream_playing(music)) {
    console.log("music playing");
}
```

Stop without closing:

```js
Audio.stop_stream(music);
```

Close the stream when it is no longer needed:

```js
Audio.close_stream(music);
```

Do not use a stream handle after `close_stream()`.

## Master volume

The master volume affects mixer output:

```js
Audio.set_master_volume(0.8);

console.log(
    Audio.get_master_volume()
);
```

## Shutdown

```js
Audio.term();
```

`Audio.term()` shuts down the audio thread and frees loaded clips and streams. Use it when the application is permanently shutting down the audio subsystem.

## OGG/Vorbis build dependencies

OGG streaming uses Tremor:

```c
#include <tremor/ivorbisfile.h>
```

Install the VDPM packages:

```bash
./vdpm libogg libtremor
```

Link them in this order:

```cmake
vorbisidec
ogg
```

`vorbisidec` should appear before `ogg` because Tremor depends on libogg.

## Suggested OGG encoding

Quality-based encoding:

```bash
ffmpeg -i input.wav     -c:a libvorbis     -q:a 4     music.ogg
```

or around 128 kbit/s:

```bash
ffmpeg -i input.wav     -c:a libvorbis     -b:a 128k     music.ogg
```

For Vita game music, roughly 96–128 kbit/s is a reasonable starting point.

## Preparing WAV files

```bash
ffmpeg -i input.mp3     -ar 48000     -ac 2     -c:a pcm_s16le     output.wav
```
