## Proxy-Wasm plugin example: Envoy metadata

Proxy-Wasm plugin that demonstrates reading metadata set by other Envoy filters.

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

Send a HTTP request to `localhost:10000` that will return the configured response.

```sh
$ curl localhost:10000
Welcome, set the `x-custom-metadata` header to change the response!
```

Send a HTTP request to `localhost:10000` with a `x-custom-metadata` header value to get
the uppercased value in the response.

The response will also contain a response header `uppercased-metadata: SOME-VALUE`.

```sh
$ curl localhost:10000 -H "x-custom-metadata: some-value"
Custom response with Envoy metadata: "SOME-VALUE"
```
