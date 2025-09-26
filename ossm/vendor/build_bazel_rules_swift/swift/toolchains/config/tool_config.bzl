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

"""Definitions used to configure toolchain tools."""

load("@bazel_skylib//lib:paths.bzl", "paths")

def _tool_config_info_init(
        *,
        additional_tools = [],
        args = [],
        driver_config = {},
        env = {},
        executable = None,
        execution_requirements = {},
        resource_set = None,
        use_param_file = False,
        worker_mode = None):
    """Validates and initializes a new Swift toolchain tool configuration.

    The `driver_config` argument can be specified as a convenience that supports
    the various ways that the Swift driver can have its location specified or
    overridden by the build rules, such as by providing a toolchain root
    directory or a custom executable. It supports three kinds of "dispatch":

    1.  If the toolchain provides a custom driver executable, the returned tool
        config invokes it with the requested mode passed via the `--driver_mode`
        argument.
    2.  If the toolchain provides a root directory, then the returned tool
        config will use an executable that is a string with the same name as the
        driver mode in the `bin` directory of that toolchain.
    3.  If the toolchain does not provide a root, then the returned tool config
        simply uses the driver mode as the executable, assuming that it will be
        available by invoking that alone (e.g., it will be found on the system
        path or by another delegating tool like `xcrun` from Xcode).

    Args:
        additional_tools: A list of `File`s or `FilesToRunProvider`s denoting
            additional tools that should be passed as inputs to actions that
            use this tool. This should be used if `executable` is, for example,
            a symlink that points to another executable or if it is a driver
            that launches other executables as subprocesses.
        args: A list of arguments that are always passed to the tool.
        driver_config: Special configuration for a Swift driver tool. This
            dictionary must contain a `mode` key that indicates the Swift driver
            mode to launch (e.g., `swift`, `swiftc`,
            `swift-symbolgraph-extract`). It may also contain three optional
            entries: `swift_executable`, a custom Swift executable that may be
            provided by the toolchain; `toolchain_root`, the root directory of a
            custom toolchain to use; and `tool_executable_suffix`, the suffix
            for executable tools to use (e.g. `.exe` on Windows). This may not
            be specified if `executable` is specified.
        env: A dictionary of environment variables that should be set when
            invoking actions using this tool.
        executable: The `File` or `string` denoting the tool that should be
            executed. This will be used as the `executable` argument of spawned
            actions unless `worker_mode` is set, in which case it will be used
            as the first argument to the worker.
        execution_requirements: A dictionary of execution requirements that
            should be passed when creating actions with this tool.
        resource_set: The function which build resource set (mem, cpu) for local
            invocation of the action.
        use_param_file: If True, actions invoked using this tool will have their
            arguments written to a param file.
        worker_mode: A string, or `None`, describing how the tool is invoked
            using the build rules' worker, if at all. If `None`, the tool will
            be invoked directly. If `"wrap"`, the tool will be wrapped in an
            invocation of the worker but otherwise run as a single process. If
            `"persistent"`, then the action will be launched with execution
            requirements that indicate that Bazel should attempt to use a
            persistent worker if the spawn strategy allows for it (starting a
            new instance if necessary, or connecting to an existing one).

    Returns:
        A validated dictionary with the fields of the `ToolConfigInfo` provider.
    """
    if driver_config:
        if executable:
            fail("Both 'driver_config' and 'executable' cannot be specified.")

        driver_mode = driver_config.get("mode", None)
        if not driver_mode:
            fail(
                "When using 'driver_config', the 'mode' key must be specified.",
            )

        swift_executable = driver_config.get("swift_executable", None)
        toolchain_root = driver_config.get("toolchain_root", None)

        if swift_executable:
            executable = swift_executable
            args = ["--driver-mode={}".format(driver_mode)] + args
        elif toolchain_root:
            executable = paths.join(toolchain_root, "bin", driver_mode)
        else:
            executable = driver_mode

        tool_executable_suffix = driver_config.get("tool_executable_suffix", "")

        if not executable.endswith(tool_executable_suffix):
            executable = "{}{}".format(executable, tool_executable_suffix)

    return {
        "additional_tools": additional_tools,
        "args": args,
        "env": env,
        "executable": executable,
        "execution_requirements": execution_requirements,
        "resource_set": resource_set,
        "use_param_file": use_param_file,
        "worker_mode": _validate_worker_mode(worker_mode),
    }

ToolConfigInfo, _tool_config_info_init_unchecked = provider(
    doc = "A tool used by the Swift toolchain and its requirements.",
    fields = [
        "additional_tools",
        "args",
        "env",
        "executable",
        "execution_requirements",
        "resource_set",
        "use_param_file",
        "worker_mode",
    ],
    init = _tool_config_info_init,
)

def _validate_worker_mode(worker_mode):
    """Validates the `worker_mode` argument of `tool_config`.

    This function fails the build if the worker mode is not None, "persistent",
    or "wrap".

    Args:
        worker_mode: The worker mode to validate.

    Returns:
        The original worker mode, if it was valid.
    """
    if worker_mode != None and worker_mode not in ("persistent", "wrap"):
        fail(
            "The 'worker_mode' argument of " +
            "'swift_toolchain_config.tool_config' must be either None, " +
            "'persistent', or 'wrap'.",
        )

    return worker_mode
