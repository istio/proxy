## Proxy-Wasm plugin example: HTTP auth (random)

Proxy-Wasm plugin that grants access based on a result of HTTP callout.

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

#### Access granted.

Send HTTP request to `localhost:10000/headers`:

```sh
$ curl localhost:10000/headers
{
  "headers": {
    "Accept": "*/*", 
    "Host": "localhost", 
    "User-Agent": "curl/7.81.0", 
    "X-Amzn-Trace-Id": "Root=1-637c4767-6e31776a0b407a0219b5b570", 
    "X-Envoy-Expected-Rq-Timeout-Ms": "15000"
  }
}
```

Expected Envoy logs:

```console
[...] wasm log http_auth_random: Access granted.
```

#### Access forbidden.

Send HTTP request to `localhost:10000/headers`:

```sh
$ curl localhost:10000/headers
Access forbidden.
```

Expected Envoy logs:

```console
[...] wasm log http_auth_random: Access forbidden.
```
