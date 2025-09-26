#!/usr/bin/env python3

import argparse
import sys
import shutil
import os
import plistlib

from lib.logger import Logger
from lib.shell import shell, cp_r
from lib.model import Configuration
from lib.lipo_util import LipoUtil
from lib.dependencies import FRAMEWORK_DEPS, PRIVATE_FRAMEWORK_DEPS, DYLIB_DEPS


class DefaultHelpParser(argparse.ArgumentParser):
    """Argument parser error."""

    def error(self, message):
        sys.stderr.write(f"error: {message}\n")
        self.print_help()
        sys.exit(2)


def main(argv) -> None:
    "Script entrypoint."
    parser = DefaultHelpParser()
    parser.add_argument(
        "--name",
        required=True,
        help="Name for the merged test bundle.",
    )
    parser.add_argument(
        "--platform",
        default="iPhoneOS.platform",
        help="Runtime platform. Default: iPhoneOS.platform",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Output path for merged test bundle.",
    )
    parser.add_argument(
        "--xctest",
        required=True,
        action="append",
        help="Path to xctest archive to bundle.",
    )
    parser.add_argument(
        "--verbose",
        required=False,
        default=False,
        type=bool,
        help="Enable verbose logging to console.",
    )
    args = parser.parse_args()

    # Generator configuration
    xcode_path = shell("xcode-select -p").strip()
    config = Configuration(
        name=args.name,
        xctests=args.xctest,
        platform=args.platform,
        xctrunner_path=args.output,
        xcode_path=xcode_path,
        verbose_logging=args.verbose,
    )

    # Shared logger
    log = Logger(config).get(__name__)

    # Log configuration
    log.info("Bundle: %s", config.xctrunner.app)
    log.info("Platform: %s", config.platform)
    log.info("Xcode: %s", config.xcode.path)
    xctest_names = ", ".join([os.path.basename(x) for x in config.xctests])
    log.info("XCTests: %s", xctest_names)
    log.info("Output: %s", config.xctrunner.path)

    # Copy XCTRunner.app template
    log.info("Copying XCTRunner.app Template: %s", config.xctrunner.path)
    shutil.rmtree(
        config.xctrunner.path, ignore_errors=True
    )  # Clean up any existing bundle
    cp_r(config.xctrunner.template_path, config.xctrunner.path)

    # Rename XCTRunner binary to match the bundle name
    os.rename(
        f"{config.xctrunner.path}/XCTRunner",
        f"{config.xctrunner.path}/{config.xctrunner.name}",
    )

    # Create PlugIns and Frameworks directories
    os.makedirs(f"{config.xctrunner.path}/PlugIns", exist_ok=True)
    os.makedirs(f"{config.xctrunner.path}/Frameworks", exist_ok=True)

    # Move each xctest bundle into PlugIns directory and get
    # architecture info.
    lipo = LipoUtil()
    xctest_archs = []
    for xctest in config.xctests:
        name = os.path.basename(xctest).split(".")[
            0
        ]  # MyTests.__internal__.__test_bundle.extension -> MyTests
        bundle = f"{name}.xctest"  # MyTest.xctest
        plugins = f"{config.xctrunner.path}/PlugIns"

        # Unzip if needed
        if xctest.endswith(".zip"):
            log.debug(
                "Unzipping: %s -> %s/%s",
                xctest,
                plugins,
                bundle,
            )
            shell(f"unzip -q -o {xctest} -d {plugins}/")  # .../PlugIns/MyTest.xctest
            bin_path = f"{plugins}/{bundle}/{name}"  # .../PlugIns/MyTest.xctest/MyTest
        else:
            log.debug("Copying: %s -> %s/%s", xctest, plugins, bundle)
            cp_r(xctest, f"{plugins}/{bundle}/")  # .../Plugins/MyTest.xctest
            bin_path = f"{plugins}/{bundle}/{name}"  # .../Plugins/MyTest.xctest/MyTest

        # Get architecture info for each binary
        log.debug("Lipo: XCTest binary - %s", bin_path)
        archs = lipo.current_archs(bin_path)
        log.debug("Lipo: %s archs: %s)", name, archs)
        xctest_archs.extend(archs)

    archs_to_keep = list(set(xctest_archs))  # unique
    log.info("Bundle Architectures: %s)", archs_to_keep)

    # Remove unwanted architectures from XCTRunner bundle
    lipo.extract_or_thin(
        f"{config.xctrunner.path}/{config.xctrunner.name}", archs_to_keep
    )

    # Update Info.plist with bundle info
    with open(config.xctrunner.info_plist_path, "rb") as content:
        plist = plistlib.load(content)
        plist["CFBundleName"] = config.xctrunner.name
        plist["CFBundleExecutable"] = config.xctrunner.name
        plist["CFBundleIdentifier"] = config.xctrunner.bundle_identifier
        plistlib.dump(plist, open(config.xctrunner.info_plist_path, "wb"))

    # Copy dependencies to the bundle and remove unwanted architectures
    for framework in FRAMEWORK_DEPS:
        log.info("Bundling fwk: %s", framework)
        fwk_path = f"{config.xcode.frameworks_dir}/{framework}"

        # Older Xcode versions may not have some of the frameworks
        if not os.path.exists(fwk_path):
            log.warning("Framework '%s' not available at %s", framework, fwk_path)
            continue

        cp_r(
            fwk_path,
            f"{config.xctrunner.path}/Frameworks/{framework}",
        )
        fwk_binary = framework.replace(".framework", "")
        bin_path = f"{config.xctrunner.path}/Frameworks/{framework}/{fwk_binary}"
        lipo.extract_or_thin(
            bin_path, archs_to_keep
        )  # Strip architectures not in test bundles.

    for framework in PRIVATE_FRAMEWORK_DEPS:
        log.info("Bundling fwk: %s", framework)
        cp_r(
            f"{config.xcode.private_frameworks_dir}/{framework}",
            f"{config.xctrunner.path}/Frameworks/{framework}",
        )
        fwk_binary = framework.replace(".framework", "")
        bin_path = f"{config.xctrunner.path}/Frameworks/{framework}/{fwk_binary}"
        lipo.extract_or_thin(bin_path, archs_to_keep)

    for dylib in DYLIB_DEPS:
        log.info("Bundling dylib: %s", dylib)
        shutil.copy(
            f"{config.xcode.dylib_dir}/{dylib}",
            f"{config.xctrunner.path}/Frameworks/{dylib}",
        )
        lipo.extract_or_thin(
            f"{config.xctrunner.path}/Frameworks/{dylib}", archs_to_keep
        )

    log.info("Output: %s", f"{config.xctrunner.path}")
    log.info("Done.")


if __name__ == "__main__":
    main(sys.argv[1:])
