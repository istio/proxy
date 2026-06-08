"""PyO3 Toolchains"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_rust//rust:defs.bzl", "rust_common")

PYO3_TOOLCHAIN = str(Label("//:toolchain_type"))

RUST_PYO3_TOOLCHAIN = str(Label("//:rust_toolchain_type"))

PY_IMPLEMENTATIONS = {
    "cpython": "CPython",
    "graalpy": "GraalVM",
    "graalvm": "GraalVM",
    "pypy": "PyPy",
}

def _pyo3_toolchain_impl(ctx):
    py_toolchain = ctx.toolchains["@rules_python//python:toolchain_type"]

    py_runtime = py_toolchain.py3_runtime
    if py_runtime.interpreter:
        interpreter = py_runtime.interpreter.path
    else:
        interpreter = py_runtime.interpreter_path

    version_info = py_runtime.interpreter_version_info
    version = "{}.{}".format(
        version_info.major,
        version_info.minor,
    )

    py_cc_toolchain = ctx.toolchains["@rules_python//python/cc:toolchain_type"].py_cc_toolchain

    libs = []
    for linker_input in py_cc_toolchain.libs.providers_map["CcInfo"].linking_context.linker_inputs.to_list():
        for library in linker_input.libraries:
            if library.dynamic_library:
                libs.append(library.dynamic_library)
            if library.static_library:
                libs.append(library.static_library)

    implementation = PY_IMPLEMENTATIONS[py_runtime.implementation_name.lower()]

    preferred_lib_exts = (".dylib", ".so", ".lib")

    root_lib = None
    for lib in libs:
        if not root_lib:
            root_lib = lib
            continue

        if lib.basename.endswith(preferred_lib_exts) and not root_lib.basename.endswith(preferred_lib_exts):
            root_lib = lib

    if not root_lib:
        fail("Failed to find python libraries for linking in '{}'".format(ctx.label))

    # This set of environment variables is required for correctly building extension
    # modules for any target platform.
    make_variable_info = platform_common.TemplateVariableInfo({
        "PYO3_CROSS": "1",
        "PYO3_CROSS_LIB_DIR": root_lib.dirname,
        "PYO3_CROSS_PYTHON_IMPLEMENTATION": implementation,
        "PYO3_CROSS_PYTHON_VERSION": version,
        "PYO3_NO_PYTHON": "1",
        "PYO3_PYTHON": "$${pwd}/" + interpreter,
    })

    return [
        platform_common.ToolchainInfo(
            make_variable_info = make_variable_info,
            python_libs = depset(libs),
            experimental_stubgen = ctx.attr._experimental_stubgen[BuildSettingInfo].value,
        ),
        make_variable_info,
        DefaultInfo(files = depset()),
    ]

pyo3_toolchain = rule(
    doc = """\
Define a toolchain which generates config data for the PyO3 for producing extension modules on any target platform.

Note that this toolchain expects the `pyo3` crate to be built with the following features:
- [`abi3`](https://pyo3.rs/v0.26.0/features.html?highlight=abi3#abi3)
- [`abi3-py3*`](https://pyo3.rs/v0.26.0/features.html?highlight=abi3#the-abi3-pyxy-features) (e.g `abi3-py311`)
- [`extension-module`](https://pyo3.rs/v0.26.0/features.html?highlight=abi3#extension-module)

When using [rules_rust's crate_universe](https://bazelbuild.github.io/rules_rust/crate_universe.html), this data can be plubmed into the target using the following snippet.
```python
annotations = {
    "pyo3-build-config": [
        crate.annotation(
            build_script_data = [
                "@rules_rust_pyo3//:current_pyo3_toolchain",
            ],
            build_script_env = {
                "PYO3_CROSS": "$(PYO3_CROSS)",
                "PYO3_CROSS_LIB_DIR": "$(PYO3_CROSS_LIB_DIR)",
                "PYO3_CROSS_PYTHON_IMPLEMENTATION": "$(PYO3_CROSS_PYTHON_IMPLEMENTATION)",
                "PYO3_CROSS_PYTHON_VERSION": "$(PYO3_CROSS_PYTHON_VERSION)",
                "PYO3_NO_PYTHON": "$(PYO3_NO_PYTHON)",
                "PYO3_PYTHON": "$(PYO3_PYTHON)",
            },
            build_script_toolchains = [
                "@rules_rust_pyo3//:current_pyo3_toolchain",
            ],
        ),
    ],
    "pyo3-ffi": [
        crate.annotation(
            build_script_data = [
                "@rules_rust_pyo3//:current_pyo3_toolchain",
            ],
            build_script_env = {
                "PYO3_CROSS": "$(PYO3_CROSS)",
                "PYO3_CROSS_LIB_DIR": "$(PYO3_CROSS_LIB_DIR)",
                "PYO3_CROSS_PYTHON_IMPLEMENTATION": "$(PYO3_CROSS_PYTHON_IMPLEMENTATION)",
                "PYO3_CROSS_PYTHON_VERSION": "$(PYO3_CROSS_PYTHON_VERSION)",
                "PYO3_NO_PYTHON": "$(PYO3_NO_PYTHON)",
                "PYO3_PYTHON": "$(PYO3_PYTHON)",
            },
            build_script_toolchains = [
                "@rules_rust_pyo3//:current_pyo3_toolchain",
            ],
        ),
    ],
},
```
""",
    implementation = _pyo3_toolchain_impl,
    attrs = {
        "_experimental_stubgen": attr.label(
            default = Label("//settings:experimental_stubgen"),
        ),
    },
    toolchains = [
        "@rules_python//python/cc:toolchain_type",
        "@rules_python//python:toolchain_type",
    ],
)

def _current_pyo3_toolchain_impl(ctx):
    toolchain = ctx.toolchains[PYO3_TOOLCHAIN]
    return [
        toolchain.make_variable_info,
        DefaultInfo(
            files = depset(transitive = [toolchain.python_libs]),
        ),
    ]

current_pyo3_toolchain = rule(
    doc = "A rule for accessing the `pyo3_toolchain` from the current configuration.",
    implementation = _current_pyo3_toolchain_impl,
    toolchains = [PYO3_TOOLCHAIN],
)

def _rust_pyo3_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            pyo3 = ctx.attr.pyo3,
            pyo3_introspection = ctx.attr.pyo3_introspection,
        ),
    ]

rust_pyo3_toolchain = rule(
    doc = """\
Define a toolchain for PyO3 Rust dependencies which power internal rules.

This toolchain is how the rules know which version of `pyo3` to link against.
""",
    implementation = _rust_pyo3_toolchain_impl,
    attrs = {
        "pyo3": attr.label(
            doc = "The PyO3 library.",
            providers = [[rust_common.crate_info], [rust_common.crate_group_info]],
            mandatory = True,
        ),
        "pyo3_introspection": attr.label(
            doc = "The PyO3 introspection library.",
            providers = [[rust_common.crate_info], [rust_common.crate_group_info]],
            mandatory = True,
        ),
    },
)

def _current_rust_pyo3_toolchain_impl(ctx):
    toolchain = ctx.toolchains[RUST_PYO3_TOOLCHAIN]
    target = toolchain.pyo3

    providers = []

    # TODO: Remove this hack when we can just pass the input target's
    # DefaultInfo provider through. Until then, we need to construct
    # a new DefaultInfo provider with the files from the input target's
    # provider.
    providers.append(
        DefaultInfo(
            files = target[DefaultInfo].files,
            runfiles = target[DefaultInfo].default_runfiles,
        ),
    )

    if rust_common.crate_info in target:
        providers.append(target[rust_common.crate_info])

    if rust_common.dep_info in target:
        providers.append(target[rust_common.dep_info])

    if rust_common.crate_group_info in target:
        providers.append(target[rust_common.crate_group_info])

    return providers

current_rust_pyo3_toolchain = rule(
    doc = "A rule for accessing the `rust_pyo3_toolchain.pyo3` library from the current configuration.",
    implementation = _current_rust_pyo3_toolchain_impl,
    toolchains = [RUST_PYO3_TOOLCHAIN],
)

def _current_rust_pyo3_introspection_toolchain_impl(ctx):
    toolchain = ctx.toolchains[RUST_PYO3_TOOLCHAIN]
    target = toolchain.pyo3_introspection

    providers = []

    # TODO: Remove this hack when we can just pass the input target's
    # DefaultInfo provider through. Until then, we need to construct
    # a new DefaultInfo provider with the files from the input target's
    # provider.
    providers.append(
        DefaultInfo(
            files = target[DefaultInfo].files,
            runfiles = target[DefaultInfo].default_runfiles,
        ),
    )

    if rust_common.crate_info in target:
        providers.append(target[rust_common.crate_info])

    if rust_common.dep_info in target:
        providers.append(target[rust_common.dep_info])

    if rust_common.crate_group_info in target:
        providers.append(target[rust_common.crate_group_info])

    return providers

current_rust_pyo3_introspection_toolchain = rule(
    doc = "A rule for accessing the `rust_pyo3_toolchain.pyo3_introspection` library from the current configuration.",
    implementation = _current_rust_pyo3_introspection_toolchain_impl,
    toolchains = [RUST_PYO3_TOOLCHAIN],
)
