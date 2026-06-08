"""The `cargo_toml_env_vars` rule is used to retrieve the env vars that Cargo would set, based on the contents of a Cargo.toml file."""

def _cargo_toml_env_vars_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + ".env")

    inputs = [ctx.file.src]
    args = ctx.actions.args()
    args.add(out)
    args.add(ctx.file.src)

    if ctx.attr.workspace:
        inputs.append(ctx.file.workspace)
        args.add(ctx.file.workspace)

    ctx.actions.run(
        outputs = [out],
        executable = ctx.file._cargo_toml_variable_extractor,
        inputs = inputs,
        arguments = [args],
        mnemonic = "ExtractCargoTomlEnvVars",
    )

    return [
        DefaultInfo(files = depset([out]), runfiles = ctx.runfiles([out])),
    ]

cargo_toml_env_vars = rule(
    implementation = _cargo_toml_env_vars_impl,
    attrs = {
        "src": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "Cargo.toml file to derive env vars from",
        ),
        "workspace": attr.label(
            allow_single_file = True,
            doc = "Workspace Cargo.toml file from which values may be inherited",
        ),
        "_cargo_toml_variable_extractor": attr.label(
            allow_single_file = True,
            executable = True,
            default = Label("//cargo/private/cargo_toml_variable_extractor"),
            cfg = "exec",
        ),
    },
    doc = "A rule for extracting environment variables which Cargo would set for a crate, making it easy to have Bazel set them too.",
)
