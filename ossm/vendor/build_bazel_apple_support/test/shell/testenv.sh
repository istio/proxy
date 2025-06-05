#!/bin/bash

function setup_objc_test_support() {
  cat > WORKSPACE.bazel <<EOF
local_repository(
    name = 'build_bazel_apple_support',
    path = '$(rlocation build_bazel_apple_support)',
)

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)

apple_support_dependencies()
EOF

  cat > .bazelrc <<EOF
build --apple_crosstool_top=@local_config_apple_cc//:toolchain
build --crosstool_top=@local_config_apple_cc//:toolchain
build --host_crosstool_top=@local_config_apple_cc//:toolchain
EOF
}
