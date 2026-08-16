# Quick start

A VitaJS application starts from:

```text
app0:/assets/main.js
```

A small render loop can look like this:

```js
const font = Font.load_font_file("app0:/assets/segoeui.ttf");

const WHITE = 0xffffffff;

const loop = os.setInterval(() => {
    Screen.start_drawing();
    Screen.clear(15, 18, 24, 255);

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
        Font.free_font(font);
    }
}, 16);
```

A typical frame is:

```text
input
  ↓
update game state
  ↓
Screen.start_drawing()
  ↓
render
  ↓
Screen.end_drawing()
  ↓
Dialog.update() if a native dialog is active
  ↓
Screen.swap_buffers()
```

## ES modules

VitaJS supports ES modules:

```js
import { Game } from "./game.js";

Game.start();
```

```js
// game.js
export const Game = {
    start() {
        console.log("Game started");
    }
};
```

Every imported JavaScript file must be packaged in the VPK:

```cmake
FILE
    src/assets/main.js assets/main.js
    src/assets/game.js assets/game.js
```

Do not start multiple independent render loops unless that is intentional. For games, keeping one main loop and delegating to scene/module `update()` and `render()` methods is usually simpler.
