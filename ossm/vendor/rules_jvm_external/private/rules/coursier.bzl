# Copyright 2019 The Bazel Authors. All rights reserved.
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

load("//:specs.bzl", "utils")
load("//private:artifact_utilities.bzl", "deduplicate_and_sort_artifacts")
load(
    "//private:coursier_utilities.bzl",
    "SUPPORTED_PACKAGING_TYPES",
    "contains_git_conflict_markers",
    "is_maven_local_path",
    "to_repository_name",
)
load("//private:dependency_tree_parser.bzl", "parser")
load("//private:java_utilities.bzl", "build_java_argsfile_content")
load("//private:proxy.bzl", "get_java_proxy_args")
load(
    "//private:versions.bzl",
    "COURSIER_CLI_BAZEL_MIRROR_URL",
    "COURSIER_CLI_GITHUB_ASSET_URL",
    "COURSIER_CLI_SHA256",
)
load("//private/lib:urls.bzl", "remove_auth_from_url")
load("//private/rules:v1_lock_file.bzl", "v1_lock_file")
load("//private/rules:v2_lock_file.bzl", "v2_lock_file")

_BUILD = """
# package(default_visibility = [{visibilities}])  # https://github.com/bazelbuild/bazel/issues/13681

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_license//rules:package_info.bzl", "package_info")
load("@rules_java//java:defs.bzl", "java_binary", "java_library", "java_plugin")
load("@rules_jvm_external//private/rules:pin_dependencies.bzl", "pin_dependencies")
load("@rules_jvm_external//private/rules:jvm_import.bzl", "jvm_import")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
{aar_import_statement}

{imports}

# Required by stardoc if the repo is ever frozen
bzl_library(
   name = "defs",
   srcs = ["defs.bzl"],
   deps = [
       "@rules_jvm_external//:implementation",
   ],
   visibility = ["//visibility:public"],
)
"""

DEFAULT_AAR_IMPORT_LABEL = "@build_bazel_rules_android//android:rules.bzl"

_AAR_IMPORT_STATEMENT = """\
load("%s", "aar_import")
"""

_BUILD_PIN = """
sh_binary(
    name = "pin",
    srcs = ["pin.sh"],
    args = [
        "$(rlocationpath :unsorted_deps.json)",
    ],
    data = [
        ":unsorted_deps.json",
    ],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
    visibility = ["//visibility:public"],
)
"""

_BUILD_OUTDATED = """
sh_binary(
    name = "outdated",
    srcs = ["outdated.sh"],
    data = [
        "@rules_jvm_external//private/tools/prebuilt:outdated_deploy.jar",
        "outdated.artifacts",
        "outdated.boms",
        "outdated.repositories",
    ],
    args = [
        "$(location @rules_jvm_external//private/tools/prebuilt:outdated_deploy.jar)",
        "$(location outdated.artifacts)",
        "$(location outdated.boms)",
        "$(location outdated.repositories)",
    ],
    visibility = ["//visibility:public"],
)
"""

EMPTY_FILE_SHA256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

_IN_REPO_PIN = """
pin_dependencies(
    name = "pin",
    boms = {boms},
    artifacts = {artifacts},
    excluded_artifacts = {excluded_artifacts},
    repositories = {repos},
    fetch_sources = {fetch_sources},
    fetch_javadocs = {fetch_javadocs},
    lock_file = {lock_file},
    jvm_flags = {jvm_flags},
    visibility = ["//visibility:public"],
    resolver = {resolver},
)
"""

_BUILD_PIN_ALIAS = """
# Alias to unpinned to allow pinning
alias(
  name = "pin",
  actual = "{unpinned_pin_target}",
  visibility = ["//visibility:public"],
)
"""

def _is_verbose(repository_ctx):
    return bool(repository_ctx.os.environ.get("RJE_VERBOSE"))

def _is_windows(repository_ctx):
    return repository_ctx.os.name.find("windows") != -1

def _is_linux(repository_ctx):
    return repository_ctx.os.name.find("linux") != -1

def _is_macos(repository_ctx):
    return repository_ctx.os.name.find("mac") != -1

def _is_file(repository_ctx, path):
    return repository_ctx.which("test") and repository_ctx.execute(["test", "-f", path]).return_code == 0

def _is_directory(repository_ctx, path):
    return repository_ctx.which("test") and repository_ctx.execute(["test", "-d", path]).return_code == 0

def _is_unpinned(repository_ctx):
    return repository_ctx.attr.pinned_repo_name != ""

def _shell_quote(s):
    # Lifted from
    #   https://github.com/bazelbuild/bazel-skylib/blob/6a17363a3c27dde70ab5002ad9f2e29aff1e1f4b/lib/shell.bzl#L49
    # because this file cannot load symbols from bazel_skylib since commit
    # 47505f644299aa2483d0df06c2bb2c7aa10d26d4.
    return "'" + s.replace("'", "'\\''") + "'"

def _execute(repository_ctx, cmd, timeout = 600, environment = {}, progress_message = None):
    if progress_message:
        repository_ctx.report_progress(progress_message)

    verbose = _is_verbose(repository_ctx)
    if verbose:
        repository_ctx.execute(
            ["echo", "\n%s" % " ".join([str(c) for c in cmd])],
            quiet = False,
        )

    return repository_ctx.execute(
        cmd,
        timeout = timeout,
        environment = environment,
        quiet = not verbose,
    )

def _execute_with_argsfile(
        repository_ctx,
        tool,
        tool_name,
        progress_message,
        error_description,
        files_to_inspect):
    # Currently each tool can only be used once per repository.
    # This could be avoided by adding a disambiguator to the argsfile name.

    # Avoid argument limits by putting list of files to inspect into a file
    repository_ctx.file(
        "{}_argsfile".format(tool_name),
        "\n".join([str(f) for f in files_to_inspect]) + "\n",
        executable = False,
    )

    command = _generate_java_jar_command(
        repository_ctx,
        repository_ctx.path(tool),
    )

    exec_result = _execute(
        repository_ctx,
        command + ["--argsfile", repository_ctx.path("{}_argsfile".format(tool_name))],
        progress_message = progress_message,
    )
    if exec_result.return_code != 0:
        fail("Error while " + error_description + ": " + exec_result.stderr)

    return exec_result.stdout

# The representation of a Windows path when read from the parsed Coursier JSON
# is delimited by 4 back slashes. Replace them with 1 forward slash.
def _normalize_to_unix_path(path):
    return path.replace("\\", "/")

# Relativize an absolute path to an artifact in coursier's default cache location.
# After relativizing, also symlink the path into the workspace's output base.
# Then return the relative path for further processing
def _relativize_and_symlink_file_in_coursier_cache(repository_ctx, absolute_path, coursier_cache_path):
    # The path manipulation from here on out assumes *nix paths, not Windows.
    # for artifact_absolute_path in artifact_absolute_paths:
    #
    # Also replace '\' with '/` to normalize windows paths to *nix style paths
    # BUILD files accept only *nix paths, so we normalize them here.
    absolute_path_parts = absolute_path.split(coursier_cache_path)
    if len(absolute_path_parts) != 2:
        fail("Error while trying to parse the path of file in the coursier cache: " + absolute_path)
    else:
        relative_path = absolute_path_parts[1]

        # Coursier prefixes private repositories with the username, which obfuscates
        # changes to the pinned json so we remove it from the relative path.
        credential_marker = relative_path.find("%40")
        if credential_marker > -1:
            user_prefix = relative_path[:credential_marker + 3].split("/")[-1]
            relative_path = relative_path.replace(user_prefix, "")

        # Make a symlink from the absolute path of the artifact to the relative
        # path within the output_base/external.
        artifact_relative_path = "v1" + relative_path
        repository_ctx.symlink(absolute_path, repository_ctx.path(artifact_relative_path))
    return artifact_relative_path

# Relativize an absolute path to an artifact in maven local.
# After relativizing, also symlink the path into the workspace's output base.
# Then return the relative path for further processing
def _relativize_and_symlink_file_in_maven_local(repository_ctx, absolute_path):
    absolute_path_parts = absolute_path.split(".m2/repository")
    if len(absolute_path_parts) != 2:
        fail("Error while trying to parse the path of file in maven local: " + absolute_path_parts)
    else:
        # Make a symlink from the absolute path of the artifact to the relative
        # path within the output_base/external.
        artifact_relative_path = "v1" + absolute_path_parts[1]
        repository_ctx.symlink(absolute_path, repository_ctx.path(artifact_relative_path))
    return artifact_relative_path

def _get_aar_import_statement_or_empty_str(repository_ctx):
    if repository_ctx.attr.use_starlark_android_rules:
        # parse the label to validate it
        _ = Label(repository_ctx.attr.aar_import_bzl_label)
        return _AAR_IMPORT_STATEMENT % repository_ctx.attr.aar_import_bzl_label
    else:
        return ""

def _java_path(repository_ctx):
    # Allow setting an env var to keep legacy JAVA_HOME behavior
    use_java_home = repository_ctx.os.environ.get("RJE_COURSIER_USE_JAVA_HOME")

    if use_java_home == None:
        embedded_java = "../bazel_tools/jdk/bin/java"
        if _is_file(repository_ctx, embedded_java):
            return repository_ctx.path(embedded_java)

    java_home = repository_ctx.os.environ.get("JAVA_HOME")
    if java_home != None:
        return repository_ctx.path(java_home + "/bin/java")
    elif repository_ctx.which("java") != None:
        return repository_ctx.which("java")
    return None

# Generate the base `coursier` command depending on the OS, JAVA_HOME or the
# location of `java`.
def _generate_java_jar_command(repository_ctx, jar_path):
    coursier_opts = repository_ctx.os.environ.get("COURSIER_OPTS", "")
    coursier_opts = coursier_opts.split(" ") if len(coursier_opts) > 0 else []
    java_path = _java_path(repository_ctx)

    if java_path != None:
        # https://github.com/coursier/coursier/blob/master/doc/FORMER-README.md#how-can-the-launcher-be-run-on-windows-or-manually-with-the-java-program
        # The -noverify option seems to be required after the proguarding step
        # of the main JAR of coursier.
        cmd = [java_path, "-noverify", "-jar"] + coursier_opts + _get_java_proxy_args(repository_ctx) + [jar_path]
    else:
        # Try to execute coursier directly
        cmd = [jar_path] + coursier_opts + ["-J%s" % arg for arg in _get_java_proxy_args(repository_ctx)]

    return cmd

# Extract the well-known environment variables http_proxy, https_proxy and
# no_proxy and convert them to java.net-compatible property arguments.
def _get_java_proxy_args(repository_ctx):
    # Check both lower- and upper-case versions of the environment variables, preferring the former
    http_proxy = repository_ctx.os.environ.get("http_proxy", repository_ctx.os.environ.get("HTTP_PROXY"))
    https_proxy = repository_ctx.os.environ.get("https_proxy", repository_ctx.os.environ.get("HTTPS_PROXY"))
    no_proxy = repository_ctx.os.environ.get("no_proxy", repository_ctx.os.environ.get("NO_PROXY"))
    return get_java_proxy_args(http_proxy, https_proxy, no_proxy)

def _windows_check(repository_ctx):
    # TODO(jin): Remove BAZEL_SH usage ASAP. Bazel is going bashless, so BAZEL_SH
    # will not be around for long.
    #
    # On Windows, run msys once to bootstrap it
    # https://github.com/bazelbuild/rules_jvm_external/issues/53
    if (_is_windows(repository_ctx)):
        bash = repository_ctx.os.environ.get("BAZEL_SH")
        if (bash == None):
            fail("Please set the BAZEL_SH environment variable to the path of MSYS2 bash. " +
                 "This is typically `c:\\msys64\\usr\\bin\\bash.exe`. For more information, read " +
                 "https://docs.bazel.build/versions/master/install-windows.html#getting-bazel")

def _stable_artifact(artifact):
    parsed = json.decode(artifact)

    # Sort the keys to provide a stable order
    keys = sorted(parsed.keys())
    return ":".join(["%s=%s" % (key, parsed[key]) for key in keys])

# Compute a signature of the list of artifacts that will be used to build
# the dependency tree. This is used as a check to see whether the dependency
# tree needs to be repinned.
# Returns a tuple where the first element is the currently used hash, and the
# second element is a list of hashes in previous formats. This is to allow for
# upgrading rules_jvm_external when the hash inputs change.
#
# Visible for testing
def compute_dependency_inputs_signature(boms = [], artifacts = [], repositories = [], excluded_artifacts = []):
    if len(repositories) == 0:
        fail("Repositories must be set to calculate input signature")

    if len(artifacts) == 0 and len(boms) == 0:
        fail("Cannot calculate input hash without artifacts or boms")

    artifact_inputs = []
    excluded_artifact_inputs = []

    if boms and len(boms):
        for bom in sorted(boms):
            artifact_inputs.append(_stable_artifact(bom))

    for artifact in sorted(artifacts):
        artifact_inputs.append(_stable_artifact(artifact))

    for artifact in sorted(excluded_artifacts):
        excluded_artifact_inputs.append(_stable_artifact(artifact))

    v1_sig = hash(repr(sorted(artifact_inputs))) ^ hash(repr(sorted(repositories)))

    hash_parts = [sorted(artifact_inputs), sorted(repositories), sorted(excluded_artifact_inputs)]
    current_version_sig = 0
    for part in hash_parts:
        current_version_sig ^= hash(repr(part))

    return (current_version_sig, [v1_sig])

def get_netrc_lines_from_entries(netrc_entries):
    netrc_lines = []
    for machine, login_dict in sorted(netrc_entries.items()):
        for login, password in sorted(login_dict.items()):
            netrc_lines.append("machine {}".format(machine))
            netrc_lines.append("login {}".format(login))
            if password:
                netrc_lines.append("password {}".format(password))
    return netrc_lines

def get_home_netrc_contents(repository_ctx):
    if repository_ctx.os.name.startswith("windows"):
        home_dir = repository_ctx.os.environ.get("USERPROFILE", "")
    else:
        home_dir = repository_ctx.os.environ.get("HOME", "")

    if not home_dir:
        return ""

    netrcfile = "{}/.netrc".format(home_dir)
    if not repository_ctx.path(netrcfile).exists:
        return ""

    return repository_ctx.read(netrcfile)

def _add_outdated_files(repository_ctx, artifacts, boms, repositories):
    repository_ctx.file(
        "outdated.artifacts",
        "\n".join(["{}:{}:{}".format(artifact["group"], artifact["artifact"], artifact["version"]) for artifact in artifacts]) + "\n",
        executable = False,
    )

    repository_ctx.file(
        "outdated.boms",
        "\n".join(["{}:{}:{}".format(bom["group"], bom["artifact"], bom["version"]) for bom in boms]) + "\n",
        executable = False,
    )

    repository_ctx.file(
        "outdated.repositories",
        "\n".join([repo["repo_url"] for repo in repositories]) + "\n",
        executable = False,
    )

    repository_ctx.template(
        "outdated.sh",
        repository_ctx.attr._outdated,
        {
            "{repository_name}": repository_ctx.name,
            "{proxy_opts}": " ".join([_shell_quote(arg) for arg in _get_java_proxy_args(repository_ctx)]),
        },
        executable = True,
    )

def is_repin_required(repository_ctx):
    env_var_names = repository_ctx.os.environ.keys()
    return "RULES_JVM_EXTERNAL_REPIN" not in env_var_names and "REPIN" not in env_var_names

def _get_fail_if_repin_required(repository_ctx):
    if not repository_ctx.attr.fail_if_repin_required:
        return False

    return is_repin_required(repository_ctx)

def print_if_not_repinning(repository_ctx, *args):
    if is_repin_required(repository_ctx):
        return
    print(*args)

def _pinned_coursier_fetch_impl(repository_ctx):
    if not repository_ctx.attr.maven_install_json:
        fail("Please specify the file label to maven_install.json (e.g." +
             "//:maven_install.json).")

    _windows_check(repository_ctx)

    repositories = [json.decode(repository) for repository in repository_ctx.attr.repositories]

    artifacts = [json.decode(artifact) for artifact in repository_ctx.attr.artifacts]
    _check_artifacts_are_unique(artifacts, repository_ctx.attr.duplicate_version_warning)

    boms = [json.decode(bom) for bom in repository_ctx.attr.boms]
    _check_artifacts_are_unique(boms, repository_ctx.attr.duplicate_version_warning)

    # Read Coursier state from maven_install.json.
    repository_ctx.symlink(
        repository_ctx.path(repository_ctx.attr.maven_install_json),
        repository_ctx.path("imported_maven_install.json"),
    )
    lock_file_content = repository_ctx.read(repository_ctx.attr.maven_install_json)
    if not len(lock_file_content) or contains_git_conflict_markers(repository_ctx.attr.maven_install_json, lock_file_content):
        maven_install_json_content = {
            "artifacts": {},
            "dependencies": {},
            "repositories": {},
            "version": "2",
        }
    else:
        maven_install_json_content = json.decode(lock_file_content)

    if v1_lock_file.is_valid_lock_file(maven_install_json_content):
        importer = v1_lock_file
        print_if_not_repinning(
            repository_ctx,
            "Lock file should be updated. Please run `REPIN=1 bazel run @unpinned_%s//:pin`" % repository_ctx.name,
        )
    elif v2_lock_file.is_valid_lock_file(maven_install_json_content):
        importer = v2_lock_file
    else:
        fail("Unable to read lock file: %s" % repository_ctx.attr.maven_install_json)

    # Validation steps for maven_install.json.

    # Validate that there's a dependency_tree element in the parsed JSON.
    if not importer.is_valid_lock_file(maven_install_json_content):
        fail("Failed to parse %s. " % repository_ctx.path(repository_ctx.attr.maven_install_json) +
             "It is not a valid maven_install.json file. Has this " +
             "file been modified manually?")

    input_artifacts_hash = importer.get_input_artifacts_hash(maven_install_json_content)

    # With Bzlmod, repository_ctx.name is the mangled ("canonical") name of the repository, so we
    # use the user_provided_name attribute to get the original name (always set by maven_install).
    user_provided_name = repository_ctx.attr.user_provided_name or repository_ctx.name

    if user_provided_name == repository_ctx.name:
        unpinned_repo = "unpinned_" + repository_ctx.name
    else:
        # Generate a canonical label pointing to the pin target so that users don't have to use_repo
        # the unpinned_ repo with Bzlmod.
        unpinned_repo = "@{}unpinned_{}".format(
            repository_ctx.name[:-len(user_provided_name)],
            user_provided_name,
        )

    # pin_target will alias to unpinned_pin_target later on, so we need both.
    unpinned_pin_target = "@{}//:pin".format(unpinned_repo)
    pin_target = "@{}//:pin".format(user_provided_name)

    user_provided_repin_instructions = repository_ctx.attr.repin_instructions
    repin_instructions = user_provided_repin_instructions if user_provided_repin_instructions else (
        " REPIN=1 bazel run %s\n" % pin_target
    )

    # Then, check to see if we need to repin our deps because inputs have changed
    if input_artifacts_hash == None:
        print_if_not_repinning(
            repository_ctx,
            "NOTE: %s_install.json does not contain a signature of the required artifacts. " % user_provided_name +
            "This feature ensures that the build does not use stale dependencies when the inputs " +
            "have changed. To generate this signature, run 'bazel run %s'." % pin_target,
        )
    else:
        computed_artifacts_hash, old_hashes = compute_dependency_inputs_signature(
            boms = repository_ctx.attr.boms,
            artifacts = repository_ctx.attr.artifacts,
            repositories = repository_ctx.attr.repositories,
            excluded_artifacts = repository_ctx.attr.excluded_artifacts,
        )
        if input_artifacts_hash in old_hashes:
            print_if_not_repinning(
                repository_ctx,
                "WARNING: %s_install.json contains an outdated input signature. " % (user_provided_name) +
                "It is recommended that you regenerate it by running either:\n" + repin_instructions,
            )
        elif computed_artifacts_hash != input_artifacts_hash:
            if _get_fail_if_repin_required(repository_ctx):
                fail("%s_install.json contains an invalid input signature (expected %s and got %s) and must be regenerated. " % (
                         user_provided_name,
                         input_artifacts_hash,
                         computed_artifacts_hash,
                     ) +
                     "This typically happens when the maven_install artifacts have been changed but not repinned. " +
                     "PLEASE DO NOT MODIFY THIS FILE DIRECTLY! To generate a new " +
                     "%s_install.json and re-pin the artifacts, please run:\n" % user_provided_name +
                     repin_instructions)
            else:
                print_if_not_repinning(
                    repository_ctx,
                    "The inputs to %s_install.json have changed, but the lock file has not been regenerated. " % user_provided_name +
                    "Consider running 'bazel run %s'" % pin_target,
                )

    dep_tree_signature = importer.get_lock_file_hash(maven_install_json_content)

    if dep_tree_signature == None:
        print_if_not_repinning(
            repository_ctx,
            "NOTE: %s_install.json does not contain a signature entry of the dependency tree. " % user_provided_name +
            "This feature ensures that the file is not modified manually. To generate this " +
            "signature, run 'bazel run %s'." % pin_target,
        )
    elif importer.compute_lock_file_hash(maven_install_json_content) != dep_tree_signature:
        # Then, validate that the signature provided matches the contents of the dependency_tree.
        # This is to stop users from manually modifying maven_install.json.
        if _get_fail_if_repin_required(repository_ctx):
            fail(
                "%s_install.json contains an invalid signature (expected %s and got %s) and may be corrupted. " % (
                    user_provided_name,
                    dep_tree_signature,
                    importer.compute_lock_file_hash(maven_install_json_content),
                ) +
                "PLEASE DO NOT MODIFY THIS FILE DIRECTLY! To generate a new " +
                "%s_install.json and re-pin the artifacts, follow these steps: \n\n" % user_provided_name +
                repin_instructions,
            )
        else:
            print_if_not_repinning(
                repository_ctx,
                "NOTE: %s_install.json does not contain an up to date hash of its contents. " % user_provided_name +
                "This feature ensures that pinned dependencies are up to date. To generate this " +
                "signature, run 'bazel run %s'." % pin_target,
            )

    # Create the list of http_file repositories for each of the artifacts
    # in maven_install.json. This will be loaded additionally like so:
    #
    # load("@maven//:defs.bzl", "pinned_maven_install")
    # pinned_maven_install()
    http_files = [
        "load(\"@bazel_tools//tools/build_defs/repo:http.bzl\", \"http_file\")",
        "load(\"@bazel_tools//tools/build_defs/repo:utils.bzl\", \"maybe\")",
        "def pinned_maven_install():",
        "    pass",  # Keep it syntactically correct in case of empty dependencies.
    ]
    maven_artifacts = []
    netrc_entries = importer.get_netrc_entries(maven_install_json_content)

    for artifact in importer.get_artifacts(maven_install_json_content):
        http_file_repository_name = to_repository_name(artifact["coordinates"])
        if artifact.get("file"):
            maven_artifacts.extend([artifact["coordinates"]])
            http_files.extend([
                "    http_file(",
                "        name = \"%s\"," % http_file_repository_name,
                "        sha256 = \"%s\"," % artifact["sha256"],
                # repository_ctx should point to external/$repository_ctx.name
                # The http_file should point to external/$http_file_repository_name
                # File-path is relative defined from http_file traveling to repository_ctx.
                "        netrc = \"../%s/netrc\"," % (repository_ctx.name),
            ])
            if len(artifact["urls"]) == 0 and importer.has_m2local(maven_install_json_content) and artifact.get("file") != None:
                if _is_windows(repository_ctx):
                    user_home = repository_ctx.os.environ.get("USERPROFILE").replace("\\", "/")
                else:
                    user_home = repository_ctx.os.environ.get("HOME")
                m2local_urls = [
                    "file://%s/.m2/repository/%s" % (user_home, artifact["file"]),
                ]
            else:
                m2local_urls = []
            http_files.append("        urls = %s," % repr(
                [remove_auth_from_url(url) for url in artifact["urls"] + m2local_urls],
            ))

            # https://github.com/bazelbuild/rules_jvm_external/issues/1028
            # http_rule does not like directories named "build" so prepend v1 to the path.
            download_path = "v1/%s" % artifact["file"] if artifact["file"] else artifact["file"]
            http_files.append("        downloaded_file_path = \"%s\"," % download_path)
            http_files.append("    )")
        elif _is_verbose(repository_ctx):
            print("No file downloaded for %s, skipping http_file definition" % http_file_repository_name)

    http_files.extend(["maven_artifacts = [\n%s\n]" % (",\n".join(["    \"%s\"" % artifact for artifact in maven_artifacts]))])

    repository_ctx.file("defs.bzl", "\n".join(http_files), executable = False)
    repository_ctx.file(
        "netrc",
        "\n".join(
            repository_ctx.attr.additional_netrc_lines +
            get_home_netrc_contents(repository_ctx).splitlines() +
            get_netrc_lines_from_entries(netrc_entries),
        ),
        executable = False,
    )

    repository_ctx.report_progress("Generating BUILD targets..")
    (generated_imports, jar_versionless_target_labels) = parser.generate_imports(
        repository_ctx = repository_ctx,
        dependencies = importer.get_artifacts(maven_install_json_content),
        explicit_artifacts = {
            a["group"] + ":" + a["artifact"] + (":" + a["classifier"] if "classifier" in a else ""): True
            for a in artifacts
        },
        neverlink_artifacts = {
            a["group"] + ":" + a["artifact"] + (":" + a["classifier"] if "classifier" in a else ""): True
            for a in artifacts
            if a.get("neverlink", False)
        },
        testonly_artifacts = {
            a["group"] + ":" + a["artifact"] + (":" + a["classifier"] if "classifier" in a else ""): True
            for a in artifacts
            if a.get("testonly", False)
        },
        override_targets = repository_ctx.attr.override_targets,
        skip_maven_local_dependencies = False,
    )

    repository_ctx.template(
        "compat_repository.bzl",
        repository_ctx.attr._compat_repository,
        substitutions = {},
        executable = False,
    )

    pin_target = generate_pin_target(repository_ctx, unpinned_pin_target)

    repository_ctx.file(
        "BUILD",
        (_BUILD + _BUILD_OUTDATED).format(
            visibilities = ",".join(["\"%s\"" % s for s in (["//visibility:public"] if not repository_ctx.attr.strict_visibility else repository_ctx.attr.strict_visibility_value)]),
            repository_name = repository_ctx.name,
            imports = generated_imports,
            aar_import_statement = _get_aar_import_statement_or_empty_str(repository_ctx),
            unpinned_pin_target = unpinned_pin_target,
        ) + pin_target,
        executable = False,
    )

    _add_outdated_files(repository_ctx, artifacts, boms, repositories)

    # Generate a compatibility layer of external repositories for all jar artifacts.
    if repository_ctx.attr.generate_compat_repositories:
        compat_repositories_bzl = ["load(\"@%s//:compat_repository.bzl\", \"compat_repository\")" % repository_ctx.name]
        compat_repositories_bzl.append("def compat_repositories():")
        for versionless_target_label in jar_versionless_target_labels:
            compat_repositories_bzl.extend([
                "    compat_repository(",
                "        name = \"%s\"," % versionless_target_label,
                "        generating_repository = \"%s\"," % repository_ctx.name,
                "    )",
            ])
            repository_ctx.file(
                "compat.bzl",
                "\n".join(compat_repositories_bzl) + "\n",
                executable = False,
            )

def generate_pin_target(repository_ctx, unpinned_pin_target):
    if repository_ctx.attr.resolver == "coursier":
        return _BUILD_PIN_ALIAS.format(unpinned_pin_target = unpinned_pin_target)
    else:
        package_path = repository_ctx.attr.maven_install_json.package
        file_name = repository_ctx.attr.maven_install_json.name
        if package_path == "":
            lock_file_location = file_name  # e.g. some.json
        else:
            lock_file_location = "/".join([package_path, file_name])  # e.g. path/to/some.json

        return _IN_REPO_PIN.format(
            boms = repr(repository_ctx.attr.boms),
            artifacts = repr(repository_ctx.attr.artifacts),
            excluded_artifacts = repr(repository_ctx.attr.excluded_artifacts),
            jvm_flags = repr(repository_ctx.os.environ.get("JDK_JAVA_OPTIONS")),
            repos = repr(repository_ctx.attr.repositories),
            fetch_sources = repr(repository_ctx.attr.fetch_sources),
            fetch_javadocs = repr(repository_ctx.attr.fetch_javadoc),
            lock_file = repr(lock_file_location),
            resolver = repr(repository_ctx.attr.resolver),
        )

def infer_artifact_path_from_primary_and_repos(primary_url, repository_urls):
    """Returns the artifact path inferred by comparing primary_url with urls in repository_urls.

    When given a list of repository urls and a primary url that has a repository url as a prefix and a maven artifact
    path as a suffix, this method will try to determine what the maven artifact path is and return it.

    This method has some handling for basic http-based auth parsing and will do a url comparison with the user:pass@
    portion stripped.

    Ex.
    infer_artifact_path_from_primary_and_repos(
        "http://a:b@c/group/path/to/artifact/file.jar",
        ["http://c"])
    == "group/path/to/artifact/file.jar"

    Returns:
        String of the artifact path used by maven to find a particular artifact. Does not have a leading slash (`/`).
    """
    userless_repository_urls = [remove_auth_from_url(r.rstrip("/")) for r in repository_urls]
    userless_primary_url = remove_auth_from_url(primary_url)
    primary_artifact_path = None
    for url in userless_repository_urls:
        if userless_primary_url.find(url) != -1:
            primary_artifact_path = userless_primary_url[len(url) + 1:]
            break
    return primary_artifact_path

def _check_artifacts_are_unique(artifacts, duplicate_version_warning):
    if duplicate_version_warning == "none":
        return
    seen_artifacts = {}
    duplicate_artifacts = {}
    for artifact in artifacts:
        artifact_coordinate = artifact["group"] + ":" + artifact["artifact"] + (":%s" % artifact["classifier"] if artifact.get("classifier") != None else "")
        if artifact_coordinate in seen_artifacts:
            # Don't warn if the same version is in the list multiple times
            if seen_artifacts[artifact_coordinate] != artifact["version"]:
                if artifact_coordinate in duplicate_artifacts:
                    duplicate_artifacts[artifact_coordinate].append(artifact["version"])
                else:
                    duplicate_artifacts[artifact_coordinate] = [artifact["version"]]
        else:
            seen_artifacts[artifact_coordinate] = artifact["version"]

    if duplicate_artifacts:
        msg_parts = ["Found duplicate artifact versions"]
        for duplicate in duplicate_artifacts:
            msg_parts.append("    {} has multiple versions {}".format(duplicate, ", ".join([seen_artifacts[duplicate]] + duplicate_artifacts[duplicate])))
        msg_parts.append("Please remove duplicate artifacts from the artifact list so you do not get unexpected artifact versions")
        if duplicate_version_warning == "error":
            fail("\n".join(msg_parts))
        else:
            print("\n".join(msg_parts))

# Get the path to the cache directory containing Coursier-downloaded artifacts.
#
# This method is public for testing.
def get_coursier_cache_or_default(repository_ctx, use_unsafe_shared_cache):
    # If we're not using the unsafe shared cache use 'external/<this repo>/v1/'.
    # 'v1' is the current version of the Coursier cache.
    if not use_unsafe_shared_cache:
        return "v1"

    os_env = repository_ctx.os.environ
    coursier_cache_env_var = os_env.get("COURSIER_CACHE")
    if coursier_cache_env_var:
        # This is an absolute path.
        return coursier_cache_env_var

    # cache locations from https://get-coursier.io/docs/2.0.0-RC5-3/cache.html#default-location
    # Use linux as the default cache directory
    default_cache_dir = "%s/.cache/coursier/v1" % os_env.get("HOME")
    if _is_windows(repository_ctx):
        default_cache_dir = "%s/Coursier/cache/v1" % os_env.get("LOCALAPPDATA").replace("\\", "/")
    elif _is_macos(repository_ctx):
        default_cache_dir = "%s/Library/Caches/Coursier/v1" % os_env.get("HOME")
    else:
        # Coursier respects $XDG_CACHE_HOME as a replacement for $HOME/.cache
        # outside of Windows and macOS.
        #
        # - https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
        # - https://github.com/dirs-dev/directories-jvm/tree/006ca7ff804ca48f692d59a7fce8599f8a1eadfc#projectdirectories
        # - https://github.com/coursier/coursier/blob/d5ad55d1dcb025084ba9bd994ea47ceae0608a8f/modules/paths/src/main/java/coursier/paths/CoursierPaths.java#L44-L59
        xdg_cache_home = os_env.get("XDG_CACHE_HOME")
        if xdg_cache_home:
            return "%s/coursier/v1" % xdg_cache_home

    # Logic based on # https://github.com/coursier/coursier/blob/f48c1c6b01ac5b720e66e06cf93587b21d030e8c/modules/paths/src/main/java/coursier/paths/CoursierPaths.java#L60
    if _is_directory(repository_ctx, default_cache_dir):
        return default_cache_dir
    elif _is_directory(repository_ctx, "%s/.coursier" % os_env.get("HOME")):
        return "%s/.coursier/cache/v1" % os_env.get("HOME")

    return default_cache_dir

def make_coursier_dep_tree(
        repository_ctx,
        artifacts,
        boms,
        excluded_artifacts,
        repositories,
        version_conflict_policy,
        fail_on_missing_checksum,
        fetch_sources,
        fetch_javadoc,
        timeout,
        additional_coursier_options,
        report_progress_prefix = ""):
    if not repositories:
        fail("repositories cannot be empty")

    # Set up artifact exclusion, if any. From coursier fetch --help:
    #
    # Path to the local exclusion file. Syntax: <org:name>--<org:name>. `--` means minus. Example file content:
    # com.twitter.penguin:korean-text--com.twitter:util-tunable-internal_2.11
    # org.apache.commons:commons-math--com.twitter.search:core-query-nodes
    # Behavior: If root module A excludes module X, but root module B requires X, module X will still be fetched.
    artifact_coordinates = []
    exclusion_lines = []
    forced_versions = []
    for a in artifacts:
        coordinates = utils.artifact_coordinate(a)

        # Special-case handling of specific versions that are requested. Without this, coursier will claim that
        # if a lower version of a dependency is requested by a transitive dep than what is specified here, a
        # version mismatch will occur, and this will fail the resolution.
        if a.get("force_version", False):
            forced_versions.append(coordinates)

        artifact_coordinates.append(coordinates)
        if "exclusions" in a:
            for e in a["exclusions"]:
                exclusion_lines.append(":".join([a["group"], a["artifact"]]) +
                                       "--" +
                                       ":".join([e["group"], e["artifact"]]))

    cmd = _generate_java_jar_command(repository_ctx, repository_ctx.path("coursier"))
    cmd.extend(["fetch"])

    cmd.extend(artifact_coordinates)
    if version_conflict_policy == "pinned":
        for coord in artifact_coordinates:
            # check if the artifact has version set
            version = coord.split(",")[0].split(":")[2]

            if version:
                # Undo any `,classifier=` and/or `,type=` suffix from `utils.artifact_coordinate`.
                cmd.extend([
                    "--force-version",
                    ",".join([c for c in coord.split(",") if not c.startswith("classifier=") and not c.startswith("type=")]),
                ])
    else:
        for coord in forced_versions:
            cmd.extend([
                "--force-version",
                ",".join([c for c in coord.split(",") if not c.startswith("classifier=") and not c.startswith("type=")]),
            ])
    cmd.extend(["--artifact-type", ",".join(SUPPORTED_PACKAGING_TYPES + ["src", "doc"])])
    cmd.append("--verbose" if _is_verbose(repository_ctx) else "--quiet")
    cmd.append("--no-default")
    cmd.extend(["--json-output-file", "dep-tree.json"])

    for bom in boms:
        cmd.extend(["--bom", utils.artifact_coordinate(bom)])

    if fail_on_missing_checksum:
        cmd.extend(["--checksum", "SHA-1,MD5"])
    else:
        cmd.extend(["--checksum", "SHA-1,MD5,None"])

    if len(exclusion_lines) > 0:
        repository_ctx.file("exclusion-file.txt", "\n".join(exclusion_lines), False)
        cmd.extend(["--local-exclude-file", "exclusion-file.txt"])
    for repository in repositories:
        cmd.extend(["--repository", repository["repo_url"]])
        if "credentials" in repository:
            cmd.extend(["--credentials", utils.repo_credentials(repository)])
    if repository_ctx.attr.use_credentials_from_home_netrc_file:
        for credential in utils.netrc_credentials(get_home_netrc_contents(repository_ctx)):
            cmd.extend(["--credentials", credential])
    for a in excluded_artifacts:
        cmd.extend(["--exclude", ":".join([a["group"], a["artifact"]])])

    if fetch_sources or fetch_javadoc:
        if fetch_sources:
            cmd.append("--sources")
        if fetch_javadoc:
            cmd.append("--javadoc")
        cmd.append("--default=true")

    environment = {}
    if not _is_unpinned(repository_ctx):
        coursier_cache_location = get_coursier_cache_or_default(
            repository_ctx,
            False,
        )
        cmd.extend(["--cache", coursier_cache_location])  # Download into $output_base/external/$maven_repo_name/v1

        # If not using the shared cache and the user did not specify a COURSIER_CACHE, set the default
        # value to prevent Coursier from writing into home directories.
        # https://github.com/bazelbuild/rules_jvm_external/issues/301
        # https://github.com/coursier/coursier/blob/1cbbf39b88ee88944a8d892789680cdb15be4714/modules/paths/src/main/java/coursier/paths/CoursierPaths.java#L29-L56
        environment = {"COURSIER_CACHE": str(repository_ctx.path(coursier_cache_location))}

    cmd.extend(additional_coursier_options)

    # Use an argsfile to avoid command line length limits, requires Java version > 8
    java_cmd = cmd[0]
    java_args = cmd[1:]

    argsfile_content = build_java_argsfile_content(java_args)
    if _is_verbose(repository_ctx):
        repository_ctx.execute(
            ["echo", "\nargsfile_content:\n%s" % argsfile_content],
            quiet = False,
        )

    repository_ctx.file(
        "java_argsfile",
        argsfile_content,
        executable = False,
    )
    cmd = [java_cmd, "@{}".format(repository_ctx.path("java_argsfile"))]

    exec_result = _execute(
        repository_ctx,
        cmd,
        timeout = timeout,
        environment = environment,
        progress_message = "%sResolving and fetching the transitive closure of %s artifact(s).." % (
            report_progress_prefix,
            len(artifact_coordinates),
        ),
    )
    if (exec_result.return_code != 0):
        fail("Error while fetching artifact with coursier: " + exec_result.stderr)

    dep_tree = deduplicate_and_sort_artifacts(
        json.decode(repository_ctx.read(repository_ctx.path("dep-tree.json"))),
        artifacts,
        excluded_artifacts,
        _is_verbose(repository_ctx),
    )
    return rewrite_files_attribute_if_necessary(repository_ctx, dep_tree)

def rewrite_files_attribute_if_necessary(repository_ctx, dep_tree):
    # There are cases where `coursier` will download both the pom and the
    # jar but will include the path to the pom instead of the jar in the
    # `file` attribute. This differs from both gradle and maven. Massage the
    # `file` attributes if necessary.
    # https://github.com/bazelbuild/rules_jvm_external/issues/1250
    amended_deps = []
    for dep in dep_tree["dependencies"]:
        if not dep.get("file", None):
            amended_deps.append(dep)
            continue

        # You'd think we could use skylib here to do the heavy lifting, but
        # this is a dependency of `maven_install`, which is loaded in the
        # `repositories.bzl` file. That means we can't rely on anything that
        # comes from skylib yet, since the repo isn't loaded. If we could
        # call `maven_install` from `setup.bzl`, we'd be fine, but we can't
        # do that because then there'd be nowhere to call the
        # `pinned_maven_install`. Oh well, let's just do this the manual way.
        if dep["file"].endswith(".pom"):
            jar_path = dep["file"].removesuffix(".pom") + ".jar"

            # The same artifact can being depended on via pom and jar at different
            # places in the tree. In such case, we deduplicate it so that 2
            # entries do not reference the same file, which will otherwise lead
            # in symlink error because of existing file down the road.
            if is_dep(jar_path, amended_deps):
                continue
            if repository_ctx.path(jar_path).exists:
                dep["file"] = jar_path

        amended_deps.append(dep)

    dep_tree["dependencies"] = amended_deps

    return dep_tree

def is_dep(jar_path, deps):
    for dep in deps:
        if jar_path == dep.get("file", None):
            return True
    return False

def remove_prefix(s, prefix):
    if s.startswith(prefix):
        return s[len(prefix):]
    return s

def _coursier_fetch_impl(repository_ctx):
    # Not using maven_install.json, so we resolve and fetch from scratch.
    # This takes significantly longer as it doesn't rely on any local
    # caches and uses Coursier's own download mechanisms.

    # Download Coursier's standalone (deploy) jar from Maven repositories.
    coursier_download_urls = [
        COURSIER_CLI_GITHUB_ASSET_URL,
        COURSIER_CLI_BAZEL_MIRROR_URL,
    ]

    coursier_url_from_env = repository_ctx.os.environ.get("COURSIER_URL")
    if coursier_url_from_env != None:
        coursier_download_urls.insert(0, coursier_url_from_env)

    repository_ctx.download(coursier_download_urls, "coursier", sha256 = COURSIER_CLI_SHA256, executable = True)

    # Try running coursier once
    cmd = _generate_java_jar_command(repository_ctx, repository_ctx.path("coursier"))

    # Add --help because calling the default coursier command on Windows will
    # hang waiting for input
    cmd.append("--help")
    hasher_exec_result = _execute(
        repository_ctx,
        cmd,
    )
    if hasher_exec_result.return_code != 0:
        fail("Unable to run coursier: " + hasher_exec_result.stderr)

    _windows_check(repository_ctx)

    # Deserialize the spec blobs
    repositories = []
    for repository in repository_ctx.attr.repositories:
        repositories.append(json.decode(repository))

    artifacts = []
    for artifact in repository_ctx.attr.artifacts:
        artifacts.append(json.decode(artifact))

    _check_artifacts_are_unique(artifacts, repository_ctx.attr.duplicate_version_warning)

    boms = [json.decode(bom) for bom in repository_ctx.attr.boms]
    _check_artifacts_are_unique(boms, repository_ctx.attr.duplicate_version_warning)

    excluded_artifacts = []
    for artifact in repository_ctx.attr.excluded_artifacts:
        excluded_artifacts.append(json.decode(artifact))

    # Once coursier finishes a fetch, it generates a tree of artifacts and their
    # transitive dependencies in a JSON file. We use that as the source of truth
    # to generate the repository's BUILD file.
    #
    # Coursier generates duplicate artifacts sometimes. Deduplicate them using
    # the file name value as the key.
    dep_tree = make_coursier_dep_tree(
        repository_ctx,
        artifacts,
        boms,
        excluded_artifacts,
        repositories,
        repository_ctx.attr.version_conflict_policy,
        repository_ctx.attr.fail_on_missing_checksum,
        repository_ctx.attr.fetch_sources,
        repository_ctx.attr.fetch_javadoc,
        repository_ctx.attr.resolve_timeout,
        repository_ctx.attr.additional_coursier_options,
    )

    files_to_inspect = []

    # Also, replace '//' with '/', otherwise parsing of the file path for the
    # coursier cache will fail if variables like HOME or COURSIER_CACHE have a
    # trailing slash.
    #
    # We assume that coursier uses the default cache location
    # TODO(jin): allow custom cache locations
    coursier_cache_path = get_coursier_cache_or_default(
        repository_ctx,
        _is_unpinned(repository_ctx),
    ).replace("//", "/")

    for artifact in dep_tree["dependencies"]:
        # Some artifacts don't contain files; they are just parent artifacts
        # to other artifacts.
        if artifact["file"] == None:
            continue

        coord_split = artifact["coord"].split(":")
        coord_unversioned = "{}:{}".format(coord_split[0], coord_split[1])

        # Normalize paths in place here.
        artifact.update({"file": _normalize_to_unix_path(artifact["file"])})

        if is_maven_local_path(artifact["file"]):
            # This file comes from maven local, so handle it in two different ways depending if
            # dependency pinning is used:
            # a) If the repository is unpinned, we keep the file as is, but clear the url to skip it
            # b) Otherwise, we clear the url and also symlink the file from the maven local directory
            #    to file within the repository rule workspace
            print("Assuming maven local for artifact: %s" % artifact["coord"])
            artifact.update({"url": None})
            if not _is_unpinned(repository_ctx):
                artifact.update({"file": _relativize_and_symlink_file_in_maven_local(repository_ctx, artifact["file"])})

            files_to_inspect.append(repository_ctx.path(artifact["file"]))
            continue

        if _is_unpinned(repository_ctx):
            artifact.update({"file": _relativize_and_symlink_file_in_coursier_cache(repository_ctx, artifact["file"], coursier_cache_path)})

        # Coursier saves the artifacts into a subdirectory structure
        # that mirrors the URL where the artifact's fetched from. Using
        # this, we can reconstruct the original URL.
        primary_url_parts = []

        # _normalize_to_unix_path uses 2 backslashes, but the paths themselves have a single backslash at this point
        filepath_parts = _normalize_to_unix_path(artifact["file"]).split("/")
        protocol = None

        # Only support http/https transports (or maven local repository)
        for part in filepath_parts:
            if part == "http" or part == "https":
                protocol = part
                break
        if protocol == None:
            fail("Only artifacts downloaded over http(s) are supported: %s - %s (analyzed %s)" % (artifact["coord"], artifact["file"], filepath_parts))
        primary_url_parts.extend([protocol, "://"])
        for part in filepath_parts[filepath_parts.index(protocol) + 1:]:
            primary_url_parts.extend([part, "/"])
        primary_url_parts.pop()  # pop the final "/"

        # Coursier encodes:
        # - ':' as '%3A'
        # - '@' as '%40'
        #
        # The primary_url is the url from which Coursier downloaded the jar from. It looks like this:
        # https://repo1.maven.org/maven2/org/threeten/threetenbp/1.3.3/threetenbp-1.3.3.jar
        primary_url = "".join(primary_url_parts).replace("%3A", ":").replace("%40", "@")

        # Coursier prepends the username from the provided credentials if needed to authenticate
        # with the repository. We remove it from the url and file attributes if only the username is present
        # and no password, as it has no function and obfuscates changes to the pinned json
        credential_marker = primary_url.find("@")
        if credential_marker > -1:
            potential_credentials = remove_prefix(primary_url[:credential_marker + 1], protocol + "://")
            if len(potential_credentials.split(":")) == 1:
                primary_url = primary_url.replace(potential_credentials, "")

        artifact.update({"url": primary_url})

        # The repository for the primary_url has to be one of the repositories provided through
        # maven_install. Since Maven artifact URLs are standardized, we can make the `http_file`
        # targets more robust by replicating the primary url for each specified repository url.
        #
        # It does not matter if the artifact is on a repository or not, since http_file takes
        # care of 404s.
        #
        # If the artifact does exist, Bazel's HttpConnectorMultiplexer enforces the SHA-256 checksum
        # is correct. By applying the SHA-256 checksum verification across all the mirrored files,
        # we get increased robustness in the case where our primary artifact has been tampered with,
        # and we somehow ended up using the tampered checksum. Attackers would need to tamper *all*
        # mirrored artifacts.
        #
        # See https://github.com/bazelbuild/bazel/blob/77497817b011f298b7f3a1138b08ba6a962b24b8/src/main/java/com/google/devtools/build/lib/bazel/repository/downloader/HttpConnectorMultiplexer.java#L103
        # for more information on how Bazel's HTTP multiplexing works.
        #
        # TODO(https://github.com/bazelbuild/rules_jvm_external/issues/186): Make this work with
        # basic auth.
        repository_urls = []
        for r in repositories:
            # filter out m2Local since it's not a valid mirror url
            if r["repo_url"] != "m2Local":
                repository_urls.append(r["repo_url"].rstrip("/"))
        primary_artifact_path = infer_artifact_path_from_primary_and_repos(primary_url, repository_urls)

        mirror_urls = [url + "/" + primary_artifact_path for url in repository_urls]
        if primary_url in mirror_urls:
            # http_file tries URLs in order, so putting the URL that actually worked first
            # minimizes repository fetch 404s. See: https://github.com/bazelbuild/rules_jvm_external/issues/349
            mirror_urls = [primary_url] + [url for url in mirror_urls if url != primary_url]
        artifact.update({"mirror_urls": mirror_urls})

        files_to_inspect.append(repository_ctx.path(artifact["file"]))

    hasher_stdout = _execute_with_argsfile(
        repository_ctx,
        repository_ctx.attr._sha256_hasher,
        "hasher",
        "Calculating sha256 checksums..",
        "obtaining the sha256 checksums",
        files_to_inspect,
    )

    shas = {}
    for line in hasher_stdout.splitlines():
        parts = line.split(" ")
        path = str(repository_ctx.path(parts[1]))
        shas[path] = parts[0]

    index_jars_stdout = _execute_with_argsfile(
        repository_ctx,
        repository_ctx.attr._index_jar,
        "jar_indexer",
        "Indexing jars",
        "indexing jars",
        files_to_inspect,
    )

    jars_to_index_results = json.decode(index_jars_stdout)
    for jar, index_results in jars_to_index_results.items():
        path = str(repository_ctx.path(jar))
        if path != jar:
            jars_to_index_results[path] = jars_to_index_results.pop(jar)

    for artifact in dep_tree["dependencies"]:
        file = artifact["file"]
        if file == None:
            continue
        path = str(repository_ctx.path(file))

        if repository_ctx.attr.ignore_empty_files and shas[path] == EMPTY_FILE_SHA256:
            # Sometimes it happens that coursier sees jar files with 0 bytes.
            # Treat them as if coursier found no file in the first place.
            print("Ignoring empty file for artifact: %s" % artifact)
            artifact["file"] = None

            # Restore attributes set earlier in this function.
            if artifact.get("mirror_urls") != None:
                artifact.pop("mirror_urls")
            if artifact.get("url") != None:
                artifact.pop("url")
            continue
        artifact.update({"sha256": shas[path]})
        artifact.update({"packages": jars_to_index_results[path]["packages"]})
        service_implementations = jars_to_index_results[path].get("serviceImplementations", {})
        if service_implementations:
            artifact.update({"services": service_implementations})

    # Keep the original output from coursier for debugging
    repository_ctx.file(
        "coursier-deps.json",
        content = json.encode_indent(dep_tree),
    )
    reformat_lock_file_cmd = _generate_java_jar_command(
        repository_ctx,
        repository_ctx.path(repository_ctx.attr._lock_file_converter),
    )
    for repo in repositories:
        reformat_lock_file_cmd.extend(["--repo", repo["repo_url"]])
    reformat_lock_file_cmd.extend(["--json", "coursier-deps.json"])

    # But update the format to the latest lock file
    result = _execute(
        repository_ctx,
        cmd = reformat_lock_file_cmd,
        progress_message = "Updating lock file format",
    )
    if result.return_code:
        fail("Unable to generate lock file: " + result.stderr)

    lock_file_contents = json.decode(result.stdout)

    inputs_hash, _ = compute_dependency_inputs_signature(
        boms = repository_ctx.attr.boms,
        artifacts = repository_ctx.attr.artifacts,
        repositories = repository_ctx.attr.repositories,
        excluded_artifacts = repository_ctx.attr.excluded_artifacts,
    )

    repository_ctx.file(
        "unsorted_deps.json",
        content = v2_lock_file.render_lock_file(
            lock_file_contents,
            inputs_hash,
        ),
    )

    repository_ctx.report_progress("Generating BUILD targets..")
    (generated_imports, jar_versionless_target_labels) = parser.generate_imports(
        repository_ctx = repository_ctx,
        dependencies = v2_lock_file.get_artifacts(lock_file_contents),
        explicit_artifacts = {
            a["group"] + ":" + a["artifact"] + (":" + a["classifier"] if "classifier" in a else ""): True
            for a in artifacts
        },
        neverlink_artifacts = {
            a["group"] + ":" + a["artifact"] + (":" + a["classifier"] if "classifier" in a else ""): True
            for a in artifacts
            if a.get("neverlink", False)
        },
        testonly_artifacts = {
            a["group"] + ":" + a["artifact"] + (":" + a["classifier"] if "classifier" in a else ""): True
            for a in artifacts
            if a.get("testonly", False)
        },
        override_targets = repository_ctx.attr.override_targets,
        # Skip maven local dependencies if generating the unpinned repository
        skip_maven_local_dependencies = _is_unpinned(repository_ctx),
    )

    # This repository rule can be either in the pinned or unpinned state, depending on when
    # the user invokes artifact pinning. Normalize the repository name here.
    if _is_unpinned(repository_ctx):
        repository_name = repository_ctx.attr.pinned_repo_name
        outdated_build_file_content = ""
    else:
        repository_name = repository_ctx.name

        # Add outdated artifact files if this is a pinned repo
        outdated_build_file_content = _BUILD_OUTDATED
        _add_outdated_files(repository_ctx, artifacts, boms, repositories)

    repository_ctx.file(
        "BUILD",
        (_BUILD + _BUILD_PIN + outdated_build_file_content).format(
            visibilities = ",".join(["\"%s\"" % s for s in (["//visibility:public"] if not repository_ctx.attr.strict_visibility else repository_ctx.attr.strict_visibility_value)]),
            repository_name = repository_name,
            imports = generated_imports,
            aar_import_statement = _get_aar_import_statement_or_empty_str(repository_ctx),
        ),
        executable = False,
    )

    # If maven_install.json has already been used in maven_install,
    # we don't need to instruct user to update WORKSPACE and load pinned_maven_install.
    # If maven_install.json is not used yet, provide complete instructions.
    #
    # Also support custom locations for maven_install.json and update the pin.sh script
    # accordingly.
    predefined_maven_install = bool(repository_ctx.attr.maven_install_json)
    if predefined_maven_install:
        package_path = repository_ctx.attr.maven_install_json.package
        file_name = repository_ctx.attr.maven_install_json.name
        if package_path == "":
            maven_install_location = file_name  # e.g. some.json
        else:
            maven_install_location = "/".join([package_path, file_name])  # e.g. path/to/some.json
    else:
        # Default maven_install.json file name.
        maven_install_location = "{repository_name}_install.json"

    # Expose the script to let users pin the state of the fetch in
    # `<workspace_root>/maven_install.json`.
    #
    # $ bazel run @unpinned_maven//:pin
    #
    # Create the maven_install.json export script for unpinned repositories.
    repository_ctx.template(
        "pin.sh",
        repository_ctx.attr._pin,
        {
            "{maven_install_location}": "$BUILD_WORKSPACE_DIRECTORY/" + maven_install_location,
            "{predefined_maven_install}": str(predefined_maven_install),
            "{repository_name}": repository_name,
        },
        executable = True,
    )

    # Generate 'defs.bzl' with just the dependencies for ':pin'.
    http_files = [
        "load(\"@bazel_tools//tools/build_defs/repo:http.bzl\", \"http_file\")",
        "load(\"@bazel_tools//tools/build_defs/repo:utils.bzl\", \"maybe\")",
        "def pinned_maven_install():",
        "    pass",  # Ensure we're syntactically correct even if no deps are added
    ]
    repository_ctx.file(
        "defs.bzl",
        "\n".join(http_files),
        executable = False,
    )

    # Generate a compatibility layer of external repositories for all jar artifacts.
    if repository_ctx.attr.generate_compat_repositories:
        repository_ctx.template(
            "compat_repository.bzl",
            repository_ctx.attr._compat_repository,
            substitutions = {},
            executable = False,
        )

        compat_repositories_bzl = ["load(\"@%s//:compat_repository.bzl\", \"compat_repository\")" % repository_ctx.name]
        compat_repositories_bzl.append("def compat_repositories():")
        for versionless_target_label in jar_versionless_target_labels:
            compat_repositories_bzl.extend([
                "    compat_repository(",
                "        name = \"%s\"," % versionless_target_label,
                "        generating_repository = \"%s\"," % repository_ctx.name,
                "    )",
            ])
        repository_ctx.file(
            "compat.bzl",
            "\n".join(compat_repositories_bzl) + "\n",
            executable = False,
        )

pinned_coursier_fetch = repository_rule(
    attrs = {
        "_compat_repository": attr.label(default = "//private:compat_repository.bzl"),
        "_outdated": attr.label(default = "//private:outdated.sh"),
        "user_provided_name": attr.string(),
        "resolver": attr.string(doc = "The resolver to use", values = ["coursier", "gradle", "maven"], default = "coursier"),
        "repositories": attr.string_list(),  # list of repository objects, each as json
        "artifacts": attr.string_list(),  # list of artifact objects, each as json
        "boms": attr.string_list(),  # list of bom objects, each as json
        "fetch_sources": attr.bool(default = False),
        "fetch_javadoc": attr.bool(default = False),
        "generate_compat_repositories": attr.bool(default = False),  # generate a compatible layer with repositories for each artifact
        "maven_install_json": attr.label(allow_single_file = True),
        "override_targets": attr.string_dict(default = {}),
        "strict_visibility": attr.bool(
            doc = """Controls visibility of transitive dependencies.

            If "True", transitive dependencies are private and invisible to user's rules.
            If "False", transitive dependencies are public and visible to user's rules.
            """,
            default = False,
        ),
        "strict_visibility_value": attr.label_list(default = ["//visibility:private"]),
        "additional_netrc_lines": attr.string_list(doc = "Additional lines prepended to the netrc file used by `http_file` (with `maven_install_json` only).", default = []),
        "use_credentials_from_home_netrc_file": attr.bool(default = False, doc = "Whether to include coursier credentials gathered from the user home ~/.netrc file"),
        "fail_if_repin_required": attr.bool(doc = "Whether to fail the build if the maven_artifact inputs have changed but the lock file has not been repinned.", default = False),
        "use_starlark_android_rules": attr.bool(default = False, doc = "Whether to use the native or Starlark version of the Android rules."),
        "aar_import_bzl_label": attr.string(default = DEFAULT_AAR_IMPORT_LABEL, doc = "The label (as a string) to use to import aar_import from"),
        "duplicate_version_warning": attr.string(
            doc = """What to do if there are duplicate artifacts

            If "error", then print a message and fail the build.
            If "warn", then print a warning and continue.
            If "none", then do nothing.
            """,
            default = "warn",
            values = [
                "error",
                "warn",
                "none",
            ],
        ),
        "repin_instructions": attr.string(
            doc = "Instructions to re-pin the repository if required. Many people have wrapper scripts for keeping dependencies up to date, and would like to point users to that instead of the default.",
        ),
        "excluded_artifacts": attr.string_list(default = []),  # only used for hash generation
        # Use @@// to refer to the main repo with Bzlmod.
        "_workspace_label": attr.label(default = ("@@" if str(Label("//:invalid")).startswith("@@") else "@") + "//does/not:exist"),
    },
    implementation = _pinned_coursier_fetch_impl,
)

coursier_fetch = repository_rule(
    attrs = {
        "_sha256_hasher": attr.label(default = "//private/tools/prebuilt:hasher_deploy.jar"),
        "_index_jar": attr.label(default = "//private/tools/prebuilt:index_jar_deploy.jar"),
        "_lock_file_converter": attr.label(default = "//private/tools/prebuilt:lock_file_converter_deploy.jar"),
        "_pin": attr.label(default = "//private:pin.sh"),
        "_compat_repository": attr.label(default = "//private:compat_repository.bzl"),
        "_outdated": attr.label(default = "//private:outdated.sh"),
        "user_provided_name": attr.string(),
        "repositories": attr.string_list(),  # list of repository objects, each as json
        "artifacts": attr.string_list(),  # list of artifact objects, each as json
        "boms": attr.string_list(),  # list of bom objects, each as json
        "fail_on_missing_checksum": attr.bool(default = True),
        "fetch_sources": attr.bool(default = False),
        "fetch_javadoc": attr.bool(default = False),
        "use_credentials_from_home_netrc_file": attr.bool(default = False, doc = "Whether to include coursier credentials gathered from the user home ~/.netrc file"),
        "excluded_artifacts": attr.string_list(default = []),  # list of artifacts to exclude
        "generate_compat_repositories": attr.bool(default = False),  # generate a compatible layer with repositories for each artifact
        "version_conflict_policy": attr.string(
            doc = """Policy for user-defined vs. transitive dependency version conflicts

            If "pinned", choose the user-specified version in maven_install unconditionally.
            If "default", follow Coursier's default policy.
            """,
            default = "default",
            values = [
                "default",
                "pinned",
            ],
        ),
        "maven_install_json": attr.label(allow_single_file = True),
        "override_targets": attr.string_dict(default = {}),
        "strict_visibility": attr.bool(
            doc = """Controls visibility of transitive dependencies

            If "True", transitive dependencies are private and invisible to user's rules.
            If "False", transitive dependencies are public and visible to user's rules.
            """,
            default = False,
        ),
        "strict_visibility_value": attr.label_list(default = ["//visibility:private"]),
        "resolve_timeout": attr.int(default = 600),
        "use_starlark_android_rules": attr.bool(default = False, doc = "Whether to use the native or Starlark version of the Android rules."),
        "aar_import_bzl_label": attr.string(default = DEFAULT_AAR_IMPORT_LABEL, doc = "The label (as a string) to use to import aar_import from"),
        "duplicate_version_warning": attr.string(
            doc = """What to do if there are duplicate artifacts

            If "error", then print a message and fail the build.
            If "warn", then print a warning and continue.
            If "none", then do nothing.
            """,
            default = "warn",
            values = [
                "error",
                "warn",
                "none",
            ],
        ),
        "ignore_empty_files": attr.bool(default = False, doc = "Treat jars that are empty as if they were not found."),
        "additional_coursier_options": attr.string_list(doc = "Additional options that will be passed to coursier."),
        "pinned_repo_name": attr.string(
            doc = "Name of the corresponding pinned repo for this repo. Presence implies that this is an unpinned repo.",
            mandatory = False,
        ),
    },
    environ = [
        "JAVA_HOME",
        "JDK_JAVA_OPTIONS",
        "http_proxy",
        "HTTP_PROXY",
        "https_proxy",
        "HTTPS_PROXY",
        "no_proxy",
        "NO_PROXY",
        "COURSIER_CACHE",
        "COURSIER_OPTS",
        "COURSIER_URL",
        "RJE_VERBOSE",
        "XDG_CACHE_HOME",
    ],
    implementation = _coursier_fetch_impl,
)
