# Net API

`Net` currently exposes a **synchronous** native HTTP API.

> `Net.get()`, `Net.post()` and `Net.request()` block the JavaScript/game loop until the request completes. Do not call them every frame.

## GET

```js
const response = Net.get(
    "http://192.168.1.20:3000/test.json"
);

console.log(response.status);
console.log(response.ok);
console.log(response.body);
```

Parse JSON normally:

```js
if (response.ok) {
    const data = JSON.parse(response.body);
    console.log(data);
}
```

Response shape:

```js
{
    status: 200,
    ok: true,
    body: "..."
}
```

## POST

```js
const response = Net.post(
    "http://192.168.1.20:3000/api",
    JSON.stringify({
        score: 12500,
        level: 4
    })
);
```

The default content type for `Net.post()` is:

```text
application/json
```

Custom content type:

```js
Net.post(
    "http://192.168.1.20:3000/api",
    "hello=world",
    "application/x-www-form-urlencoded"
);
```

## Generic request

```js
const response = Net.request(
    "PUT",
    "http://192.168.1.20:3000/player",
    JSON.stringify({
        hp: 90
    }),
    "application/json"
);
```

Supported methods include:

```text
GET
POST
PUT
DELETE
HEAD
OPTIONS
```

## Network state

```js
console.log(
    Net.is_connected()
);

console.log(
    Net.get_ip()
);
```

## Shutdown

```js
Net.term();
```

Call `Net.term()` only when you are done with the networking subsystem.
