"""
Re-export of https://registry.bazel.build/modules/yq.bzl to avoid breaking change.
TODO(3.0): delete
"""

load("@yq.bzl//yq:yq.bzl", _yq = "yq")

yq = _yq
