# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Install npm packages

Rules to install NodeJS dependencies during WORKSPACE evaluation.
This happens before the first build or test runs, allowing you to use Bazel
as the package manager.

See discussion in the README.
"""

load("@rules_nodejs//nodejs/private:os_name.bzl", "is_windows_os", "os_name")
load("@rules_nodejs//nodejs/private:node_labels.bzl", "get_node_label", "get_npm_label")
load("//:version.bzl", "VERSION")
load("//internal/common:is_js_file.bzl", "is_javascript_filename")
load("@bazel_skylib//lib:paths.bzl", "paths")

COMMON_ATTRIBUTES = dict(dict(), **{
    "data": attr.label_list(
        doc = """Data files required by this rule.

If symlink_node_modules is True, this attribute is optional since the package manager
will run in your workspace folder. It is recommended, however, that all files that the
package manager depends on, such as `.rc` files or files used in `postinstall`, are added
so that the repository rule is rerun when any of these files change.

If symlink_node_modules is False, the package manager is run in the bazel external
repository so all files that the package manager depends on must be listed.
""",
    ),
    "environment": attr.string_dict(
        doc = """Environment variables to set before calling the package manager.""",
        default = {},
    ),
    "exports_directories_only": attr.bool(
        default = True,
        doc = """Export only top-level package directory artifacts from node_modules.

Set to `False` to export all individual files from node_modules.

When enabled, this decreases the time it takes for Bazel to setup runfiles and sandboxing when
there are a large number of npm dependencies as inputs to an action.

To reference files within npm packages, you can use the `directory_file_path` rule and/or `DirectoryFilePathInfo` provider.
Note, some rules still need upgrading to support consuming `DirectoryFilePathInfo` where needed.

NB: This feature requires runfiles be enabled due to an issue in Bazel which we are still investigating.
    On Windows runfiles are off by default and must be enabled with the `--enable_runfiles` flag when
    using this feature.

NB: `ts_library` does not support directory npm deps due to internal dependency on having all input sources files explicitly specified.

NB: `protractor_web_test` and `protractor_web_test_suite` do not support directory npm deps.

For the `nodejs_binary` & `nodejs_test` `entry_point` attribute (which often needs to reference a file within
an npm package) you can set the entry_point to a dict with a single entry, where the key corresponds to the directory
label and the value corresponds to the path within that directory to the entry point, e.g.

```
nodejs_binary(
    name = "prettier",
    data = ["@npm//prettier"],
    entry_point = { "@npm//:node_modules/prettier": "bin-prettier.js" },
)
```

For labels that are passed to `$(rootpath)`, `$(execpath)`, or `$(location)` you can simply break these apart into
the directory label that gets passed to the expander & path part to follows it, e.g.

```
$(rootpath @npm///:node_modules/prettier)/bin-prettier.js
```
""",
    ),
    "generate_local_modules_build_files": attr.bool(
        default = True,
        doc = """Enables the BUILD files auto generation for local modules installed with `file:` (npm) or `link:` (yarn)

When using a monorepo it's common to have modules that we want to use locally and
publish to an external package repository. This can be achieved using a `js_library` rule
with a `package_name` attribute defined inside the local package `BUILD` file. However,
if the project relies on the local package dependency with `file:` (npm) or `link:` (yarn) to be used outside Bazel, this
could introduce a race condition with both `npm_install` or `yarn_install` rules.

In order to overcome it, a link could be created to the package `BUILD` file from the
npm external Bazel repository (so we can use a local BUILD file instead of an auto generated one),
which require us to set `generate_local_modules_build_files = False` and complete a last step which is writing the
expected targets on that same `BUILD` file to be later used both by `npm_install` or `yarn_install`
rules, which are: `<package_name__files>`, `<package_name__nested_node_modules>`,
`<package_name__contents>`, `<package_name__typings>` and the last one just `<package_name>`. If you doubt what those targets
should look like, check the generated `BUILD` file for a given node module.

When true, the rule will follow the default behaviour of auto generating BUILD files for each `node_module` at install time.

When False, the rule will not auto generate BUILD files for `node_modules` that are installed as symlinks for local modules.
""",
    ),
    "included_files": attr.string_list(
        doc = """List of file extensions to be included in the npm package targets.

NB: This option has no effect when exports_directories_only is True as all files are
automatically included in the exported directory for each npm package.

For example, [".js", ".d.ts", ".proto", ".json", ""].

This option is useful to limit the number of files that are inputs
to actions that depend on npm package targets. See
https://github.com/bazelbuild/bazel/issues/5153.

If set to an empty list then all files are included in the package targets.
If set to a list of extensions, only files with matching extensions are
included in the package targets. An empty string in the list is a special
string that denotes that files with no extensions such as `README` should
be included in the package targets.

This attribute applies to both the coarse `@wksp//:node_modules` target
as well as the fine grained targets such as `@wksp//foo`.
""",
        default = [],
    ),
    "links": attr.string_dict(
        doc = """Targets to link as npm packages.

A mapping of npm package names to bazel targets to linked into node_modules.

For example,

```
yarn_install(
    name = "npm",
    package_json = "//web:package.json",
    yarn_lock = "//web:yarn.lock",
    links = {
        "@scope/target": "//some/scoped/target",
        "target": "//some/target",
    },
)
```

creates targets in the @npm external workspace that can be used by other rules which
are linked into `web/node_modules` along side the 3rd party deps since the `project_path` is `web`.

The above links will create the targets,

```
@npm//@scope/target
@npm//target
```

that can be referenced as `data` or `deps` by other rules such as `nodejs_binary` and `ts_project`
and can be required as `@scope/target` and `target` with standard node_modules resolution at runtime,

```
nodejs_binary(
    name = "bin",
    entry_point = "bin.js",
    deps = [
        "@npm//@scope/target",
        "@npm//target"
        "@npm//other/dep"
    ],
)

ts_project(
    name = "test",
    srcs = [...],
    deps = [
        "@npm//@scope/target",
        "@npm//target"
        "@npm//other/dep"
    ],
)
```
""",
    ),
    "manual_build_file_contents": attr.string(
        doc = """Experimental attribute that can be used to override the generated BUILD.bazel file and set its contents manually.

Can be used to work-around a bazel performance issue if the
default `@wksp//:node_modules` target has too many files in it.
See https://github.com/bazelbuild/bazel/issues/5153. If
you are running into performance issues due to a large
node_modules target it is recommended to switch to using
fine grained npm dependencies.
""",
    ),
    "node_repository": attr.string(
        default = "nodejs",
        doc = """The basename for a nodejs toolchain to use for running npm.
        Usually this is the value of the `name` attribute given to a nodejs_register_toolchains call in WORKSPACE""",
    ),
    "package_json": attr.label(
        mandatory = True,
        allow_single_file = True,
    ),
    "package_path": attr.string(
        default = "",
        doc = """The directory to link `node_modules` to in the execroot and in runfiles.

If unset, link `node_modules` to the directory of the `package.json` file specified in the
`package_json` attribute. Set to "/" to link to the root directory.""",
    ),
    "patch_args": attr.string_list(
        default = ["-p0"],
        doc =
            "The arguments given to the patch tool. Defaults to -p0, " +
            "however -p1 will usually be needed for patches generated by " +
            "git. If multiple -p arguments are specified, the last one will take effect." +
            "If arguments other than -p are specified, Bazel will fall back to use patch " +
            "command line tool instead of the Bazel-native patch implementation. When falling " +
            "back to patch command line tool and patch_tool attribute is not specified, " +
            "`patch` will be used.",
    ),
    "patch_tool": attr.string(
        default = "",
        doc = "The patch(1) utility to use. If this is specified, Bazel will use the specifed " +
              "patch tool instead of the Bazel-native patch implementation.",
    ),
    "post_install_patches": attr.label_list(
        doc = """Patch files to apply after running package manager.

This can be used to make changes to installed packages after the package manager runs.

File paths in patches should be relative to workspace root.

Use with caution when `symlink_node_modules` enabled as the patches will run in your workspace and
will modify files in your workspace.

NB: If `symlink_node_modules` is enabled, the node_modules folder is re-used between executions of the
    repository rule. Patches may be re-applied to files in this case and fail to apply. A marker file
    `node_modules/.bazel-post-install-patches` is left in this mode when patches are applied. When the
    marker file is detected, patch file failures are treated as WARNINGS. For this reason, it is recommended
    to patch npm packages with an npm tool such as https://www.npmjs.com/package/patch-package when
    `symlink_node_modules` is enabled which handles re-apply patching logic more robustly.""",
    ),
    "pre_install_patches": attr.label_list(
        doc = """Patch files to apply before running package manager.

This can be used to make changes to package.json or other data files passed in before running the
package manager.

File paths in patches should be relative to workspace root.

Not supported with `symlink_node_modules` enabled.""",
    ),
    "quiet": attr.bool(
        default = True,
        doc = "If stdout and stderr should be printed to the terminal.",
    ),
    "strict_visibility": attr.bool(
        default = True,
        doc = """Turn on stricter visibility for generated BUILD.bazel files

When enabled, only dependencies within the given `package.json` file are given public visibility.
All transitive dependencies are given limited visibility, enforcing that all direct dependencies are
listed in the `package.json` file.
""",
    ),
    "package_json_remove": attr.string_list(
        doc = """List of `package.json` keys to remove before running the package manager.

Keys are '.' separated. For example, a key of `dependencies.my-dep` in the list corresponds to the `package.json`
entry,

```
{
    "dependencies": {
        "my-dep": "..."
    }
}
```

This can be used, for example, during a migration to remove first party file: deps that are required
for the non-bazel build but should not be installed via the package manager in the bazel build since
they will be reference as bazel targets instead.

NB: removals specified are performed after preinstall_patches so if you are using both then the patch file should be relative
to the source `package.json`. Non-existant keys are silently ignored.""",
    ),
    "package_json_replace": attr.string_dict(
        doc = """Map of `package.json` keys to values to replace or create before running the package mangager.

Keys are '.' separated. For example, a key of `scripts.postinstall` corresponds to the `package.json`
entry,

```
{
    "scripts": {
        "postinstall": "..."
    }
}
```

This can be used, for example, during a migration to override npm scripts such as preinstall & postinstall.

NB: replaces specified are performed after preinstall_patches so if you are using both then the patch file should be relative
to the source `package.json`.""",
    ),
    "symlink_node_modules": attr.bool(
        doc = """Turn symlinking of node_modules on.

**Use of this feature is not recommended, as Bazel is removing `managed_directories`.
See https://github.com/bazelbuild/bazel/issues/15463

When False, the package manager will run in the external repository
created by this rule.
This requires that any files required for it to run should be listed in the
`data` attribute. These files would include things like patch files that are
read by a postinstall lifecycle hook such as the `patch-package` package uses.
`package.json` and the lock file are already specified in dedicated attributes
of this rule and do not need to be included in the `data`.

When True, we run the package manager (npm or yarn) with the working directory
set in your source tree, in the folder containing the package.json file.
The resulting `node_modules` folder in the source tree will be symlinked to the
external repository created by this rule.

This gives the user experience of running `bazel build` in a clean clone,
and the `node_modules` folder is created as if you had run `npm install` yourself.
It avoids installing the dependencies twice in a typical developer workflow, where you'd
need to `npm install` for tooling like formatters or editors, and then Bazel installs
a second copy in its external repository. This can save some time.
It also means you can patch the source files in the `node_modules` folder and these changes
will be reflected in subsequent Bazel executions.

WARNING: we suspect there are some hard-to-reproduce bugs when this feature is used,
because any corruption in the node_modules tree won't be corrected by Bazel.
When repairing this with `bazel clean --expunge` you'll also have to `rm -rf node_modules`
or else the next Bazel run will re-use the same corrupted `node_modules` folder by restoring
the symlink to it.

When using symlink_node_modules, we recommend also enabling Bazel's `managed_directories`
for the node_modules folder. This is documented in the
[workspace global](https://docs.bazel.build/versions/main/skylark/lib/globals.html#workspace)
Be sure to include the `node_modules` path in the `.bazelignore` file.

Using managed_directories will mean that

1. changes to files under `node_modules` are tracked by Bazel as if they were located
   in the external repository folder, and
2. if the `node_modules` folder is deleted from the source tree, Bazel will re-run the
   repository rule that creates it again on the next run.
""",
        default = False,
    ),
    "timeout": attr.int(
        default = 3600,
        doc = """Maximum duration of the package manager execution in seconds.""",
    ),
    "all_node_modules_target_name": attr.string(
        default = "node_modules",
        doc = """The name used for the generated all node_modules js_library target.

        This can be used to name the all node_modules target something other than `//:node_modules`,
        such as `//:node_modules_all`, so you can use the `//:node_modules` label to reference the
        `node_modules` source directory in `manual_build_file_contents` for custom use cases.
        """,
    ),
    "generate_build_files_concurrency_limit": attr.int(
        default = 64,
        doc = """Limit the maximum concurrency of npm package processing when generating
        BUILD files from the node_modules tree. Unlimited concurrency can lead to too many
        open files errors (https://github.com/bazelbuild/rules_nodejs/issues/3507).

        Set to 0 or negative for unlimited concurrency.""",
    ),
})

def _apply_pre_install_patches(repository_ctx):
    if len(repository_ctx.attr.pre_install_patches) == 0:
        return
    if repository_ctx.attr.symlink_node_modules:
        fail("pre_install_patches cannot be used with symlink_node_modules enabled")
    _apply_patches(repository_ctx, _WORKSPACE_REROOTED_PATH, repository_ctx.attr.pre_install_patches)

def _apply_post_install_patches(repository_ctx):
    if len(repository_ctx.attr.post_install_patches) == 0:
        return
    if repository_ctx.attr.symlink_node_modules:
        print("\nWARNING: @%s post_install_patches with symlink_node_modules enabled will run in your workspace and potentially modify source files" % repository_ctx.name)
    working_directory = _user_workspace_root(repository_ctx) if repository_ctx.attr.symlink_node_modules else _WORKSPACE_REROOTED_PATH
    marker_file = None
    if repository_ctx.attr.symlink_node_modules:
        marker_file = "%s/node_modules/.bazel-post-install-patches" % repository_ctx.path(repository_ctx.attr.package_json).dirname
    _apply_patches(repository_ctx, working_directory, repository_ctx.attr.post_install_patches, marker_file)

def _apply_patches(repository_ctx, working_directory, patches, marker_file = None):
    bash_exe = repository_ctx.os.environ["BAZEL_SH"] if "BAZEL_SH" in repository_ctx.os.environ else "bash"

    patch_tool = repository_ctx.attr.patch_tool
    if not patch_tool:
        patch_tool = "patch"
    patch_args = repository_ctx.attr.patch_args

    for patch_file in patches:
        if marker_file:
            command = """{patch_tool} {patch_args} < {patch_file}
CODE=$?
if [ $CODE -ne 0 ]; then
    CODE=1
    if [ -f \"{marker_file}\" ]; then
        CODE=2
    fi
fi
echo '1' > \"{marker_file}\"
exit $CODE""".format(
                patch_tool = patch_tool,
                patch_file = repository_ctx.path(patch_file),
                patch_args = " ".join([
                    "'%s'" % arg
                    for arg in patch_args
                ]),
                marker_file = marker_file,
            )
        else:
            command = "{patch_tool} {patch_args} < {patch_file}".format(
                patch_tool = patch_tool,
                patch_file = repository_ctx.path(patch_file),
                patch_args = " ".join([
                    "'%s'" % arg
                    for arg in patch_args
                ]),
            )

        if not repository_ctx.attr.quiet:
            print("@%s appling patch file %s in %s" % (repository_ctx.name, patch_file, working_directory))
            if marker_file:
                print("@%s leaving patches marker file %s" % (repository_ctx.name, marker_file))
        st = repository_ctx.execute(
            [bash_exe, "-c", command],
            quiet = repository_ctx.attr.quiet,
            # Working directory is _ which is where all files are copied to and
            # where the install is run; patches should be relative to workspace root.
            working_directory = working_directory,
        )
        if st.return_code:
            # If return code is 2 (see bash snippet above) that means a marker file was found before applying patches;
            # Treat patch failure as a warning in this case
            if st.return_code == 2:
                print("""\nWARNING: @%s failed to apply patch file %s in %s:\n%s%s
This can happen with symlink_node_modules enabled since your workspace node_modules is re-used between executions of the repository rule.""" % (repository_ctx.name, patch_file, working_directory, st.stderr, st.stdout))
            else:
                fail("Error applying patch %s in %s:\n%s%s" % (str(patch_file), working_directory, st.stderr, st.stdout))

def _create_build_files(repository_ctx, rule_type, node, lock_file, generate_local_modules_build_files):
    repository_ctx.report_progress("Processing node_modules: installing Bazel packages and generating BUILD files")
    if repository_ctx.attr.manual_build_file_contents:
        repository_ctx.file("manual_build_file_contents", repository_ctx.attr.manual_build_file_contents)

    # validate links
    validated_links = {}
    for k, v in repository_ctx.attr.links.items():
        if v.startswith("//"):
            v = "@%s" % v
        if not v.startswith("@"):
            fail("link target must be label of form '@wksp//path/to:target', '@//path/to:target' or '//path/to:target'")
        validated_links[k] = v

    package_json_dir = paths.dirname(paths.normalize(paths.join(
        repository_ctx.attr.package_json.package,
        repository_ctx.attr.package_json.name,
    )))
    package_path = repository_ctx.attr.package_path
    if not package_path:
        # By default the package_path is the directory of the package.json file
        package_path = paths.join(repository_ctx.attr.package_json.workspace_root, package_json_dir)
    elif package_path == "/":
        # User specified root path
        package_path = ""

    generate_config_json = json.encode(
        struct(
            exports_directories_only = repository_ctx.attr.exports_directories_only,
            generate_build_files_concurrency_limit = repository_ctx.attr.generate_build_files_concurrency_limit,
            generate_local_modules_build_files = generate_local_modules_build_files,
            included_files = repository_ctx.attr.included_files,
            links = validated_links,
            package_json = str(repository_ctx.path(repository_ctx.attr.package_json)),
            package_lock = str(repository_ctx.path(lock_file)),
            package_path = package_path,
            rule_type = rule_type,
            strict_visibility = repository_ctx.attr.strict_visibility,
            workspace = repository_ctx.attr.name,
            workspace_rerooted_package_json_dir = paths.normalize(paths.join(_WORKSPACE_REROOTED_PATH, package_json_dir)),
            workspace_rerooted_path = _WORKSPACE_REROOTED_PATH,
            all_node_modules_target_name = repository_ctx.attr.all_node_modules_target_name,
        ),
    )
    repository_ctx.file("generate_config.json", generate_config_json)
    result = repository_ctx.execute(
        [node, "index.js"],
        # double the default timeout in case of many packages, see #2231
        timeout = 1200,
        quiet = repository_ctx.attr.quiet,
    )
    if result.return_code:
        fail("generate_build_file.ts failed: \nSTDOUT:\n%s\nSTDERR:\n%s" % (result.stdout, result.stderr))

def _add_scripts(repository_ctx):
    repository_ctx.template(
        "pre_process_package_json.js",
        repository_ctx.path(Label("//internal/npm_install:pre_process_package_json.js")),
        {},
    )

    repository_ctx.template(
        "index.js",
        repository_ctx.path(Label("//internal/npm_install:index.js")),
        {},
    )

# The directory in the external repository where we re-root the workspace by copying
# package.json, lock file & data files to and running the package manager in the
# folder of the package.json file.
_WORKSPACE_REROOTED_PATH = "_"

# Returns the root of the user workspace. No built-in way to get
# this but we can derive it from the path of the package.json file
# in the user workspace sources.
def _user_workspace_root(repository_ctx):
    package_json = repository_ctx.attr.package_json
    segments = []
    if package_json.package:
        segments.extend(package_json.package.split("/"))
    segments.extend(package_json.name.split("/"))
    segments.pop()
    user_workspace_root = repository_ctx.path(package_json).dirname
    for i in segments:
        user_workspace_root = user_workspace_root.dirname
    return str(user_workspace_root)

# Returns the path to a file within the re-rooted user workspace
# under _WORKSPACE_REROOTED_PATH in this repo rule's external workspace
def _rerooted_workspace_path(repository_ctx, f):
    return paths.normalize(paths.join(_WORKSPACE_REROOTED_PATH, f.package, f.name))

# Returns the path to the package.json directory within the re-rooted user workspace
# under _WORKSPACE_REROOTED_PATH in this repo rule's external workspace
def _rerooted_workspace_package_json_dir(repository_ctx):
    return str(repository_ctx.path(_rerooted_workspace_path(repository_ctx, repository_ctx.attr.package_json)).dirname)

def _copy_file(repository_ctx, f):
    to = _rerooted_workspace_path(repository_ctx, f)

    # ensure the destination directory exists
    to_segments = to.split("/")
    if len(to_segments) > 1:
        dirname = "/".join(to_segments[:-1])
        args = ["mkdir", "-p", dirname] if not is_windows_os(repository_ctx) else ["cmd", "/c", "if not exist {dir} (mkdir {dir})".format(dir = dirname.replace("/", "\\"))]
        result = repository_ctx.execute(
            args,
            quiet = repository_ctx.attr.quiet,
        )
        if result.return_code:
            fail("mkdir -p %s failed: \nSTDOUT:\n%s\nSTDERR:\n%s" % (dirname, result.stdout, result.stderr))

    # copy the file; don't use the repository_ctx.template trick with empty substitution as this
    # does not copy over binary files properly
    cp_args = ["cp", "-f", repository_ctx.path(f), to] if not is_windows_os(repository_ctx) else ["xcopy", "/Y", str(repository_ctx.path(f)).replace("/", "\\"), "\\".join(to_segments) + "*"]
    result = repository_ctx.execute(
        cp_args,
        quiet = repository_ctx.attr.quiet,
    )
    if result.return_code:
        fail("cp -f {} {} failed: \nSTDOUT:\n{}\nSTDERR:\n{}".format(repository_ctx.path(f), to, result.stdout, result.stderr))

def _package_json_changes(repository_ctx):
    if len(repository_ctx.attr.package_json_replace.keys()) == 0 and len(repository_ctx.attr.package_json_remove) == 0:
        # there are no replacements to make
        return

    # split key segments by dot
    key_split_char = "."

    # read, save a .orig & and decode the contents of the package.json file
    package_json_path = _rerooted_workspace_path(repository_ctx, repository_ctx.attr.package_json)
    package_json_contents = repository_ctx.read(package_json_path)
    repository_ctx.file(
        package_json_path + ".orig",
        content = package_json_contents,
    )
    package_json = json.decode(package_json_contents)

    # process package_json_replace attr
    for replace_key, replace_value in repository_ctx.attr.package_json_replace.items():
        segments = replace_key.split(key_split_char)
        num_segments = len(segments)
        loc = package_json
        for i, segment in enumerate(segments):
            if i < num_segments - 1:
                if segment not in loc:
                    loc[segment] = {}
                loc = loc[segment]
            else:
                loc[segment] = replace_value

    # process package_json_remove attr
    for remove_key in repository_ctx.attr.package_json_remove:
        segments = remove_key.split(key_split_char)
        num_segments = len(segments)
        loc = package_json
        for i, segment in enumerate(segments):
            if i < num_segments - 1:
                if segment not in loc:
                    break
                loc = loc[segment]
            else:
                loc.pop(segment, None)

    # overwrite the package.json file with the changes made
    repository_ctx.file(
        package_json_path,
        content = json.encode_indent(package_json, indent = "  "),
    )

def _symlink_file(repository_ctx, f):
    repository_ctx.symlink(f, _rerooted_workspace_path(repository_ctx, f))

def _copy_data_dependencies(repository_ctx):
    """Add data dependencies to the repository."""
    for f in repository_ctx.attr.data:
        # Make copies of the data files instead of symlinking
        # as yarn under linux will have trouble using symlinked
        # files as npm file:// packages
        _copy_file(repository_ctx, f)

def _repository_contains_file(rctx, repo, file):
    """Detect whether the file exists relative to the repo.

    Surprisingly rctx.path() throws when the argument doesn't exist,
    so you can't get a path object directly to check exists on.
    As a workaround, make a path object from the WORKSPACE file first
    and then go relative to it.
    """
    wksp = Label("@%s//:WORKSPACE" % repo)
    child = rctx.path(wksp).dirname
    for sub in file.split("/"):
        child = child.get_child(sub)
    return child.exists

def _add_node_repositories_info_deps(repository_ctx, yarn = None):
    # Add a dep to the node_info & yarn_info files from node_repositories
    # so that if the node or yarn versions change we re-run the repository rule
    # But in case they are vendored, our info file may not be present, so check first.
    # A vendored node may have no info file in the repo.
    node_repo = "_".join([repository_ctx.attr.node_repository, os_name(repository_ctx)])
    if _repository_contains_file(repository_ctx, node_repo, "node_info"):
        repository_ctx.symlink(
            Label("@{}//:node_info".format(node_repo)),
            repository_ctx.path("_node_info"),
        )

    # A custom yarn might be vendored, and not have a yarn_info file in the repo.
    if yarn and _repository_contains_file(repository_ctx, yarn.workspace_name, "yarn_info"):
        repository_ctx.symlink(
            Label("@{}//:yarn_info".format(yarn.workspace_name)),
            repository_ctx.path("_yarn_info"),
        )

def _symlink_node_modules(repository_ctx):
    package_json_dir = repository_ctx.path(repository_ctx.attr.package_json).dirname
    if repository_ctx.attr.exports_directories_only:
        if repository_ctx.attr.symlink_node_modules:
            # Create a directory symlink in the rerooted workspace path within the external
            # repository, _WORKSPACE_REROOTED_PATH/package/json/dir/node_modules that points to the
            # the newly created node_modules folder in the user's workspace (which is in the same
            # directory as the package.json file there)
            repository_ctx.symlink(
                repository_ctx.path(str(package_json_dir) + "/node_modules"),
                repository_ctx.path(_rerooted_workspace_package_json_dir(repository_ctx) + "/node_modules"),
            )
    elif repository_ctx.attr.symlink_node_modules:
        # Create a directory symlink at the root external repository that points to the the
        # newly created node_modules folder in the user's workspace (which is in the same
        # directory as the package.json file there)
        repository_ctx.symlink(
            repository_ctx.path(str(package_json_dir) + "/node_modules"),
            repository_ctx.path("node_modules"),
        )
    else:
        # Create a directory symlink at the root external repository that points to the the
        # newly created node_modules folder in the rerooted workspace path within the external
        # repository, _WORKSPACE_REROOTED_PATH/package/json/dir/node_modules.
        repository_ctx.symlink(
            repository_ctx.path(_rerooted_workspace_package_json_dir(repository_ctx) + "/node_modules"),
            repository_ctx.path("node_modules"),
        )

def _npm_install_impl(repository_ctx):
    """Core implementation of npm_install."""
    is_windows_host = is_windows_os(repository_ctx)
    node = repository_ctx.path(get_node_label(repository_ctx))
    npm = get_npm_label(repository_ctx)

    # Set the base command (install or ci)
    npm_args = [repository_ctx.attr.npm_command]

    npm_args.extend(repository_ctx.attr.args)

    # Run the package manager in the package.json folder
    if repository_ctx.attr.symlink_node_modules:
        root = str(repository_ctx.path(repository_ctx.attr.package_json).dirname)
    else:
        root = str(repository_ctx.path(_rerooted_workspace_package_json_dir(repository_ctx)))

    # The entry points for npm install for osx/linux and windows
    if not is_windows_host:
        # Prefix filenames with _ so they don't conflict with the npm package `npm`
        repository_ctx.file(
            "_npm.sh",
            content = """#!/usr/bin/env bash
# Immediately exit if any command fails.
set -e
(cd "{root}"; "{npm}" {npm_args})
""".format(
                root = root,
                npm = repository_ctx.path(npm),
                npm_args = " ".join(npm_args),
            ),
            executable = True,
        )
    else:
        repository_ctx.file(
            "_npm.cmd",
            content = """@echo off
cd /D "{root}" && "{npm}" {npm_args}
""".format(
                root = root,
                npm = repository_ctx.path(npm),
                npm_args = " ".join(npm_args),
            ),
            executable = True,
        )

    _symlink_file(repository_ctx, repository_ctx.attr.package_lock_json)
    _copy_file(repository_ctx, repository_ctx.attr.package_json)
    _copy_data_dependencies(repository_ctx)
    _add_scripts(repository_ctx)
    _add_node_repositories_info_deps(repository_ctx)
    _apply_pre_install_patches(repository_ctx)

    # _package_json_changes should be called _after_ _apply_pre_install_patches (as per docstring)
    _package_json_changes(repository_ctx)

    result = repository_ctx.execute(
        [node, "pre_process_package_json.js", repository_ctx.path(_rerooted_workspace_path(repository_ctx, repository_ctx.attr.package_json)), "npm"],
        quiet = repository_ctx.attr.quiet,
    )
    if result.return_code:
        fail("pre_process_package_json.js failed: \nSTDOUT:\n%s\nSTDERR:\n%s" % (result.stdout, result.stderr))

    env = dict(repository_ctx.attr.environment)
    env_key = "BAZEL_NPM_INSTALL"
    if env_key not in env.keys():
        env[env_key] = "1"
    env["BUILD_BAZEL_RULES_NODEJS_VERSION"] = VERSION

    # NB: after running npm install, it's essential that we don't cause the repository rule to restart
    # This means we must not reference any additional labels after this point.
    # See https://github.com/bazelbuild/rules_nodejs/issues/2620
    repository_ctx.report_progress("Running npm install on %s" % repository_ctx.attr.package_json)
    result = repository_ctx.execute(
        [repository_ctx.path("_npm.cmd" if is_windows_host else "_npm.sh")],
        timeout = repository_ctx.attr.timeout,
        quiet = repository_ctx.attr.quiet,
        environment = env,
    )

    if result.return_code:
        fail("npm_install failed: %s (%s)" % (result.stdout, result.stderr))

    # removeNPMAbsolutePaths is run on node_modules after npm install as the package.json files
    # generated by npm are non-deterministic. They contain absolute install paths and other private
    # information fields starting with "_". removeNPMAbsolutePaths removes all fields starting with "_".
    fix_absolute_paths_cmd = [
        node,
        repository_ctx.path(repository_ctx.attr._remove_npm_absolute_paths),
        root + "/node_modules",
    ]

    if not repository_ctx.attr.quiet:
        print(fix_absolute_paths_cmd)
    result = repository_ctx.execute(fix_absolute_paths_cmd)

    if result.return_code:
        fail("remove_npm_absolute_paths failed: %s (%s)" % (result.stdout, result.stderr))

    _symlink_node_modules(repository_ctx)
    _apply_post_install_patches(repository_ctx)

    _create_build_files(repository_ctx, "npm_install", node, repository_ctx.attr.package_lock_json, repository_ctx.attr.generate_local_modules_build_files)

npm_install = repository_rule(
    attrs = dict(COMMON_ATTRIBUTES, **{
        "args": attr.string_list(
            doc = """Arguments passed to npm install.

See npm CLI docs https://docs.npmjs.com/cli/install.html for complete list of supported arguments.""",
            default = [],
        ),
        "npm_command": attr.string(
            default = "ci",
            doc = """The npm command to run, to install dependencies.

            See npm docs <https://docs.npmjs.com/cli/v6/commands>

            In particular, for "ci" it says:
            > If dependencies in the package lock do not match those in package.json, npm ci will exit with an error, instead of updating the package lock.
            """,
            values = ["ci", "install"],
        ),
        "package_lock_json": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "_remove_npm_absolute_paths": attr.label(default = Label("//third_party/github.com/juanjoDiaz/removeNPMAbsolutePaths:bin/removeNPMAbsolutePaths")),
    }),
    doc = """Runs npm install during workspace setup.

This rule will set the environment variable `BAZEL_NPM_INSTALL` to '1' (unless it
set to another value in the environment attribute). Scripts may use to this to
check if yarn is being run by the `npm_install` repository rule.""",
    implementation = _npm_install_impl,
)

def _detect_yarn_version(rctx, yarn):
    result = rctx.execute(
        yarn + ["--version"],
        working_directory = str(rctx.path(rctx.attr.package_json).dirname),
    )
    if result.return_code:
        fail("yarn --version failed: %s (%s)" % (result.stdout, result.stderr))
    if result.stdout.startswith("1."):
        return "classic"

    return "berry"

def _yarn_install_impl(repository_ctx):
    """Core implementation of yarn_install."""
    is_windows_host = is_windows_os(repository_ctx)
    node = repository_ctx.path(get_node_label(repository_ctx))
    yarn_label = repository_ctx.attr.yarn

    # A custom yarn won't have our special wrapper batch script
    if is_windows_host and _repository_contains_file(repository_ctx, yarn_label.workspace_name, "bin/yarn.cmd"):
        yarn_label = yarn_label.relative(":bin/yarn.cmd")

    if is_javascript_filename(yarn_label.name):
        yarn_cmd = [node, yarn_label]
    else:
        # Our wrapper scripts include the "node" executable
        yarn_cmd = [yarn_label]

    yarn_version = _detect_yarn_version(repository_ctx, yarn_cmd)

    # Quoted command line for use in scripts
    yarn = " ".join(["\"%s\"" % repository_ctx.path(s) for s in yarn_cmd])
    yarn_args = []

    # CLI arguments changed in yarn 2+
    if yarn_version == "berry":
        # According to maintainers, the yarn cache is now atomic in berry, so this flag is only needed in classic.
        # See https://github.com/bazelbuild/rules_nodejs/pull/3195#discussion_r780719932
        if not repository_ctx.attr.use_global_yarn_cache:
            fail("""Disabling global_yarn_cache has no effect while using Yarn Berry.
                Please configure it through a .yarnrc file through 'data' attribute.""")
        if not repository_ctx.attr.frozen_lockfile:
            fail("Disabling frozen_lockfile has no effect while using Yarn Berry. Just pass arguments like --immutable.")
    elif yarn_version == "classic":
        # Set frozen lockfile as default install to install the exact version from the yarn.lock
        # file. To perform an yarn install use the vendord yarn binary with:
        # `bazel run @yarn//:yarn install` or `bazel run @yarn//:yarn install -- -D <dep-name>`
        if repository_ctx.attr.frozen_lockfile:
            yarn_args.append("--frozen-lockfile")

        if repository_ctx.attr.use_global_yarn_cache:
            # Multiple yarn rules cannot run simultaneously using a shared cache.
            # See https://github.com/yarnpkg/yarn/issues/683
            # The --mutex option ensures only one yarn runs at a time, see
            # https://yarnpkg.com/en/docs/cli#toc-concurrency-and-mutex
            # The shared cache is not necessarily hermetic, but we need to cache downloaded
            # artifacts somewhere, so we rely on yarn to be correct.
            yarn_args.extend(["--mutex", "network"])
        else:
            yarn_args.extend(["--cache-folder", str(repository_ctx.path("_yarn_cache"))])
    else:
        fail("yarn_version has unknown value " + yarn_version)

    yarn_args.extend(repository_ctx.attr.args)

    # Run the package manager in the package.json folder
    if repository_ctx.attr.symlink_node_modules:
        root = str(repository_ctx.path(repository_ctx.attr.package_json).dirname)
    else:
        root = str(repository_ctx.path(_rerooted_workspace_package_json_dir(repository_ctx)))

    # The entry points for npm install for osx/linux and windows
    if not is_windows_host:
        # Prefix filenames with _ so they don't conflict with the npm packages.
        #
        # Unset YARN_IGNORE_PATH before calling yarn incase it is set so that
        # .yarnrc yarn-path is followed if set. This is for the case when
        # calling bazel from yarn with `yarn bazel ...` and yarn follows
        # yarn-path in .yarnrc it will set YARN_IGNORE_PATH=1 which will prevent
        # the bazel call into yarn from also following the yarn-path as desired.
        #
        # Unset INIT_CWD before calling yarn. This env variable can be set by an
        # outer yarn if bazel is invoked from a package.json script. It can
        # confuse post-install scripts if they use this env var.
        #
        # Unset npm_config_registry before calling yarn. This env variable can
        # be set by an outer yarn if bazel is invoked from a package.json script
        # and they can break the yarn install in some cases. The case observed
        # was an .npmrc file with a registry override in the user WORKSPACE
        # breaking the install with the error: (error An unexpected error
        # occurred: “Cannot create property ‘https’ on string
        # ‘https://domain/artifactory/api/npm/npm/’“.)
        repository_ctx.file(
            "_yarn.sh",
            content = """#!/usr/bin/env bash
# Immediately exit if any command fails.
set -e
unset YARN_IGNORE_PATH
unset INIT_CWD
unset npm_config_registry
(cd "{root}"; {yarn} {yarn_args})
""".format(
                root = root,
                yarn = yarn,
                yarn_args = " ".join(yarn_args),
            ),
            executable = True,
        )
    else:
        repository_ctx.file(
            "_yarn.cmd",
            content = """@echo off
set "YARN_IGNORE_PATH="
set “INIT_CWD=”
set “npm_config_registry=”
cd /D "{root}" && {yarn} {yarn_args}
""".format(
                root = root,
                yarn = yarn,
                yarn_args = " ".join(yarn_args),
            ),
            executable = True,
        )

    _symlink_file(repository_ctx, repository_ctx.attr.yarn_lock)
    _copy_file(repository_ctx, repository_ctx.attr.package_json)
    _copy_data_dependencies(repository_ctx)
    _add_scripts(repository_ctx)
    _add_node_repositories_info_deps(repository_ctx, yarn = repository_ctx.attr.yarn)
    _apply_pre_install_patches(repository_ctx)

    # _package_json_changes should be called _after_ _apply_pre_install_patches (as per docstring)
    _package_json_changes(repository_ctx)

    result = repository_ctx.execute(
        [node, "pre_process_package_json.js", repository_ctx.path(_rerooted_workspace_path(repository_ctx, repository_ctx.attr.package_json)), "yarn"],
        quiet = repository_ctx.attr.quiet,
    )
    if result.return_code:
        fail("pre_process_package_json.js failed: \nSTDOUT:\n%s\nSTDERR:\n%s" % (result.stdout, result.stderr))

    env = dict(repository_ctx.attr.environment)
    env_key = "BAZEL_YARN_INSTALL"
    if env_key not in env.keys():
        env[env_key] = "1"
    env["BUILD_BAZEL_RULES_NODEJS_VERSION"] = VERSION

    repository_ctx.report_progress("Running yarn install on %s" % repository_ctx.attr.package_json)
    result = repository_ctx.execute(
        [repository_ctx.path("_yarn.cmd" if is_windows_host else "_yarn.sh")],
        timeout = repository_ctx.attr.timeout,
        quiet = repository_ctx.attr.quiet,
        environment = env,
    )
    if result.return_code:
        fail("yarn_install failed: %s (%s)" % (result.stdout, result.stderr))

    _symlink_node_modules(repository_ctx)
    _apply_post_install_patches(repository_ctx)

    _create_build_files(repository_ctx, "yarn_install", node, repository_ctx.attr.yarn_lock, repository_ctx.attr.generate_local_modules_build_files)

_DEFAULT_YARN = Label("@yarn//:bin/yarn")

yarn_install = repository_rule(
    attrs = dict(COMMON_ATTRIBUTES, **{
        "args": attr.string_list(
            doc = """Arguments passed to yarn install.

See yarn CLI docs for complete list of supported arguments.
Yarn 1: https://yarnpkg.com/en/docs/cli/install
Yarn 2+ (Berry): https://yarnpkg.com/cli/install

Note that Yarn Berry PnP is *not* supported, follow
https://github.com/bazelbuild/rules_nodejs/issues/1599
""",
            default = [],
        ),
        "frozen_lockfile": attr.bool(
            default = True,
            doc = """Use the `--frozen-lockfile` flag for yarn 1

Users of Yarn 2+ (Berry) should just pass `--immutable` to the `args` attribute.

Don't generate a `yarn.lock` lockfile and fail if an update is needed.

This flag enables an exact install of the version that is specified in the `yarn.lock`
file. This helps to have reproducible builds across builds.

To update a dependency or install a new one run the `yarn install` command with the
vendored yarn binary. `bazel run @yarn//:yarn install`. You can pass the options like
`bazel run @yarn//:yarn install -- -D <dep-name>`.
""",
        ),
        "use_global_yarn_cache": attr.bool(
            default = True,
            doc = """Use the global yarn cache on the system.

The cache lets you avoid downloading packages multiple times.
However, it can introduce non-hermeticity, and the yarn cache can
have bugs.

Disabling this attribute causes every run of yarn to have a unique
cache_directory.

If True and using Yarn 1, this rule will pass `--mutex network` to yarn to ensure that
the global cache can be shared by parallelized yarn_install rules.

The True value has no effect on Yarn 2+ (Berry).

If False, this rule will pass `--cache-folder /path/to/external/repository/__yarn_cache`
to yarn so that the local cache is contained within the external repository.
""",
        ),
        "yarn": attr.label(
            default = _DEFAULT_YARN,
            doc = "The yarn.js entry point to execute",
        ),
        "yarn_lock": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
    }),
    doc = """Runs yarn install during workspace setup.

This rule will set the environment variable `BAZEL_YARN_INSTALL` to '1' (unless it
set to another value in the environment attribute). Scripts may use to this to
check if yarn is being run by the `yarn_install` repository rule.""",
    implementation = _yarn_install_impl,
)
