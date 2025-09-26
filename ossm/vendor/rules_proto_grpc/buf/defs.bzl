"""buf protobuf and grpc rules."""

load(":buf_proto_breaking_test.bzl", _buf_proto_breaking_test = "buf_proto_breaking_test")
load(":buf_proto_lint_test.bzl", _buf_proto_lint_test = "buf_proto_lint_test")

# Export buf rules
buf_proto_breaking_test = _buf_proto_breaking_test
buf_proto_lint_test = _buf_proto_lint_test
