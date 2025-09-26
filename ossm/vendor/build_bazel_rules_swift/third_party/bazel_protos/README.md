This directory contains protocol buffers vendored from the
[main Bazel repository](https://raw.githubusercontent.com/bazelbuild/bazel/master/src/main/protobuf/worker_protocol.proto),
so that rules_swift does not need to depend on the entire
`@io_bazel` workspace, which is approximately 100MB.
