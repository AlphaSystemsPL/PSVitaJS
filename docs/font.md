# Font API

Fonts are rendered through vita2d.

## Load a font

```js
const font = Font.load_font_file(
    "app0:/assets/segoeui.ttf"
);
```

TTF and OTF fonts can be used.

## Draw text

Text must be drawn inside the normal Screen draw pass:

```js
const WHITE = 0xffffffff;

Screen.start_drawing();
Screen.clear(10, 10, 15, 255);

Font.font_draw_text(
    font,
    40,
    80,
    WHITE,
    32,
    "Hello PS Vita!"
);

Screen.end_drawing();
Screen.swap_buffers();
```

The `y` coordinate acts as the text baseline.

## Line spacing

```js
Font.font_draw_text_ls(
    font,
    40,
    80,
    8,
    0xffffffff,
    28,
    "Line 1\nLine 2"
);
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
    width,
    height,
    dimensions.width,
    dimensions.height
);
```

## Free a font

```js
Font.free_font(font);
```

Do not use the font object after it has been freed.
