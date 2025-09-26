"""`local_crate_mirror` rule implementation."""

load("//crate_universe/private:common_utils.bzl", "execute")
load("//crate_universe/private:generate_utils.bzl", "get_generator")
load("//crate_universe/private:urls.bzl", "CARGO_BAZEL_SHA256S", "CARGO_BAZEL_URLS")
load("//rust/platform:triple.bzl", "get_host_triple")

def _local_crate_mirror_impl(repository_ctx):
    path = repository_ctx.path(repository_ctx.attr.path)

    host_triple = get_host_triple(repository_ctx)

    generator, _generator_sha256 = get_generator(repository_ctx, host_triple.str)

    # TODO: Work out why we can't just symlink here and actually copy.
    # illicitonion thinks it may be that symlinks didn't get invalidated properly?
    for child in repository_ctx.path(path).readdir():
        repository_ctx.execute(["cp", "-r", child, repository_ctx.path(child.basename)])

    paths_to_track = execute(repository_ctx, ["find", path, "-type", "f"]).stdout.strip().split("\n")
    for path_to_track in paths_to_track:
        if path_to_track:
            repository_ctx.read(path_to_track)

    execute(repository_ctx, [generator, "render", "--options-json", repository_ctx.attr.options_json, "--output-path", repository_ctx.path("BUILD.bazel")])

    repository_ctx.file("WORKSPACE.bazel", "")

local_crate_mirror = repository_rule(
    doc = """This is a private implementation detail of crate_universe, and should not be relied on in manually written code.

This is effectively a `local_repository` rule impementation, but where the BUILD.bazel file is generated using the `cargo-bazel render` command.""",
    implementation = _local_crate_mirror_impl,
    attrs = {
        "generator": attr.string(
            doc = (
                "The absolute label of a generator. Eg. `@cargo_bazel_bootstrap//:cargo-bazel`. " +
                "This is typically used when bootstrapping"
            ),
        ),
        "generator_sha256s": attr.string_dict(
            doc = "Dictionary of `host_triple` -> `sha256` for a `cargo-bazel` binary.",
            default = CARGO_BAZEL_SHA256S,
        ),
        "generator_urls": attr.string_dict(
            doc = (
                "URL template from which to download the `cargo-bazel` binary. `{host_triple}` and will be " +
                "filled in according to the host platform."
            ),
            default = CARGO_BAZEL_URLS,
        ),
        "options_json": attr.string(
            doc = "JSON serialized instance of a crate_universe::context::SingleBuildFileRenderContext",
        ),
        "path": attr.string(
            # TODO: Verify what happens if this is not an absolute path.
            doc = "Absolute path to the BUILD.bazel file to generate.",
        ),
        "quiet": attr.bool(
            doc = "If stdout and stderr should not be printed to the terminal.",
            default = True,
        ),
    },
)
