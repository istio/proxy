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

import argparse
import json
import pathlib
from typing import Any, Dict, Set

from python.private.pypi.whl_installer.platform import Platform


def parser(**kwargs: Any) -> argparse.ArgumentParser:
    """Create a parser for the wheel_installer tool."""
    parser = argparse.ArgumentParser(
        **kwargs,
    )
    parser.add_argument(
        "--requirement",
        action="store",
        required=True,
        help="A single PEP508 requirement specifier string.",
    )
    parser.add_argument(
        "--isolated",
        action="store_true",
        help="Whether or not to include the `--isolated` pip flag.",
    )
    parser.add_argument(
        "--extra_pip_args",
        action="store",
        help="Extra arguments to pass down to pip.",
    )
    parser.add_argument(
        "--platform",
        action="extend",
        type=Platform.from_string,
        help="Platforms to target dependencies. Can be used multiple times.",
    )
    parser.add_argument(
        "--pip_data_exclude",
        action="store",
        help="Additional data exclusion parameters to add to the pip packages BUILD file.",
    )
    parser.add_argument(
        "--enable_implicit_namespace_pkgs",
        action="store_true",
        help="Disables conversion of implicit namespace packages into pkg-util style packages.",
    )
    parser.add_argument(
        "--environment",
        action="store",
        help="Extra environment variables to set on the pip environment.",
    )
    parser.add_argument(
        "--download_only",
        action="store_true",
        help="Use 'pip download' instead of 'pip wheel'. Disables building wheels from source, but allows use of "
        "--platform, --python-version, --implementation, and --abi in --extra_pip_args.",
    )
    parser.add_argument(
        "--whl-file",
        type=pathlib.Path,
        help="Extract a whl file to be used within Bazel.",
    )
    return parser


def deserialize_structured_args(args: Dict[str, str]) -> Dict:
    """Deserialize structured arguments passed from the starlark rules.

    Args:
        args: dict of parsed command line arguments
    """
    structured_args = ("extra_pip_args", "pip_data_exclude", "environment")
    for arg_name in structured_args:
        if args.get(arg_name) is not None:
            args[arg_name] = json.loads(args[arg_name])["arg"]
        else:
            args[arg_name] = []
    return args


def get_platforms(args: argparse.Namespace) -> Set:
    """Aggregate platforms into a single set.

    Args:
        args: dict of parsed command line arguments
    """
    platforms = set()
    if args.platform is None:
        return platforms

    platforms.update(args.platform)

    return platforms
