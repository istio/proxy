# Debugging Cacheable Swift Modules

This is a guide to enable debugging of cacheable Swift modules in local debug builds.

At the time of writing, `lldb` depends on debugging options embedded in `.swiftmodule` files. These options include paths that are only valid on the build host. For local builds, this all just works, but for remote builds, it doesn't.

The solution is two parts:

1. Use `-no-serialize-debugging-options` globally, to prevent embedded paths
2. Use `-serialize-debugging-options` locally in one empty module

Globally disabling debugging options makes those `.swiftmodule`s usable on any machine. Locally enabling debugging options for one module provides lldb with enough info to make debugging work.

An lldb bug has been filed here: https://bugs.swift.org/browse/SR-11485

> **Note**
>
> If you don't care about cache misses, instead of following this guide you can instead disable the `swift.cacheable_swiftmodules` feature: `--features=-swift.cacheable_swiftmodules`. This is not recommended though if you use a remote cache.

### Disable Debugging Options Globally

To globally disable debugging options, use the `swift.cacheable_swiftmodules` feature, which is enabled by default, in rules_swift. What this does is ensure all modules are built explicitly with `-no-serialize-debugging-options`. It has to be explicit because `swiftc` enables `-serialize-debugging-options` in some cases.

### Add Debug Build Config

In a `BUILD` file - in this example, the root `BUILD` file, define the following [`config_setting`](https://docs.bazel.build/versions/master/be/general.html#config_setting). This will allow targets to conditionally depend on the locally built debugging module.

```python
config_setting(
    name = "debug",
    values = {
        "compilation_mode": "dbg",
    },
)
```

### Define Local Debug Target

In the `BUILD` file of your choice, define a `swift_library` with:

* A single empty source file
* Enables `-serialize-debugging-options`
* Built locally, not remote - using the `no-remote` tag

Here is one way to define the `BUILD` file, using a [`genrule`](https://docs.bazel.build/versions/master/be/general.html#genrule) to create the empty swift file.

```python
genrule(
    name = "empty",
    outs = ["empty.swift"],
    cmd = "touch $(OUTS)",
)

swift_library(
    name = "_LocalDebugOptions",
    srcs = [":empty"],
    copts = [
        "-Xfrontend",
        "-serialize-debugging-options",
    ],
    module_name = "_LocalDebugOptions",
    tags = ["no-remote"],
    visibility = ["//visibility:public"],
)
```

### Update Top Level Test Targets

Finally, for each top-level test target (`ios_unit_test`, `ios_ui_test`*, etc), conditionally add the local debugging module to the deps. This is done via the [debug config](#add-debug-build-config). In the past this also had to be done for `ios_application` targets but that has since been fixed in lldb.

```python
debug_deps = select({
    "//:debug": ["//some/path:_LocalDebugOptions"],
    "//conditions:default": [],
})

ios_unit_test(
    name = "...",
    deps = debug_deps + [
        # ...
    ],
    # ...
)
```

##### Note about `ios_unit_test`

When using a test host, the debugging module must be added to the test host target only, not the unit test target. _However_, for tests without a test host, the debugging module must be added to the unit test target.

### LLDB Settings

Additional settings may be required, depending on your build setup. For example, an Xcode Run Script may look like:

```
echo "settings set target.sdk-path $SDKROOT"
echo "settings set target.swift-framework-search-paths $FRAMEWORK_SEARCH_PATHS"
```

Other settings you can try customizing are:

* `target.clang-module-search-paths`
* `target.debug-file-search-paths`
* `target.sdk-path`
* `target.swift-extra-clang-flags`
* `target.swift-framework-search-paths`
* `target.swift-module-search-paths`
* `target.use-all-compiler-flags`
* `symbols.clang-modules-cache-path`

These settings would be written to some project specific lldbinit file which you can include directly in Xcode's scheme.
