# WebAssembly

This plugin can be compiled and run via the Envoy WebAssembly support.

## Creating build Docker image.

Follow the instructions in the github.com/istio/envoy/api/wasm/cpp/README.md to build the WebAssembly Docker build image.

## Checkout abseil-cpp

```bash
git clone https://github.com/abseil/abseil-cpp.git
```

## Build via the Docker image.

```bash
docker run -v $PWD:/work -w /work -v $(realpath $PWD/../../extensions):/work/extensions wasmsdk:v1 bash /build_wasm.sh
```
