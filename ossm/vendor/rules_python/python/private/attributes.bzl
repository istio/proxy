# Copyright 2022 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Attributes for Python rules."""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(":common.bzl", "union_attrs")
load(":enum.bzl", "enum")
load(":flags.bzl", "PrecompileFlag", "PrecompileSourceRetentionFlag")
load(":py_info.bzl", "PyInfo")
load(":py_internal.bzl", "py_internal")
load(":reexports.bzl", "BuiltinPyInfo")
load(
    ":semantics.bzl",
    "DEPS_ATTR_ALLOW_RULES",
    "SRCS_ATTR_ALLOW_FILES",
)

_PackageSpecificationInfo = getattr(py_internal, "PackageSpecificationInfo", None)

# Due to how the common exec_properties attribute works, rules must add exec
# groups even if they don't actually use them. This is due to two interactions:
# 1. Rules give an error if users pass an unsupported exec group.
# 2. exec_properties is configurable, so macro-code can't always filter out
#    exec group names that aren't supported by the rule.
# The net effect is, if a user passes exec_properties to a macro, and the macro
# invokes two rules, the macro can't always ensure each rule is only passed
# valid exec groups, and is thus liable to cause an error.
#
# NOTE: These are no-op/empty exec groups. If a rule *does* support an exec
# group and needs custom settings, it should merge this dict with one that
# overrides the supported key.
REQUIRED_EXEC_GROUPS = {
    # py_binary may invoke C++ linking, or py rules may be used in combination
    # with cc rules (e.g. within the same macro), so support that exec group.
    # This exec group is defined by rules_cc for the cc rules.
    "cpp_link": exec_group(),
    "py_precompile": exec_group(),
}

_STAMP_VALUES = [-1, 0, 1]

def _precompile_attr_get_effective_value(ctx):
    precompile_flag = PrecompileFlag.get_effective_value(ctx)

    if precompile_flag == PrecompileFlag.FORCE_ENABLED:
        return PrecompileAttr.ENABLED
    if precompile_flag == PrecompileFlag.FORCE_DISABLED:
        return PrecompileAttr.DISABLED

    precompile_attr = ctx.attr.precompile
    if precompile_attr == PrecompileAttr.INHERIT:
        precompile = precompile_flag
    else:
        precompile = precompile_attr

    # Guard against bad final states because the two enums are similar with
    # magic values.
    if precompile not in (
        PrecompileAttr.ENABLED,
        PrecompileAttr.DISABLED,
    ):
        fail("Unexpected final precompile value: {}".format(repr(precompile)))

    return precompile

# buildifier: disable=name-conventions
PrecompileAttr = enum(
    # Determine the effective value from --precompile
    INHERIT = "inherit",
    # Compile Python source files at build time.
    ENABLED = "enabled",
    # Don't compile Python source files at build time.
    DISABLED = "disabled",
    get_effective_value = _precompile_attr_get_effective_value,
)

# buildifier: disable=name-conventions
PrecompileInvalidationModeAttr = enum(
    # Automatically pick a value based on build settings.
    AUTO = "auto",
    # Use the pyc file if the hash of the originating source file matches the
    # hash recorded in the pyc file.
    CHECKED_HASH = "checked_hash",
    # Always use the pyc file, even if the originating source has changed.
    UNCHECKED_HASH = "unchecked_hash",
)

def _precompile_source_retention_get_effective_value(ctx):
    attr_value = ctx.attr.precompile_source_retention
    if attr_value == PrecompileSourceRetentionAttr.INHERIT:
        attr_value = PrecompileSourceRetentionFlag.get_effective_value(ctx)

    if attr_value not in (
        PrecompileSourceRetentionAttr.KEEP_SOURCE,
        PrecompileSourceRetentionAttr.OMIT_SOURCE,
    ):
        fail("Unexpected final precompile_source_retention value: {}".format(repr(attr_value)))
    return attr_value

# buildifier: disable=name-conventions
PrecompileSourceRetentionAttr = enum(
    INHERIT = "inherit",
    KEEP_SOURCE = "keep_source",
    OMIT_SOURCE = "omit_source",
    get_effective_value = _precompile_source_retention_get_effective_value,
)

def _pyc_collection_attr_is_pyc_collection_enabled(ctx):
    pyc_collection = ctx.attr.pyc_collection
    if pyc_collection == PycCollectionAttr.INHERIT:
        precompile_flag = PrecompileFlag.get_effective_value(ctx)
        if precompile_flag in (PrecompileFlag.ENABLED, PrecompileFlag.FORCE_ENABLED):
            pyc_collection = PycCollectionAttr.INCLUDE_PYC
        else:
            pyc_collection = PycCollectionAttr.DISABLED

    if pyc_collection not in (PycCollectionAttr.INCLUDE_PYC, PycCollectionAttr.DISABLED):
        fail("Unexpected final pyc_collection value: {}".format(repr(pyc_collection)))

    return pyc_collection == PycCollectionAttr.INCLUDE_PYC

# buildifier: disable=name-conventions
PycCollectionAttr = enum(
    INHERIT = "inherit",
    INCLUDE_PYC = "include_pyc",
    DISABLED = "disabled",
    is_pyc_collection_enabled = _pyc_collection_attr_is_pyc_collection_enabled,
)

def create_stamp_attr(**kwargs):
    return {
        "stamp": attr.int(
            values = _STAMP_VALUES,
            doc = """
Whether to encode build information into the binary. Possible values:

* `stamp = 1`: Always stamp the build information into the binary, even in
  `--nostamp` builds. **This setting should be avoided**, since it potentially kills
  remote caching for the binary and any downstream actions that depend on it.
* `stamp = 0`: Always replace build information by constant values. This gives
  good build result caching.
* `stamp = -1`: Embedding of build information is controlled by the
  `--[no]stamp` flag.

Stamped binaries are not rebuilt unless their dependencies change.

WARNING: Stamping can harm build performance by reducing cache hits and should
be avoided if possible.
""",
            **kwargs
        ),
    }

def create_srcs_attr(*, mandatory):
    return {
        "srcs": attr.label_list(
            # Google builds change the set of allowed files.
            allow_files = SRCS_ATTR_ALLOW_FILES,
            mandatory = mandatory,
            # Necessary for --compile_one_dependency to work.
            flags = ["DIRECT_COMPILE_TIME_INPUT"],
            doc = """
The list of Python source files that are processed to create the target. This
includes all your checked-in code and may include generated source files.  The
`.py` files belong in `srcs` and library targets belong in `deps`. Other binary
files that may be needed at run time belong in `data`.
""",
        ),
    }

SRCS_VERSION_ALL_VALUES = ["PY2", "PY2ONLY", "PY2AND3", "PY3", "PY3ONLY"]
SRCS_VERSION_NON_CONVERSION_VALUES = ["PY2AND3", "PY2ONLY", "PY3ONLY"]

def create_srcs_version_attr(values):
    return {
        "srcs_version": attr.string(
            default = "PY2AND3",
            values = values,
            doc = "Defunct, unused, does nothing.",
        ),
    }

def copy_common_binary_kwargs(kwargs):
    return {
        key: kwargs[key]
        for key in BINARY_ATTR_NAMES
        if key in kwargs
    }

def copy_common_test_kwargs(kwargs):
    return {
        key: kwargs[key]
        for key in TEST_ATTR_NAMES
        if key in kwargs
    }

CC_TOOLCHAIN = {
    # NOTE: The `cc_helper.find_cpp_toolchain()` function expects the attribute
    # name to be this name.
    "_cc_toolchain": attr.label(default = "@bazel_tools//tools/cpp:current_cc_toolchain"),
}

# The common "data" attribute definition.
DATA_ATTRS = {
    # NOTE: The "flags" attribute is deprecated, but there isn't an alternative
    # way to specify that constraints should be ignored.
    "data": attr.label_list(
        allow_files = True,
        flags = ["SKIP_CONSTRAINTS_OVERRIDE"],
        doc = """
The list of files need by this library at runtime. See comments about
the [`data` attribute typically defined by rules](https://bazel.build/reference/be/common-definitions#typical-attributes).

There is no `py_embed_data` like there is `cc_embed_data` and `go_embed_data`.
This is because Python has a concept of runtime resources.
""",
    ),
}

def _create_native_rules_allowlist_attrs():
    if py_internal:
        # The fragment and name are validated when configuration_field is called
        default = configuration_field(
            fragment = "py",
            name = "native_rules_allowlist",
        )

        # A None provider isn't allowed
        providers = [_PackageSpecificationInfo]
    else:
        default = None
        providers = []

    return {
        "_native_rules_allowlist": attr.label(
            default = default,
            providers = providers,
        ),
    }

NATIVE_RULES_ALLOWLIST_ATTRS = _create_native_rules_allowlist_attrs()

# Attributes common to all rules.
COMMON_ATTRS = union_attrs(
    DATA_ATTRS,
    NATIVE_RULES_ALLOWLIST_ATTRS,
    # buildifier: disable=attr-licenses
    {
        # NOTE: This attribute is deprecated and slated for removal.
        "distribs": attr.string_list(),
        # TODO(b/148103851): This attribute is deprecated and slated for
        # removal.
        # NOTE: The license attribute is missing in some Java integration tests,
        # so fallback to a regular string_list for that case.
        # buildifier: disable=attr-license
        "licenses": attr.license() if hasattr(attr, "license") else attr.string_list(),
    },
    allow_none = True,
)

IMPORTS_ATTRS = {
    "imports": attr.string_list(
        doc = """
List of import directories to be added to the PYTHONPATH.

Subject to "Make variable" substitution. These import directories will be added
for this rule and all rules that depend on it (note: not the rules this rule
depends on. Each directory will be added to `PYTHONPATH` by `py_binary` rules
that depend on this rule. The strings are repo-runfiles-root relative,

Absolute paths (paths that start with `/`) and paths that references a path
above the execution root are not allowed and will result in an error.
""",
    ),
}

_MaybeBuiltinPyInfo = [[BuiltinPyInfo]] if BuiltinPyInfo != None else []

# Attributes common to rules accepting Python sources and deps.
PY_SRCS_ATTRS = union_attrs(
    {
        "deps": attr.label_list(
            providers = [
                [PyInfo],
                [CcInfo],
            ] + _MaybeBuiltinPyInfo,
            # TODO(b/228692666): Google-specific; remove these allowances once
            # the depot is cleaned up.
            allow_rules = DEPS_ATTR_ALLOW_RULES,
            doc = """
List of additional libraries to be linked in to the target.
See comments about
the [`deps` attribute typically defined by
rules](https://bazel.build/reference/be/common-definitions#typical-attributes).
These are typically `py_library` rules.

Targets that only provide data files used at runtime belong in the `data`
attribute.
""",
        ),
        "precompile": attr.string(
            doc = """
Whether py source files **for this target** should be precompiled.

Values:

* `inherit`: Allow the downstream binary decide if precompiled files are used.
* `enabled`: Compile Python source files at build time.
* `disabled`: Don't compile Python source files at build time.

:::{seealso}

* The {flag}`--precompile` flag, which can override this attribute in some cases
  and will affect all targets when building.
* The {obj}`pyc_collection` attribute for transitively enabling precompiling on
  a per-target basis.
* The [Precompiling](precompiling) docs for a guide about using precompiling.
:::
""",
            default = PrecompileAttr.INHERIT,
            values = sorted(PrecompileAttr.__members__.values()),
        ),
        "precompile_invalidation_mode": attr.string(
            doc = """
How precompiled files should be verified to be up-to-date with their associated
source files. Possible values are:
* `auto`: The effective value will be automatically determined by other build
  settings.
* `checked_hash`: Use the pyc file if the hash of the source file matches the hash
  recorded in the pyc file. This is most useful when working with code that
  you may modify.
* `unchecked_hash`: Always use the pyc file; don't check the pyc's hash against
  the source file. This is most useful when the code won't be modified.

For more information on pyc invalidation modes, see
https://docs.python.org/3/library/py_compile.html#py_compile.PycInvalidationMode
""",
            default = PrecompileInvalidationModeAttr.AUTO,
            values = sorted(PrecompileInvalidationModeAttr.__members__.values()),
        ),
        "precompile_optimize_level": attr.int(
            doc = """
The optimization level for precompiled files.

For more information about optimization levels, see the `compile()` function's
`optimize` arg docs at https://docs.python.org/3/library/functions.html#compile

NOTE: The value `-1` means "current interpreter", which will be the interpreter
used _at build time when pycs are generated_, not the interpreter used at
runtime when the code actually runs.
""",
            default = 0,
        ),
        "precompile_source_retention": attr.string(
            default = PrecompileSourceRetentionAttr.INHERIT,
            values = sorted(PrecompileSourceRetentionAttr.__members__.values()),
            doc = """
Determines, when a source file is compiled, if the source file is kept
in the resulting output or not. Valid values are:

* `inherit`: Inherit the value from the {flag}`--precompile_source_retention` flag.
* `keep_source`: Include the original Python source.
* `omit_source`: Don't include the original py source.
""",
        ),
        "pyi_deps": attr.label_list(
            doc = """
Dependencies providing type definitions the library needs.

These are dependencies that satisfy imports guarded by `typing.TYPE_CHECKING`.
These are build-time only dependencies and not included as part of a runnable
program (packaging rules may include them, however).

:::{versionadded} 1.1.0
:::
""",
            providers = [
                [PyInfo],
                [CcInfo],
            ] + _MaybeBuiltinPyInfo,
        ),
        "pyi_srcs": attr.label_list(
            doc = """
Type definition files for the library.

These are typically `.pyi` files, but other file types for type-checker specific
formats are allowed. These files are build-time only dependencies and not included
as part of a runnable program (packaging rules may include them, however).

:::{versionadded} 1.1.0
:::
""",
            allow_files = True,
        ),
        # Required attribute, but details vary by rule.
        # Use create_srcs_attr to create one.
        "srcs": None,
        # NOTE: In Google, this attribute is deprecated, and can only
        # effectively be PY3 or PY3ONLY. Externally, with Bazel, this attribute
        # has a separate story.
        # Required attribute, but the details vary by rule.
        # Use create_srcs_version_attr to create one.
        "srcs_version": None,
        "_precompile_flag": attr.label(
            default = "//python/config_settings:precompile",
            providers = [BuildSettingInfo],
        ),
        "_precompile_source_retention_flag": attr.label(
            default = "//python/config_settings:precompile_source_retention",
            providers = [BuildSettingInfo],
        ),
        # Force enabling auto exec groups, see
        # https://bazel.build/extending/auto-exec-groups#how-enable-particular-rule
        "_use_auto_exec_groups": attr.bool(default = True),
    },
    allow_none = True,
)

# Attributes specific to Python executable-equivalent rules. Such rules may not
# accept Python sources (e.g. some packaged-version of a py_test/py_binary), but
# still accept Python source-agnostic settings.
AGNOSTIC_EXECUTABLE_ATTRS = union_attrs(
    DATA_ATTRS,
    {
        "env": attr.string_dict(
            doc = """\
Dictionary of strings; optional; values are subject to `$(location)` and "Make
variable" substitution.

Specifies additional environment variables to set when the target is executed by
`test` or `run`.
""",
        ),
        # The value is required, but varies by rule and/or rule type. Use
        # create_stamp_attr to create one.
        "stamp": None,
    },
    allow_none = True,
)

# Attributes specific to Python test-equivalent executable rules. Such rules may
# not accept Python sources (e.g. some packaged-version of a py_test/py_binary),
# but still accept Python source-agnostic settings.
AGNOSTIC_TEST_ATTRS = union_attrs(
    AGNOSTIC_EXECUTABLE_ATTRS,
    # Tests have stamping disabled by default.
    create_stamp_attr(default = 0),
    {
        "env_inherit": attr.string_list(
            doc = """\
List of strings; optional

Specifies additional environment variables to inherit from the external
environment when the test is executed by bazel test.
""",
        ),
        # TODO(b/176993122): Remove when Bazel automatically knows to run on darwin.
        "_apple_constraints": attr.label_list(
            default = [
                "@platforms//os:ios",
                "@platforms//os:macos",
                "@platforms//os:tvos",
                "@platforms//os:visionos",
                "@platforms//os:watchos",
            ],
        ),
    },
)

# Attributes specific to Python binary-equivalent executable rules. Such rules may
# not accept Python sources (e.g. some packaged-version of a py_test/py_binary),
# but still accept Python source-agnostic settings.
AGNOSTIC_BINARY_ATTRS = union_attrs(
    AGNOSTIC_EXECUTABLE_ATTRS,
    create_stamp_attr(default = -1),
)

# Attribute names common to all Python rules
COMMON_ATTR_NAMES = [
    "compatible_with",
    "deprecation",
    "distribs",  # NOTE: Currently common to all rules, but slated for removal
    "exec_compatible_with",
    "exec_properties",
    "features",
    "restricted_to",
    "tags",
    "target_compatible_with",
    # NOTE: The testonly attribute requires careful handling: None/unset means
    # to use the `package(default_testonly`) value, which isn't observable
    # during the loading phase.
    "testonly",
    "toolchains",
    "visibility",
] + list(COMMON_ATTRS)  # Use list() instead .keys() so it's valid Python

# Attribute names common to all test=True rules
TEST_ATTR_NAMES = COMMON_ATTR_NAMES + [
    "args",
    "size",
    "timeout",
    "flaky",
    "shard_count",
    "local",
] + list(AGNOSTIC_TEST_ATTRS)  # Use list() instead .keys() so it's valid Python

# Attribute names common to all executable=True rules
BINARY_ATTR_NAMES = COMMON_ATTR_NAMES + [
    "args",
    "output_licenses",  # NOTE: Common to all rules, but slated for removal
] + list(AGNOSTIC_BINARY_ATTRS)  # Use list() instead .keys() so it's valid Python
