"""js_library groups together JS sources and arranges them and their transitive and npm dependencies into a provided
`JsInfo`. There are no Bazel actions to run.

For example, this `BUILD` file groups a pair of `.js/.d.ts` files along with the `package.json`.
The latter is needed because it contains a `typings` key that allows downstream
users of this library to resolve the `one.d.ts` file.
The `main` key is another commonly used field in `package.json` which would require including it in the library.

```starlark
load("@aspect_rules_js//js:defs.bzl", "js_library")

js_library(
    name = "one",
    srcs = [
        "one.d.ts",
        "one.js",
        "package.json",
    ],
)
```

| This is similar to [`py_library`](https://docs.bazel.build/versions/main/be/python.html#py_library) which depends on
| Python sources and provides a `PyInfo`.
"""

load(":js_info.bzl", "JsInfo", "js_info")
load(":js_helpers.bzl", "DOWNSTREAM_LINKED_NPM_DEPS_DOCSTRING", "JS_LIBRARY_DATA_ATTR", "copy_js_file_to_bin_action", "gather_npm_linked_packages", "gather_npm_package_store_deps", "gather_runfiles", "gather_transitive_declarations", "gather_transitive_sources")
load("@aspect_bazel_lib//lib:copy_to_bin.bzl", "COPY_FILE_TO_BIN_TOOLCHAINS")

_DOC = """A library of JavaScript sources. Provides JsInfo, the primary provider used in rules_js
and derivative rule sets.

Declaration files are handled separately from sources since they are generally not needed at
runtime and build rules, such as ts_project, are optimal in their build graph if they only depend
on declarations from `deps` since these they don't need the JavaScript source files from deps to
typecheck.

Linked npm dependences are also handled separately from sources since not all rules require them and it
is optimal for these rules to not depend on them in the build graph.

NB: `js_library` copies all source files to the output tree before providing them in JsInfo. See
https://github.com/aspect-build/rules_js/tree/dbb5af0d2a9a2bb50e4cf4a96dbc582b27567155/docs#javascript
for more context on why we do this."""

_ATTRS = {
    "srcs": attr.label_list(
        doc = """Source files that are included in this library.

This includes all your checked-in code and any generated source files.

The transitive npm dependencies, transitive sources & runfiles of targets in the `srcs` attribute are added to the
runfiles of this target. They should appear in the '*.runfiles' area of any executable which is output by or has a
runtime dependency on this target.

Source files that are JSON files, declaration files or directory artifacts will be automatically provided as
"declarations" available to downstream rules for type checking. To explicitly provide source files as "declarations"
available to downstream rules for type checking that do not match these criteria, move those files to the `declarations`
attribute instead.
""",
        allow_files = True,
    ),
    "declarations": attr.label_list(
        doc = """Same as `srcs` except all files are also provided as "declarations" available to downstream rules for type checking.

For example, a js_library with only `.js` files that are intended to be imported as `.js` files by downstream type checking
rules such as `ts_project` would list those files in `declarations`:

```
js_library(
    name = "js_lib",
    declarations = ["index.js"],
)
```
""",
        allow_files = True,
    ),
    "deps": attr.label_list(
        doc = """Dependencies of this target.

This may include other js_library targets or other targets that provide JsInfo

The transitive npm dependencies, transitive sources & runfiles of targets in the `deps` attribute are added to the
runfiles of this target. They should appear in the '*.runfiles' area of any executable which is output by or has a
runtime dependency on this target.

{downstream_linked_npm_deps}
""".format(downstream_linked_npm_deps = DOWNSTREAM_LINKED_NPM_DEPS_DOCSTRING),
        providers = [JsInfo],
    ),
    "data": JS_LIBRARY_DATA_ATTR,
    "no_copy_to_bin": attr.label_list(
        allow_files = True,
        doc = """List of files to not copy to the Bazel output tree when `copy_data_to_bin` is True.

        This is useful for exceptional cases where a `copy_to_bin` is not possible or not suitable for an input
        file such as a file in an external repository. In most cases, this option is not needed.
        See `copy_data_to_bin` docstring for more info.
        """,
    ),
    "copy_data_to_bin": attr.bool(
        doc = """When True, `data` files are copied to the Bazel output tree before being passed as inputs to runfiles.""",
        default = True,
    ),
}

def _gather_sources_and_declarations(ctx, targets, files):
    """Gathers sources and declarations from a list of targets

    Args:
        ctx: the rule context

        targets: List of targets to gather sources and declarations from their JsInfo providers.

            These typically come from the `srcs` and/or `data` attributes of a rule

        files: List of files to gather as sources and declarations.

            These typically come from the `srcs` and/or `data` attributes of a rule

    Returns:
        Sources & declaration files depsets in the sequence (sources, declarations)
    """
    sources = []
    declarations = []

    for file in files:
        if file.is_source:
            file = copy_js_file_to_bin_action(ctx, file)

        if file.is_directory:
            # assume a directory contains declarations since we can't know that it doesn't
            declarations.append(file)
            sources.append(file)
        elif (
            file.path.endswith(".d.ts") or
            file.path.endswith(".d.ts.map") or
            file.path.endswith(".d.mts") or
            file.path.endswith(".d.mts.map") or
            file.path.endswith(".d.cts") or
            file.path.endswith(".d.cts.map")
        ):
            declarations.append(file)
        elif file.path.endswith(".json"):
            # Any .json can produce types: https://www.typescriptlang.org/tsconfig/#resolveJsonModule
            # package.json may be required to resolve declarations with the "typings" key
            declarations.append(file)
            sources.append(file)
        else:
            sources.append(file)

    # sources as depset
    sources = depset(sources, transitive = [
        target[JsInfo].sources
        for target in targets
        if JsInfo in target and hasattr(target[JsInfo], "sources")
    ])

    # declarations as depset
    declarations = depset(declarations, transitive = [
        target[JsInfo].declarations
        for target in targets
        if JsInfo in target and hasattr(target[JsInfo], "declarations")
    ])

    return (sources, declarations)

def _js_library_impl(ctx):
    sources, declarations = _gather_sources_and_declarations(
        ctx = ctx,
        targets = ctx.attr.srcs,
        files = ctx.files.srcs,
    )

    additional_sources, additional_declarations = _gather_sources_and_declarations(
        ctx = ctx,
        targets = ctx.attr.declarations,
        files = ctx.files.declarations,
    )

    sources = depset(transitive = [sources, additional_sources])
    declarations = depset(transitive = [declarations, additional_sources, additional_declarations])

    transitive_sources = gather_transitive_sources(
        sources = sources,
        targets = ctx.attr.srcs + ctx.attr.declarations + ctx.attr.deps,
    )

    transitive_declarations = gather_transitive_declarations(
        declarations = declarations,
        targets = ctx.attr.srcs + ctx.attr.declarations + ctx.attr.deps,
    )

    npm_linked_packages = gather_npm_linked_packages(
        srcs = ctx.attr.srcs + ctx.attr.declarations,
        deps = ctx.attr.deps,
    )

    npm_package_store_deps = gather_npm_package_store_deps(
        targets = ctx.attr.srcs + ctx.attr.data + ctx.attr.deps,
    )

    runfiles = gather_runfiles(
        ctx = ctx,
        sources = transitive_sources,
        data = ctx.attr.data,
        deps = ctx.attr.srcs + ctx.attr.declarations + ctx.attr.deps,
        data_files = ctx.files.data,
        copy_data_files_to_bin = ctx.attr.copy_data_to_bin,
        no_copy_to_bin = ctx.files.no_copy_to_bin,
        include_transitive_sources = True,
        include_declarations = False,
        include_npm_linked_packages = True,
    )

    return [
        js_info(
            declarations = declarations,
            npm_linked_package_files = npm_linked_packages.direct_files,
            npm_linked_packages = npm_linked_packages.direct,
            npm_package_store_deps = npm_package_store_deps,
            sources = sources,
            transitive_declarations = transitive_declarations,
            transitive_npm_linked_package_files = npm_linked_packages.transitive_files,
            transitive_npm_linked_packages = npm_linked_packages.transitive,
            transitive_sources = transitive_sources,
        ),
        DefaultInfo(
            files = sources,
            runfiles = runfiles,
        ),
        OutputGroupInfo(
            types = declarations,
            runfiles = runfiles.files,
        ),
    ]

js_library_lib = struct(
    attrs = _ATTRS,
    implementation = _js_library_impl,
    provides = [DefaultInfo, JsInfo, OutputGroupInfo],
)

js_library = rule(
    doc = _DOC,
    implementation = js_library_lib.implementation,
    attrs = js_library_lib.attrs,
    provides = js_library_lib.provides,
    toolchains = COPY_FILE_TO_BIN_TOOLCHAINS,
)
