"""
Re-export of https://registry.bazel.build/modules/jq.bzl to avoid breaking change.
TODO(3.0): delete
"""

load("@jq.bzl//jq:jq.bzl", _jq = "jq")

jq = _jq
