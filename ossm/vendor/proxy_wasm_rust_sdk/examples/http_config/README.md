## Proxy-Wasm plugin example: HTTP config

Proxy-Wasm plugin that injects HTTP response header with a value from Envoy config.

### Building

```sh
$ cargo build --target wasm32-wasip1 --release
```

### Using in Envoy

This example can be run with [`docker compose`](https://docs.docker.com/compose/install/)
and has a matching Envoy configuration.

```sh
$ docker compose up
```

Send HTTP request to `localhost:10000/hello`:

```sh
$ curl -I localhost:10000/hello
HTTP/1.1 200 OK
content-length: 40
content-type: text/plain
custom-header: The secret to life is meaningless unless you discover it yourself
date: Tue, 22 Nov 2022 04:09:05 GMT
server: envoy
```
