"""Shared code for use with visibility specs."""

# Use when a target isn't actually public, but needs public
# visibility to keep Bazel happy.
# Such cases are typically for defaults of rule attributes or macro args that
# get used outside of rules_python itself.
NOT_ACTUALLY_PUBLIC = ["//visibility:public"]
