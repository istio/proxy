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

"""Rules to generate Sphinx-compatible documentation for bzl files."""

load("@bazel_skylib//:bzl_library.bzl", "StarlarkLibraryInfo")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//lib:types.bzl", "types")
load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@io_bazel_stardoc//stardoc:stardoc.bzl", "stardoc")
load("//python/private:util.bzl", "add_tag", "copy_propagating_kwargs")  # buildifier: disable=bzl-visibility
load("//sphinxdocs/private:sphinx_docs_library_macro.bzl", "sphinx_docs_library")

_StardocInputHelperInfo = provider(
    doc = "Extracts the single source file from a bzl library.",
    fields = {
        "file": """
:type: File

The sole output file from the wrapped target.
""",
    },
)

def sphinx_stardocs(
        *,
        name,
        srcs = [],
        deps = [],
        docs = {},
        prefix = None,
        strip_prefix = None,
        **kwargs):
    """Generate Sphinx-friendly Markdown docs using Stardoc for bzl libraries.

    A `build_test` for the docs is also generated to ensure Stardoc is able
    to process the files.

    NOTE: This generates MyST-flavored Markdown.

    Args:
        name: {type}`Name`, the name of the resulting file group with the generated docs.
        srcs: {type}`list[label]` Each source is either the bzl file to process
            or a `bzl_library` target with one source file of the bzl file to
            process.
        deps: {type}`list[label]` Targets that provide files loaded by `src`
        docs: {type}`dict[str, str|dict]` of the bzl files to generate documentation
            for. The `output` key is the path of the output filename, e.g.,
            `foo/bar.md`. The `source` values can be either of:
            * A `str` label that points to a `bzl_library` target. The target
              name will replace `_bzl` with `.bzl` and use that as the input
              bzl file to generate docs for. The target itself provides the
              necessary dependencies.
            * A `dict` with keys `input` and `dep`. The `input` key is a string
              label to the bzl file to generate docs for. The `dep` key is a
              string label to a `bzl_library` providing the necessary dependencies.
        prefix: {type}`str` Prefix to add to the output file path. It is prepended
            after `strip_prefix` is removed.
        strip_prefix: {type}`str | None` Prefix to remove from the input file path;
            it is removed before `prefix` is prepended. If not specified, then
            {any}`native.package_name` is used.
        **kwargs: Additional kwargs to pass onto each `sphinx_stardoc` target
    """
    internal_name = "_{}".format(name)
    add_tag(kwargs, "@rules_python//sphinxdocs:sphinx_stardocs")
    common_kwargs = copy_propagating_kwargs(kwargs)
    common_kwargs["target_compatible_with"] = kwargs.get("target_compatible_with")

    stardocs = []
    for out_name, entry in docs.items():
        stardoc_kwargs = {}
        stardoc_kwargs.update(kwargs)

        if types.is_string(entry):
            stardoc_kwargs["deps"] = [entry]
            stardoc_kwargs["src"] = entry.replace("_bzl", ".bzl")
        else:
            stardoc_kwargs.update(entry)

            # input is accepted for backwards compatiblity. Remove when ready.
            if "src" not in stardoc_kwargs and "input" in stardoc_kwargs:
                stardoc_kwargs["src"] = stardoc_kwargs.pop("input")
            stardoc_kwargs["deps"] = [stardoc_kwargs.pop("dep")]

        doc_name = "{}_{}".format(internal_name, _name_from_label(out_name))
        sphinx_stardoc(
            name = doc_name,
            output = out_name,
            create_test = False,
            **stardoc_kwargs
        )
        stardocs.append(doc_name)

    for label in srcs:
        doc_name = "{}_{}".format(internal_name, _name_from_label(label))
        sphinx_stardoc(
            name = doc_name,
            src = label,
            # NOTE: We set prefix/strip_prefix here instead of
            # on the sphinx_docs_library so that building the
            # target produces markdown files in the expected location, which
            # is convenient.
            prefix = prefix,
            strip_prefix = strip_prefix,
            deps = deps,
            create_test = False,
            **common_kwargs
        )
        stardocs.append(doc_name)

    sphinx_docs_library(
        name = name,
        deps = stardocs,
        **common_kwargs
    )
    if stardocs:
        build_test(
            name = name + "_build_test",
            targets = stardocs,
            **common_kwargs
        )

def sphinx_stardoc(
        name,
        src,
        deps = [],
        public_load_path = None,
        prefix = None,
        strip_prefix = None,
        create_test = True,
        output = None,
        **kwargs):
    """Generate Sphinx-friendly Markdown for a single bzl file.

    Args:
        name: {type}`Name` name for the target.
        src: {type}`label` The bzl file to process, or a `bzl_library`
            target with one source file of the bzl file to process.
        deps: {type}`list[label]` Targets that provide files loaded by `src`
        public_load_path: {type}`str | None` override the file name that
            is reported as the file being.
        prefix: {type}`str | None` prefix to add to the output file path
        strip_prefix: {type}`str | None` Prefix to remove from the input file path.
            If not specified, then {any}`native.package_name` is used.
        create_test: {type}`bool` True if a test should be defined to verify the
            docs are buildable, False if not.
        output: {type}`str | None` Optional explicit output file to use. If
            not set, the output name will be derived from `src`.
        **kwargs: {type}`dict` common args passed onto rules.
    """
    internal_name = "_{}".format(name.lstrip("_"))
    add_tag(kwargs, "@rules_python//sphinxdocs:sphinx_stardoc")
    common_kwargs = copy_propagating_kwargs(kwargs)
    common_kwargs["target_compatible_with"] = kwargs.get("target_compatible_with")

    input_helper_name = internal_name + ".primary_bzl_src"
    _stardoc_input_helper(
        name = input_helper_name,
        target = src,
        **common_kwargs
    )

    stardoc_name = internal_name + "_stardoc"

    # NOTE: The .binaryproto suffix is an optimization. It makes the stardoc()
    # call avoid performing a copy of the output to the desired name.
    stardoc_pb = stardoc_name + ".binaryproto"

    stardoc(
        name = stardoc_name,
        input = input_helper_name,
        out = stardoc_pb,
        format = "proto",
        deps = [src] + deps,
        **common_kwargs
    )

    pb2md_name = internal_name + "_pb2md"
    _stardoc_proto_to_markdown(
        name = pb2md_name,
        src = stardoc_pb,
        output = output,
        output_name_from = input_helper_name if not output else None,
        public_load_path = public_load_path,
        strip_prefix = strip_prefix,
        prefix = prefix,
        **common_kwargs
    )
    sphinx_docs_library(
        name = name,
        srcs = [pb2md_name],
        **common_kwargs
    )
    if create_test:
        build_test(
            name = name + "_build_test",
            targets = [name],
            **common_kwargs
        )

def _stardoc_input_helper_impl(ctx):
    target = ctx.attr.target
    if StarlarkLibraryInfo in target:
        files = ctx.attr.target[StarlarkLibraryInfo].srcs
    else:
        files = target[DefaultInfo].files.to_list()

    if len(files) == 0:
        fail("Target {} produces no files, but must produce exactly 1 file".format(
            ctx.attr.target.label,
        ))
    elif len(files) == 1:
        primary = files[0]
    else:
        fail("Target {} produces {} files, but must produce exactly 1 file.".format(
            ctx.attr.target.label,
            len(files),
        ))

    return [
        DefaultInfo(
            files = depset([primary]),
        ),
        _StardocInputHelperInfo(
            file = primary,
        ),
    ]

_stardoc_input_helper = rule(
    implementation = _stardoc_input_helper_impl,
    attrs = {
        "target": attr.label(allow_files = True),
    },
)

def _stardoc_proto_to_markdown_impl(ctx):
    args = ctx.actions.args()
    args.use_param_file("@%s")
    args.set_param_file_format("multiline")

    inputs = [ctx.file.src]
    args.add("--proto", ctx.file.src)

    if not ctx.outputs.output:
        output_name = ctx.attr.output_name_from[_StardocInputHelperInfo].file.short_path
        output_name = paths.replace_extension(output_name, ".md")
        output_name = ctx.attr.prefix + output_name.removeprefix(ctx.attr.strip_prefix)
        output = ctx.actions.declare_file(output_name)
    else:
        output = ctx.outputs.output

    args.add("--output", output)

    if ctx.attr.public_load_path:
        args.add("--public-load-path={}".format(ctx.attr.public_load_path))

    ctx.actions.run(
        executable = ctx.executable._proto_to_markdown,
        arguments = [args],
        inputs = inputs,
        outputs = [output],
        mnemonic = "SphinxStardocProtoToMd",
        progress_message = "SphinxStardoc: converting proto to markdown: %{input} -> %{output}",
    )
    return [DefaultInfo(
        files = depset([output]),
    )]

_stardoc_proto_to_markdown = rule(
    implementation = _stardoc_proto_to_markdown_impl,
    attrs = {
        "output": attr.output(mandatory = False),
        "output_name_from": attr.label(),
        "prefix": attr.string(),
        "public_load_path": attr.string(),
        "src": attr.label(allow_single_file = True, mandatory = True),
        "strip_prefix": attr.string(),
        "_proto_to_markdown": attr.label(
            default = "//sphinxdocs/private:proto_to_markdown",
            executable = True,
            cfg = "exec",
        ),
    },
)

def _name_from_label(label):
    label = label.lstrip("/").lstrip(":").replace(":", "/")
    return label
