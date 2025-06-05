#!/usr/bin/env python3

# Copyright 2020 The Bazel Authors. All rights reserved.
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

"""Invoked by `bazel run` to launch *_application targets in the simulator."""

# This script works in one of two modes.
#
# If either --ios_simulator_version or --ios_simulator_device were not
# passed to bazel:
#
# 1. Discovers a simulator compatible with the minimum_os of the
#    *_application target, preferring already-booted simulators
#    if possible
# 2. Boots the simulator if needed
# 3. Installs and launches the application
# 4. Displays the application's output on the console
#
# This mode does not kill running simulators or shutdown or delete the simulator
# after it completes.
#
# If --ios_simulator_version and --ios_simulator_device were both passed
# to bazel:
#
# 1. Creates a new temporary simulator by running "simctl create ..."
# 2. Boots the new temporary simulator
# 3. Installs and launches the application
# 4. Displays the application's output on the console
# 5. When done, shuts down and deletes the newly-created simulator
#
# All environment variables with names starting with "IOS_" are passed to the
# application, after stripping the prefix "IOS_".

import collections.abc
import contextlib
import json
import logging
import os
import os.path
import pathlib
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Dict, Optional
import zipfile


# Custom type for methods yielding an Apple simulator UDID.
AppleSimulatorUDID = collections.abc.Generator[str, None, None]


logging.basicConfig(
    format="%(asctime)s.%(msecs)03d %(levelname)s %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    level=logging.INFO,
)
logger = logging.getLogger(__name__)

if platform.system() != "Darwin":
  raise Exception(
      "Cannot run Apple platform application targets on a non-mac machine."
  )


class DeviceType(collections.abc.Mapping):
  """Wraps the `devicetype` dictionary from `simctl list -j`.

  Provides an ordering so iPhones > iPads. In addition, maintains the
  original order from `simctl list` as `simctl_list_index` to ensure
  newer device types are sorted after older device types.
  """

  def __init__(self, device_type, simctl_list_index):
    self.device_type = device_type
    self.simctl_list_index = simctl_list_index

  def __getitem__(self, name):
    return self.device_type[name]

  def __iter__(self):
    return iter(self.device_type)

  def __len__(self):
    return len(self.device_type)

  def __repr__(self):
    return self["name"] + " (" + self["identifier"] + ")"

  def __lt__(self, other):
    # Order iPhones ahead of (later in the list than) iPads.
    if self.is_ipad() and other.is_iphone():
      return True
    elif self.is_iphone() and other.is_ipad():
      return False
    # Order device types from the same product family in the same order
    # as `simctl list`.
    return self.simctl_list_index < other.simctl_list_index

  def supports_platform_type(self, platform_type: str) -> bool:
    """Returns boolean to indicate if device supports given Apple platform type."""
    if platform_type == "ios":
      return self.is_iphone() or self.is_ipad()
    elif platform_type == "tvos":
      return self.is_apple_tv()
    elif platform_type == "watchos":
      return self.is_apple_watch()
    elif platform_type == "visionos":
      return self.is_apple_vision()
    else:
      raise ValueError(
          f"Apple platform type not supported for simulator: {platform_type}."
      )

  def is_apple_tv(self) -> bool:
    return self.has_product_family_or_identifier("Apple TV")

  def is_apple_watch(self) -> bool:
    return self.has_product_family_or_identifier("Apple Watch")
  
  def is_apple_vision(self) -> bool:
    return self.has_product_family_or_identifier("Apple Vision")

  def is_iphone(self) -> bool:
    return self.has_product_family_or_identifier("iPhone")

  def is_ipad(self) -> bool:
    return self.has_product_family_or_identifier("iPad")

  def has_product_family_or_identifier(self, device_type: str) -> bool:
    product_family = self.get("productFamily")
    if product_family:
      return product_family == device_type
    # Some older simulators are missing `productFamily`. Try to guess from the
    # identifier.
    return device_type in self["identifier"]


class Device(collections.abc.Mapping):
  """Wraps the `device` dictionary from `simctl list -j`.

  Provides an ordering so booted devices > shutdown devices, delegating
  to `DeviceType` order when both devices have the same state.
  """

  def __init__(self, device, device_type):
    self.device = device
    self.device_type = device_type

  def is_shutdown(self):
    return self["state"] == "Shutdown"

  def is_booted(self):
    return self["state"] == "Booted"

  def __getitem__(self, name):
    return self.device[name]

  def __iter__(self):
    return iter(self.device)

  def __len__(self):
    return len(self.device)

  def __repr__(self):
    return self["name"] + "(" + self["udid"] + ")"

  def __lt__(self, other):
    if self.is_shutdown() and other.is_booted():
      return True
    elif self.is_booted() and other.is_shutdown():
      return False
    else:
      return self.device_type < other.device_type


def minimum_os_to_simctl_runtime_version(minimum_os: str) -> int:
  """Converts a minimum OS string to a simctl RuntimeVersion integer.

  Args:
    minimum_os: A string in the form '12.2' or '13.2.3'.

  Returns:
    An integer in the form 0xAABBCC, where AA is the major version, BB is
    the minor version, and CC is the micro version.
  """
  # Pad the minimum OS version to major.minor.micro.
  minimum_os_components = (minimum_os.split(".") + ["0"] * 3)[:3]
  result = 0
  for component in minimum_os_components:
    result = (result << 8) | int(component)
  return result


def discover_best_compatible_simulator(
    *,
    platform_type: str,
    simctl_path: str,
    minimum_os: str,
    sim_device: str,
    sim_os_version: str,
) -> (Optional[DeviceType], Optional[Device]):
  """Discovers the best compatible simulator device type and device.

  Args:
    platform_type: The Apple platform type for the given *_application() target.
    simctl_path: The path to the `simctl` binary.
    minimum_os: The minimum OS version required by the *_application() target.
    sim_device: Optional name of the device (e.g. "iPhone 8 Plus").
    sim_os_version: Optional version of the Apple platform runtime (e.g.
      "13.2").

  Returns:
    A tuple (device_type, device) containing the DeviceType and Device
    of the best compatible simulator (might be None if no match was found).

  Raises:
    subprocess.SubprocessError: if `simctl list` fails or times out.
  """
  # The `simctl list` CLI provides only very basic case-insensitive description
  # matching search term functionality.
  #
  # This code needs to enforce a numeric floor on `minimum_os`, so it directly
  # parses the JSON output by `simctl list` instead of repeatedly invoking
  # `simctl list` with search terms.
  cmd = [simctl_path, "list", "-j"]
  with subprocess.Popen(cmd, stdout=subprocess.PIPE) as process:
    simctl_data = json.load(process.stdout)
    if process.wait() != os.EX_OK:
      raise subprocess.CalledProcessError(process.returncode, cmd)
  compatible_device_types = []
  minimum_runtime_version = minimum_os_to_simctl_runtime_version(minimum_os)
  # Prepare the device name for case-insensitive matching.
  sim_device = sim_device and sim_device.casefold()
  # `simctl list` orders device types from oldest to newest. Remember
  # the index of each device type to preserve that ordering when
  # sorting device types.
  for simctl_list_index, device_type in enumerate(simctl_data["devicetypes"]):
    device_type = DeviceType(device_type, simctl_list_index)
    if not device_type.supports_platform_type(platform_type):
      continue
    # Some older simulators are missing `maxRuntimeVersion`. Assume those
    # simulators support all OSes (even though it's not true).
    max_runtime_version = device_type.get("maxRuntimeVersion")
    if max_runtime_version and max_runtime_version < minimum_runtime_version:
      continue
    if sim_device and device_type["name"].casefold().find(sim_device) == -1:
      continue
    compatible_device_types.append(device_type)
  compatible_device_types.sort()
  logger.debug(
      "Found %d compatible device types.", len(compatible_device_types)
  )
  compatible_runtime_identifiers = set()
  for runtime in simctl_data["runtimes"]:
    if not runtime["isAvailable"]:
      continue
    if sim_os_version and runtime["version"] != sim_os_version:
      continue
    compatible_runtime_identifiers.add(runtime["identifier"])
  compatible_devices = []
  for runtime_identifier, devices in simctl_data["devices"].items():
    if runtime_identifier not in compatible_runtime_identifiers:
      continue
    for device in devices:
      if not device["isAvailable"]:
        continue
      compatible_device = None
      for device_type in compatible_device_types:
        if device["deviceTypeIdentifier"] == device_type["identifier"]:
          compatible_device = Device(device, device_type)
          break
      if not compatible_device:
        continue
      compatible_devices.append(compatible_device)
  compatible_devices.sort()
  logger.debug("Found %d compatible devices.", len(compatible_devices))
  if compatible_device_types:
    best_compatible_device_type = compatible_device_types[-1]
  else:
    best_compatible_device_type = None
  if compatible_devices:
    best_compatible_device = compatible_devices[-1]
  else:
    best_compatible_device = None
  return (best_compatible_device_type, best_compatible_device)


def persistent_simulator(
    *,
    platform_type: str,
    simctl_path: str,
    minimum_os: str,
    sim_device: str,
    sim_os_version: str,
) -> str:
  """Finds or creates a persistent compatible Apple simulator.

  Boots the simulator if needed. Does not shut down or delete the simulator when
  done.

  Args:
    platform_type: The Apple platform type for the given *_application() target.
    simctl_path: The path to the `simctl` binary.
    minimum_os: The minimum OS version required by the *_application() target.
    sim_device: Optional name of the device (e.g. "iPhone 8 Plus").
    sim_os_version: Optional version of the Apple platform runtime (e.g.
      "13.2").

  Returns:
    The UDID of the compatible Apple simulator.

  Raises:
    Exception: if a compatible simulator was not found.
  """
  (best_compatible_device_type, best_compatible_device) = (
      discover_best_compatible_simulator(
          platform_type=platform_type,
          simctl_path=simctl_path,
          minimum_os=minimum_os,
          sim_device=sim_device,
          sim_os_version=sim_os_version,
      )
  )
  if best_compatible_device:
    udid = best_compatible_device["udid"]
    if best_compatible_device.is_shutdown():
      logger.debug("Booting compatible device: %s", best_compatible_device)
      subprocess.run([simctl_path, "boot", udid], check=True)
    else:
      logger.debug("Using compatible device: %s", best_compatible_device)
    return udid
  if best_compatible_device_type:
    device_name = best_compatible_device_type["name"]
    device_id = best_compatible_device_type["identifier"]
    logger.info("Creating new %s simulator", device_name)
    create_result = subprocess.run(
        [simctl_path, "create", device_name, device_id],
        encoding="utf-8",
        stdout=subprocess.PIPE,
        check=True,
    )
    udid = create_result.stdout.rstrip()
    logger.debug("Created new simulator: %s", udid)
    return udid
  raise Exception(
      f"Could not find or create a simulator for the {platform_type} platform"
      f"compatible with minimum OS version {minimum_os} (device name "
      f"{sim_device}, OS version {sim_os_version})"
  )


def wait_for_sim_to_boot(simctl_path: str, udid: str) -> bool:
  """Blocks until the given simulator is booted.

  Args:
    simctl_path: The path to the `simctl` binary.
    udid: The identifier of the simulator to wait for.

  Returns:
    True if the simulator boots within 60 seconds, False otherwise.
  """
  logger.info("Waiting for simulator to boot...")
  for _ in range(0, 60):
    # The expected output of "simctl list" is like:
    # -- iOS 8.4 --
    # iPhone 5s (E946FA1C-26AB-465C-A7AC-24750D520BEA) (Shutdown)
    # TestDevice (8491C4BC-B18E-4E2D-934A-54FA76365E48) (Booted)
    # So if there's any booted simulator, $booted_device will not be empty.
    simctl_list_result = subprocess.run(
        [simctl_path, "list", "devices"],
        encoding="utf-8",
        check=True,
        stdout=subprocess.PIPE,
    )
    for line in simctl_list_result.stdout.split("\n"):
      if line.find(udid) != -1 and line.find("Booted") != -1:
        logger.debug("Simulator is booted.")
        # Simulator is booted.
        return True
    logger.debug("Simulator not booted, still waiting...")
    time.sleep(1)
  return False


def boot_simulator(*, developer_path: str, simctl_path: str, udid: str) -> None:
  """Launches the Apple simulator for the given identifier.

  Ensures the Simulator process is in the foreground.

  Args:
    developer_path: The path to /Applications/Xcode.app/Contents/Developer.
    simctl_path: The path to the `simctl` binary.
    udid: The identifier of the simulator to wait for.

  Raises:
    Exception: if the simulator did not launch within 60 seconds.
  """
  logger.info("Launching simulator with udid: %s", udid)
  # Using subprocess.Popen() to launch Simulator.app and then
  # `osascript -e "tell application \"Simulator\" to activate" is racy
  # and can fail with:
  #
  #   Simulator got an error: Connection is invalid. (-609)
  #
  # This is likely because the newly-spawned Simulator.app process
  # hasn't had time to connect to the Apple Events system which
  # `osascript` relies on.
  simulator_path = os.path.join(developer_path, "Applications/Simulator.app")
  subprocess.run(
      ["open", "-a", simulator_path, "--args", "-CurrentDeviceUDID", udid],
      check=True,
  )
  logger.debug("Simulator launched.")
  if not wait_for_sim_to_boot(simctl_path, udid):
    raise Exception("Failed to launch simulator with UDID: " + udid)


@contextlib.contextmanager
def temporary_simulator(
    *, platform_type: str, simctl_path: str, device: str, version: str
) -> AppleSimulatorUDID:
  """Creates a temporary Apple simulator, cleaned up automatically upon close.

  Args:
    platform_type: The Apple platform type for the given *_application() target.
    simctl_path: The path to the `simctl` binary.
    device: The name of the device (e.g. "iPhone 8 Plus").
    version: The version of the Apple platform runtime (e.g. "13.2").

  Yields:
    The UDID of the newly-created Apple simulator.
  """
  runtime_version_name = version.replace(".", "-")
  # capitalizes 'os' from Apple platform type string (e.g. watchos -> watchOS)
  runtime_platform = platform_type[0:-2].lower() + platform_type[-2:].upper()
  logger.info("Creating simulator, device=%s, version=%s", device, version)
  simctl_create_result = subprocess.run(
      [
          simctl_path,
          "create",
          "TestDevice",
          device,
          "{prefix}.{runtime_platform}-{runtime_version_name}".format(
              prefix="com.apple.CoreSimulator.SimRuntime",
              runtime_platform=runtime_platform,
              runtime_version_name=runtime_version_name,
          ),
      ],
      encoding="utf-8",
      check=True,
      stdout=subprocess.PIPE,
  )
  udid = simctl_create_result.stdout.rstrip()
  try:
    logger.info("Killing all running simulators...")
    subprocess.run(
        ["pkill", "Simulator"], stderr=subprocess.DEVNULL, check=False
    )
    yield udid
  finally:
    logger.info("Shutting down simulator with udid: %s", udid)
    subprocess.run(
        [simctl_path, "shutdown", udid], stderr=subprocess.DEVNULL, check=False
    )
    logger.info("Deleting simulator with udid: %s", udid)
    subprocess.run([simctl_path, "delete", udid], check=True)


def register_dsyms(dsyms_dir: str):
  """Adds all dSYMs in `dsyms_dir` to the symbolscache.

  Args:
    dsyms_dir: Path to directory potentially containing dSYMs
  """
  symbolscache_command = [
      "/usr/bin/symbolscache",
      "delete",
      "--tag",
      "Bazel",
      "compact",
      "add",
      "--tag",
      "Bazel",
  ] + [
      a
      for a in pathlib.Path(dsyms_dir).glob(
          "**/*.dSYM/Contents/Resources/DWARF/*"
      )
  ]
  logger.debug("Running command: %s", symbolscache_command)
  result = subprocess.run(
      symbolscache_command,
      capture_output=True,
      check=True,
      encoding="utf-8",
      text=True,
  )
  logger.debug("symbolscache output: %s", result.stdout)


@contextlib.contextmanager
def extracted_app(
    application_output_path: str, app_name: str
) -> AppleSimulatorUDID:
  """Extracts Foo.app from *_application() output and makes it writable.

  Args:
    application_output_path: Path to the output of an `*_application()`. If the
      path is a directory, copies it to a temporary directory and makes the
      contents writable, as `simctl install` fails to install an `.app` that is
      read-only. If the path is an .ipa archive, unzips it to a temporary
      directory.
    app_name: The name of the application (e.g. "Foo" for "Foo.app").

  Yields:
    Path to Foo.app in temporary directory (re-used if already present).
  """
  if os.path.isdir(application_output_path):
    # Re-use the same path for each run and rsync to it (reducing
    # copies). Ensure the result is writable, or `simctl install` will
    # fail with `Unhandled error domain NSPOSIXErrorDomain, code 13`.
    dst_dir = os.path.join(tempfile.gettempdir(), "bazel_temp_" + app_name)
    os.makedirs(dst_dir, exist_ok=True)
    rsync_command = [
        "/usr/bin/rsync",
        "--archive",
        "--delete",
        "--checksum",
        "--chmod=u+w",
        "--verbose",
        # The output path might itself be a symlink; resolve to the
        # real path so rsync doesn't just copy the symlink.
        os.path.realpath(application_output_path),
        dst_dir,
    ]
    logger.debug(
        "Found app directory: %s, running command: %s",
        application_output_path,
        rsync_command,
    )
    result = subprocess.run(
        rsync_command,
        capture_output=True,
        check=True,
        encoding="utf-8",
        text=True,
    )
    logger.debug("rsync output: %s", result.stdout)
    yield os.path.join(dst_dir, app_name + ".app")
  else:
    # Create a new temporary directory for each run, deleting it
    # afterwards (there's no efficient way to "sync" an unzip, so this
    # can't re-use the output directory).
    with tempfile.TemporaryDirectory(prefix="bazel_temp") as temp_dir:
      logger.debug(
          "Unzipping IPA from %s to %s", application_output_path, temp_dir
      )
      with zipfile.ZipFile(application_output_path) as ipa_zipfile:
        ipa_zipfile.extractall(temp_dir)
        yield os.path.join(temp_dir, "Payload", app_name + ".app")


def bundle_id(bundle_path: str) -> str:
  """Returns the bundle ID given a bundle directory path."""
  info_plist_path = os.path.join(bundle_path, "Info.plist")
  with open(info_plist_path, mode="rb") as plist_file:
    plist = plistlib.load(plist_file)
    return plist["CFBundleIdentifier"]


def simctl_launch_environ() -> Dict[str, str]:
  """Calculates an environment dictionary for running `simctl launch`."""
  # Pass environment variables prefixed with "IOS_" to the simulator, replace
  # the prefix with "SIMCTL_CHILD_". bazel adds "IOS_" to the env vars which
  # will be passed to the app as prefix to differentiate from other env vars. We
  # replace the prefix "IOS_" with "SIMCTL_CHILD_" here, because "simctl" only
  # pass the env vars prefixed with "SIMCTL_CHILD_" to the app.
  result = {}
  for k, v in os.environ.items():
    if not k.startswith("IOS_"):
      continue
    new_key = k.replace("IOS_", "SIMCTL_CHILD_", 1)
    result[new_key] = v
  if "IDE_DISABLED_OS_ACTIVITY_DT_MODE" not in os.environ:
    # Ensure os_log() mirrors writes to stderr. (lldb and Xcode set this
    # environment variable as well.)
    result["SIMCTL_CHILD_OS_ACTIVITY_DT_MODE"] = "enable"
  return result


@contextlib.contextmanager
def apple_simulator(
    *,
    platform_type: str,
    simctl_path: str,
    minimum_os: str,
    sim_device: str,
    sim_os_version: str,
) -> AppleSimulatorUDID:
  """Finds either a temporary or persistent Apple simulator based on args.

  Args:
    platform_type: The Apple platform type for the given *_application() target.
    simctl_path: The path to the `simctl` binary.
    minimum_os: The minimum OS version required by the *_application() target.
    sim_device: Optional name of the device (e.g. "iPhone 8 Plus").
    sim_os_version: Optional version of the Apple platform runtime (e.g.
      "13.2").

  Yields:
    The UDID of the simulator.
  """
  if sim_device and sim_os_version:
    with temporary_simulator(
        platform_type=platform_type,
        simctl_path=simctl_path,
        device=sim_device,
        version=sim_os_version,
    ) as udid:
      yield udid
  else:
    yield persistent_simulator(
        platform_type=platform_type,
        simctl_path=simctl_path,
        minimum_os=minimum_os,
        sim_device=sim_device,
        sim_os_version=sim_os_version,
    )


def run_app_in_simulator(
    *,
    simulator_udid: str,
    developer_path: str,
    simctl_path: str,
    application_output_path: str,
    app_name: str,
) -> None:
  """Installs and runs an app in the specified simulator.

  Args:
    simulator_udid: The UDID of the simulator in which to run the app.
    developer_path: The path to /Applications/Xcode.app/Contents/Developer.
    simctl_path: The path to the `simctl` binary.
    application_output_path: Path to the output of an `*_application()`.
    app_name: The name of the application (e.g. "Foo" for "Foo.app").
  """
  boot_simulator(
      developer_path=developer_path,
      simctl_path=simctl_path,
      udid=simulator_udid,
  )
  root_dir = os.path.dirname(application_output_path)
  register_dsyms(root_dir)
  with extracted_app(application_output_path, app_name) as app_path:
    logger.debug("Installing app %s to simulator %s", app_path, simulator_udid)
    subprocess.run(
        [simctl_path, "install", simulator_udid, app_path], check=True
    )
    app_bundle_id = bundle_id(app_path)
    logger.info(
        "Launching app %s in simulator %s", app_bundle_id, simulator_udid
    )
    args = [
        simctl_path,
        "launch",
        "--console-pty",
        simulator_udid,
        app_bundle_id,
    ]
    # Append optional launch arguments.
    args.extend(sys.argv[1:])
    subprocess.run(args, env=simctl_launch_environ(), check=False)


def main(
    *,
    app_name: str,
    application_output_path: str,
    minimum_os: str,
    platform_type: str,
    sim_device: str,
    sim_os_version: str,
):
  """Main entry point to `bazel run` for *_application() targets.

  Args:
    app_name: The name of the application (e.g. "Foo" for "Foo.app").
    application_output_path: Path to the output of an *_application().
    minimum_os: The minimum OS version required by the *_application() target.
    platform_type: The Apple platform type for the given *_application() target.
    sim_device: The name of the device (e.g. "iPhone 8 Plus").
    sim_os_version: The version of the Apple platform runtime (e.g. "13.2").
  """
  xcode_select_result = subprocess.run(
      ["xcode-select", "-p"],
      encoding="utf-8",
      check=True,
      stdout=subprocess.PIPE,
  )
  developer_path = xcode_select_result.stdout.rstrip()
  simctl_path = os.path.join(developer_path, "usr", "bin", "simctl")

  with apple_simulator(
      platform_type=platform_type,
      simctl_path=simctl_path,
      minimum_os=minimum_os,
      sim_device=sim_device,
      sim_os_version=sim_os_version,
  ) as simulator_udid:
    run_app_in_simulator(
        simulator_udid=simulator_udid,
        developer_path=developer_path,
        simctl_path=simctl_path,
        application_output_path=application_output_path,
        app_name=app_name,
    )


if __name__ == "__main__":
  try:
    # Template values filled in by rules_apple/apple/internal/run_support.bzl.
    main(
        app_name="%app_name%",
        application_output_path="%ipa_file%",
        minimum_os="%minimum_os%",
        platform_type="%platform_type%",
        sim_device="%sim_device%",
        sim_os_version="%sim_os_version%",
    )
  except subprocess.CalledProcessError as e:
    logger.error("%s exited with error code %d", e.cmd, e.returncode)
  except KeyboardInterrupt:
    pass
