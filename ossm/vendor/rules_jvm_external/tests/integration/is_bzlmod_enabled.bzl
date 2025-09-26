# Lifted from `aspect_bazel_lib`. We don't import the whole
# library because it's an awful lot for just this one check
# (particularly in the workspace-based builds) and we want
# to have as few dependencies as possible
def is_bzlmod_enabled():
    return str(Label("@//:BUILD.bazel")).startswith("@@")
