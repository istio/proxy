## Proxy-Wasm plugin example: gRPC auth (random)

Proxy-Wasm plugin that grants access based on a result of gRPC callout.

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

Send gRPC request to `localhost:10000` service `hello.HelloService`:

```sh
$ grpcurl -d '{"greeting": "Rust"}' -plaintext localhost:10000 hello.HelloService/SayHello
{
  "reply": "hello Rust"
}
```

Expected Envoy logs:

```console
[...] wasm log grpc_auth_random: Access granted.
```

#### Access forbidden.

Send gRPC request to `localhost:10000` service `hello.HelloService`:

```sh
$ grpcurl -d '{"greeting": "Rust"}' -plaintext localhost:10000 hello.HelloService/SayHello
ERROR:
  Code: Aborted
  Message: Aborted by Proxy-Wasm!
```

Expected Envoy logs:

```console
[...] wasm log grpc_auth_random: Access forbidden.
```
