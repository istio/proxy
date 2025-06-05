# Copyright 2015 The Bazel Authors. All rights reserved.
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

"""Utility functions not specific to the rust toolchain."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", find_rules_cc_toolchain = "find_cpp_toolchain")
load(":compat.bzl", "abs")
load(":providers.bzl", "BuildInfo", "CrateGroupInfo", "CrateInfo", "DepInfo", "DepVariantInfo", "RustcOutputDiagnosticsInfo")

UNSUPPORTED_FEATURES = [
    "thin_lto",
    "module_maps",
    "use_header_modules",
    "fdo_instrument",
    "fdo_optimize",
    # This feature is unsupported by definition. The authors of C++ toolchain
    # configuration can place any linker flags that should not be applied when
    # linking Rust targets in a feature with this name.
    "rules_rust_unsupported_feature",
]

def find_toolchain(ctx):
    """Finds the first rust toolchain that is configured.

    Args:
        ctx (ctx): The ctx object for the current target.

    Returns:
        rust_toolchain: A Rust toolchain context.
    """
    return ctx.toolchains[Label("//rust:toolchain_type")]

def find_cc_toolchain(ctx, extra_unsupported_features = tuple()):
    """Extracts a CcToolchain from the current target's context

    Args:
        ctx (ctx): The current target's rule context object
        extra_unsupported_features (sequence of str): Extra featrures to disable

    Returns:
        tuple: A tuple of (CcToolchain, FeatureConfiguration)
    """
    cc_toolchain = find_rules_cc_toolchain(ctx)

    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = UNSUPPORTED_FEATURES + ctx.disabled_features +
                               list(extra_unsupported_features),
    )
    return cc_toolchain, feature_configuration

# TODO: Replace with bazel-skylib's `path.dirname`. This requires addressing some
# dependency issues or generating docs will break.
def relativize(path, start):
    """Returns the relative path from start to path.

    Args:
        path (str): The path to relativize.
        start (str): The ancestor path against which to relativize.

    Returns:
        str: The portion of `path` that is relative to `start`.
    """
    src_parts = _path_parts(start)
    dest_parts = _path_parts(path)
    n = 0
    for src_part, dest_part in zip(src_parts, dest_parts):
        if src_part != dest_part:
            break
        n += 1

    relative_path = ""
    for _ in range(n, len(src_parts)):
        relative_path += "../"
    relative_path += "/".join(dest_parts[n:])

    return relative_path

def _path_parts(path):
    """Takes a path and returns a list of its parts with all "." elements removed.

    The main use case of this function is if one of the inputs to relativize()
    is a relative path, such as "./foo".

    Args:
      path (str): A string representing a unix path

    Returns:
      list: A list containing the path parts with all "." elements removed.
    """
    path_parts = path.split("/")
    return [part for part in path_parts if part != "."]

def get_lib_name_default(lib):
    """Returns the name of a library artifact.

    Args:
        lib (File): A library file

    Returns:
        str: The name of the library
    """
    # On macos and windows, dynamic/static libraries always end with the
    # extension and potential versions will be before the extension, and should
    # be part of the library name.
    # On linux, the version usually comes after the extension.
    # So regardless of the platform we want to find the extension and make
    # everything left to it the library name.

    # Search for the extension - starting from the right - by removing any
    # trailing digit.
    comps = lib.basename.split(".")
    for comp in reversed(comps):
        if comp.isdigit():
            comps.pop()
        else:
            break

    # The library name is now everything minus the extension.
    libname = ".".join(comps[:-1])

    if libname.startswith("lib"):
        return libname[3:]
    else:
        return libname

# TODO: Could we remove this function in favor of a "windows" parameter in the
# above function? It looks like currently lambdas cannot accept local parameters
# so the following doesn't work:
#     args.add_all(
#         cc_toolchain.dynamic_runtime_lib(feature_configuration = feature_configuration),
#         map_each = lambda x: get_lib_name(x, for_windows = toolchain.target_os.startswith("windows)),
#         format_each = "-ldylib=%s",
#     )
def get_lib_name_for_windows(lib):
    """Returns the name of a library artifact for Windows builds.

    Args:
        lib (File): A library file

    Returns:
        str: The name of the library
    """
    # On macos and windows, dynamic/static libraries always end with the
    # extension and potential versions will be before the extension, and should
    # be part of the library name.
    # On linux, the version usually comes after the extension.
    # So regardless of the platform we want to find the extension and make
    # everything left to it the library name.

    # Search for the extension - starting from the right - by removing any
    # trailing digit.
    comps = lib.basename.split(".")
    for comp in reversed(comps):
        if comp.isdigit():
            comps.pop()
        else:
            break

    # The library name is now everything minus the extension.
    libname = ".".join(comps[:-1])

    return libname

def determine_output_hash(crate_root, label):
    """Generates a hash of the crate root file's path.

    Args:
        crate_root (File): The crate's root file (typically `lib.rs`).
        label (Label): The label of the target.

    Returns:
        str: A string representation of the hash.
    """

    # Take the absolute value of hash() since it could be negative.
    h = abs(hash(crate_root.path) + hash(repr(label)))
    return repr(h)

def get_preferred_artifact(library_to_link, use_pic):
    """Get the first available library to link from a LibraryToLink object.

    Args:
        library_to_link (LibraryToLink): See the followg links for additional details:
            https://docs.bazel.build/versions/master/skylark/lib/LibraryToLink.html
        use_pic: If set, prefers pic_static_library over static_library.

    Returns:
        File: Returns the first valid library type (only one is expected)
    """
    if use_pic:
        # Order consistent with https://github.com/bazelbuild/bazel/blob/815dfdabb7df31d4e99b6fc7616ff8e2f9385d98/src/main/java/com/google/devtools/build/lib/rules/cpp/CcLinkingContext.java#L437.
        return (
            library_to_link.pic_static_library or
            library_to_link.static_library or
            library_to_link.interface_library or
            library_to_link.dynamic_library
        )
    else:
        return (
            library_to_link.static_library or
            library_to_link.pic_static_library or
            library_to_link.interface_library or
            library_to_link.dynamic_library
        )

# The normal ctx.expand_location, but with an additional deduplication step.
# We do this to work around a potential crash, see
# https://github.com/bazelbuild/bazel/issues/16664
def dedup_expand_location(ctx, input, targets = []):
    return ctx.expand_location(input, _deduplicate(targets))

def _deduplicate(xs):
    return {x: True for x in xs}.keys()

def concat(xss):
    return [x for xs in xss for x in xs]

def _expand_location_for_build_script_runner(ctx, env, data, known_variables):
    """A trivial helper for `expand_dict_value_locations` and `expand_list_element_locations`

    Args:
        ctx (ctx): The rule's context object
        env (str): The value possibly containing location macros to expand.
        data (sequence of Targets): See one of the parent functions.
        known_variables (dict): Make variables (probably from toolchains) to substitute in when doing make variable expansion.

    Returns:
        string: The location-macro expanded version of the string.
    """
    for directive in ("$(execpath ", "$(location "):
        if directive in env:
            # build script runner will expand pwd to execroot for us
            env = env.replace(directive, "$${pwd}/" + directive)
    return ctx.expand_make_variables(
        env,
        dedup_expand_location(ctx, env, data),
        known_variables,
    )

def expand_dict_value_locations(ctx, env, data, known_variables):
    """Performs location-macro expansion on string values.

    $(execroot ...) and $(location ...) are prefixed with ${pwd},
    which process_wrapper and build_script_runner will expand at run time
    to the absolute path. This is necessary because include_str!() is relative
    to the currently compiled file, and build scripts run relative to the
    manifest dir, so we can not use execroot-relative paths.

    $(rootpath ...) is unmodified, and is useful for passing in paths via
    rustc_env that are encoded in the binary with env!(), but utilized at
    runtime, such as in tests. The absolute paths are not usable in this case,
    as compilation happens in a separate sandbox folder, so when it comes time
    to read the file at runtime, the path is no longer valid.

    For detailed documentation, see:
    - [`expand_location`](https://bazel.build/rules/lib/ctx#expand_location)
    - [`expand_make_variables`](https://bazel.build/rules/lib/ctx#expand_make_variables)

    Args:
        ctx (ctx): The rule's context object
        env (dict): A dict whose values we iterate over
        data (sequence of Targets): The targets which may be referenced by
            location macros. This is expected to be the `data` attribute of
            the target, though may have other targets or attributes mixed in.
        known_variables (dict): Make variables (probably from toolchains) to substitute in when doing make variable expansion.

    Returns:
        dict: A dict of environment variables with expanded location macros
    """
    return dict([(k, _expand_location_for_build_script_runner(ctx, v, data, known_variables)) for (k, v) in env.items()])

def expand_list_element_locations(ctx, args, data, known_variables):
    """Performs location-macro expansion on a list of string values.

    $(execroot ...) and $(location ...) are prefixed with ${pwd},
    which process_wrapper and build_script_runner will expand at run time
    to the absolute path.

    For detailed documentation, see:
    - [`expand_location`](https://bazel.build/rules/lib/ctx#expand_location)
    - [`expand_make_variables`](https://bazel.build/rules/lib/ctx#expand_make_variables)

    Args:
        ctx (ctx): The rule's context object
        args (list): A list we iterate over
        data (sequence of Targets): The targets which may be referenced by
            location macros. This is expected to be the `data` attribute of
            the target, though may have other targets or attributes mixed in.
        known_variables (dict): Make variables (probably from toolchains) to substitute in when doing make variable expansion.

    Returns:
        list: A list of arguments with expanded location macros
    """
    return [_expand_location_for_build_script_runner(ctx, arg, data, known_variables) for arg in args]

def name_to_crate_name(name):
    """Converts a build target's name into the name of its associated crate.

    Crate names cannot contain certain characters, such as -, which are allowed
    in build target names. All illegal characters will be converted to
    underscores.

    This is a similar conversion as that which cargo does, taking a
    `Cargo.toml`'s `package.name` and canonicalizing it

    Note that targets can specify the `crate_name` attribute to customize their
    crate name; in situations where this is important, use the
    compute_crate_name() function instead.

    Args:
        name (str): The name of the target.

    Returns:
        str: The name of the crate for this target.
    """
    for illegal in ("-", "/"):
        name = name.replace(illegal, "_")
    return name

def _invalid_chars_in_crate_name(name):
    """Returns any invalid chars in the given crate name.

    Args:
        name (str): Name to test.

    Returns:
        list: List of invalid characters in the crate name.
    """

    return dict([(c, ()) for c in name.elems() if not (c.isalnum() or c == "_")]).keys()

def compute_crate_name(workspace_name, label, toolchain, name_override = None):
    """Returns the crate name to use for the current target.

    Args:
        workspace_name (string): The current workspace name.
        label (struct): The label of the current target.
        toolchain (struct): The toolchain in use for the target.
        name_override (String): An optional name to use (as an override of label.name).

    Returns:
        str: The crate name to use for this target.
    """
    if name_override:
        invalid_chars = _invalid_chars_in_crate_name(name_override)
        if invalid_chars:
            fail("Crate name '{}' contains invalid character(s): {}".format(
                name_override,
                " ".join(invalid_chars),
            ))
        return name_override

    if (toolchain and label and toolchain._rename_first_party_crates and
        should_encode_label_in_crate_name(workspace_name, label, toolchain._third_party_dir)):
        crate_name = encode_label_as_crate_name(label.package, label.name)
    else:
        crate_name = name_to_crate_name(label.name)

    invalid_chars = _invalid_chars_in_crate_name(crate_name)
    if invalid_chars:
        fail(
            "Crate name '{}' ".format(crate_name) +
            "derived from Bazel target name '{}' ".format(label.name) +
            "contains invalid character(s): {}\n".format(" ".join(invalid_chars)) +
            "Consider adding a crate_name attribute to set a valid crate name",
        )
    return crate_name

def dedent(doc_string):
    """Remove any common leading whitespace from every line in text.

    This functionality is similar to python's `textwrap.dedent` functionality
    https://docs.python.org/3/library/textwrap.html#textwrap.dedent

    Args:
        doc_string (str): A docstring style string

    Returns:
        str: A string optimized for stardoc rendering
    """
    lines = doc_string.splitlines()
    if not lines:
        return doc_string

    # If the first line is empty, use the second line
    first_line = lines[0]
    if not first_line:
        first_line = lines[1]

    # Detect how much space prepends the first line and subtract that from all lines
    space_count = len(first_line) - len(first_line.lstrip())

    # If there are no leading spaces, do not alter the docstring
    if space_count == 0:
        return doc_string
    else:
        # Remove the leading block of spaces from the current line
        block = " " * space_count
        return "\n".join([line.replace(block, "", 1).rstrip() for line in lines])

def make_static_lib_symlink(ctx_package, actions, rlib_file):
    """Add a .a symlink to an .rlib file.

    The name of the symlink is derived from the <name> of the <name>.rlib file as follows:
    * `<name>.a`, if <name> starts with `lib`
    * `lib<name>.a`, otherwise.

    For example, the name of the symlink for
    * `libcratea.rlib` is `libcratea.a`
    * `crateb.rlib` is `libcrateb.a`.

    Args:
        ctx_package (string): The rule's context package name.
        actions (actions): The rule's context actions object.
        rlib_file (File): The file to symlink, which must end in .rlib.

    Returns:
        The symlink's File.
    """

    if not rlib_file.basename.endswith(".rlib"):
        fail("file is not an .rlib: ", rlib_file.basename)
    basename = rlib_file.basename[:-5]
    if not basename.startswith("lib"):
        basename = "lib" + basename

    # The .a symlink below is created as a sibling to the .rlib file.
    # Bazel doesn't allow creating a symlink outside of the rule's package,
    # so if the .rlib file comes from a different package, first symlink it
    # to the rule's package. The name of the new .rlib symlink is derived
    # as the name of the original .rlib relative to its package.
    if rlib_file.owner.package != ctx_package:
        new_path = rlib_file.short_path.removeprefix(rlib_file.owner.package).removeprefix("/")
        new_rlib_file = actions.declare_file(new_path)
        actions.symlink(output = new_rlib_file, target_file = rlib_file)
        rlib_file = new_rlib_file

    dot_a = actions.declare_file(basename + ".a", sibling = rlib_file)
    actions.symlink(output = dot_a, target_file = rlib_file)
    return dot_a

def is_exec_configuration(ctx):
    """Determine if a context is building for the exec configuration.

    This is helpful when processing command line flags that should apply
    to the target configuration but not the exec configuration.

    Args:
        ctx (ctx): The ctx object for the current target.

    Returns:
        True if the exec configuration is detected, False otherwise.
    """

    # TODO(djmarcin): Is there any better way to determine cfg=exec?
    return ctx.genfiles_dir.path.find("-exec") != -1

def transform_deps(deps):
    """Transforms a [Target] into [DepVariantInfo].

    This helper function is used to transform ctx.attr.deps and ctx.attr.proc_macro_deps into
    [DepVariantInfo].

    Args:
        deps (list of Targets): Dependencies coming from ctx.attr.deps or ctx.attr.proc_macro_deps

    Returns:
        list of DepVariantInfos.
    """
    return [DepVariantInfo(
        crate_info = dep[CrateInfo] if CrateInfo in dep else None,
        dep_info = dep[DepInfo] if DepInfo in dep else None,
        build_info = dep[BuildInfo] if BuildInfo in dep else None,
        cc_info = dep[CcInfo] if CcInfo in dep else None,
        crate_group_info = dep[CrateGroupInfo] if CrateGroupInfo in dep else None,
    ) for dep in deps]

def get_import_macro_deps(ctx):
    """Returns a list of targets to be added to proc_macro_deps.

    Args:
        ctx (struct): the ctx of the current target.

    Returns:
        list of Targets. Either empty (if the fake import macro implementation
        is being used), or a singleton list with the real implementation.
    """
    if not hasattr(ctx.attr, "_import_macro_dep"):
        return []

    if ctx.attr._import_macro_dep.label.name == "fake_import_macro_impl":
        return []

    return [ctx.attr._import_macro_dep]

def should_encode_label_in_crate_name(workspace_name, label, third_party_dir):
    """Determines if the crate's name should include the Bazel label, encoded.

    Crate names may only encode the label if the target is in the current repo,
    the target is not in the third_party_dir, and the current repo is not
    rules_rust.

    Args:
        workspace_name (string): The name of the current workspace.
        label (Label): The package in question.
        third_party_dir (string): The directory in which third-party packages are kept.

    Returns:
        True if the crate name should encode the label, False otherwise.
    """

    # TODO(hlopko): This code assumes a monorepo; make it work with external
    # repositories as well.
    return (
        workspace_name != "rules_rust" and
        not label.workspace_root and
        not ("//" + label.package + "/").startswith(third_party_dir + "/")
    )

# This is a list of pairs, where the first element of the pair is a character
# that is allowed in Bazel package or target names but not in crate names; and
# the second element is an encoding of that char suitable for use in a crate
# name.
_encodings = (
    (":", "x"),
    ("!", "excl"),
    ("%", "prc"),
    ("@", "ao"),
    ("^", "caret"),
    ("`", "bt"),
    (" ", "sp"),
    ("\"", "dq"),
    ("#", "octo"),
    ("$", "dllr"),
    ("&", "amp"),
    ("'", "sq"),
    ("(", "lp"),
    (")", "rp"),
    ("*", "astr"),
    ("-", "d"),
    ("+", "pl"),
    (",", "cm"),
    (";", "sm"),
    ("<", "la"),
    ("=", "eq"),
    (">", "ra"),
    ("?", "qm"),
    ("[", "lbk"),
    ("]", "rbk"),
    ("{", "lbe"),
    ("|", "pp"),
    ("}", "rbe"),
    ("~", "td"),
    ("/", "y"),
    (".", "pd"),
)

# For each of the above encodings, we generate two substitution rules: one that
# ensures any occurrences of the encodings themselves in the package/target
# aren't clobbered by this translation, and one that does the encoding itself.
# We also include a rule that protects the clobbering-protection rules from
# getting clobbered.
_substitutions = [("_z", "_zz_")] + [
    subst
    for (pattern, replacement) in _encodings
    for subst in (
        ("_{}_".format(replacement), "_z{}_".format(replacement)),
        (pattern, "_{}_".format(replacement)),
    )
]

# Expose the substitutions for testing only.
substitutions_for_testing = _substitutions

def encode_label_as_crate_name(package, name):
    """Encodes the package and target names in a format suitable for a crate name.

    Args:
        package (string): The package of the target in question.
        name (string): The name of the target in question.

    Returns:
        A string that encodes the package and target name, to be used as the crate's name.
    """
    return _encode_raw_string(package + ":" + name)

def _encode_raw_string(str):
    """Encodes a string using the above encoding format.

    Args:
        str (string): The string to be encoded.

    Returns:
        An encoded version of the input string.
    """
    return _replace_all(str, _substitutions)

# Expose the underlying encoding function for testing only.
encode_raw_string_for_testing = _encode_raw_string

def decode_crate_name_as_label_for_testing(crate_name):
    """Decodes a crate_name that was encoded by encode_label_as_crate_name.

    This is used to check that the encoding is bijective; it is expected to only
    be used in tests.

    Args:
        crate_name (string): The name of the crate.

    Returns:
        A string representing the Bazel label (package and target).
    """
    return _replace_all(crate_name, [(t[1], t[0]) for t in _substitutions])

def _replace_all(string, substitutions):
    """Replaces occurrences of the given patterns in `string`.

    There are a few reasons this looks complicated:
    * The substitutions are performed with some priority, i.e. patterns that are
      listed first in `substitutions` are higher priority than patterns that are
      listed later.
    * We also take pains to avoid doing replacements that overlap with each
      other, since overlaps invalidate pattern matches.
    * To avoid hairy offset invalidation, we apply the substitutions
      right-to-left.
    * To avoid the "_quote" -> "_quotequote_" rule introducing new pattern
      matches later in the string during decoding, we take the leftmost
      replacement, in cases of overlap.  (Note that no rule can induce new
      pattern matches *earlier* in the string.) (E.g. "_quotedot_" encodes to
      "_quotequote_dot_". Note that "_quotequote_" and "_dot_" both occur in
      this string, and overlap.).

    Args:
        string (string): the string in which the replacements should be performed.
        substitutions: the list of patterns and replacements to apply.

    Returns:
        A string with the appropriate substitutions performed.
    """

    # Find the highest-priority pattern matches for each string index, going
    # left-to-right and skipping indices that are already involved in a
    # pattern match.
    plan = {}
    matched_indices_set = {}
    for pattern_start in range(len(string)):
        if pattern_start in matched_indices_set:
            continue
        for (pattern, replacement) in substitutions:
            if not string.startswith(pattern, pattern_start):
                continue
            length = len(pattern)
            plan[pattern_start] = (length, replacement)
            matched_indices_set.update([(pattern_start + i, True) for i in range(length)])
            break

    # Execute the replacement plan, working from right to left.
    for pattern_start in sorted(plan.keys(), reverse = True):
        length, replacement = plan[pattern_start]
        after_pattern = pattern_start + length
        string = string[:pattern_start] + replacement + string[after_pattern:]

    return string

def can_build_metadata(toolchain, ctx, crate_type):
    """Can we build metadata for this rust_library?

    Args:
        toolchain (toolchain): The rust toolchain
        ctx (ctx): The rule's context object
        crate_type (String): one of lib|rlib|dylib|staticlib|cdylib|proc-macro

    Returns:
        bool: whether we can build metadata for this rule.
    """

    # In order to enable pipelined compilation we require that:
    # 1) The _pipelined_compilation flag is enabled,
    # 2) the OS running the rule is something other than windows as we require sandboxing (for now),
    # 3) process_wrapper is enabled (this is disabled when compiling process_wrapper itself),
    # 4) the crate_type is rlib or lib.
    return toolchain._pipelined_compilation and \
           toolchain.exec_triple.system != "windows" and \
           ctx.attr._process_wrapper and \
           crate_type in ("rlib", "lib")

def crate_root_src(name, crate_name, srcs, crate_type):
    """Determines the source file for the crate root, should it not be specified in `attr.crate_root`.

    Args:
        name (str): The name of the target.
        crate_name (str): The target's `crate_name` attribute.
        srcs (list): A list of all sources for the target Crate.
        crate_type (str): The type of this crate ("bin", "lib", "rlib", "cdylib", etc).

    Returns:
        File: The root File object for a given crate. See the following links for more details:
            - https://doc.rust-lang.org/cargo/reference/cargo-targets.html#library
            - https://doc.rust-lang.org/cargo/reference/cargo-targets.html#binaries
    """
    default_crate_root_filename = "main.rs" if crate_type == "bin" else "lib.rs"

    if not crate_name:
        crate_name = name

    crate_root = (
        (srcs[0] if len(srcs) == 1 else None) or
        _shortest_src_with_basename(srcs, default_crate_root_filename) or
        _shortest_src_with_basename(srcs, name + ".rs") or
        _shortest_src_with_basename(srcs, crate_name + ".rs")
    )
    if not crate_root:
        file_names = [default_crate_root_filename, name + ".rs"]
        fail("Couldn't find {} among `srcs`, please use `crate_root` to specify the root file.".format(" or ".join(file_names)))
    return crate_root

def _shortest_src_with_basename(srcs, basename):
    """Finds the shortest among the paths in srcs that match the desired basename.

    Args:
        srcs (list): A list of File objects
        basename (str): The target basename to match against.

    Returns:
        File: The File object with the shortest path that matches `basename`
    """
    shortest = None
    for f in srcs:
        if f.basename == basename:
            if not shortest or len(f.dirname) < len(shortest.dirname):
                shortest = f
    return shortest

def determine_lib_name(name, crate_type, toolchain, lib_hash = None):
    """See https://github.com/bazelbuild/rules_rust/issues/405

    Args:
        name (str): The name of the current target
        crate_type (str): The `crate_type`
        toolchain (rust_toolchain): The current `rust_toolchain`
        lib_hash (str, optional): The hashed crate root path

    Returns:
        str: A unique library name
    """
    extension = None
    prefix = ""
    if crate_type in ("dylib", "cdylib", "proc-macro"):
        extension = toolchain.dylib_ext
    elif crate_type == "staticlib":
        extension = toolchain.staticlib_ext
    elif crate_type in ("lib", "rlib"):
        # All platforms produce 'rlib' here
        extension = ".rlib"
        prefix = "lib"
    elif crate_type == "bin":
        fail("crate_type of 'bin' was detected in a rust_library. Please compile " +
             "this crate as a rust_binary instead.")

    if not extension:
        fail(("Unknown crate_type: {}. If this is a cargo-supported crate type, " +
              "please file an issue!").format(crate_type))

    prefix = "lib"
    if toolchain.target_triple and toolchain.target_os == "windows" and crate_type not in ("lib", "rlib"):
        prefix = ""
    if toolchain.target_arch in ("wasm32", "wasm64") and crate_type == "cdylib":
        prefix = ""

    return "{prefix}{name}{lib_hash}{extension}".format(
        prefix = prefix,
        name = name,
        lib_hash = "-" + lib_hash if lib_hash else "",
        extension = extension,
    )

def transform_sources(ctx, srcs, crate_root):
    """Creates symlinks of the source files if needed.

    Rustc assumes that the source files are located next to the crate root.
    In case of a mix between generated and non-generated source files, this
    we violate this assumption, as part of the sources will be located under
    bazel-out/... . In order to allow for targets that contain both generated
    and non-generated source files, we generate symlinks for all non-generated
    files.

    Args:
        ctx (struct): The current rule's context.
        srcs (List[File]): The sources listed in the `srcs` attribute
        crate_root (File): The file specified in the `crate_root` attribute,
                           if it exists, otherwise None

    Returns:
        Tuple(List[File], File): The transformed srcs and crate_root
    """
    has_generated_sources = len([src for src in srcs if not src.is_source]) > 0

    if not has_generated_sources:
        return srcs, crate_root

    package_root = paths.join(ctx.label.workspace_root, ctx.label.package)
    generated_sources = [_symlink_for_non_generated_source(ctx, src, package_root) for src in srcs if src != crate_root]
    generated_root = crate_root
    if crate_root:
        generated_root = _symlink_for_non_generated_source(ctx, crate_root, package_root)
        generated_sources.append(generated_root)

    return generated_sources, generated_root

def get_edition(attr, toolchain, label):
    """Returns the Rust edition from either the current rule's attributes or the current `rust_toolchain`

    Args:
        attr (struct): The current rule's attributes
        toolchain (rust_toolchain): The `rust_toolchain` for the current target
        label (Label): The label of the target being built

    Returns:
        str: The target Rust edition
    """
    if getattr(attr, "edition"):
        return attr.edition
    elif not toolchain.default_edition:
        fail("Attribute `edition` is required for {}.".format(label))
    else:
        return toolchain.default_edition

def _symlink_for_non_generated_source(ctx, src_file, package_root):
    """Creates and returns a symlink for non-generated source files.

    This rule uses the full path to the source files and the rule directory to compute
    the relative paths. This is needed, instead of using `short_path`, because of non-generated
    source files in external repositories possibly returning relative paths depending on the
    current version of Bazel.

    Args:
        ctx (struct): The current rule's context.
        src_file (File): The source file.
        package_root (File): The full path to the directory containing the current rule.

    Returns:
        File: The created symlink if a non-generated file, or the file itself.
    """

    if src_file.is_source or src_file.root.path != ctx.bin_dir.path:
        src_short_path = paths.relativize(src_file.path, src_file.root.path)
        src_symlink = ctx.actions.declare_file(paths.relativize(src_short_path, package_root))
        ctx.actions.symlink(
            output = src_symlink,
            target_file = src_file,
            progress_message = "Creating symlink to source file: {}".format(src_file.path),
        )
        return src_symlink
    else:
        return src_file

def generate_output_diagnostics(ctx, sibling, require_process_wrapper = True):
    """Generates a .rustc-output file if it's required.

    Args:
        ctx: (ctx): The current rule's context object
        sibling: (File): The file to generate the diagnostics for.
        require_process_wrapper: (bool): Whether to require the process wrapper
          in order to generate the .rustc-output file.
    Returns:
        Optional[File] The .rustc-object file, if generated.
    """

    # Since this feature requires error_format=json, we usually need
    # process_wrapper, since it can write the json here, then convert it to the
    # regular error format so the user can see the error properly.
    if require_process_wrapper and not ctx.attr._process_wrapper:
        return None
    provider = ctx.attr._rustc_output_diagnostics[RustcOutputDiagnosticsInfo]
    if not provider.rustc_output_diagnostics:
        return None

    return ctx.actions.declare_file(
        sibling.basename + ".rustc-output",
        sibling = sibling,
    )

def is_std_dylib(file):
    """Whether the file is a dylib crate for std

    """
    basename = file.basename
    return (
        # for linux and darwin
        basename.startswith("libstd-") and (basename.endswith(".so") or basename.endswith(".dylib")) or
        # for windows
        basename.startswith("std-") and basename.endswith(".dll")
    )
