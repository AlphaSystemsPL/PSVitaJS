# Audio API

The current audio module is designed primarily for game sound effects.

Supported source format:

- RIFF/WAVE
- PCM
- 16-bit
- mono or stereo

Audio is converted internally to 48 kHz stereo for Vita audio output.

## Load and play

```js
const sound = Audio.load_wav(
    "app0:/assets/audio.wav"
);

const voice = Audio.play(sound);
```

## Volume

```js
Audio.play(
    sound,
    0.5
);
```

Volume range:

```text
0.0 = silent
1.0 = full volume
```

## Looping sound

```js
const voice = Audio.play(
    sound,
    0.4,
    true
);
```

Stop the voice later:

```js
Audio.stop(voice);
```

## Check a voice

```js
if (Audio.is_playing(voice)) {
    console.log("still playing");
}
```

## Voice volume

```js
Audio.set_voice_volume(
    voice,
    0.2
);
```

## Master volume

```js
Audio.set_master_volume(0.8);

console.log(
    Audio.get_master_volume()
);
```

## Stop all voices

```js
Audio.stop_all();
```

## Free audio

```js
Audio.free(sound);
```

Freeing a clip stops voices that use it.

## Shutdown

```js
Audio.term();
```

Use `term()` when the application is permanently shutting down the audio subsystem.

## Preparing WAV files

A predictable source can be generated with FFmpeg:

```bash
ffmpeg -i input.mp3 \
    -ar 48000 \
    -ac 2 \
    -c:a pcm_s16le \
    output.wav
```
