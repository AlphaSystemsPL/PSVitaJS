# Screen API

`Screen` exposes the main vita2d rendering API.

## Frame lifecycle

Rendering belongs between `Screen.start_drawing()` and `Screen.end_drawing()`:

```js
Screen.start_drawing();

Screen.clear(20, 20, 25, 255);

// draw here

Screen.end_drawing();
Screen.swap_buffers();
```

## RGBA colors

vita2d colors can be created with:

```js
function rgba(r, g, b, a = 255) {
    return (
        ((a & 255) << 24) |
        ((b & 255) << 16) |
        ((g & 255) << 8) |
        (r & 255)
    ) >>> 0;
}
```

## Basic primitives

```js
const BLUE = rgba(60, 130, 240);
const WHITE = rgba(255, 255, 255);

Screen.start_drawing();
Screen.clear(20, 20, 25, 255);

Screen.draw_rectangle(
    40,
    40,
    200,
    100,
    BLUE
);

Screen.draw_line(
    40,
    180,
    300,
    180,
    WHITE
);

Screen.draw_pixel(
    50,
    220,
    WHITE
);

// Current VitaJS argument order:
// radius, x, y, color
Screen.draw_fill_circle(
    24,
    420,
    150,
    BLUE
);

Screen.end_drawing();
Screen.swap_buffers();
```

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

JPG and BMP loaders are also available:

```js
const jpg = Screen.load_jpg_file("app0:/assets/image.jpg");
const bmp = Screen.load_bmp_file("app0:/assets/image.bmp");
```

## Texture size

```js
const width = Screen.texture_get_width(texture);
const height = Screen.texture_get_height(texture);

console.log(width, height);
```

## Free a texture

```js
Screen.free_texture(texture);
```

Do not use a texture handle after it has been freed.

## Native dialogs

When `Dialog` is active, use `Dialog.update()` after ending the normal vita2d draw pass and before swapping buffers:

```js
Screen.start_drawing();
Screen.clear(20, 20, 25, 255);

// game UI

Screen.end_drawing();

Dialog.update();

Screen.swap_buffers();
```

`Dialog.update()` already performs the vita2d common-dialog update, so you normally do not need to call `Screen.common_dialog_update()` separately.
