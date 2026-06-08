"""# Rust settings

Definitions for all `@rules_rust//rust` settings
"""

load(
    "@bazel_skylib//rules:common_settings.bzl",
    "bool_flag",
    "int_flag",
    "string_flag",
)
load(
    "//rust/private:clippy.bzl",
    _capture_clippy_output = "capture_clippy_output",
    _clippy_flag = "clippy_flag",
    _clippy_flags = "clippy_flags",
    _clippy_output_diagnostics = "clippy_output_diagnostics",
)
load("//rust/private:lto.bzl", "rust_lto_flag")
load(
    "//rust/private:rustc.bzl",
    _always_enable_metadata_output_groups = "always_enable_metadata_output_groups",
    _error_format = "error_format",
    _extra_exec_rustc_env = "extra_exec_rustc_env",
    _extra_exec_rustc_flag = "extra_exec_rustc_flag",
    _extra_exec_rustc_flags = "extra_exec_rustc_flags",
    _extra_rustc_env = "extra_rustc_env",
    _extra_rustc_flag = "extra_rustc_flag",
    _extra_rustc_flags = "extra_rustc_flags",
    _no_std = "no_std",
    _per_crate_rustc_flag = "per_crate_rustc_flag",
    _rustc_output_diagnostics = "rustc_output_diagnostics",
)
load("//rust/private:unpretty.bzl", "UNPRETTY_MODES", "rust_unpretty_flag")
load(":incompatible.bzl", "incompatible_flag")

# buildifier: disable=unnamed-macro
def unpretty():
    """A build setting to control the output of `RustUnpretty*` actions

    Supported values are:
    - `ast-tree,expanded`
    - `ast-tree`
    - `expanded,hygiene`
    - `expanded,identified`
    - `expanded`
    - `hir-tree`
    - `hir,identified`
    - `hir,typed`
    - `hir`
    - `identified`
    - `mir-cfg`
    - `mir`
    - `normal`
    """
    rust_unpretty_flag(
        name = "unpretty",
        build_setting_default = UNPRETTY_MODES,
    )

# buildifier: disable=unnamed-macro
def lto():
    """A build setting which specifies the link time optimization mode used when building Rust code."""
    rust_lto_flag(
        name = "lto",
        build_setting_default = "unspecified",
    )

def rename_first_party_crates():
    """A flag controlling whether to rename first-party crates such that their names \
    encode the Bazel package and target name, instead of just the target name.

    First-party vs. third-party crates are identified using the value of
    `@rules_rust//settings:third_party_dir`.
    """
    bool_flag(
        name = "rename_first_party_crates",
        build_setting_default = False,
    )

def require_explicit_unstable_features():
    """A flag controlling whether unstable features should be disallowed by default

    If true, an empty `-Zallow-features=` will be added to the rustc command line whenever no other
    `-Zallow-features=` is present in the rustc flags. The effect is to disallow all unstable
    features by default, with the possibility to explicitly re-enable them selectively using
    `-Zallow-features=...`.
    """
    bool_flag(
        name = "require_explicit_unstable_features",
        build_setting_default = False,
    )

def third_party_dir():
    """A flag specifying the location of vendored third-party rust crates within this \
    repository that must not be renamed when `rename_first_party_crates` is enabled.

    Must be specified as a Bazel package, e.g. "//some/location/in/repo".
    """
    string_flag(
        name = "third_party_dir",
        build_setting_default = str(Label("//third_party/rust")),
    )

def use_real_import_macro():
    """A flag to control whether rust_library and rust_binary targets should \
    implicitly depend on the *real* import macro, or on a no-op target.
    """
    bool_flag(
        name = "use_real_import_macro",
        build_setting_default = False,
    )

def pipelined_compilation():
    """When set, this flag causes rustc to emit `*.rmeta` files and use them for `rlib -> rlib` dependencies.

    While this involves one extra (short) rustc invocation to build the rmeta file,
    it allows library dependencies to be unlocked much sooner, increasing parallelism during compilation.
    """
    bool_flag(
        name = "pipelined_compilation",
        build_setting_default = False,
    )

# buildifier: disable=unnamed-macro
def experimental_use_cc_common_link():
    """A flag to control whether to link rust_binary and rust_test targets using \
    cc_common.link instead of rustc.
    """
    bool_flag(
        name = "experimental_use_cc_common_link",
        build_setting_default = False,
    )

    native.config_setting(
        name = "experimental_use_cc_common_link_on",
        flag_values = {
            ":experimental_use_cc_common_link": "true",
        },
    )

    native.config_setting(
        name = "experimental_use_cc_common_link_off",
        flag_values = {
            ":experimental_use_cc_common_link": "false",
        },
    )

# buildifier: disable=unnamed-macro
def default_allocator_library():
    """A flag that determines the default allocator library for `rust_toolchain` targets."""

    native.label_flag(
        name = "default_allocator_library",
        build_setting_default = Label("//ffi/cc/allocator_library"),
    )

# buildifier: disable=unnamed-macro
def experimental_use_global_allocator():
    """A flag to indicate that a global allocator is in use when using `--@rules_rust//rust/settings:experimental_use_cc_common_link`

    Users need to specify this flag because rustc generates different set of symbols at link time when a global allocator is in use.
    When the linking is not done by rustc, the `rust_toolchain` itself provides the appropriate set of symbols.
    """
    bool_flag(
        name = "experimental_use_global_allocator",
        build_setting_default = False,
    )

    native.config_setting(
        name = "experimental_use_global_allocator_on",
        flag_values = {
            ":experimental_use_global_allocator": "true",
        },
    )

    native.config_setting(
        name = "experimental_use_global_allocator_off",
        flag_values = {
            ":experimental_use_global_allocator": "false",
        },
    )

def experimental_use_allocator_libraries_with_mangled_symbols(
        name = "experimental_use_allocator_libraries_with_mangled_symbols"):
    """A flag used to select allocator libraries implemented in rust that are compatible with the rustc allocator symbol mangling.

    The symbol mangling mechanism relies on unstable language features and requires a nightly rustc from 2025-04-05 or later.

    Rustc generates references to internal allocator symbols when building rust
    libraries.  At link time, rustc generates the definitions of these symbols.
    When rustc is not used as the final linker, we need to generate the
    definitions ourselves.  This happens for example when a rust_library is
    used as a dependency of a rust_binary, or when the
    experimental_use_cc_common_link setting is used.


    For older versions of rustc, the allocator symbol definitions can be provided
    via the `rust_toolchain`'s `allocator_library` or `global_allocator_library`
    attributes, with sample targets like `@rules_rust//ffi/cc/allocator_library`
    and `@rules_rust//ffi/cc/global_allocator_library`.

    Recent versions of rustc started mangling these allocator symbols (https://github.com/rust-lang/rust/pull/127173).
    The mangling uses a scheme that is specific to the exact version of the compiler.
    This makes the cc allocator library definitions ineffective. When rustc builds a
    staticlib it provides the mapping definitions. We rely on this and build an empty
    staticlib as a basis for the allocator definitions.

    Since the new symbol definitions are written in rust, we cannot just attach
    them as attributes on the `rust_toolchain` as the old cc versions, as that
    would create a build graph cycle (we need a `rust_toolchain` to build a
    `rust_library`, so the allocator library cannot be a rust_library directly).

    The bootstrapping cycle can be avoided by defining a separate internal
    "initial" rust toolchain specifically for building the rust allocator libraries,
    and use a transition to attach the generated libraries to the "main" rust
    toolchain. But that duplicates the whole sub-graph of the build around the
    rust toolchains, repository and supporting tools used for them.

    Instead, we define a new custom `rust_allocator_library` rule, which exposes
    the result of building the rust allocator libraries via a provider, which
    can be consumed by the rust build actions. We attach an instance of this
    as a common attribute to the rust rule set.
    """
    bool_flag(
        name = name,
        build_setting_default = False,
    )

    native.config_setting(
        name = "%s_on" % name,
        flag_values = {
            ":{}".format(name): "true",
        },
    )

    native.config_setting(
        name = "%s_off" % name,
        flag_values = {
            ":{}".format(name): "false",
        },
    )

def experimental_use_coverage_metadata_files():
    """A flag to have coverage tooling added as `coverage_common.instrumented_files_info.metadata_files` instead of \
    reporting tools like `llvm-cov` and `llvm-profdata` as runfiles to each test.
    """
    bool_flag(
        name = "experimental_use_coverage_metadata_files",
        build_setting_default = True,
    )

def toolchain_generated_sysroot():
    """A flag to set rustc --sysroot flag to the sysroot generated by rust_toolchain."""
    bool_flag(
        name = "toolchain_generated_sysroot",
        build_setting_default = True,
        visibility = ["//visibility:public"],
    )

# buildifier: disable=unnamed-macro
def incompatible_change_rust_test_compilation_output_directory():
    """A flag to put rust_test compilation outputs in the same directory as the rust_library compilation outputs.
    """
    incompatible_flag(
        name = "incompatible_change_rust_test_compilation_output_directory",
        build_setting_default = False,
        issue = "https://github.com/bazelbuild/rules_rust/issues/2827",
    )

def experimental_link_std_dylib():
    """A flag to control whether to link libstd dynamically."""
    bool_flag(
        name = "experimental_link_std_dylib",
        build_setting_default = False,
    )

def experimental_use_sh_toolchain_for_bootstrap_process_wrapper():
    """A flag to control whether the shell path from a shell toolchain (`@bazel_tools//tools/sh:toolchain_type`) \
    is embedded into the bootstrap process wrapper for the `.sh` file.
    """
    bool_flag(
        name = "experimental_use_sh_toolchain_for_bootstrap_process_wrapper",
        build_setting_default = False,
    )

def toolchain_linker_preference():
    """A flag to control which linker is preferred for linking Rust binaries.

    Accepts three values:
    - "rust": Use `rust_toolchain.linker` always (e.g., `rust-lld`). This uses rustc to invoke
      the linker directly.
    - "cc": Use the linker provided by the configured `cc_toolchain`. This uses rustc to invoke
      the C++ toolchain's linker (e.g., `clang`, `gcc`, `link.exe`).
    - "none": Default to `cc` being the preference and falling back to `rust` if no `cc_toolchain`
      is available.
    """
    string_flag(
        name = "toolchain_linker_preference",
        build_setting_default = "none",
        values = ["rust", "cc", "none"],
    )

# buildifier: disable=unnamed-macro
def clippy_toml():
    """This setting is used by the clippy rules. See https://bazelbuild.github.io/rules_rust/rust_clippy.html

    Note that this setting is actually called `clippy.toml`.
    """
    native.label_flag(
        name = "clippy.toml",
        build_setting_default = ".clippy.toml",
    )

# buildifier: disable=unnamed-macro
def rustfmt_toml():
    """This setting is used by the rustfmt rules. See https://bazelbuild.github.io/rules_rust/rust_fmt.html

    Note that this setting is actually called `rustfmt.toml`.
    """
    native.label_flag(
        name = "rustfmt.toml",
        build_setting_default = ".rustfmt.toml",
    )

# buildifier: disable=unnamed-macro
def capture_clippy_output():
    """Control whether to print clippy output or store it to a file, using the configured error_format."""
    _capture_clippy_output(
        name = "capture_clippy_output",
        build_setting_default = False,
    )

# buildifier: disable=unnamed-macro
def no_std():
    """This setting may be used to enable builds without the standard library.

    Currently only no_std + alloc is supported, which can be enabled with setting the value to "alloc".
    In the future we could add support for additional modes, e.g "core", "alloc,collections".
    """
    string_flag(
        name = "no_std",
        build_setting_default = "off",
        values = [
            "alloc",
            "off",
        ],
    )

    # A hack target to allow as to only apply the no_std mode in target config.
    _no_std(
        name = "build_target_in_no_std",
        visibility = ["//visibility:private"],
    )

    # A config setting for setting conditional `cargo_features`, `deps`, based on the `:no_std` value.
    native.config_setting(
        name = "is_no_std",
        flag_values = {
            ":build_target_in_no_std": "alloc",
        },
    )

# buildifier: disable=unnamed-macro
def error_format():
    """This setting may be changed from the command line to generate machine readable errors.
    """
    _error_format(
        name = "error_format",
        build_setting_default = "human",
    )

# buildifier: disable=unnamed-macro
def clippy_error_format():
    """This setting may be changed from the command line to generate machine readable errors.
    """
    _error_format(
        name = "clippy_error_format",
        build_setting_default = "human",
    )

# buildifier: disable=unnamed-macro
def incompatible_change_clippy_error_format():
    """A flag to enable the `clippy_error_format` setting.

    If this flag is true, Clippy uses the format set in `clippy_error_format` to
    format its diagnostics; otherwise, it uses the format set in `error_format`.
    """
    incompatible_flag(
        name = "incompatible_change_clippy_error_format",
        build_setting_default = True,
        issue = "https://github.com/bazelbuild/rules_rust/issues/3494",
    )

# buildifier: disable=unnamed-macro
def always_enable_metadata_output_groups():
    """A flag to enable the `always_enable_metadata_output_groups` setting.

    If this flag is true, all rules will support the `metadata` and
    `rustc_rmeta_output` output groups."""
    _always_enable_metadata_output_groups(
        name = "always_enable_metadata_output_groups",
        build_setting_default = False,
        visibility = ["//visibility:public"],
    )

# buildifier: disable=unnamed-macro
def rustc_output_diagnostics():
    """This setting may be changed from the command line to generate rustc diagnostics.
    """
    _rustc_output_diagnostics(
        name = "rustc_output_diagnostics",
        build_setting_default = False,
        visibility = ["//visibility:public"],
    )

# buildifier: disable=unnamed-macro
def clippy_output_diagnostics():
    """A flag to enable the `clippy_output_diagnostics` setting.

    If this flag is true, rules_rust will save clippy json output (suitable for consumption
    by rust-analyzer) in a file, available from the `clippy_output` output group. This is the
    clippy equivalent of `rustc_output_diagnostics`.
    """
    _clippy_output_diagnostics(
        name = "clippy_output_diagnostics",
        build_setting_default = False,
        visibility = ["//visibility:public"],
    )

# buildifier: disable=unnamed-macro
def clippy_flags():
    """This setting may be used to pass extra options to clippy from the command line.

    It applies across all targets.
    """
    _clippy_flags(
        name = "clippy_flags",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def extra_rustc_env():
    """This setting may be used to pass extra environment variables to rustc from the command line in non-exec configuration.

    It applies across all targets whereas environment variables set in a specific rule apply only to that target.
    This can be useful for setting build-wide env flags such as `RUSTC_BOOTSTRAP=1`.
    """
    _extra_rustc_env(
        name = "extra_rustc_env",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def clippy_flag():
    """Add a custom clippy flag from the command line with `--@rules_rust//rust/settings:clippy_flag`.

    Multiple uses are accumulated and appended after the `extra_rustc_flags`.
    """
    _clippy_flag(
        name = "clippy_flag",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def extra_rustc_flags():
    """This setting may be used to pass extra options to rustc from the command line in non-exec configuration.

    It applies across all targets whereas the rustc_flags option on targets applies only
    to that target. This can be useful for passing build-wide options such as LTO.
    """
    _extra_rustc_flags(
        name = "extra_rustc_flags",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def extra_rustc_flag():
    """Add additional rustc_flag from the command line with `--@rules_rust//rust/settings:extra_rustc_flag`.

    Multiple uses are accumulated and appended after the `extra_rustc_flags`.
    """
    _extra_rustc_flag(
        name = "extra_rustc_flag",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def extra_exec_rustc_env():
    """This setting may be used to pass extra environment variables to rustc from the command line in exec configuration.

    It applies to tools built and run during the build process, such as proc-macros and build scripts.
    This can be useful for enabling features that are needed during tool compilation.
    """
    _extra_exec_rustc_env(
        name = "extra_exec_rustc_env",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def extra_exec_rustc_flags():
    """This setting may be used to pass extra options to rustc from the command line in exec configuration.

    It applies across all targets whereas the rustc_flags option on targets applies only
    to that target. This can be useful for passing build-wide options such as LTO.
    """
    _extra_exec_rustc_flags(
        name = "extra_exec_rustc_flags",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def extra_exec_rustc_flag():
    """Add additional rustc_flags in the exec configuration from the command line with `--@rules_rust//rust/settings:extra_exec_rustc_flag`.

    Multiple uses are accumulated and appended after the extra_exec_rustc_flags.
    """
    _extra_exec_rustc_flag(
        name = "extra_exec_rustc_flag",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def experimental_per_crate_rustc_flag():
    """Add additional rustc_flag to matching crates from the command line with `--@rules_rust//rust/settings:experimental_per_crate_rustc_flag`.

    The expected flag format is prefix_filter@flag, where any crate with a label or execution path starting
    with the prefix filter will be built with the given flag. The label matching uses the canonical form of
    the label (i.e `//package:label_name`). The execution path is the relative path to your workspace directory
    including the base name (including extension) of the crate root. This flag is not applied to the exec
    configuration (proc-macros, cargo_build_script, etc). Multiple uses are accumulated.
    """
    _per_crate_rustc_flag(
        name = "experimental_per_crate_rustc_flag",
        build_setting_default = [],
    )

# buildifier: disable=unnamed-macro
def incompatible_do_not_include_data_in_compile_data():
    """A flag to control whether to include data files in compile_data.
    """
    incompatible_flag(
        name = "incompatible_do_not_include_data_in_compile_data",
        build_setting_default = True,
        issue = "https://github.com/bazelbuild/rules_rust/issues/2977",
    )

def codegen_units():
    """The default value for `--codegen-units` which also affects resource allocation for rustc actions.

    Note that any value 0 or less will prevent this flag from being passed by Bazel and allow rustc to
    perform it's default behavior.

    https://doc.rust-lang.org/rustc/codegen-options/index.html#codegen-units
    """
    int_flag(
        name = "codegen_units",
        build_setting_default = -1,
    )

# buildifier: disable=unnamed-macro
def collect_cfgs():
    """Enable collection of cfg flags with results stored in CrateInfo.cfgs.
    """
    bool_flag(
        name = "collect_cfgs",
        build_setting_default = False,
    )
