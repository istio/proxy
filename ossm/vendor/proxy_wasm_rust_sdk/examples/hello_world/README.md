## Proxy-Wasm plugin example: Hello World

Proxy-Wasm background service plugin that logs time and random numbers.

### Building

```sh
$ cargo build --target wasm32-wasi --release
```

### Using in Envoy

This example can be run with [`docker compose`](https://docs.docker.com/compose/install/)
and has a matching Envoy configuration.

```sh
$ docker compose up
```

Expected Envoy logs (new line generated every 5s):

```console
[...] wasm log: Hello, World!
[...] wasm log: It's 2022-11-22 03:39:17.849616 UTC, your lucky number is 41.
[...] wasm log: It's 2022-11-22 03:39:22.846531 UTC, your lucky number is 28.
[...] wasm log: It's 2022-11-22 03:39:27.847489 UTC, your lucky number is 102.
[...] wasm log: It's 2022-11-22 03:39:32.848443 UTC, your lucky number is 250.
```
