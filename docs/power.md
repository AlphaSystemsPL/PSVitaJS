# Power API

`Power` exposes battery information, hardware clocks and power-management functions.

## Battery percentage

```js
console.log(
    Power.getBatteryPercent()
);
```

## Charging state

```js
console.log(
    Power.isCharging()
);

console.log(
    Power.isPluggedIn()
);

console.log(
    Power.isLowBattery()
);
```

## Full battery information

```js
const battery = Power.getBatteryInfo();

console.log(
    battery.percent,
    battery.charging,
    battery.pluggedIn,
    battery.low
);
```

The object also exposes hardware-dependent information such as remaining minutes, capacities, temperature, voltage, health and cycle count.

## Clock frequencies

```js
const clocks = Power.getClocks();

console.log(
    clocks.cpu,
    clocks.bus,
    clocks.gpu,
    clocks.gpuXbar
);
```

## Set clocks

```js
const clocks = Power.setClocks({
    cpu: 444,
    bus: 222,
    gpu: 222,
    gpuXbar: 166
});

console.log(clocks);
```

Clock changes affect hardware behavior, battery use and thermals. Use supported values and test on real hardware.

## Suspend requirement

```js
if (Power.isSuspendRequired()) {
    console.log("Suspend requested");
}
```

## Suspend and standby

```js
Power.suspend();
```

```js
Power.standby();
```

## Display power

```js
Power.requestDisplayOn();
```

```js
Power.requestDisplayOff();
```

## Vita3K note

`Power` maps directly to `ScePower`. Emulator coverage is not identical to physical hardware; extended battery metrics, suspend/display requests and other calls can be unavailable in Vita3K.
