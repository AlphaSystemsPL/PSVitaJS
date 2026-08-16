# Dialog API

`Dialog` exposes native PS Vita message dialogs and the IME keyboard.

Only one native dialog can be active at a time.

## Important render-loop integration

While a dialog is active, call `Dialog.update()` every frame **after** `Screen.end_drawing()` and **before** `Screen.swap_buffers()`:

```js
Screen.start_drawing();
Screen.clear(20, 20, 25, 255);

// draw game/application UI here

Screen.end_drawing();

Dialog.update();

Screen.swap_buffers();
```

`Dialog.update()` performs the common-dialog vita2d update internally. Normally you should not call `Screen.common_dialog_update()` as well.

## Message dialog

```js
Dialog.message({
    text: "Exit the game?",
    buttons: "yesNo"
});
```

Supported button layouts:

```text
ok
yesNo
okCancel
cancel
none
```

Check status:

```js
if (Dialog.status() === "finished") {
    const result = Dialog.result();

    console.log(result);
}
```

Example result:

```js
{
    type: "message",
    result: "ok",
    button: "yes",
    buttonId: 1
}
```

A complete pattern:

```js
let dialogOpen = false;

function askToExit() {
    if (dialogOpen) {
        return;
    }

    Dialog.message({
        text: "Exit VitaJS?",
        buttons: "yesNo"
    });

    dialogOpen = true;
}

function updateDialog() {
    const status = Dialog.status();

    if (status !== "finished") {
        return;
    }

    const result = Dialog.result();
    dialogOpen = false;

    if (
        result?.type === "message" &&
        result.button === "yes"
    ) {
        App.exit(0);
    }
}
```

Call `updateDialog()` from the main loop, and call `Dialog.update()` in the render phase.

## Native keyboard

```js
Dialog.keyboard({
    title: "Player name",
    initialText: "VitaPlayer",
    maxLength: 24,
    type: "text",
    withClear: true,
    withCancel: true,
    enterLabel: "default"
});
```

Keyboard types:

```text
text
latin
number
extendedNumber
url
email
```

Enter labels:

```text
default
send
search
go
```

Additional options include:

```js
{
    password: false,
    multiline: false,
    noAutoCapitalization: false,
    noAssistance: false
}
```

Read the result:

```js
if (Dialog.status() === "finished") {
    const result = Dialog.result();

    if (
        result?.type === "keyboard" &&
        result.confirmed
    ) {
        console.log(
            result.text
        );
    }
}
```

Keyboard result shape:

```js
{
    type: "keyboard",
    result: "ok",
    confirmed: true,
    text: "VitaPlayer"
}
```

## Abort a dialog

```js
if (Dialog.status() === "running") {
    Dialog.abort();
}
```

`abort()` returns `false` if no dialog is active.

## Vita3K note

Native common dialogs are system-level functionality. Emulator support can differ from real PS Vita behavior.
