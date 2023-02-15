# Contribution guidelines

So you want to hack on Istio? Yay! Please refer to Istio's overall
[contribution guidelines](https://github.com/istio/community/blob/master/CONTRIBUTING.md)
to find out how you can help.

## Prerequisites

To make sure you're ready to build Envoy, clone and follow the upstream Envoy [build instructions](https://github.com/envoyproxy/envoy/blob/main/bazel/README.md). Be sure to copy clang.bazelrc into this directory after running the setup_clang script. Confirm that your environment is ready to go by running `bazel build --config=clang --define=wasm=disabled :envoy` (in this directory).
