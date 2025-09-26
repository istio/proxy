# Contribution guidelines

So you want to hack on Istio? Yay! Please refer to Istio's overall
[contribution guidelines](https://github.com/istio/community/blob/master/CONTRIBUTING.md)
to find out how you can help.

## Prerequisites

To make sure you're ready to build Envoy, clone and follow the upstream Envoy [build instructions](https://github.com/envoyproxy/envoy/blob/main/bazel/README.md). Be sure to copy clang.bazelrc into this directory after running the setup_clang script. Confirm that your environment is ready to go by running `bazel build --config=clang --define=wasm=disabled :envoy` (in this directory).

## How to use a devcontainer

1. Change the image in .devcontainer.json to `build-tools-proxy` instead of `build-tools`
2. Open the directory in a container with the Remote - Containers extension
3. Install the following extensions:
    - [clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)
    - [bazel-stack-vscode-cc](https://marketplace.visualstudio.com/items?itemName=StackBuild.bazel-stack-vscode-cc)
4. Update clangd and reload the container
5. Edit the new `bsv.cc.compdb.targets` workspace setting and set it to `//:envoy_tar`
6. Execute `Bazel/C++: Generate Compilation Database` within vscode

Note: if you have a remote bazel cache or something mounted in your build container for your normal proxy builds, you'll need to configure that in the devcontainer with runArgs.
