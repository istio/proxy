"""
Rule used to retrieve the lints specified in the [lints section] of a `Cargo.toml`.

[lints section](https://doc.rust-lang.org/cargo/reference/manifest.html#the-lints-section)
"""

# buildifier: disable=bzl-visibility
load("//rust/private:providers.bzl", "LintsInfo")

def _extract_cargo_lints(ctx):
    # Cargo.toml's to read from.
    inputs = [ctx.file.manifest]
    args = ctx.actions.args()

    args.add("--manifest_toml={0}".format(ctx.file.manifest.path))
    if ctx.attr.workspace:
        inputs.append(ctx.file.workspace)
        args.add("--workspace_toml={0}".format(ctx.file.workspace.path))
    args.add("lints")

    # Files to output our formatted arguments into.
    rustc_lints_out = ctx.actions.declare_file(ctx.label.name + ".rustc" + ".lints")
    clippy_lints_out = ctx.actions.declare_file(ctx.label.name + ".clippy" + ".lints")
    rustdoc_lints_out = ctx.actions.declare_file(ctx.label.name + ".rustdoc" + ".lints")

    args.add(rustc_lints_out)
    args.add(clippy_lints_out)
    args.add(rustdoc_lints_out)

    outputs = [rustc_lints_out, clippy_lints_out, rustdoc_lints_out]

    ctx.actions.run(
        outputs = outputs,
        executable = ctx.file._cargo_toml_info,
        inputs = inputs,
        arguments = [args],
        mnemonic = "CargoLints",
        progress_message = "Reading Cargo metadata to get Lints for {}".format(ctx.attr.name),
    )

    return [
        DefaultInfo(files = depset(outputs), runfiles = ctx.runfiles(outputs)),
        LintsInfo(
            rustc_lint_flags = [],
            rustc_lint_files = [rustc_lints_out],
            clippy_lint_flags = [],
            clippy_lint_files = [clippy_lints_out],
            rustdoc_lint_flags = [],
            rustdoc_lint_files = [rustdoc_lints_out],
        ),
    ]

extract_cargo_lints = rule(
    implementation = _extract_cargo_lints,
    attrs = {
        "manifest": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "Cargo.toml to read lints from.",
        ),
        "workspace": attr.label(
            allow_single_file = True,
            doc = "Workspace Cargo.toml that the manifest Cargo.toml inherits from.",
        ),
        "_cargo_toml_info": attr.label(
            allow_single_file = True,
            executable = True,
            default = Label("//cargo/private/cargo_toml_info:cargo_toml_info"),
            cfg = "exec",
        ),
    },
)
