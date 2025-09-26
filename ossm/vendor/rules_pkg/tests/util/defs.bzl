# Copyright 2021 The Bazel Authors. All rights reserved.
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
"""Rules to aid testing"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//pkg:providers.bzl", "PackageFilegroupInfo", "PackageSymlinkInfo")
load("//pkg/private:pkg_files.bzl", "add_label_list", "create_mapping_context_from_ctx", "write_manifest")  # buildifier: disable=bzl-visibility

def _directory_impl(ctx):
    out_dir_file = ctx.actions.declare_directory(ctx.attr.outdir or ctx.attr.name)

    args = ctx.actions.args()
    args.add(out_dir_file.path)

    # This helper is horrible.  We should pass all the args in files.
    for fn in ctx.attr.filenames:
        args.add(fn)
        args.add(ctx.attr.contents)

    for link, target in ctx.attr.links.items():
        args.add(link)
        args.add("@@" + target)

    ctx.actions.run(
        outputs = [out_dir_file],
        inputs = [],
        executable = ctx.executable._dir_creator,
        arguments = [args],
    )
    return DefaultInfo(files = depset([out_dir_file]))

directory = rule(
    doc = """Helper rule to create simple TreeArtifact structures

We would normally just use genrules for this, but their directory output
creation capabilities are "unsound".
    """,
    implementation = _directory_impl,
    attrs = {
        "filenames": attr.string_list(
            doc = """Paths to create in the directory.

Paths containing directories will also have the intermediate directories created too.""",
        ),
        "links": attr.string_dict(
            doc = """Set of (virtual) links to create.

The keys of links are paths to create.  The values are the target of the links.""",
        ),
        "contents": attr.string(),
        "outdir": attr.string(),
        "_dir_creator": attr.label(
            default = ":create_directory_with_contents",
            executable = True,
            cfg = "exec",
        ),
    },
)

def _fake_artifact_impl(ctx):
    out_file = ctx.actions.declare_file(ctx.attr.name)
    content = ["echo " + rf.path for rf in ctx.files.runfiles]
    ctx.actions.write(
        output = out_file,
        content = "\r\n".join(content),
        is_executable = ctx.attr.executable,
    )
    return DefaultInfo(
        files = depset([out_file] + ctx.files.files),
        runfiles = ctx.runfiles(files = ctx.files.runfiles),
        executable = out_file if ctx.attr.executable else None,
    )

fake_artifact = rule(
    doc = """Rule to create a fake artifact that depends on its srcs.

This rule creates a file that appears to depend on its srcs and passes along
other targets in DefaultInfo as files and/or runfiles. It creates a script that
echos all the file names. It is useful for building an object that is like a
cc_binary in complexity, but does not depend on a large toolchain.""",
    implementation = _fake_artifact_impl,
    attrs = {
        "deps": attr.label_list(
            doc = "Dependencies to trigger other rules, but are then discarded.",
            allow_files = True,
        ),
        "files": attr.label_list(
            doc = "Deps which are passed in DefaultInfo as files.",
            allow_files = True,
        ),
        "runfiles": attr.label_list(
            doc = "Deps which are passed in DefaultInfo as runfiles.",
            allow_files = True,
        ),
        "executable": attr.bool(
            doc = "If True, the DefaultInfo will be marked as executable.",
            default = False,
        ),
    },
)

def _link_tree_impl(ctx):
    links = []
    prefix = ctx.attr.package_dir or ""
    if prefix and not prefix.endswith("/"):
        prefix = prefix + "/"
    for link, target in ctx.attr.links.items():
        # DBG print('  %s -> %s ' % (link, target))
        links.append(
            (
                PackageSymlinkInfo(destination = prefix + link, target = target),
                ctx.label,
            ),
        )
    return [PackageFilegroupInfo(pkg_symlinks = links)]

link_tree = rule(
    doc = """Helper rule to create a lot of fake symlinks.

The inspiration is to create test data for the kinds of layouts needed by
nodejs.  See. https://pnpm.io/symlinked-node-modules-structure
    """,
    implementation = _link_tree_impl,
    attrs = {
        "links": attr.string_dict(
            doc = """Set of (virtual) links to create.

The keys of links are paths to create.  The values are the target of the links.""",
            mandatory = True,
        ),
        "package_dir": attr.string(doc = """Prefix to apply to all link paths."""),
    },
    provides = [PackageFilegroupInfo],
)

def _write_content_manifest_impl(ctx):
    mapping_context = create_mapping_context_from_ctx(ctx, ctx.label)
    add_label_list(mapping_context, ctx.attr.srcs)
    write_manifest(ctx, ctx.outputs.out, mapping_context.content_map, use_short_path = ctx.attr.use_short_path)

_write_content_manifest = rule(
    doc = """Helper rule to write the content manifest for a pkg_*.

This is intended only for testing the manifest creation features.""",
    implementation = _write_content_manifest_impl,
    attrs = {
        "srcs": attr.label_list(
            doc = """List of source inputs.""",
            allow_files = True,
        ),
        "out": attr.output(),
        "use_short_path": attr.bool(
            doc = """Use the rootless path in the manifest.

            Useful to ensure that the platform-specific prefix (i.e. parts
            including something like "x64_windows-fastbuild") isn't present in
            paths in the manifest.

            See also https://docs.bazel.build/versions/main/skylark/lib/File.html#path
            """,
            default = True,
        ),
        "include_runfiles": attr.bool(),
    },
)

def write_content_manifest(name, srcs, **kwargs):
    out = kwargs.pop("out", name + ".manifest")
    use_short_path = kwargs.pop("use_short_path", True)
    _write_content_manifest(
        name = name,
        srcs = srcs,
        out = out,
        use_short_path = use_short_path,
        **kwargs
    )

############################################################
# Test boilerplate
############################################################
def _generic_base_case_test_impl(ctx):
    env = analysistest.begin(ctx)

    # Nothing here intentionally, this is simply an attempt to verify successful
    # analysis.

    return analysistest.end(env)

generic_base_case_test = analysistest.make(
    _generic_base_case_test_impl,
    attrs = {},
)

# Generic negative test boilerplate
def _generic_negative_test_impl(ctx):
    env = analysistest.begin(ctx)

    asserts.expect_failure(env, ctx.attr.reason)

    return analysistest.end(env)

generic_negative_test = analysistest.make(
    _generic_negative_test_impl,
    attrs = {
        "reason": attr.string(
            default = "",
        ),
    },
    expect_failure = True,
)
