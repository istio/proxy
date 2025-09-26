"""Rules for Cargo build scripts (`build.rs` files)"""

load("//rust:rust_common.bzl", "BuildInfo", "DepInfo")

# buildifier: disable=bzl-visibility
load("//rust/private:utils.bzl", "dedent")

def _cargo_dep_env_impl(ctx):
    empty_file = ctx.actions.declare_file(ctx.label.name + ".empty_file")
    empty_dir = ctx.actions.declare_directory(ctx.label.name + ".empty_dir")
    ctx.actions.write(
        output = empty_file,
        content = "",
    )
    ctx.actions.run(
        outputs = [empty_dir],
        executable = "true",
    )

    build_infos = []
    out_dir = ctx.file.out_dir
    if out_dir:
        if not out_dir.is_directory:
            fail("out_dir must be a directory artifact")

        # BuildInfos in this list are collected up for all transitive cargo_build_script
        # dependencies. This is important for any flags set in `dep_env` which reference this
        # `out_dir`.
        #
        # TLDR: This BuildInfo propagates up build script dependencies.
        build_infos.append(BuildInfo(
            dep_env = empty_file,
            flags = empty_file,
            linker_flags = empty_file,
            link_search_paths = empty_file,
            out_dir = out_dir,
            rustc_env = empty_file,
            compile_data = depset([]),
        ))
    return [
        DefaultInfo(files = depset(ctx.files.src)),
        # Parts of this BuildInfo is used when building all transitive dependencies
        # (cargo_build_script and otherwise), alongside the DepInfo. This is how other rules
        # identify this one as a valid dependency, but we don't otherwise have a use for it.
        #
        # TLDR: This BuildInfo propagates up normal (non build script) depenencies.
        #
        # In the future, we could consider setting rustc_env here, and also propagating dep_dir
        # so files in it can be referenced there.
        BuildInfo(
            dep_env = empty_file,
            flags = empty_file,
            linker_flags = empty_file,
            link_search_paths = empty_file,
            out_dir = None,
            rustc_env = empty_file,
            compile_data = depset([]),
        ),
        # Information here is used directly by dependencies, and it is an error to have more than
        # one dependency which sets this. This is the main way to specify information from build
        # scripts, which is what we're looking to do.
        DepInfo(
            dep_env = ctx.file.src,
            direct_crates = depset(),
            link_search_path_files = depset(),
            transitive_build_infos = depset(direct = build_infos),
            transitive_crate_outputs = depset(),
            transitive_crates = depset(),
            transitive_noncrates = depset(),
        ),
    ]

cargo_dep_env = rule(
    implementation = _cargo_dep_env_impl,
    doc = (
        "A rule for generating variables for dependent `cargo_build_script`s " +
        "without a build script. This is useful for using Bazel rules instead " +
        "of a build script, while also generating configuration information " +
        "for build scripts which depend on this crate."
    ),
    attrs = {
        "out_dir": attr.label(
            doc = dedent("""\
                Folder containing additional inputs when building all direct dependencies.

                This has the same effect as a `cargo_build_script` which prints
                puts files into `$OUT_DIR`, but without requiring a build script.
            """),
            allow_single_file = True,
            mandatory = False,
        ),
        "src": attr.label(
            doc = dedent("""\
                File containing additional environment variables to set for build scripts of direct dependencies.

                This has the same effect as a `cargo_build_script` which prints
                `cargo:VAR=VALUE` lines, but without requiring a build script.

                This files should  contain a single variable per line, of format
                `NAME=value`, and newlines may be included in a value by ending a
                line with a trailing back-slash (`\\\\`).
            """),
            allow_single_file = True,
            mandatory = True,
        ),
    },
)
