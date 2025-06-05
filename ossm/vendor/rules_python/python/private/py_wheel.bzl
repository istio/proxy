# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"Implementation of py_wheel rule"

load(":py_package.bzl", "py_package_lib")
load(":py_wheel_normalize_pep440.bzl", "normalize_pep440")
load(":stamp.bzl", "is_stamping_enabled")

PyWheelInfo = provider(
    doc = "Information about a wheel produced by `py_wheel`",
    fields = {
        "name_file": (
            "File: A file containing the canonical name of the wheel (after " +
            "stamping, if enabled)."
        ),
        "wheel": "File: The wheel file itself.",
    },
)

_distribution_attrs = {
    "abi": attr.string(
        default = "none",
        doc = "Python ABI tag. 'none' for pure-Python wheels.",
    ),
    "compress": attr.bool(
        default = True,
        doc = "Enable compression of the final archive.",
    ),
    "distribution": attr.string(
        mandatory = True,
        doc = """\
Name of the distribution.

This should match the project name on PyPI. It's also the name that is used to
refer to the package in other packages' dependencies.

Workspace status keys are expanded using `{NAME}` format, for example:
 - `distribution = "package.{CLASSIFIER}"`
 - `distribution = "{DISTRIBUTION}"`

For the available keys, see https://bazel.build/docs/user-manual#workspace-status
""",
    ),
    "platform": attr.string(
        default = "any",
        doc = """\
Supported platform. Use 'any' for pure-Python wheel.

If you have included platform-specific data, such as a .pyd or .so
extension module, you will need to specify the platform in standard
pip format. If you support multiple platforms, you can define
platform constraints, then use a select() to specify the appropriate
specifier, eg:

`
platform = select({
    "//platforms:windows_x86_64": "win_amd64",
    "//platforms:macos_x86_64": "macosx_10_7_x86_64",
    "//platforms:linux_x86_64": "manylinux2014_x86_64",
})
`
""",
    ),
    "python_tag": attr.string(
        default = "py3",
        doc = "Supported Python version(s), eg `py3`, `cp35.cp36`, etc",
    ),
    "stamp": attr.int(
        doc = """\
Whether to encode build information into the wheel. Possible values:

- `stamp = 1`: Always stamp the build information into the wheel, even in \
[--nostamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) builds. \
This setting should be avoided, since it potentially kills remote caching for the target and \
any downstream actions that depend on it.

- `stamp = 0`: Always replace build information by constant values. This gives good build result caching.

- `stamp = -1`: Embedding of build information is controlled by the \
[--[no]stamp](https://docs.bazel.build/versions/main/user-manual.html#flag--stamp) flag.

Stamped targets are not rebuilt unless their dependencies change.
        """,
        default = -1,
        values = [1, 0, -1],
    ),
    "version": attr.string(
        mandatory = True,
        doc = """\
Version number of the package.

Note that this attribute supports stamp format strings as well as 'make variables'.
For example:
  - `version = "1.2.3-{BUILD_TIMESTAMP}"`
  - `version = "{BUILD_EMBED_LABEL}"`
  - `version = "$(VERSION)"`

Note that Bazel's output filename cannot include the stamp information, as outputs must be known
during the analysis phase and the stamp data is available only during the action execution.

The [`py_wheel`](#py_wheel) macro produces a `.dist`-suffix target which creates a
`dist/` folder containing the wheel with the stamped name, suitable for publishing.

See [`py_wheel_dist`](#py_wheel_dist) for more info.
""",
    ),
    "_stamp_flag": attr.label(
        doc = "A setting used to determine whether or not the `--stamp` flag is enabled",
        default = Label("//python/private:stamp"),
    ),
}

_feature_flags = {}

ALLOWED_DATA_FILE_PREFIX = ("purelib", "platlib", "headers", "scripts", "data")
_requirement_attrs = {
    "extra_requires": attr.string_list_dict(
        doc = ("A mapping of [extras](https://peps.python.org/pep-0508/#extras) options to lists of requirements (similar to `requires`). This attribute " +
               "is mutually exclusive with `extra_requires_file`."),
    ),
    "extra_requires_files": attr.label_keyed_string_dict(
        doc = ("A mapping of requirements files (similar to `requires_file`) to the name of an [extras](https://peps.python.org/pep-0508/#extras) option " +
               "This attribute is mutually exclusive with `extra_requires`."),
        allow_files = True,
    ),
    "requires": attr.string_list(
        doc = ("List of requirements for this package. See the section on " +
               "[Declaring required dependency](https://setuptools.readthedocs.io/en/latest/userguide/dependency_management.html#declaring-dependencies) " +
               "for details and examples of the format of this argument. This " +
               "attribute is mutually exclusive with `requires_file`."),
    ),
    "requires_file": attr.label(
        doc = ("A file containing a list of requirements for this package. See the section on " +
               "[Declaring required dependency](https://setuptools.readthedocs.io/en/latest/userguide/dependency_management.html#declaring-dependencies) " +
               "for details and examples of the format of this argument. This " +
               "attribute is mutually exclusive with `requires`."),
        allow_single_file = True,
    ),
}

_entrypoint_attrs = {
    "console_scripts": attr.string_dict(
        doc = """\
Deprecated console_script entry points, e.g. `{'main': 'examples.wheel.main:main'}`.

Deprecated: prefer the `entry_points` attribute, which supports `console_scripts` as well as other entry points.
""",
    ),
    "entry_points": attr.string_list_dict(
        doc = """\
entry_points, e.g. `{'console_scripts': ['main = examples.wheel.main:main']}`.
""",
    ),
}

_other_attrs = {
    "author": attr.string(
        doc = "A string specifying the author of the package.",
        default = "",
    ),
    "author_email": attr.string(
        doc = "A string specifying the email address of the package author.",
        default = "",
    ),
    "classifiers": attr.string_list(
        doc = "A list of strings describing the categories for the package. For valid classifiers see https://pypi.org/classifiers",
    ),
    "data_files": attr.label_keyed_string_dict(
        doc = ("Any file that is not normally installed inside site-packages goes into the .data directory, named " +
               "as the .dist-info directory but with the .data/ extension.  Allowed paths: {prefixes}".format(prefixes = ALLOWED_DATA_FILE_PREFIX)),
        allow_files = True,
    ),
    "description_content_type": attr.string(
        doc = ("The type of contents in description_file. " +
               "If not provided, the type will be inferred from the extension of description_file. " +
               "Also see https://packaging.python.org/en/latest/specifications/core-metadata/#description-content-type"),
    ),
    "description_file": attr.label(
        doc = "A file containing text describing the package.",
        allow_single_file = True,
    ),
    "extra_distinfo_files": attr.label_keyed_string_dict(
        doc = "Extra files to add to distinfo directory in the archive.",
        allow_files = True,
    ),
    "homepage": attr.string(
        doc = "A string specifying the URL for the package homepage.",
        default = "",
    ),
    "license": attr.string(
        doc = "A string specifying the license of the package.",
        default = "",
    ),
    "project_urls": attr.string_dict(
        doc = ("A string dict specifying additional browsable URLs for the project and corresponding labels, " +
               "where label is the key and url is the value. " +
               'e.g `{{"Bug Tracker": "http://bitbucket.org/tarek/distribute/issues/"}}`'),
    ),
    "python_requires": attr.string(
        doc = (
            "Python versions required by this distribution, e.g. '>=3.5,<3.7'"
        ),
        default = "",
    ),
    "strip_path_prefixes": attr.string_list(
        default = [],
        doc = "path prefixes to strip from files added to the generated package",
    ),
    "summary": attr.string(
        doc = "A one-line summary of what the distribution does",
    ),
}

_PROJECT_URL_LABEL_LENGTH_LIMIT = 32
_DESCRIPTION_FILE_EXTENSION_TO_TYPE = {
    "md": "text/markdown",
    "rst": "text/x-rst",
}
_DEFAULT_DESCRIPTION_FILE_TYPE = "text/plain"

def _escape_filename_distribution_name(name):
    """Escape the distribution name component of a filename.

    See https://packaging.python.org/en/latest/specifications/binary-distribution-format/#escaping-and-unicode
    and https://packaging.python.org/en/latest/specifications/name-normalization/.

    Apart from the valid names according to the above, we also accept
    '{' and '}', which may be used as placeholders for stamping.
    """
    escaped = ""
    _inside_stamp_var = False
    for character in name.elems():
        if character == "{":
            _inside_stamp_var = True
            escaped += character
        elif character == "}":
            _inside_stamp_var = False
            escaped += character
        elif character.isalnum():
            escaped += character if _inside_stamp_var else character.lower()
        elif character in ["-", "_", "."]:
            if escaped == "":
                fail(
                    "A valid name must start with a letter or number.",
                    "Name '%s' does not." % name,
                )
            elif escaped.endswith("_"):
                pass
            else:
                escaped += "_"
        else:
            fail(
                "A valid name consists only of ASCII letters ",
                "and numbers, period, underscore and hyphen.",
                "Name '%s' has bad character '%s'." % (name, character),
            )
    if escaped.endswith("_"):
        fail(
            "A valid name must end with a letter or number.",
            "Name '%s' does not." % name,
        )
    return escaped

def _escape_filename_segment(segment):
    """Escape a segment of the wheel filename.

    See https://www.python.org/dev/peps/pep-0427/#escaping-and-unicode
    """

    # TODO: this is wrong, isalnum replaces non-ascii letters, while we should
    # not replace them.
    # TODO: replace this with a regexp once starlark supports them.
    escaped = ""
    for character in segment.elems():
        # isalnum doesn't handle unicode characters properly.
        if character.isalnum() or character == ".":
            escaped += character
        elif not escaped.endswith("_"):
            escaped += "_"
    return escaped

def _replace_make_variables(flag, ctx):
    """Replace $(VERSION) etc make variables in flag"""
    if "$" in flag:
        for varname, varsub in ctx.var.items():
            flag = flag.replace("$(%s)" % varname, varsub)
    return flag

def _input_file_to_arg(input_file):
    """Converts a File object to string for --input_file argument to wheelmaker"""
    return "%s;%s" % (py_package_lib.path_inside_wheel(input_file), input_file.path)

def _py_wheel_impl(ctx):
    abi = _replace_make_variables(ctx.attr.abi, ctx)
    python_tag = _replace_make_variables(ctx.attr.python_tag, ctx)
    version = _replace_make_variables(ctx.attr.version, ctx)

    filename_segments = [
        _escape_filename_distribution_name(ctx.attr.distribution),
        normalize_pep440(version),
        _escape_filename_segment(python_tag),
        _escape_filename_segment(abi),
        _escape_filename_segment(ctx.attr.platform),
    ]

    outfile = ctx.actions.declare_file("-".join(filename_segments) + ".whl")

    name_file = ctx.actions.declare_file(ctx.label.name + ".name")

    inputs_to_package = depset(
        direct = ctx.files.deps,
    )

    # Inputs to this rule which are not to be packaged.
    # Currently this is only the description file (if used).
    other_inputs = []

    # Wrap the inputs into a file to reduce command line length.
    packageinputfile = ctx.actions.declare_file(ctx.attr.name + "_target_wrapped_inputs.txt")
    content = ""
    for input_file in inputs_to_package.to_list():
        content += _input_file_to_arg(input_file) + "\n"
    ctx.actions.write(output = packageinputfile, content = content)
    other_inputs.append(packageinputfile)

    args = ctx.actions.args()
    args.add("--name", ctx.attr.distribution)
    args.add("--version", version)
    args.add("--python_tag", python_tag)
    args.add("--abi", abi)
    args.add("--platform", ctx.attr.platform)
    args.add("--out", outfile)
    args.add("--name_file", name_file)
    args.add_all(ctx.attr.strip_path_prefixes, format_each = "--strip_path_prefix=%s")

    # Pass workspace status files if stamping is enabled
    if is_stamping_enabled(ctx.attr):
        args.add("--volatile_status_file", ctx.version_file)
        args.add("--stable_status_file", ctx.info_file)
        other_inputs.extend([ctx.version_file, ctx.info_file])

    args.add("--input_file_list", packageinputfile)

    # Note: Description file and version are not embedded into metadata.txt yet,
    # it will be done later by wheelmaker script.
    metadata_file = ctx.actions.declare_file(ctx.attr.name + ".metadata.txt")
    metadata_contents = ["Metadata-Version: 2.1"]
    metadata_contents.append("Name: %s" % ctx.attr.distribution)

    if ctx.attr.author:
        metadata_contents.append("Author: %s" % ctx.attr.author)
    if ctx.attr.author_email:
        metadata_contents.append("Author-email: %s" % ctx.attr.author_email)
    if ctx.attr.homepage:
        metadata_contents.append("Home-page: %s" % ctx.attr.homepage)
    if ctx.attr.license:
        metadata_contents.append("License: %s" % ctx.attr.license)
    if ctx.attr.description_content_type:
        metadata_contents.append("Description-Content-Type: %s" % ctx.attr.description_content_type)
    elif ctx.attr.description_file:
        # infer the content type from description file extension.
        description_file_type = _DESCRIPTION_FILE_EXTENSION_TO_TYPE.get(
            ctx.file.description_file.extension,
            _DEFAULT_DESCRIPTION_FILE_TYPE,
        )
        metadata_contents.append("Description-Content-Type: %s" % description_file_type)
    if ctx.attr.summary:
        metadata_contents.append("Summary: %s" % ctx.attr.summary)

    for label, url in sorted(ctx.attr.project_urls.items()):
        if len(label) > _PROJECT_URL_LABEL_LENGTH_LIMIT:
            fail("`label` {} in `project_urls` is too long. It is limited to {} characters.".format(len(label), _PROJECT_URL_LABEL_LENGTH_LIMIT))
        metadata_contents.append("Project-URL: %s, %s" % (label, url))

    for c in ctx.attr.classifiers:
        metadata_contents.append("Classifier: %s" % c)

    if ctx.attr.python_requires:
        metadata_contents.append("Requires-Python: %s" % ctx.attr.python_requires)

    if ctx.attr.requires and ctx.attr.requires_file:
        fail("`requires` and `requires_file` are mutually exclusive. Please update {}".format(ctx.label))

    for requires in ctx.attr.requires:
        metadata_contents.append("Requires-Dist: %s" % requires)
    if ctx.attr.requires_file:
        # The @ prefixed paths will be resolved by the PyWheel action.
        # Expanding each line containing a constraint in place of this
        # directive.
        metadata_contents.append("Requires-Dist: @%s" % ctx.file.requires_file.path)
        other_inputs.append(ctx.file.requires_file)

    if ctx.attr.extra_requires and ctx.attr.extra_requires_files:
        fail("`extra_requires` and `extra_requires_files` are mutually exclusive. Please update {}".format(ctx.label))
    for option, option_requirements in sorted(ctx.attr.extra_requires.items()):
        metadata_contents.append("Provides-Extra: %s" % option)
        for requirement in option_requirements:
            metadata_contents.append(
                "Requires-Dist: %s; extra == '%s'" % (requirement, option),
            )
    extra_requires_files = {}
    for option_requires_target, option in ctx.attr.extra_requires_files.items():
        if option in extra_requires_files:
            fail("Duplicate `extra_requires_files` option '{}' found on target {}".format(option, ctx.label))
        option_requires_files = option_requires_target[DefaultInfo].files.to_list()
        if len(option_requires_files) != 1:
            fail("Labels in `extra_requires_files` must result in a single file, but {label} provides {files} from {owner}".format(
                label = ctx.label,
                files = option_requires_files,
                owner = option_requires_target.label,
            ))
        extra_requires_files.update({option: option_requires_files[0]})

    for option, option_requires_file in sorted(extra_requires_files.items()):
        metadata_contents.append("Provides-Extra: %s" % option)
        metadata_contents.append(
            # The @ prefixed paths will be resolved by the PyWheel action.
            # Expanding each line containing a constraint in place of this
            # directive and appending the extra option.
            "Requires-Dist: @%s; extra == '%s'" % (option_requires_file.path, option),
        )
        other_inputs.append(option_requires_file)

    ctx.actions.write(
        output = metadata_file,
        content = "\n".join(metadata_contents) + "\n",
    )
    other_inputs.append(metadata_file)
    args.add("--metadata_file", metadata_file)

    # Merge console_scripts into entry_points.
    entrypoints = dict(ctx.attr.entry_points)  # Copy so we can mutate it
    if ctx.attr.console_scripts:
        # Copy a console_scripts group that may already exist, so we can mutate it.
        console_scripts = list(entrypoints.get("console_scripts", []))
        entrypoints["console_scripts"] = console_scripts
        for name, ref in ctx.attr.console_scripts.items():
            console_scripts.append("{name} = {ref}".format(name = name, ref = ref))

    # If any entry_points are provided, construct the file here and add it to the files to be packaged.
    # see: https://packaging.python.org/specifications/entry-points/
    if entrypoints:
        lines = []
        for group, entries in sorted(entrypoints.items()):
            if lines:
                # Blank line between groups
                lines.append("")
            lines.append("[{group}]".format(group = group))
            lines += sorted(entries)
        entry_points_file = ctx.actions.declare_file(ctx.attr.name + "_entry_points.txt")
        content = "\n".join(lines)
        ctx.actions.write(output = entry_points_file, content = content)
        other_inputs.append(entry_points_file)
        args.add("--entry_points_file", entry_points_file)

    if ctx.attr.description_file:
        description_file = ctx.file.description_file
        args.add("--description_file", description_file)
        other_inputs.append(description_file)

    if not ctx.attr.compress:
        args.add("--no_compress")

    for target, filename in ctx.attr.extra_distinfo_files.items():
        target_files = target.files.to_list()
        if len(target_files) != 1:
            fail(
                "Multi-file target listed in extra_distinfo_files %s",
                filename,
            )
        other_inputs.extend(target_files)
        args.add(
            "--extra_distinfo_file",
            filename + ";" + target_files[0].path,
        )

    for target, filename in ctx.attr.data_files.items():
        target_files = target.files.to_list()
        if len(target_files) != 1:
            fail(
                "Multi-file target listed in data_files %s",
                filename,
            )

        if filename.partition("/")[0] not in ALLOWED_DATA_FILE_PREFIX:
            fail(
                "The target data file must start with one of these prefixes: '%s'.  Target filepath: '%s'" %
                (
                    ",".join(ALLOWED_DATA_FILE_PREFIX),
                    filename,
                ),
            )
        other_inputs.extend(target_files)
        args.add(
            "--data_files",
            filename + ";" + target_files[0].path,
        )

    ctx.actions.run(
        mnemonic = "PyWheel",
        inputs = depset(direct = other_inputs, transitive = [inputs_to_package]),
        outputs = [outfile, name_file],
        arguments = [args],
        executable = ctx.executable._wheelmaker,
        # The default shell env is used to better support toolchains that look
        # up python at runtime using PATH.
        use_default_shell_env = True,
        progress_message = "Building wheel {}".format(ctx.label),
    )
    return [
        DefaultInfo(
            files = depset([outfile]),
            runfiles = ctx.runfiles(files = [outfile]),
        ),
        PyWheelInfo(
            wheel = outfile,
            name_file = name_file,
        ),
    ]

def _concat_dicts(*dicts):
    result = {}
    for d in dicts:
        result.update(d)
    return result

py_wheel_lib = struct(
    implementation = _py_wheel_impl,
    attrs = _concat_dicts(
        {
            "deps": attr.label_list(
                doc = """\
Targets to be included in the distribution.

The targets to package are usually `py_library` rules or filesets (for packaging data files).

Note it's usually better to package `py_library` targets and use
`entry_points` attribute to specify `console_scripts` than to package
`py_binary` rules. `py_binary` targets would wrap a executable script that
tries to locate `.runfiles` directory which is not packaged in the wheel.
""",
            ),
            "_wheelmaker": attr.label(
                executable = True,
                cfg = "exec",
                default = "//tools:wheelmaker",
            ),
        },
        _distribution_attrs,
        _feature_flags,
        _requirement_attrs,
        _entrypoint_attrs,
        _other_attrs,
    ),
)

py_wheel = rule(
    implementation = py_wheel_lib.implementation,
    doc = """\
Internal rule used by the [py_wheel macro](#py_wheel).

These intentionally have the same name to avoid sharp edges with Bazel macros.
For example, a `bazel query` for a user's `py_wheel` macro expands to `py_wheel` targets,
in the way they expect.
""",
    attrs = py_wheel_lib.attrs,
)
