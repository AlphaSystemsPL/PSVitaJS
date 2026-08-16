# App API

`App` exposes PS Vita application/process helpers.

## Exit the application

```js
App.exit();
```

Exit with a code:

```js
App.exit(0);
```

## Launch parameters

```js
const params = App.getLaunchParams();

console.log(params);
```

## Poll system events

`App.pollEvent()` returns an event object or `null`:

```js
const event = App.pollEvent();

if (event !== null) {
    console.log(
        event.type,
        event.rawType
    );
}
```

Known event names include:

```text
resume
storePurchase
npMessageArrived
storeRedemption
unknown
```

A simple frame-level check:

```js
const event = App.pollEvent();

if (event?.type === "resume") {
    console.log("Application resumed");
}
```

Do not assume an event will be available on every frame.

## Info bar

```js
App.setInfoBar({
    visible: true,
    color: "black",
    translucent: true
});
```

Available colors:

```text
black
white
```

## Vita3K note

`App` maps directly to Vita system APIs. Some AppMgr calls can be missing or behave differently in Vita3K. Validate lifecycle and info-bar behavior on real hardware.
