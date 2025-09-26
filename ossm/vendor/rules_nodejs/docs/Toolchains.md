# Toolchains

API docs for [Toolchain](https://docs.bazel.build/versions/main/toolchains.html) support.

When you call `nodejs_register_toolchains()` in your `WORKSPACE` file it will setup a node toolchain for executing tools on all currently supported platforms.

If you have an advanced use-case and want to use a version of node not supported by this repository, you can also register your own toolchains.

## Registering a custom toolchain

To run a custom toolchain (i.e., to run a node binary not supported by the built-in toolchains), you'll need four things:

1) A rule which can build or load a node binary from your repository
   (a checked-in binary or a build using a relevant [`rules_foreign_cc` build rule](https://bazelbuild.github.io/rules_foreign_cc/) will do nicely).
2) A [`nodejs_toolchain` rule](Core.html#nodejs_toolchain) which depends on your binary defined in step 1 as its `node`.
3) A [`toolchain` rule](https://bazel.build/reference/be/platform#toolchain) that depends on your `nodejs_toolchain` rule defined in step 2 as its `toolchain`
   and on `@rules_nodejs//nodejs:toolchain_type` as its `toolchain_type`. Make sure to define appropriate platform restrictions as described in the
   documentation for the `toolchain` rule.
4) A call to [the `register_toolchains` function](https://bazel.build/rules/lib/globals#register_toolchains) in your `WORKSPACE`
   that refers to the `toolchain` rule defined in step 3.

Examples of steps 2-4 can be found in the [documentation for `nodejs_toolchain`](Core.html#nodejs_toolchain).

If necessary, you can substitute building the node binary as part of the build with using a locally installed version by skipping step 1 and replacing step 2 with:

2) A `nodejs_toolchain` rule which has the path of the system binary as its `node_path`