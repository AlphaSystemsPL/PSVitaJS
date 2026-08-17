# Pads API

`Pads` exposes buttons, analog sticks and controller-port helpers.

## Preferred per-frame read

`Pads.read(port?)` is the preferred API when you need buttons and both analog sticks in the same frame. It performs one native controller poll and returns the complete state.

```js
const pad = Pads.read();

console.log(
    pad.buttons,
    pad.lx,
    pad.ly,
    pad.rx,
    pad.ry
);
```

Controller ports `0..5` are supported:

```js
const pad0 = Pads.read(0);
const pad1 = Pads.read(1);
```

Returned state:

```js
{
    port: 0,
    btns: 0,
    buttons: 0,
    timestamp: 123456,
    lx: 128,
    ly: 128,
    rx: 128,
    ry: 128
}
```

`buttons` is an alias of `btns`.

`Pads.analog(port?)` remains available as a backwards-compatible alias of `Pads.read(port?)`.

Analog stick values are native controller values rather than normalized `-1..1` values.

## Check a button

For a simple button test:

```js
if (Pads.check(Pads.CROSS)) {
    console.log("CROSS");
}
```

An explicit controller port can be supplied:

```js
if (Pads.check(Pads.CROSS, 1)) {
    console.log("CROSS on port 1");
}
```

## Button constants

```js
Pads.SELECT
Pads.START

Pads.UP
Pads.RIGHT
Pads.DOWN
Pads.LEFT

Pads.TRIANGLE
Pads.CIRCLE
Pads.CROSS
Pads.SQUARE

Pads.L1
Pads.L2
Pads.L3
Pads.R1
Pads.R2
Pads.R3

Pads.POWER
```

## Edge-triggered input

For actions that should happen only once per press:

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

## Sampling mode

```js
Pads.setSamplingMode(
    Pads.MODE_ANALOG_WIDE
);

console.log(
    Pads.getSamplingMode()
);
```

Available mode constants:

```js
Pads.MODE_DIGITAL
Pads.MODE_ANALOG
Pads.MODE_ANALOG_WIDE
```

## Controller type constants

```js
Pads.TYPE_UNPAIRED
Pads.TYPE_PSVITA
Pads.TYPE_PSTV
Pads.TYPE_DUALSHOCK3
Pads.TYPE_DUALSHOCK4
```

## Rumble

```js
Pads.rumble(
    64,
    180,
    1
);
```

Arguments are the small-motor strength, large-motor strength and optional controller port.

Rumble only makes sense for controller types that support it.

## Controller battery

```js
const level = Pads.battery_info(1);
console.log(level);
```

Controller battery behavior is hardware-dependent and should be tested with the controller types your application supports.
