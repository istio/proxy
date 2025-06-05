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

"""Common util functions for java_* rules"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_helper.bzl", "cc_helper")
load("//java/common:java_semantics.bzl", "semantics")

visibility(["//java/..."])

def _collect_all_targets_as_deps(ctx, classpath_type = "all"):
    deps = []
    if not classpath_type == "compile_only":
        if hasattr(ctx.attr, "runtime_deps"):
            deps.extend(ctx.attr.runtime_deps)
        if hasattr(ctx.attr, "exports"):
            deps.extend(ctx.attr.exports)

    deps.extend(ctx.attr.deps or [])

    launcher = _filter_launcher_for_target(ctx)
    if launcher:
        deps.append(launcher)

    return deps

def _filter_launcher_for_target(ctx):
    # create_executable=0 disables the launcher
    if hasattr(ctx.attr, "create_executable") and not ctx.attr.create_executable:
        return None

    # use_launcher=False disables the launcher
    if hasattr(ctx.attr, "use_launcher") and not ctx.attr.use_launcher:
        return None

    # BUILD rule "launcher" attribute
    if ctx.attr.launcher and cc_common.launcher_provider in ctx.attr.launcher:
        return ctx.attr.launcher

    return None

def _launcher_artifact_for_target(ctx):
    launcher = _filter_launcher_for_target(ctx)
    if not launcher:
        return None
    files = launcher[DefaultInfo].files.to_list()
    if len(files) != 1:
        fail("%s expected a single artifact in %s" % (ctx.label, launcher))
    return files[0]

def _check_and_get_main_class(ctx):
    create_executable = ctx.attr.create_executable
    use_testrunner = ctx.attr.use_testrunner
    main_class = ctx.attr.main_class

    if not create_executable and use_testrunner:
        fail("cannot have use_testrunner without creating an executable")
    if not create_executable and main_class:
        fail("main class must not be specified when executable is not created")
    if create_executable and not use_testrunner:
        if not main_class:
            if not ctx.attr.srcs:
                fail("need at least one of 'main_class', 'use_testrunner' or Java source files")
            main_class = _primary_class(ctx)
            if main_class == None:
                fail("main_class was not provided and cannot be inferred: " +
                     "source path doesn't include a known root (java, javatests, src, testsrc)")
    if not create_executable:
        return None
    if not main_class:
        if use_testrunner:
            main_class = "com.google.testing.junit.runner.GoogleTestRunner"
        else:
            main_class = _primary_class(ctx)
    return main_class

def _primary_class(ctx):
    if ctx.attr.srcs:
        main = ctx.label.name + ".java"
        for src in ctx.files.srcs:
            if src.basename == main:
                return _full_classname(_strip_extension(src))
    return _full_classname(_get_relative(ctx.label.package, ctx.label.name))

def _strip_extension(file):
    return file.dirname + "/" + (
        file.basename[:-(1 + len(file.extension))] if file.extension else file.basename
    )

# TODO(b/193629418): once out of builtins, create a canonical implementation and remove duplicates in depot
def _full_classname(path):
    java_segments = _java_segments(path)
    return ".".join(java_segments) if java_segments != None else None

def _java_segments(path):
    if path.startswith("/"):
        fail("path must not be absolute: '%s'" % path)
    segments = path.split("/")
    root_idx = -1
    for idx, segment in enumerate(segments):
        if segment in ["java", "javatests", "src", "testsrc"]:
            root_idx = idx
            break
    if root_idx < 0:
        return None
    is_src = "src" == segments[root_idx]
    check_mvn_idx = root_idx if is_src else -1
    if (root_idx == 0 or is_src):
        for i in range(root_idx + 1, len(segments) - 1):
            segment = segments[i]
            if "src" == segment or (is_src and (segment in ["java", "javatests"])):
                next = segments[i + 1]
                if next in ["com", "org", "net"]:
                    root_idx = i
                elif "src" == segment:
                    check_mvn_idx = i
                break

    if check_mvn_idx >= 0 and check_mvn_idx < len(segments) - 2:
        next = segments[check_mvn_idx + 1]
        if next in ["main", "test"]:
            next = segments[check_mvn_idx + 2]
            if next in ["java", "resources"]:
                root_idx = check_mvn_idx + 2
    return segments[(root_idx + 1):]

def _concat(*lists):
    result = []
    for list in lists:
        result.extend(list)
    return result

def _get_shared_native_deps_path(
        linker_inputs,
        link_opts,
        linkstamps,
        build_info_artifacts,
        features,
        is_test_target_partially_disabled_thin_lto):
    """
    Returns the path of the shared native library.

    The name must be generated based on the rule-specific inputs to the link actions. At this point
    this includes order-sensitive list of linker inputs and options collected from the transitive
    closure and linkstamp-related artifacts that are compiled during linking. All those inputs can
    be affected by modifying target attributes (srcs/deps/stamp/etc). However, target build
    configuration can be ignored since it will either change output directory (in case of different
    configuration instances) or will not affect anything (if two targets use same configuration).
    Final goal is for all native libraries that use identical linker command to use same output
    name.

    <p>TODO(bazel-team): (2010) Currently process of identifying parameters that can affect native
    library name is manual and should be kept in sync with the code in the
    CppLinkAction.Builder/CppLinkAction/Link classes which are responsible for generating linker
    command line. Ideally we should reuse generated command line for both purposes - selecting a
    name of the native library and using it as link action payload. For now, correctness of the
    method below is only ensured by validations in the CppLinkAction.Builder.build() method.
    """

    fp = ""
    for artifact in linker_inputs:
        fp += artifact.short_path
    fp += str(len(link_opts))
    for opt in link_opts:
        fp += opt
    for artifact in linkstamps:
        fp += artifact.short_path
    for artifact in build_info_artifacts:
        fp += artifact.short_path
    for feature in features:
        fp += feature

    # Sharing of native dependencies may cause an ActionConflictException when ThinLTO is
    # disabled for test and test-only targets that are statically linked, but enabled for other
    # statically linked targets. This happens in case the artifacts for the shared native
    # dependency are output by actions owned by the non-test and test targets both. To fix
    # this, we allow creation of multiple artifacts for the shared native library - one shared
    # among the test and test-only targets where ThinLTO is disabled, and the other shared among
    # other targets where ThinLTO is enabled.
    fp += "1" if is_test_target_partially_disabled_thin_lto else "0"

    fingerprint = "%x" % hash(fp)
    return "_nativedeps/" + fingerprint

def _check_and_get_one_version_attribute(ctx, attr):
    value = getattr(semantics.find_java_toolchain(ctx), attr)
    return value

def _jar_and_target_arg_mapper(jar):
    # Emit pretty labels for targets in the main repository.
    label = str(jar.owner)
    if label.startswith("@@//"):
        label = label.lstrip("@")
    return jar.path + "," + label

def _get_feature_config(ctx):
    cc_toolchain = find_cc_toolchain(ctx, mandatory = False)
    if not cc_toolchain:
        return None
    feature_config = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features + ["java_launcher_link", "static_linking_mode"],
        unsupported_features = ctx.disabled_features,
    )
    return feature_config

def _should_strip_as_default(ctx, feature_config):
    fission_is_active = ctx.fragments.cpp.fission_active_for_current_compilation_mode()
    create_per_obj_debug_info = fission_is_active and cc_common.is_enabled(
        feature_name = "per_object_debug_info",
        feature_configuration = feature_config,
    )
    compilation_mode = ctx.var["COMPILATION_MODE"]
    strip_as_default = create_per_obj_debug_info and compilation_mode == "opt"

    return strip_as_default

def _get_coverage_config(ctx, runner):
    toolchain = semantics.find_java_toolchain(ctx)
    if not ctx.configuration.coverage_enabled:
        return None
    runner = runner if ctx.attr.create_executable else None
    manifest = ctx.actions.declare_file("runtime_classpath_for_coverage/%s/runtime_classpath.txt" % ctx.label.name)
    singlejar = toolchain.single_jar
    return struct(
        runner = runner,
        main_class = "com.google.testing.coverage.JacocoCoverageRunner",
        manifest = manifest,
        env = {
            "JAVA_RUNTIME_CLASSPATH_FOR_COVERAGE": manifest.path,
            "SINGLE_JAR_TOOL": singlejar.executable.path,
        },
        support_files = [manifest, singlejar.executable],
    )

def _get_java_executable(ctx, java_runtime_toolchain, launcher):
    java_executable = launcher.short_path if launcher else java_runtime_toolchain.java_executable_runfiles_path
    if not _is_absolute_target_platform_path(ctx, java_executable):
        java_executable = ctx.workspace_name + "/" + java_executable
    return paths.normalize(java_executable)

def _has_target_constraints(ctx, constraints):
    # Constraints is a label_list.
    for constraint in constraints:
        constraint_value = constraint[platform_common.ConstraintValueInfo]
        if ctx.target_platform_has_constraint(constraint_value):
            return True
    return False

def _is_target_platform_windows(ctx):
    return _has_target_constraints(ctx, ctx.attr._windows_constraints)

def _is_absolute_target_platform_path(ctx, path):
    if _is_target_platform_windows(ctx):
        return len(path) > 2 and path[1] == ":"
    return path.startswith("/")

def _runfiles_enabled(ctx):
    return ctx.configuration.runfiles_enabled()

def _get_test_support(ctx):
    if ctx.attr.create_executable and ctx.attr.use_testrunner:
        return ctx.attr._test_support
    return None

def _test_providers(ctx):
    test_providers = []
    if _has_target_constraints(ctx, ctx.attr._apple_constraints):
        test_providers.append(testing.ExecutionInfo({"requires-darwin": ""}))

    test_env = {}
    test_env.update(cc_helper.get_expanded_env(ctx, {}))

    coverage_config = _get_coverage_config(
        ctx,
        runner = None,  # we only need the environment
    )
    if coverage_config:
        test_env.update(coverage_config.env)
    test_providers.append(testing.TestEnvironment(
        environment = test_env,
        inherited_environment = ctx.attr.env_inherit,
    ))

    return test_providers

def _executable_providers(ctx):
    if ctx.attr.create_executable:
        return [RunEnvironmentInfo(cc_helper.get_expanded_env(ctx, {}))]
    return []

def _resource_mapper(file):
    root_relative_path = paths.relativize(
        path = file.path,
        start = paths.join(file.root.path, file.owner.workspace_root),
    )
    return "%s:%s" % (
        file.path,
        semantics.get_default_resource_path(root_relative_path, segment_extractor = _java_segments),
    )

def _create_single_jar(
        actions,
        toolchain,
        output,
        sources = depset(),
        resources = depset(),
        mnemonic = "JavaSingleJar",
        progress_message = "Building singlejar jar %{output}",
        build_target = None,
        output_creator = None):
    """Register singlejar action for the output jar.

    Args:
      actions: (actions) ctx.actions
      toolchain: (JavaToolchainInfo) The java toolchain
      output: (File) Output file of the action.
      sources: (depset[File]) The jar files to merge into the output jar.
      resources: (depset[File]) The files to add to the output jar.
      mnemonic: (str) The action identifier
      progress_message: (str) The action progress message
      build_target: (Label) The target label to stamp in the manifest. Optional.
      output_creator: (str) The name of the tool to stamp in the manifest. Optional,
          defaults to 'singlejar'
    Returns:
      (File) Output file which was used for registering the action.
    """
    args = actions.args()
    args.set_param_file_format("shell").use_param_file("@%s", use_always = True)
    args.add("--output", output)
    args.add_all(
        [
            "--compression",
            "--normalize",
            "--exclude_build_data",
            "--warn_duplicate_resources",
        ],
    )
    args.add_all("--sources", sources)
    args.add_all("--resources", resources, map_each = _resource_mapper)

    args.add("--build_target", build_target)
    args.add("--output_jar_creator", output_creator)

    actions.run(
        mnemonic = mnemonic,
        progress_message = progress_message,
        executable = toolchain.single_jar,
        toolchain = semantics.JAVA_TOOLCHAIN_TYPE,
        inputs = depset(transitive = [resources, sources]),
        tools = [toolchain.single_jar],
        outputs = [output],
        arguments = [args],
    )
    return output

# TODO(hvd): use skylib shell.quote()
def _shell_escape(s):
    """Shell-escape a string

    Quotes a word so that it can be used, without further quoting, as an argument
    (or part of an argument) in a shell command.

    Args:
        s: (str) the string to escape

    Returns:
        (str) the shell-escaped string
    """
    if not s:
        # Empty string is a special case: needs to be quoted to ensure that it
        # gets treated as a separate argument.
        return "''"
    for c in s.elems():
        # We do this positively so as to be sure we don't inadvertently forget
        # any unsafe characters.
        if not c.isalnum() and c not in "@%-_+:,./":
            return "'" + s.replace("'", "'\\''") + "'"
    return s

def _detokenize_javacopts(opts):
    """Detokenizes a list of options to a depset.

    Args:
        opts: ([str]) the javac options to detokenize

    Returns:
        (depset[str]) depset of detokenized options
    """
    return depset(
        [" ".join([_shell_escape(opt) for opt in opts])],
        order = "preorder",
    )

def _derive_output_file(ctx, base_file, *, name_suffix = "", extension = None, extension_suffix = ""):
    """Declares a new file whose name is derived from the given file

    This method allows appending a suffix to the name (before extension), changing
    the extension or appending a suffix after the extension. The new file is declared
    as a sibling of the given base file. At least one of the three options must be
    specified. It is an error to specify both `extension` and `extension_suffix`.

    Args:
        ctx: (RuleContext) the rule context.
        base_file: (File) the file from which to derive the resultant file.
        name_suffix: (str) Optional. The suffix to append to the name before the
        extension.
        extension: (str) Optional. The new extension to use (without '.'). By default,
        the base_file's extension is used.
        extension_suffix: (str) Optional. The suffix to append to the base_file's extension

    Returns:
        (File) the derived file
    """
    if not name_suffix and not extension_suffix and not extension:
        fail("At least one of name_suffix, extension or extension_suffix is required")
    if extension and extension_suffix:
        fail("only one of extension or extension_suffix can be specified")
    if extension == None:
        extension = base_file.extension
    new_basename = paths.replace_extension(base_file.basename, name_suffix + "." + extension + extension_suffix)
    return ctx.actions.declare_file(new_basename, sibling = base_file)

def _is_stamping_enabled(ctx, stamp):
    if ctx.configuration.is_tool_configuration():
        return 0
    if stamp == 1 or stamp == 0:
        return stamp

    # stamp == -1 / auto
    return int(ctx.configuration.stamp_binaries())

def _get_relative(path_a, path_b):
    if paths.is_absolute(path_b):
        return path_b
    return paths.normalize(paths.join(path_a, path_b))

helper = struct(
    collect_all_targets_as_deps = _collect_all_targets_as_deps,
    filter_launcher_for_target = _filter_launcher_for_target,
    launcher_artifact_for_target = _launcher_artifact_for_target,
    check_and_get_main_class = _check_and_get_main_class,
    primary_class = _primary_class,
    strip_extension = _strip_extension,
    concat = _concat,
    get_shared_native_deps_path = _get_shared_native_deps_path,
    check_and_get_one_version_attribute = _check_and_get_one_version_attribute,
    jar_and_target_arg_mapper = _jar_and_target_arg_mapper,
    get_feature_config = _get_feature_config,
    should_strip_as_default = _should_strip_as_default,
    get_coverage_config = _get_coverage_config,
    get_java_executable = _get_java_executable,
    is_absolute_target_platform_path = _is_absolute_target_platform_path,
    is_target_platform_windows = _is_target_platform_windows,
    runfiles_enabled = _runfiles_enabled,
    get_test_support = _get_test_support,
    test_providers = _test_providers,
    executable_providers = _executable_providers,
    create_single_jar = _create_single_jar,
    shell_escape = _shell_escape,
    detokenize_javacopts = _detokenize_javacopts,
    derive_output_file = _derive_output_file,
    is_stamping_enabled = _is_stamping_enabled,
    get_relative = _get_relative,
)
