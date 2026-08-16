# System and filesystem

VitaJS exposes its native `System` API together with QuickJS `std` and `os`.

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

const entries = System.listDir();
console.log(entries);
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
