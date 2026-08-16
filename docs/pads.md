# Pads API

`Pads` exposes Vita buttons and analog state.

## Buttons

```js
if (Pads.check(Pads.CROSS)) {
    console.log("CROSS");
}

if (Pads.check(Pads.START)) {
    console.log("START");
}
```

Common constants:

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

Additional constants for L2/R2/L3/R3, controller modes and controller types are declared in `vitajs.d.ts`.

## Edge-triggered input

For actions that should happen once per press:

```js
let crossWasPressed = false;

const loop = os.setInterval(() => {
    const cross = Pads.check(Pads.CROSS);

    if (cross && !crossWasPressed) {
        console.log("Pressed once");
    }

    crossWasPressed = cross;
}, 16);
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

The returned object also contains the raw button bitmask:

```js
console.log(pad.btns);
```

Analog values are native controller values rather than normalized `-1..1` values.

## Notes

`rumble()` and `battery_info()` are legacy/experimental APIs in the current runtime and should not be treated as stable game APIs yet.
