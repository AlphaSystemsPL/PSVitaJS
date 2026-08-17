# Touch API

`Touch` exposes the PS Vita front touchscreen and rear touch panel.

The module supports non-blocking polling, blocking reads, panel metadata, sampling control and touch-force reporting.

## Ports

Use the exported port constants:

```js
Touch.FRONT
Touch.BACK
```

Most methods accept one of these values. When the port argument is omitted from `peek()` or `read()`, the front panel is used.

## Poll the front touchscreen

For a normal frame loop, `Touch.front()` or `Touch.peek()` is usually the most convenient choice:

```js
const data = Touch.front();

for (const point of data.touches) {
    console.log(
        point.id,
        point.x,
        point.y,
        point.force
    );
}
```

Equivalent explicit call:

```js
const data = Touch.peek(Touch.FRONT);
```

## Poll the rear touch panel

```js
const data = Touch.back();

for (const point of data.touches) {
    console.log(point.x, point.y);
}
```

or:

```js
const data = Touch.peek(Touch.BACK);
```

## Returned touch data

`Touch.peek()`, `Touch.read()`, `Touch.front()` and `Touch.back()` return:

```js
{
    port: 0,
    timestamp: 123456,
    status: 0,
    count: 1,
    touches: [
        {
            id: 12,
            x: 950,
            y: 540,
            force: 64,
            info: 0
        }
    ]
}
```

Fields:

- `port` — front or rear touch port
- `timestamp` — native sample timestamp
- `status` — native SceTouch status value
- `count` — number of touch reports in the sample
- `touches` — array of touch points

Each touch point contains:

- `id` — touch identifier
- `x`, `y` — native panel coordinates
- `force` — pressure/force value when supported and enabled
- `info` — native touch report flags

Do not assume that raw panel coordinates are normalized. Use panel information when you need to map native coordinates explicitly.

## Blocking read

`Touch.read()` uses the blocking native read path:

```js
const data = Touch.read(Touch.FRONT);
```

For a render/game loop, prefer `peek()` / `front()` / `back()` so input polling does not intentionally wait for a sample.

## Panel information

```js
const info = Touch.getPanelInfo(Touch.FRONT);

console.log(
    info.minAaX,
    info.minAaY,
    info.maxAaX,
    info.maxAaY,
    info.minDispX,
    info.minDispY,
    info.maxDispX,
    info.maxDispY,
    info.minForce,
    info.maxForce
);
```

The returned object exposes the native active-area, display-area and force ranges reported by SceTouch.

## Sampling state

Start or stop sampling for a panel:

```js
Touch.setSampling(
    Touch.FRONT,
    true
);

console.log(
    Touch.getSampling(Touch.FRONT)
);
```

Disable it again:

```js
Touch.setSampling(
    Touch.FRONT,
    false
);
```

## Touch force

Enable force reporting:

```js
Touch.setForce(
    Touch.FRONT,
    true
);
```

Disable it:

```js
Touch.setForce(
    Touch.FRONT,
    false
);
```

Force support and behavior are hardware-facing. Validate it on a physical Vita when your application depends on it.

## Report info flag

The module also exports:

```js
Touch.INFO_HIDE_UPPER_LAYER
```

This corresponds to the native `SCE_TOUCH_REPORT_INFO_HIDE_UPPER_LAYER` report flag.

## Simple touch-in-frame example

```js
const loop = os.setInterval(() => {
    const touch = Touch.front();

    if (touch.count > 0) {
        const first = touch.touches[0];

        console.log(
            "touch:",
            first.x,
            first.y
        );
    }

    Screen.start_drawing();
    Screen.clear(15, 18, 24, 255);

    // draw application/game state

    Screen.end_drawing();
    Screen.swap_buffers();
}, 16);
```
