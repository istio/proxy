# grpc-httpjson-transcoding

grpc-httpjson-transcoding is a library that supports
[transcoding](https://github.com/googleapis/googleapis/blob/master/google/api/http.proto)
so that HTTP/JSON can be converted to gRPC.

It helps you to provide your APIs in both gRPC and RESTful style at the same
time. The code is used in istio [proxy](https://github.com/istio/proxy) and
cloud [endpoints](https://cloud.google.com/endpoints/) to provide HTTP+JSON
interface to gRPC service.

[![CI Status](https://oss.gprow.dev/badge.svg?jobs=grpc-transcoder-periodic)](https://testgrid.k8s.io/googleoss-grpc-transcoder#Summary)

[![Fuzzing Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/grpc-httpjson-transcoding.svg)](https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:grpc-httpjson-transcoding)

## Develop

[Bazel](https://bazel.build/) is used for build and dependency management. The
following commands build and test sources:

```bash
$ bazel build //...
$ bazel test //...
```

Use the following script to check and fix code format:

```bash
$ script/check-style
```

## Toolchain

The Bazel build system defaults to using clang 14 to enable reproducible builds.

## Continuous Integration

This repository is integrated with [OSS Prow](https://github.com/kubernetes/test-infra/tree/master/prow). Prow will run the [presubmit script](https://github.com/grpc-ecosystem/grpc-httpjson-transcoding/blob/master/script/ci.sh) on each Pull Request to verify tests pass. Note:

- PR submission is only allowed if the job passes.
- If you are an outside contributor, Prow may not run until a Googler LGTMs.

# Contribution
See [CONTRIBUTING.md](CONTRIBUTING.md).

# License
grpc-httpjson-transcoding is licensed under the Apache 2.0 license. See
[LICENSE](LICENSE) for more details.

