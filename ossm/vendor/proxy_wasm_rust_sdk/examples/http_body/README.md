## Proxy-Wasm plugin example: HTTP body

Proxy-Wasm plugin that redacts sensitive HTTP responses.

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

#### Response without secrets.

Send HTTP request to `localhost:10000/hello`:

```sh
$ curl localhost:10000/hello
Everyone may read this message.
```

#### Response with (redacted) secrets.

Send HTTP request to `localhost:10000/secret`:

```sh
$ curl localhost:10000/secret
Original message body (50 bytes) redacted.
```
