<div align="center">
    <img width="200" height="200" src="https://raw.githubusercontent.com/rules-proto-grpc/rules_proto_grpc/master/docs/_static/logo.svg">
    <h1>Protobuf and gRPC rules for <a href="https://bazel.build">Bazel</a></h1>
</div>

<div align="center">
    <a href="https://bazel.build">Bazel</a> rules for building <a href="https://developers.google.com/protocol-buffers">Protobuf</a> and <a href="https://grpc.io/">gRPC</a> code and libraries from <a href="https://docs.bazel.build/versions/master/be/protocol-buffer.html#proto_library">proto_library</a> targets<br><br>
    <a href="https://github.com/rules-proto-grpc/rules_proto_grpc/releases"><img src="https://img.shields.io/github/v/tag/rules-proto-grpc/rules_proto_grpc?label=release&sort=semver&color=38a3a5"></a>
    <a href="https://buildkite.com/bazel/rules-proto-grpc-rules-proto-grpc"><img src="https://badge.buildkite.com/a0c88e60f21c85a8bb53a8c73175aebd64f50a0d4bacbdb038.svg?branch=master"></a>
    <a href="https://github.com/rules-proto-grpc/rules_proto_grpc/actions"><img src="https://github.com/rules-proto-grpc/rules_proto_grpc/workflows/CI/badge.svg"></a>
    <a href="https://bazelbuild.slack.com/archives/CKU1D04RM"><img src="https://img.shields.io/badge/bazelbuild-%23proto-38a3a5?logo=slack"></a>
</div>


## Announcements 📣

#### 2023/12/14 - Version 4.6.0

[Version 4.6.0 has been released](https://github.com/rules-proto-grpc/rules_proto_grpc/releases/tag/4.6.0),
which contains a few bug fixes for Bazel 7 support. **Note that this is likely to be the last
WORKSPACE supporting release of rules_proto_grpc**, as new bzlmod supporting rules are introduced
in the next major release

#### 2023/09/12 - Version 4.5.0

[Version 4.5.0 has been released](https://github.com/rules-proto-grpc/rules_proto_grpc/releases/tag/4.5.0),
which contains a number of version updates, bug fixes and usability improvements over 4.4.0.
Additionally, the Rust rules contain a major change of underlying gRPC and Protobuf library; the
rules now use Tonic and Prost respectively


## Usage

Full documentation for the current and previous versions [can be found here](https://rules-proto-grpc.com)

- [Overview](https://rules-proto-grpc.com/en/latest/)
- [Installation](https://rules-proto-grpc.com/en/latest/#installation)
- [Example Usage](https://rules-proto-grpc.com/en/latest/example.html)
- [Custom Plugins](https://rules-proto-grpc.com/en/latest/custom_plugins.html)
- [Changelog](https://rules-proto-grpc.com/en/latest/changelog.html)
