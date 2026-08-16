# Timers and game loop

VitaJS exposes QuickJS timers through the global `os` object.

## Interval

```js
const loop = os.setInterval(() => {
    // update
    // render
}, 16);
```

Stop it:

```js
os.clearInterval(loop);
```

A delay of roughly `16` ms is a convenient starting point for a 60 Hz-oriented loop.

Actual presentation timing is also affected by rendering and Vita display synchronization, so timer delay alone is not a frame-rate guarantee.

## Timeout

```js
const timer = os.setTimeout(() => {
    console.log("one shot");
}, 1000);
```

Cancel:

```js
os.clearTimeout(timer);
```

## Immediate callback

```js
const immediate = os.setImmediate(() => {
    console.log("next event-loop turn");
});
```

Cancel:

```js
os.clearImmediate(immediate);
```

## Typical game loop

```js
const loop = os.setInterval(() => {
    // 1. Input
    const start = Pads.check(Pads.START);

    // 2. Update
    if (start) {
        // application logic
    }

    // 3. Render
    Screen.start_drawing();

    Screen.clear(
        15,
        18,
        24,
        255
    );

    // draw scene

    Screen.end_drawing();

    // 4. Native dialog rendering, if used
    Dialog.update();

    // 5. Present
    Screen.swap_buffers();
}, 16);
```

## Scene architecture

For larger applications, prefer one runtime loop:

```js
import { Game } from "./game.js";

const loop = os.setInterval(() => {
    Game.update();

    Screen.start_drawing();
    Game.render();
    Screen.end_drawing();

    Dialog.update();
    Screen.swap_buffers();
}, 16);
```

Instead of starting a second timer inside every scene:

```js
// game.js
export const Game = {
    update() {
        // game state
    },

    render() {
        // draw calls
    }
};
```

This keeps input, rendering and native dialogs on one predictable frame lifecycle.

## Blocking calls

These APIs can stall the loop:

```text
Net.get / Net.post / Net.request
System.delay
os.sleep
```

Keep blocking operations out of per-frame code whenever possible.
