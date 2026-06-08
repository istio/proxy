# Echo

A minimal but complete example of using buf with bazel via rules_buf. 


## Generation

We generate the files using `buf` directly, this lets us use remote plugins and managed mode. We can do so from within bazel:

```sh
bazel run @rules_buf_toolchains//:buf -- generate
```

Everytime we generate the files we can run, gazelle to auto generate the BUILD files:

```sh
bazel run //:gazelle
```

This will generate the language rules based on the configured gazelle plugins.

## Running the server

The server can be started using `bazel run //cmd/echo:echo` and we can use `buf curl` to test:

```sh
buf curl http://localhost:8080/echo.v1.EchoService/Echo --schema 
```