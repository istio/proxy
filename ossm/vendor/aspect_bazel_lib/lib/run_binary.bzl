"""Runs a binary as a build action. This rule does not require Bash (unlike native.genrule()).

This fork of bazel-skylib's run_binary adds directory output support and better makevar expansions.
"""

load(
    "//lib/private:run_binary.bzl",
    _run_binary = "run_binary",
)

run_binary = _run_binary
