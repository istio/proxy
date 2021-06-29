#!/bin/bash
bazel build --config=libc++ \
  --config=release \
  --define tcmalloc=gperftools \
  --action_env=PATH=/usr/lib/llvm/bin:/usr/local/go/bin:/gobin:/usr/local/google-cloud-sdk/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  --action_env=CC=clang \
  --action_env=CXX=clang++ \
  --action_env=CXXFLAGS=-stdlib="libc++ -Wno-unused-variable" \
  --override_repository=envoy=/envoy \
  //src/envoy:envoy
