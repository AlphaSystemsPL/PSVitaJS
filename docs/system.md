# System and filesystem

`System` exposes Vita filesystem helpers, application metadata and selected system information. QuickJS `std` and `os` remain available alongside it.

## Application manifest

`System.manifest` contains the parsed contents of:

```text
app0:/sce_sys/param.sfo
```

All SFO keys are exposed on the object when they can be parsed.

```js
console.log(
    System.manifest
);
```

Convenience properties are available for common fields:

```js
console.log(
    System.titleId,
    System.title,
    System.shortTitle,
    System.version,
    System.category,
    System.contentId
);
```

These values are read-only runtime module values.

## System language

```js
console.log(
    System.language,
    System.languageId
);
```

`System.language` uses a BCP-47-like tag such as:

```text
pl-PL
en-US
de-DE
```

`languageId` exposes the underlying Vita system language identifier.

## Hardware model

```js
console.log(
    System.model,
    System.modelId,
    System.isPSTV
);
```

`System.model` currently distinguishes:

```text
PS Vita
PS Vita TV
```

It does not distinguish PCH-1000 from PCH-2000.

## Check for a file

```js
const path = "ux0:data/VITAJS001/config.json";

if (System.doesFileExist(path)) {
    console.log("File exists");
}
```

## Directories

```js
console.log(
    System.currentDir()
);

System.currentDir(
    "ux0:data/VITAJS001"
);
```

List the current directory:

```js
const entries = System.listDir();

for (const entry of entries) {
    console.log(
        entry.name,
        entry.size,
        entry.dir
    );
}
```

or list an explicit path:

```js
const entries = System.listDir(
    "ux0:data/VITAJS001"
);
```

Create and remove directories:

```js
System.createDirectory(
    "ux0:data/VITAJS001/save"
);

System.removeDirectory(
    "ux0:data/VITAJS001/save"
);
```

## File operations

```js
System.copyFile(
    "ux0:data/VITAJS001/source.json",
    "ux0:data/VITAJS001/copy.json"
);

System.moveFile(
    "ux0:data/VITAJS001/copy.json",
    "ux0:data/VITAJS001/archive.json"
);

System.rename(
    "ux0:data/VITAJS001/archive.json",
    "ux0:data/VITAJS001/archive-old.json"
);

System.removeFile(
    "ux0:data/VITAJS001/archive-old.json"
);
```

Low-level file descriptors are also available through:

```js
System.openFile(...)
System.readFile(...)
System.writeFile(...)
System.seekFile(...)
System.sizeFile(...)
System.closeFile(...)
```

See `vitajs.d.ts` for the file-mode and seek constants.

## Load text through QuickJS `std`

```js
const source = std.loadFile(
    "ux0:data/VITAJS001/app.js"
);

if (source !== null) {
    console.log(source);
}
```

## Dynamic JavaScript execution

```js
const source = std.loadFile(
    "ux0:data/VITAJS001/app.js"
);

if (source !== null) {
    std.evalScript(source);
}
```

This can be useful for development loaders or user/content scripts stored outside the VPK.

For normal application source code, ES module imports are preferable because dependencies are explicit and resolved by the module loader.

## Memory information

```js
console.log(
    System.get_free_memory(),
    System.get_used_memory(),
    System.get_free_vram(),
    System.get_used_vram()
);
```

## Blocking delay

```js
System.delay(100);
```

`System.delay()` blocks the current thread. Prefer timers for normal game/application scheduling.
