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

""

load("@rules_python_internal//:rules_python_config.bzl", rp_config = "config")
load("//python/private:auth.bzl", "AUTH_ATTRS", "get_auth")
load("//python/private:envsubst.bzl", "envsubst")
load("//python/private:is_standalone_interpreter.bzl", "is_standalone_interpreter")
load("//python/private:repo_utils.bzl", "REPO_DEBUG_ENV_VAR", "repo_utils")
load(":attrs.bzl", "ATTRS", "use_isolated")
load(":deps.bzl", "all_repo_names", "record_files")
load(":generate_whl_library_build_bazel.bzl", "generate_whl_library_build_bazel")
load(":parse_whl_name.bzl", "parse_whl_name")
load(":patch_whl.bzl", "patch_whl")
load(":pep508_requirement.bzl", "requirement")
load(":pypi_repo_utils.bzl", "pypi_repo_utils")
load(":whl_extract.bzl", "whl_extract")
load(":whl_metadata.bzl", "whl_metadata")
load(":whl_target_platforms.bzl", "whl_target_platforms")

_CPPFLAGS = "CPPFLAGS"
_COMMAND_LINE_TOOLS_PATH_SLUG = "commandlinetools"
_WHEEL_ENTRY_POINT_PREFIX = "rules_python_wheel_entry_point"

def _get_xcode_location_cflags(rctx, logger = None):
    """Query the xcode sdk location to update cflags

    Figure out if this interpreter target comes from rules_python, and patch the xcode sdk location if so.
    Pip won't be able to compile c extensions from sdists with the pre built python distributions from astral-sh
    otherwise. See https://github.com/astral-sh/python-build-standalone/issues/103
    """

    # Only run on MacOS hosts
    if not rctx.os.name.lower().startswith("mac os"):
        return []

    xcode_sdk_location = repo_utils.execute_unchecked(
        rctx,
        op = "GetXcodeLocation",
        arguments = [repo_utils.which_checked(rctx, "xcode-select"), "--print-path"],
        logger = logger,
    )
    if xcode_sdk_location.return_code != 0:
        return []

    xcode_root = xcode_sdk_location.stdout.strip()
    if _COMMAND_LINE_TOOLS_PATH_SLUG not in xcode_root.lower():
        # This is a full xcode installation somewhere like /Applications/Xcode13.0.app/Contents/Developer
        # so we need to change the path to to the macos specific tools which are in a different relative
        # path than xcode installed command line tools.
        xcode_sdks_json = repo_utils.execute_checked(
            rctx,
            op = "LocateXCodeSDKs",
            arguments = [
                repo_utils.which_checked(rctx, "xcrun"),
                "xcodebuild",
                "-showsdks",
                "-json",
            ],
            environment = {
                "DEVELOPER_DIR": xcode_root,
            },
            logger = logger,
        ).stdout
        xcode_sdks = json.decode(xcode_sdks_json)
        potential_sdks = [
            sdk
            for sdk in xcode_sdks
            if "productName" in sdk and
               sdk["productName"] == "macOS" and
               "darwinos" not in sdk["canonicalName"]
        ]

        # Now we'll get two entries here (one for internal and another one for public)
        # It shouldn't matter which one we pick.
        xcode_sdk_path = potential_sdks[0]["sdkPath"]
    else:
        xcode_sdk_path = "{}/SDKs/MacOSX.sdk".format(xcode_root)

    return [
        "-isysroot {}".format(xcode_sdk_path),
    ]

def _get_toolchain_unix_cflags(rctx, python_interpreter, logger = None):
    """Gather cflags from a standalone toolchain for unix systems.

    Pip won't be able to compile c extensions from sdists with the pre built python distributions from astral-sh
    otherwise. See https://github.com/astral-sh/python-build-standalone/issues/103
    """

    # Only run on Unix systems
    if not rctx.os.name.lower().startswith(("mac os", "linux")):
        return []

    # Only update the location when using a standalone toolchain.
    if not is_standalone_interpreter(rctx, python_interpreter, logger = logger):
        return []

    stdout = pypi_repo_utils.execute_checked_stdout(
        rctx,
        op = "GetPythonVersionForUnixCflags",
        # python_interpreter by default points to a symlink, however when using bazel in vendor mode,
        # and the vendored directory moves around, the execution of python fails, as it's getting confused
        # where it's running from. More to the fact that we are executing it in isolated mode "-I", which
        # results in PYTHONHOME being ignored. The solution is to run python from it's real directory.
        python = python_interpreter.realpath,
        arguments = [
            # Run the interpreter in isolated mode, this options implies -E, -P and -s.
            # Ensures environment variables are ignored that are set in userspace, such as PYTHONPATH,
            # which may interfere with this invocation.
            "-I",
            "-c",
            "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}', end='')",
        ],
        srcs = [],
        logger = logger,
    )
    _python_version = stdout
    include_path = "{}/include/python{}".format(
        python_interpreter.dirname,
        _python_version,
    )

    return ["-isystem {}".format(include_path)]

def _parse_optional_attrs(rctx, args, extra_pip_args = None):
    """Helper function to parse common attributes of pip_repository and whl_library repository rules.

    This function also serializes the structured arguments as JSON
    so they can be passed on the command line to subprocesses.

    Args:
        rctx: Handle to the rule repository context.
        args: A list of parsed args for the rule.
        extra_pip_args: The pip args to pass.
    Returns: Augmented args list.
    """

    if use_isolated(rctx, rctx.attr):
        args.append("--isolated")

    # Bazel version 7.1.0 and later (and rolling releases from version 8.0.0-pre.20240128.3)
    # support rctx.getenv(name, default): When building incrementally, any change to the value of
    # the variable named by name will cause this repository to be re-fetched.
    if "getenv" in dir(rctx):
        getenv = rctx.getenv
    else:
        getenv = rctx.os.environ.get

    # Check for None so we use empty default types from our attrs.
    # Some args want to be list, and some want to be dict.
    if extra_pip_args != None:
        args += [
            "--extra_pip_args",
            json.encode(struct(arg = [
                envsubst(pip_arg, rctx.attr.envsubst, getenv)
                for pip_arg in extra_pip_args
            ])),
        ]

    if rctx.attr.download_only:
        args.append("--download_only")

    if rctx.attr.pip_data_exclude != None:
        args += [
            "--pip_data_exclude",
            json.encode(struct(arg = rctx.attr.pip_data_exclude)),
        ]

    env = {}
    if rctx.attr.environment != None:
        for key, value in rctx.attr.environment.items():
            env[key] = value

    # This is super hacky, but working out something nice is tricky.
    # This is in particular needed for psycopg2 which attempts to link libpython.a,
    # in order to point the linker at the correct python intepreter.
    if rctx.attr.add_libdir_to_library_search_path:
        if "LDFLAGS" in env:
            fail("Can't set both environment LDFLAGS and add_libdir_to_library_search_path")
        command = [pypi_repo_utils.resolve_python_interpreter(rctx), "-c", "import sys ; sys.stdout.write('{}/lib'.format(sys.exec_prefix))"]
        result = rctx.execute(command)
        if result.return_code != 0:
            fail("Failed to get LDFLAGS path: command: {}, exit code: {}, stdout: {}, stderr: {}".format(command, result.return_code, result.stdout, result.stderr))
        libdir = result.stdout
        env["LDFLAGS"] = "-L{}".format(libdir)

    args += [
        "--environment",
        json.encode(struct(arg = env)),
    ]

    return args

def _get_python_home(rctx, python_interpreter, logger = None):
    """Get the PYTHONHOME directory from the selected python interpretter

    Args:
        rctx (repository_ctx): The repository context.
        python_interpreter (path): The resolved python interpreter.
        logger: Optional logger to use for operations.
    Returns:
        String of PYTHONHOME directory.
    """

    return pypi_repo_utils.execute_checked_stdout(
        rctx,
        op = "GetPythonHome",
        # python_interpreter by default points to a symlink, however when using bazel in vendor mode,
        # and the vendored directory moves around, the execution of python fails, as it's getting confused
        # where it's running from. More to the fact that we are executing it in isolated mode "-I", which
        # results in PYTHONHOME being ignored. The solution is to run python from it's real directory.
        python = python_interpreter.realpath,
        arguments = [
            # Run the interpreter in isolated mode, this options implies -E, -P and -s.
            # Ensures environment variables are ignored that are set in userspace, such as PYTHONPATH,
            # which may interfere with this invocation.
            "-I",
            "-c",
            "import sys; print(f'{sys.prefix}', end='')",
        ],
        srcs = [],
        logger = logger,
    )

def _create_repository_execution_environment(rctx, python_interpreter, logger = None):
    """Create a environment dictionary for processes we spawn with rctx.execute.

    Args:
        rctx (repository_ctx): The repository context.
        python_interpreter (path): The resolved python interpreter.
        logger: Optional logger to use for operations.
    Returns:
        Dictionary of environment variable suitable to pass to rctx.execute.
    """

    env = {
        "PYTHONHOME": _get_python_home(rctx, python_interpreter, logger),
        "PYTHONPATH": pypi_repo_utils.construct_pythonpath(
            rctx,
            entries = rctx.attr._python_path_entries,
        ),
    }

    # Gather any available CPPFLAGS values
    #
    # We may want to build in an environment without a cc toolchain.
    # In those cases, we're limited to --download-only, but we should respect that here.
    is_wheel = rctx.attr.filename and rctx.attr.filename.endswith(".whl")
    if not (rctx.attr.download_only or is_wheel):
        cppflags = []
        cppflags.extend(_get_xcode_location_cflags(rctx, logger = logger))
        cppflags.extend(_get_toolchain_unix_cflags(rctx, python_interpreter, logger = logger))
        env[_CPPFLAGS] = " ".join(cppflags)
    return env

def _extract_whl_py(rctx, *, python_interpreter, args, whl_path, environment, logger):
    target_platforms = rctx.attr.experimental_target_platforms or []
    if target_platforms:
        parsed_whl = parse_whl_name(whl_path.basename)

        # NOTE @aignas 2023-12-04: if the wheel is a platform specific wheel, we
        # only include deps for that target platform
        if parsed_whl.platform_tag != "any":
            target_platforms = [
                p.target_platform
                for p in whl_target_platforms(
                    platform_tag = parsed_whl.platform_tag,
                    abi_tag = parsed_whl.abi_tag.strip("tm"),
                )
            ]

    pypi_repo_utils.execute_checked(
        rctx,
        op = "whl_library.ExtractWheel({}, {})".format(rctx.attr.name, whl_path),
        python = python_interpreter,
        arguments = args + [
            "--whl-file",
            whl_path,
        ] + ["--platform={}".format(p) for p in target_platforms],
        srcs = rctx.attr._python_srcs,
        environment = environment,
        quiet = rctx.attr.quiet,
        timeout = rctx.attr.timeout,
        logger = logger,
    )

def _whl_library_impl(rctx):
    logger = repo_utils.logger(rctx)
    python_interpreter = pypi_repo_utils.resolve_python_interpreter(
        rctx,
        python_interpreter = rctx.attr.python_interpreter,
        python_interpreter_target = rctx.attr.python_interpreter_target,
    )
    args = [
        "-m",
        "python.private.pypi.whl_installer.wheel_installer",
        "--requirement",
        rctx.attr.requirement,
    ]
    extra_pip_args = []
    extra_pip_args.extend(rctx.attr.extra_pip_args)

    # Manually construct the PYTHONPATH since we cannot use the toolchain here
    environment = _create_repository_execution_environment(rctx, python_interpreter, logger = logger)

    whl_path = None
    sdist_filename = None
    if rctx.attr.whl_file:
        rctx.watch(rctx.attr.whl_file)
        whl_path = rctx.path(rctx.attr.whl_file)

        # Simulate the behaviour where the whl is present in the current directory.
        rctx.symlink(whl_path, whl_path.basename)
        whl_path = rctx.path(whl_path.basename)
    elif rctx.attr.urls and rctx.attr.filename:
        filename = rctx.attr.filename
        urls = rctx.attr.urls
        result = rctx.download(
            url = urls,
            output = filename,
            sha256 = rctx.attr.sha256,
            auth = get_auth(rctx, urls),
        )
        if not rctx.attr.sha256:
            # this is only seen when there is a direct URL reference without sha256
            logger.warn("Please update the requirement line to include the hash:\n{} \\\n    --hash=sha256:{}".format(
                rctx.attr.requirement,
                result.sha256,
            ))

        if not result.success:
            fail("could not download the '{}' from {}:\n{}".format(filename, urls, result))

        if filename.endswith(".whl"):
            whl_path = rctx.path(filename)
        else:
            sdist_filename = filename

            # It is an sdist and we need to tell PyPI to use a file in this directory
            # and, allow getting build dependencies from PYTHONPATH, which we
            # setup in this repository rule, but still download any necessary
            # build deps from PyPI (e.g. `flit_core`) if they are missing.
            extra_pip_args.extend(["--find-links", "."])

    args = _parse_optional_attrs(rctx, args, extra_pip_args)

    # also enable pipstar for any whls that are downloaded without `pip`
    enable_pipstar = (rp_config.enable_pipstar or whl_path) and rctx.attr.config_load
    enable_pipstar_extract = enable_pipstar and rp_config.bazel_8_or_later

    if not whl_path:
        if rctx.attr.urls:
            op_tmpl = "whl_library.BuildWheelFromSource({name}, {requirement})"
        elif rctx.attr.download_only:
            op_tmpl = "whl_library.DownloadWheel({name}, {requirement})"
        else:
            op_tmpl = "whl_library.ResolveRequirement({name}, {requirement})"

        pypi_repo_utils.execute_checked(
            rctx,
            # truncate the requirement value when logging it / reporting
            # progress since it may contain several ' --hash=sha256:...
            # --hash=sha256:...' substrings that fill up the console
            python = python_interpreter,
            op = op_tmpl.format(name = rctx.attr.name, requirement = rctx.attr.requirement.split(" ", 1)[0]),
            arguments = args,
            environment = environment,
            srcs = rctx.attr._python_srcs,
            quiet = rctx.attr.quiet,
            timeout = rctx.attr.timeout,
            logger = logger,
        )

        whl_path = rctx.path(json.decode(rctx.read("whl_file.json"))["whl_file"])
        if not rctx.delete("whl_file.json"):
            fail("failed to delete the whl_file.json file")

    if rctx.attr.whl_patches:
        patches = {}
        for patch_file, json_args in rctx.attr.whl_patches.items():
            patch_dst = struct(**json.decode(json_args))
            if whl_path.basename in patch_dst.whls:
                patches[patch_file] = patch_dst.patch_strip

        if patches:
            whl_path = patch_whl(
                rctx,
                op = "whl_library.PatchWhl({}, {})".format(rctx.attr.name, rctx.attr.requirement),
                python_interpreter = python_interpreter,
                whl_path = whl_path,
                patches = patches,
                quiet = rctx.attr.quiet,
                timeout = rctx.attr.timeout,
            )

    if enable_pipstar_extract:
        whl_extract(rctx, whl_path = whl_path, logger = logger)
    else:
        _extract_whl_py(
            rctx,
            python_interpreter = python_interpreter,
            args = args,
            whl_path = whl_path,
            environment = environment,
            logger = logger,
        )

    # NOTE @aignas 2025-09-28: if someone has an old vendored file that does not have the
    # dep_template set or the packages is not set either, we should still not break, best to
    # disable pipstar for that particular case.
    #
    # Remove non-pipstar and config_load check when we release rules_python 2.
    if enable_pipstar:
        install_dir_path = whl_path.dirname.get_child("site-packages")
        metadata = whl_metadata(
            install_dir = install_dir_path,
            read_fn = rctx.read,
            logger = logger,
        )
        namespace_package_files = pypi_repo_utils.find_namespace_package_files(rctx, install_dir_path)

        # NOTE @aignas 2024-06-22: this has to live on until we stop supporting
        # passing `twine` as a `:pkg` library via the `WORKSPACE` builds.
        #
        # See ../../packaging.bzl line 190
        entry_points = {}
        for item in metadata.entry_points:
            name = item.name
            module = item.module
            attribute = item.attribute

            # There is an extreme edge-case with entry_points that end with `.py`
            # See: https://github.com/bazelbuild/bazel/blob/09c621e4cf5b968f4c6cdf905ab142d5961f9ddc/src/test/java/com/google/devtools/build/lib/rules/python/PyBinaryConfiguredTargetTest.java#L174
            entry_point_without_py = name[:-3] + "_py" if name.endswith(".py") else name
            entry_point_target_name = (
                _WHEEL_ENTRY_POINT_PREFIX + "_" + entry_point_without_py
            )
            entry_point_script_name = entry_point_target_name + ".py"

            rctx.file(
                entry_point_script_name,
                _generate_entry_point_contents(module, attribute),
            )
            entry_points[entry_point_without_py] = entry_point_script_name

        build_file_contents = generate_whl_library_build_bazel(
            name = whl_path.basename,
            sdist_filename = sdist_filename,
            dep_template = rctx.attr.dep_template or "@{}{{name}}//:{{target}}".format(
                rctx.attr.repo_prefix,
            ),
            config_load = rctx.attr.config_load,
            entry_points = entry_points,
            metadata_name = metadata.name,
            metadata_version = metadata.version,
            requires_dist = metadata.requires_dist,
            # TODO @aignas 2025-05-17: maybe have a build flag for this instead
            enable_implicit_namespace_pkgs = rctx.attr.enable_implicit_namespace_pkgs,
            # TODO @aignas 2025-04-14: load through the hub:
            annotation = None if not rctx.attr.annotation else struct(**json.decode(rctx.read(rctx.attr.annotation))),
            data_exclude = rctx.attr.pip_data_exclude,
            group_deps = rctx.attr.group_deps,
            group_name = rctx.attr.group_name,
            namespace_package_files = namespace_package_files,
            extras = requirement(rctx.attr.requirement).extras,
        )
    else:
        metadata = json.decode(rctx.read("metadata.json"))
        rctx.delete("metadata.json")

        # NOTE @aignas 2024-06-22: this has to live on until we stop supporting
        # passing `twine` as a `:pkg` library via the `WORKSPACE` builds.
        #
        # See ../../packaging.bzl line 190
        entry_points = {}
        for item in metadata["entry_points"]:
            name = item["name"]
            module = item["module"]
            attribute = item["attribute"]

            # There is an extreme edge-case with entry_points that end with `.py`
            # See: https://github.com/bazelbuild/bazel/blob/09c621e4cf5b968f4c6cdf905ab142d5961f9ddc/src/test/java/com/google/devtools/build/lib/rules/python/PyBinaryConfiguredTargetTest.java#L174
            entry_point_without_py = name[:-3] + "_py" if name.endswith(".py") else name
            entry_point_target_name = (
                _WHEEL_ENTRY_POINT_PREFIX + "_" + entry_point_without_py
            )
            entry_point_script_name = entry_point_target_name + ".py"

            rctx.file(
                entry_point_script_name,
                _generate_entry_point_contents(module, attribute),
            )
            entry_points[entry_point_without_py] = entry_point_script_name

        namespace_package_files = pypi_repo_utils.find_namespace_package_files(rctx, rctx.path("site-packages"))

        build_file_contents = generate_whl_library_build_bazel(
            name = whl_path.basename,
            sdist_filename = sdist_filename,
            dep_template = rctx.attr.dep_template or "@{}{{name}}//:{{target}}".format(rctx.attr.repo_prefix),
            entry_points = entry_points,
            # TODO @aignas 2025-05-17: maybe have a build flag for this instead
            enable_implicit_namespace_pkgs = rctx.attr.enable_implicit_namespace_pkgs,
            # TODO @aignas 2025-04-14: load through the hub:
            dependencies = metadata["deps"],
            dependencies_by_platform = metadata["deps_by_platform"],
            annotation = None if not rctx.attr.annotation else struct(**json.decode(rctx.read(rctx.attr.annotation))),
            data_exclude = rctx.attr.pip_data_exclude,
            group_deps = rctx.attr.group_deps,
            group_name = rctx.attr.group_name,
            tags = [
                "pypi_name={}".format(metadata["name"]),
                "pypi_version={}".format(metadata["version"]),
            ],
            namespace_package_files = namespace_package_files,
        )

    # Delete these in case the wheel had them. They generally don't cause
    # a problem, but let's avoid the chance of that happening.
    rctx.file("WORKSPACE")
    rctx.file("WORKSPACE.bazel")
    rctx.file("MODULE.bazel")
    rctx.file("REPO.bazel")

    paths = list(rctx.path(".").readdir())
    for _ in range(10000000):
        if not paths:
            break
        path = paths.pop()

        # BUILD files interfere with globbing and Bazel package boundaries.
        if path.basename in ("BUILD", "BUILD.bazel"):
            rctx.delete(path)
        elif path.is_dir:
            paths.extend(path.readdir())

    rctx.file("BUILD.bazel", build_file_contents)

    if enable_pipstar and enable_pipstar_extract:
        if hasattr(rctx, "repo_metadata"):
            return rctx.repo_metadata(reproducible = True)

    return None

def _generate_entry_point_contents(
        module,
        attribute,
        shebang = "#!/usr/bin/env python3"):
    """Generate the contents of an entry point script.

    Args:
        module (str): The name of the module to use.
        attribute (str): The name of the attribute to call.
        shebang (str, optional): The shebang to use for the entry point python
            file.

    Returns:
        str: A string of python code.
    """
    contents = """\
{shebang}
import sys
from {module} import {attribute}
if __name__ == "__main__":
    sys.exit({attribute}())
""".format(
        shebang = shebang,
        module = module,
        attribute = attribute,
    )
    return contents

# NOTE @aignas 2024-03-21: The usage of dict({}, **common) ensures that all args to `dict` are unique
whl_library_attrs = dict({
    "annotation": attr.label(
        doc = (
            "Optional json encoded file containing annotation to apply to the extracted wheel. " +
            "See `package_annotation`"
        ),
        allow_files = True,
    ),
    "config_load": attr.string(
        doc = "The load location for configuration for pipstar.",
    ),
    "dep_template": attr.string(
        doc = """
The dep template to use for referencing the dependencies. It should have `{name}`
and `{target}` tokens that will be replaced with the normalized distribution name
and the target that we need respectively.

For example if your whl depends on `numpy` and your Python package repo is named
`pip` so that you would normally do `@pip//numpy`, then this should be: `@pip//{name}`.
""",
    ),
    "filename": attr.string(
        doc = "Download the whl file to this filename. Only used when the `urls` is passed. If not specified, will be auto-detected from the `urls`.",
    ),
    "group_deps": attr.string_list(
        doc = "List of dependencies to skip in order to break the cycles within a dependency group.",
        default = [],
    ),
    "group_name": attr.string(
        doc = "Name of the group, if any.",
    ),
    "repo": attr.string(
        doc = "Pointer to parent repo name. Used to make these rules rerun if the parent repo changes.",
    ),
    "repo_prefix": attr.string(
        doc = """
Prefix for the generated packages will be of the form `@<prefix><sanitized-package-name>//...`

DEPRECATED. Only left for people who vendor requirements.bzl.
""",
    ),
    "requirement": attr.string(
        mandatory = True,
        doc = "Python requirement string describing the package to make available, if 'urls' or 'whl_file' is given, then this only needs to include foo[any_extras] as a bare minimum.",
    ),
    "sha256": attr.string(
        doc = "The sha256 of the downloaded whl. Only used when the `urls` is passed.",
    ),
    "urls": attr.string_list(
        doc = """\
The list of urls of the whl to be downloaded using bazel downloader. Using this
attr makes `extra_pip_args` and `download_only` ignored.""",
    ),
    "whl_file": attr.label(
        doc = "The whl file that should be used instead of downloading or building the whl.",
    ),
    "whl_patches": attr.label_keyed_string_dict(
        doc = """
A label-keyed-string dict with patch files as keys and json-strings as values.

The keys are labels to the patch file to apply.

The values describe what to apply the patch to and how to apply it.
It is encoded as `json.encode(struct([whls], patch_strip])`,
where `whls` is a `list[str`] of wheel filenames, and `patch_strip`
is a number.

So it will look something like this:
```
"//path/to/package:my.patch": json.encode(struct(
    whls = ["something-2.7.1-py3-none-any.whl"],
    patch_strip = 1,
)),
```
The patch is applied within the scope of the .whl file.
I.e. you should create the patch from the same place you unziped the wheel.


This is to maintain flexibility and correct bzlmod extension interface until we have a better
way to define whl_library and move whl patching to a separate place. INTERNAL USE ONLY.""",
    ),
    "_python_path_entries": attr.label_list(
        # Get the root directory of these rules and keep them as a default attribute
        # in order to avoid unnecessary repository fetching restarts.
        #
        # This is very similar to what was done in https://github.com/bazelbuild/rules_go/pull/3478
        default = [
            Label("//:BUILD.bazel"),
        ] + [
            # Includes all the external dependencies from repositories.bzl
            Label("@" + repo + "//:BUILD.bazel")
            for repo in all_repo_names
        ],
    ),
    "_python_srcs": attr.label_list(
        # Used as a default value in a rule to ensure we fetch the dependencies.
        default = [
            Label("//python/private/pypi/whl_installer:platform.py"),
            Label("//python/private/pypi/whl_installer:wheel.py"),
            Label("//python/private/pypi/whl_installer:wheel_installer.py"),
            Label("//python/private/pypi/whl_installer:arguments.py"),
        ] + record_files.values(),
    ),
    "_rule_name": attr.string(default = "whl_library"),
}, **ATTRS)
whl_library_attrs.update(AUTH_ATTRS)

whl_library = repository_rule(
    attrs = whl_library_attrs,
    doc = """
Download and extracts a single wheel based into a bazel repo based on the requirement string passed in.
Instantiated from pip_repository and inherits config options from there.

:::{versionchanged} 1.9.0
The `whl_library` is marked as reproducible if using starlark to extract and parse the
wheel contents without building an `sdist` first.
:::
""",
    implementation = _whl_library_impl,
    environ = [
        "RULES_PYTHON_PIP_ISOLATED",
        REPO_DEBUG_ENV_VAR,
    ],
)
