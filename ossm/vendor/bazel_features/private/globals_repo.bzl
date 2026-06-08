"""Contains the internal repository rule globals_repo."""

load("//private:parse.bzl", "parse_version")

def _globals_repo_impl(rctx):
    rctx.file(
        "BUILD.bazel",
        """\
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

exports_files(["globals.bzl"])

# This keeps stardoc happy if globals.bzl is pulled into a repository.
bzl_library(
    name = "globals",
    srcs = ["globals.bzl"],
    visibility = ["//visibility:public"],
)
""",
    )

    bazel_version = parse_version(native.bazel_version)

    lines = ["globals = struct("]
    for global_, (min_version, max_version) in rctx.attr.globals.items():
        if not _is_valid_identifier(global_):
            fail("Invalid global name: %s" % global_)
        if not min_version and not max_version:
            fail("invalid config for:", global_, "expected at least one version")
        if (
            (not min_version or bazel_version >= parse_version(min_version)) and
            (not max_version or bazel_version < parse_version(max_version))
        ):
            value = global_
        else:
            value = "None"

        # If the legacy_globals is available, we take the value from it.
        # The value is populated by --incompatible_autoload_externally and may apply to older Bazel versions
        lines.append("    %s = getattr(getattr(native, 'legacy_globals', None), '%s', %s)," % (global_, global_, value))

    lines.append(")")

    rctx.file("globals.bzl", "\n".join(lines))

globals_repo = repository_rule(
    _globals_repo_impl,
    # Force reruns on server restarts to keep native.bazel_version up-to-date.
    local = True,
    attrs = {
        "globals": attr.string_list_dict(
            mandatory = True,
        ),
    },
)

def _is_valid_identifier(s):
    if not s or s[0].isdigit():
        return False
    return all([c.isalnum() or c == "_" for c in s.elems()])
