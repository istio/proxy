from ubuntu:22.04

WORKDIR /dd-trace-cpp

ARG DEBIAN_FRONTEND=noninteractive
ARG BRANCH=v0.2.1

run apt update -y \
 && apt install -y g++ make git wget sed \
 && git clone --branch "${BRANCH}" 'https://github.com/datadog/dd-trace-cpp' . \
 && bin/install-cmake \
 && mkdir dist \
 && cmake -B .build -DDD_TRACE_BUILD_EXAMPLES=1 . \
 && cmake --build .build -j \
 && cmake --install .build --prefix=dist
