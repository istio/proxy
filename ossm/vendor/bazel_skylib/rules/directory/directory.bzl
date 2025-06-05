# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Skylib module containing rules to create metadata about directories."""

load("//lib:paths.bzl", "paths")
load(":providers.bzl", "DirectoryInfo", "create_directory_info")

def _prefix_match(f, prefixes):
    for prefix in prefixes:
        if f.path.startswith(prefix):
            return prefix
    fail("Expected {path} to start with one of {prefixes}".format(path = f.path, prefixes = list(prefixes)))

def _choose_path(prefixes):
    filtered = {prefix: example for prefix, example in prefixes.items() if example}
    if len(filtered) > 1:
        examples = list(filtered.values())
        fail(
            "Your sources contain {} and {}.\n\n".format(
                examples[0],
                examples[1],
            ) +
            "Having both source and generated files in a single directory is " +
            "unsupported, since they will appear in two different " +
            "directories in the bazel execroot. You may want to consider " +
            "splitting your directory into one for source files and one for " +
            "generated files.",
        )

    # If there's no entries, use the source path (it's always first in the dict)
    return list(filtered if filtered else prefixes)[0][:-1]

def _directory_impl(ctx):
    # Declare a generated file so that we can get the path to generated files.
    f = ctx.actions.declare_file("_directory_rule_" + ctx.label.name)
    ctx.actions.write(f, "")

    source_prefix = ctx.label.package
    if ctx.label.workspace_root:
        source_prefix = ctx.label.workspace_root + "/" + source_prefix
    source_prefix = source_prefix.rstrip("/") + "/"

    # Mapping of a prefix to an arbitrary (but deterministic) file matching that path.
    # The arbitrary file is used to present error messages if we have both generated files and source files.
    prefixes = {
        source_prefix: None,
        f.dirname + "/": None,
    }

    root_metadata = struct(
        directories = {},
        files = [],
        relative = "",
        human_readable = str(ctx.label),
    )

    topological = [root_metadata]
    for src in ctx.files.srcs:
        prefix = _prefix_match(src, prefixes)
        prefixes[prefix] = src
        relative = src.path[len(prefix):].split("/")
        current_path = root_metadata
        for dirname in relative[:-1]:
            if dirname not in current_path.directories:
                dir_metadata = struct(
                    directories = {},
                    files = [],
                    relative = paths.join(current_path.relative, dirname),
                    human_readable = paths.join(current_path.human_readable, dirname),
                )
                current_path.directories[dirname] = dir_metadata
                topological.append(dir_metadata)

            current_path = current_path.directories[dirname]

        current_path.files.append(src)

    # The output DirectoryInfos. Key them by something arbitrary but unique.
    # In this case, we choose relative.
    out = {}

    root_path = _choose_path(prefixes)

    # By doing it in reversed topological order, we ensure that a child is
    # created before its parents. This means that when we create a provider,
    # we can always guarantee that a depset of its children will work.
    for dir_metadata in reversed(topological):
        directories = {
            dirname: out[subdir_metadata.relative]
            for dirname, subdir_metadata in sorted(dir_metadata.directories.items())
        }
        entries = {
            file.basename: file
            for file in dir_metadata.files
        }
        entries.update(directories)

        transitive_files = depset(
            direct = sorted(dir_metadata.files, key = lambda f: f.basename),
            transitive = [
                d.transitive_files
                for d in directories.values()
            ],
            order = "preorder",
        )
        directory = create_directory_info(
            entries = {k: v for k, v in sorted(entries.items())},
            transitive_files = transitive_files,
            path = paths.join(root_path, dir_metadata.relative) if dir_metadata.relative else root_path,
            human_readable = dir_metadata.human_readable,
        )
        out[dir_metadata.relative] = directory

    root_directory = out[root_metadata.relative]

    return [
        root_directory,
        DefaultInfo(files = root_directory.transitive_files),
    ]

directory = rule(
    implementation = _directory_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = True,
        ),
    },
    provides = [DirectoryInfo],
)
