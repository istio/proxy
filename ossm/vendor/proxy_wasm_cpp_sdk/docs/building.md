# Proxy-Wasm C++ SDK Build Instructions

The C++ SDK has dependencies on specific versions of the C++ WebAssembly
toolchain [Emscripten](https://emscripten.org). Use of a Docker image is
recommended to achieve repeatable builds and save work.

## Docker

A Dockerfile for the C++ SDK is provided in [Dockerfile-sdk](../Dockerfile-sdk).

It can built in this repository's root directory by:

```bash
docker build -t wasmsdk:v3 -f Dockerfile-sdk .
```

The docker image can be used for compiling C++ plugin code into Wasm modules.

### Creating a project for use with the Docker build image

Create a directory with your source files and a Makefile:

```makefile
PROXY_WASM_CPP_SDK=/sdk

include ${PROXY_WASM_CPP_SDK}/Makefile
```

Create a C++ source file (myproject.cc):

```c++
#include <string>
#include <unordered_map>

#include "proxy_wasm_intrinsics.h"

class ExampleContext : public Context {
public:
  explicit ExampleContext(uint32_t id, RootContext* root) : Context(id, root) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override;
  void onDone() override;
};
static RegisterContextFactory register_ExampleContext(CONTEXT_FACTORY(ExampleContext));

FilterHeadersStatus ExampleContext::onRequestHeaders(uint32_t headers, bool end_of_stream) {
  logInfo(std::string("onRequestHeaders ") + std::to_string(id()));
  auto path = getRequestHeader(":path");
  logInfo(std::string("header path ") + std::string(path->view()));
  return FilterHeadersStatus::Continue;
}

void ExampleContext::onDone() { logInfo("onDone " + std::to_string(id())); }
```

### Compiling with the Docker build image

Run docker to build wasm, using a target with a .wasm suffix:

```bash
docker run -v $PWD:/work -w /work wasmsdk:v3 /build_wasm.sh myproject.wasm
```

You can specify wasm dependencies via these Makefile variables:

-   PROTOBUF = {full, lite, none}
-   WASM_DEPS = list of libraries

For example:

```makefile
PROXY_WASM_CPP_SDK=/sdk

PROTOBUF=lite
WASM_DEPS=re2 absl_strings

include ${PROXY_WASM_CPP_SDK}/Makefile
```

### Caching the standard libraries

The first time that emscripten runs it will generate the standard libraries.  To
cache these in the docker image, after the first successful compilation (e.g
myproject.cc above), commit the image with the standard libraries:

```bash
docker commit `docker ps -l | grep wasmsdk:v3 | awk '{print $1}'` wasmsdk:v3
```

This will save time on subsequent compiles.

### Ownership of the resulting .wasm files

The compiled files may be owned by root.  To chown them, add the follow lines to
the Makefile and docker invocation:

```makefile
PROXY_WASM_CPP_SDK=/sdk

all: myproject.wasm
  chown ${uid}.${gid} $^

include ${PROXY_WASM_CPP_SDK}/Makefile
```

Invocation file (e.g. build.sh):

```bash
#!/bin/bash
docker run -e uid="$(id -u)" -e gid="$(id -g)" -v $PWD:/work -w /work wasmsdk:v3 /build_wasm.sh
```

## Dependencies for building Wasm modules:

If you do not wish to use the Docker image, the dependencies can be installed by
script (sdk\_container.sh), or by hand. First you need Emscripten to build
everything else. Then you can build other wasm-compatible libraries, such as
protobuf, abseil, and RE2.

### Emscripten

Version 3.1.67 is known to work:

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
git checkout 3.1.67
./emsdk install --shallow 3.1.67
./emsdk activate 3.1.67

source ./emsdk_env.sh
```

It is possible later versions will work, e.g.

```bash
./emsdk update-tags
./emsdk install latest
./emsdk activate latest
```
