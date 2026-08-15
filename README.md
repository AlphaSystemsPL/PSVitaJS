# VitaJS

**VitaJS** is an experimental JavaScript runtime for building games and applications for the **PlayStation Vita**.

It embeds **QuickJS** and exposes native PS Vita / vita2d functionality to JavaScript through modules such as `Screen`, `Pads`, `Font`, `Audio`, `System`, and `Net`.

The goal is simple: keep the low-level Vita integration in C, while writing most game and application logic in JavaScript.

> **Status:** work in progress. VitaJS is still experimental and APIs may change. Test on real hardware — Vita3K can behave differently from a physical PS Vita.

---

## Features

Current VitaJS functionality includes:

- JavaScript execution through QuickJS
- 2D rendering with vita2d
- PNG, JPG and BMP textures
- PS Vita controls and analog sticks
- TTF/OTF font rendering
- WAV sound effects with a native audio mixer
- synchronous HTTP requests
- filesystem access through `System`, `std` and `os`
- timers through QuickJS `os`
- VS Code IntelliSense through `vitajs.d.ts`

### Available global modules

| Module | Purpose |
| --- | --- |
| `Screen` | Rendering, textures and drawing |
| `Pads` | Buttons and analog sticks |
| `Font` | TTF/OTF font loading and text rendering |
| `Audio` | WAV loading, playback and mixing |
| `Net` | HTTP GET/POST/request |
| `System` | Vita filesystem and system helpers |
| `std` | QuickJS standard library |
| `os` | QuickJS timers, filesystem and OS helpers |

---

## Runtime architecture

VitaJS embeds QuickJS inside a native PS Vita application.

```text
JavaScript game / app
        │
        ▼
     QuickJS
        │
        ├── Screen ──► vita2d / SceGxm
        ├── Pads   ──► SceCtrl
        ├── Font   ──► vita2d / FreeType
        ├── Audio  ──► SceAudioOut
        ├── Net    ──► SceHttp / SceNet
        └── System ──► Vita filesystem APIs
```

The JavaScript entry point is:

```text
app0:/assets/main.js
```

---

## Requirements

You need a working **VitaSDK** installation.

Set the `VITASDK` environment variable and add its binaries to `PATH`:

```bash
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
```

Install the libraries used by VitaJS:

```bash
vdpm install libvita2d bzip2
```

The current build also uses VitaSDK system libraries for graphics, controls, audio and networking.

---

## Building

Clone the repository:

```bash
git clone https://github.com/AlphaSystemsPL/vitajs.git
cd vitajs
```

Configure and build:

```bash
rm -rf build
cmake -S . -B build
cmake --build build -j 8
```

The resulting VPK is:

```text
build/vitajs.vpk
```

Install the VPK on a PS Vita or run it in Vita3K.

---

## Test assets

A simple development setup can use:

```text
src/
└── assets/
    ├── main.js
    ├── test1.png
    ├── segoeui.ttf
    ├── audio.wav
    └── vitajs.d.ts
```

The runtime assets must also be included in `vita_create_vpk()`:

```cmake
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}

    FILE
        src/assets/main.js assets/main.js
        src/assets/test1.png assets/test1.png
        src/assets/segoeui.ttf assets/segoeui.ttf
        src/assets/audio.wav assets/audio.wav

        sce_sys/icon0.png sce_sys/icon0.png
        sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
        sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
        sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
```

`vitajs.d.ts` is only used by your editor and does **not** need to be packaged inside the VPK.

---

# Quick start

This example loads the current test texture, font and sound effect.

```js
const texture = Screen.load_png_file("app0:/assets/test1.png");
const font = Font.load_font_file("app0:/assets/segoeui.ttf");
const sound = Audio.load_wav("app0:/assets/audio.wav");

const WHITE = 0xffffffff;

let x = 200;
let y = 200;
let crossPressed = false;

const loop = os.setInterval(() => {
    // Input
    if (Pads.check(Pads.LEFT)) {
        x -= 2;
    }

    if (Pads.check(Pads.RIGHT)) {
        x += 2;
    }

    if (Pads.check(Pads.UP)) {
        y -= 2;
    }

    if (Pads.check(Pads.DOWN)) {
        y += 2;
    }

    // Play the sound once when CROSS is pressed.
    const cross = Pads.check(Pads.CROSS);

    if (cross && !crossPressed) {
        Audio.play(sound);
    }

    crossPressed = cross;

    // Render
    Screen.start_drawing();
    Screen.clear(15, 18, 24, 255);

    Screen.draw_texture(texture, x, y);

    Font.font_draw_text(
        font,
        30,
        50,
        WHITE,
        28,
        "VitaJS"
    );

    Screen.end_drawing();
    Screen.swap_buffers();

    if (Pads.check(Pads.START)) {
        os.clearInterval(loop);

        Audio.free(sound);
        Font.free_font(font);
        Screen.free_texture(texture);
    }
}, 0);
```

---

# Screen

`Screen` provides the main vita2d rendering API.

## Load and draw a texture

```js
const texture = Screen.load_png_file(
    "app0:/assets/test1.png"
);

Screen.start_drawing();

Screen.clear(0, 0, 0, 255);

Screen.draw_texture(
    texture,
    100,
    100
);

Screen.end_drawing();
Screen.swap_buffers();
```

## Basic shapes

```js
function rgba(r, g, b, a = 255) {
    return (
        ((a & 255) << 24) |
        ((b & 255) << 16) |
        ((g & 255) << 8) |
        (r & 255)
    ) >>> 0;
}

Screen.start_drawing();

Screen.clear(20, 20, 25, 255);

Screen.draw_rectangle(
    40,
    40,
    200,
    100,
    rgba(60, 130, 240)
);

Screen.draw_line(
    40,
    180,
    300,
    180,
    rgba(255, 255, 255)
);

Screen.end_drawing();
Screen.swap_buffers();
```

---

# Pads

Check Vita buttons using `Pads.check()`:

```js
if (Pads.check(Pads.CROSS)) {
    console.log("CROSS");
}

if (Pads.check(Pads.START)) {
    console.log("START");
}
```

Available button constants include:

```js
Pads.UP
Pads.DOWN
Pads.LEFT
Pads.RIGHT

Pads.CROSS
Pads.CIRCLE
Pads.SQUARE
Pads.TRIANGLE

Pads.L1
Pads.R1

Pads.START
Pads.SELECT
```

## Analog sticks

```js
const pad = Pads.analog();

console.log(
    pad.lx,
    pad.ly,
    pad.rx,
    pad.ry
);
```

---

# Font

Fonts are loaded through vita2d.

```js
const font = Font.load_font_file(
    "app0:/assets/segoeui.ttf"
);
```

## Draw text

```js
const WHITE = 0xffffffff;

Font.font_draw_text(
    font,
    40,
    80,
    WHITE,
    32,
    "Hello PS Vita!"
);
```

Text must be drawn between:

```js
Screen.start_drawing();
```

and:

```js
Screen.end_drawing();
```

For example:

```js
Screen.start_drawing();

Screen.clear(10, 10, 15, 255);

Font.font_draw_text(
    font,
    40,
    80,
    0xffffffff,
    32,
    "VitaJS"
);

Screen.end_drawing();
Screen.swap_buffers();
```

## Measure text

```js
const width = Font.font_text_width(
    font,
    32,
    "VitaJS"
);

const height = Font.font_text_height(
    font,
    32,
    "VitaJS"
);

const dimensions = Font.font_text_dimensions(
    font,
    32,
    "VitaJS"
);

console.log(
    dimensions.width,
    dimensions.height
);
```

## Free a font

```js
Font.free_font(font);
```

Do not use the font object after it has been freed.

---

# Audio

The current audio module is designed primarily for game sound effects.

Supported input:

- RIFF/WAVE
- PCM
- 16-bit
- mono or stereo

Audio is internally converted to **48 kHz stereo** for the Vita audio output.

## Load and play

```js
const sound = Audio.load_wav(
    "app0:/assets/audio.wav"
);

Audio.play(sound);
```

## Volume

```js
Audio.play(
    sound,
    0.5
);
```

Volume uses the range:

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

Stop it later:

```js
Audio.stop(voice);
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

## Free audio

```js
Audio.free(sound);
```

The current mixer supports multiple simultaneous voices, allowing sound effects to overlap.

For predictable source assets, WAV files can be prepared with FFmpeg:

```bash
ffmpeg -i input.mp3 \
    -ar 48000 \
    -ac 2 \
    -c:a pcm_s16le \
    output.wav
```

---

# Net

VitaJS currently exposes a **synchronous** native HTTP API.

> `Net.get()`, `Net.post()` and `Net.request()` block the JavaScript/game loop until the HTTP request completes. Avoid calling them every frame.

## GET request

```js
const response = Net.get(
    "http://192.168.1.20:3000/test.json"
);

console.log(response.status);
console.log(response.ok);
console.log(response.body);
```

Parse JSON normally:

```js
if (response.ok) {
    const data = JSON.parse(response.body);

    console.log(data);
}
```

The response object has the form:

```js
{
    status: 200,
    ok: true,
    body: "..."
}
```

## POST request

```js
const response = Net.post(
    "http://192.168.1.20:3000/api",
    JSON.stringify({
        score: 12500,
        level: 4
    })
);
```

`Net.post()` uses:

```text
application/json
```

as the default content type.

A custom content type can be passed as the third argument:

```js
Net.post(
    "http://192.168.1.20:3000/api",
    "hello=world",
    "application/x-www-form-urlencoded"
);
```

## Generic request

```js
const response = Net.request(
    "PUT",
    "http://192.168.1.20:3000/player",
    JSON.stringify({
        hp: 90
    }),
    "application/json"
);
```

Supported methods:

```text
GET
POST
PUT
DELETE
HEAD
OPTIONS
```

## Network information

```js
console.log(
    Net.is_connected()
);

console.log(
    Net.get_ip()
);
```

## Local development server

The Vita cannot use your computer's `localhost`.

Start a server that listens on the LAN:

```bash
python3 -m http.server 3000 --bind 0.0.0.0
```

On macOS you can get your Wi-Fi IP with:

```bash
ipconfig getifaddr en0
```

For example:

```text
192.168.1.20
```

Then access it from Vita:

```js
Net.get(
    "http://192.168.1.20:3000/test.json"
);
```

Both devices must be reachable through the same local network.

---

# System and filesystem

VitaJS exposes both the native `System` module and QuickJS `std` / `os` helpers.

Example:

```js
if (System.doesFileExist(
    "ux0:data/VITAJS001/config.json"
)) {
    console.log("File exists");
}
```

QuickJS can also load an entire text file:

```js
const source = std.loadFile(
    "ux0:data/VITAJS001/app.js"
);
```

and execute JavaScript dynamically:

```js
if (source !== null) {
    std.evalScript(source);
}
```

This makes it possible to build loaders and development workflows where JavaScript content is stored outside the VPK.

---

# Timers and game loop

QuickJS provides timers through `os`.

```js
const loop = os.setInterval(() => {
    // update
    // render
}, 0);
```

Stop the loop:

```js
os.clearInterval(loop);
```

A simple VitaJS game usually follows this structure:

```js
const loop = os.setInterval(() => {
    // 1. input
    // 2. update game state

    Screen.start_drawing();

    // 3. render

    Screen.end_drawing();
    Screen.swap_buffers();
}, 0);
```

---

# VS Code IntelliSense

VitaJS includes a `vitajs.d.ts` declaration file for JavaScript IntelliSense.

Recommended project layout:

```text
vitajs/
├── vitajs.d.ts
├── jsconfig.json
└── src/
    └── assets/
        └── main.js
```

Example `jsconfig.json`:

```json
{
    "compilerOptions": {
        "target": "ES2020",
        "allowJs": true,
        "checkJs": false,
        "lib": [
            "ES2020"
        ]
    },
    "include": [
        "vitajs.d.ts",
        "src/**/*.js"
    ],
    "exclude": [
        "build"
    ]
}
```

Do not add the browser `DOM` library unless you specifically need it. Browser types define globals such as `Screen` and `Audio`, which conflict with the VitaJS globals.

If IntelliSense does not update in VS Code:

```text
Cmd/Ctrl + Shift + P
→ TypeScript: Restart TS Server
```

Then autocomplete should work for:

```js
Screen.
Pads.
Font.
Audio.
Net.
System.
os.
std.
```

---

# C modules

The current runtime is composed of native modules such as:

```text
src/modules/
├── screen.c
├── system.c
├── pads.c
├── font.c
├── audio.c
└── net.c
```

The corresponding modules are registered in the custom QuickJS context and exposed to JavaScript as globals.

When adding a new native module, the usual flow is:

```text
C implementation
     ↓
JS_NewCModule / vitajs_push_module
     ↓
register in JS_NewCustomContext()
     ↓
import module in bootstrap
     ↓
assign to globalThis
```

---

# Current limitations

VitaJS is still under active development.

Notable limitations currently include:

- HTTP requests are synchronous
- audio currently focuses on WAV sound effects rather than streamed music
- several original experimental VitaJS APIs are incomplete
- there is no browser DOM
- there is no browser `fetch()`
- there is no Node.js API
- some behavior can differ between Vita3K and physical hardware

When developing native modules, always validate them on a real PS Vita.

---

# Roadmap ideas

Potential future improvements include:

- Promise-based asynchronous networking
- streamed background music
- remote JavaScript/content loading
- application/content updater
- more complete texture and graphics APIs
- improved filesystem/storage API
- better error reporting
- additional examples and documentation

---

# Example files

More focused examples are available in the [`examples`](examples) directory:

```text
examples/
├── sprite.js
├── font.js
├── audio.js
├── net.js
└── complete-demo.js
```

---

# Credits

VitaJS builds on the work of the PS Vita homebrew ecosystem.

Special thanks to:

- the original VitaJS author and contributors
- VitaSDK contributors
- vita2d contributors
- QuickJS
- AthenaEnv Team
- DanielSant0s

---

## Disclaimer

VitaJS is an unofficial homebrew project and is not affiliated with or endorsed by Sony Interactive Entertainment.
