#!/usr/bin/env python3.11
"""A script for generating the `//rust:known_shas.bzl` file."""

import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path
from typing import Any, Dict, Sequence

KNOWN_SHAS_TEMPLATE = """\
\"\"\"A module containing a mapping of Rust tools to checksums

This is a generated file -- see //util/fetch_shas
\"\"\"

FILE_KEY_TO_SHA = {}
"""


def download_manifest_data(
    stable_versions: Sequence[str], nightly_versions: Sequence[str], output_dir: Path
) -> Dict[str, Sequence[Dict[str, Any]]]:
    """Download and deserialize `channel-rust-*.toml` files for the requested versions

    Args:
        stable_versions: A list of Rust versions. E.g. `1.81.0`
        nightly_versions: A list of nightly iso dates. E.g. `2024-09-05`
        output_dir: The location where the intermediate files should be written.

    Returns:
        A mapping of channel name (`stable`, `nightly`) to deserialized data.
    """

    output_dir.mkdir(exist_ok=True, parents=True)
    curl_config = output_dir / "curl_config.txt"
    curl_config_lines = [
        "--fail",
        "--parallel",
        "--silent",
        "--create-dirs",
    ]

    stable_files = {}
    for version in stable_versions:
        output = output_dir / f"channel-rust-{version}.toml"
        curl_config_lines.extend(
            [
                f"--output {output}",
                f"--url https://static.rust-lang.org/dist/channel-rust-{version}.toml",
            ]
        )
        stable_files[version] = output
    nightly_files = {}
    for version in nightly_versions:
        output = output_dir / version / "channel-rust-nightly.toml"
        curl_config_lines.extend(
            [
                f"--output {output}",
                f"--url https://static.rust-lang.org/dist/{version}/channel-rust-nightly.toml",
            ]
        )
        nightly_files[version] = output

    curl_config.write_text("\n".join(curl_config_lines), encoding="utf-8")

    logging.info("Downloading data...")
    subprocess.run(
        [
            "curl",
            "--config",
            curl_config,
        ],
        check=True,
    )
    logging.info("Done.")

    logging.info(
        "Deserializing %s tomls...",
        len(list(stable_files.keys()) + list(nightly_files.keys())),
    )
    for collection in (stable_files, nightly_files):
        for version, file in collection.items():
            data = file.read_text(encoding="utf-8")
            assert data
            try:
                collection[version] = tomllib.loads(data)
            except:
                logging.error("Failed to parse toml: %s\n%s", file, data)
                raise

    logging.info("Done.")
    return {
        "stable": stable_files,
        "nightly": nightly_files,
    }


def download_direct_sha256s(
    artifacts: Sequence[str], output_dir: Path
) -> Dict[str, str]:
    """_summary_

    This function is mostly here for backward compatibility. There are artifacts
    referenced by the `channel-rust-*.toml` files which are marked as `available: false`
    and probably intented to not be downloaded. But for now this is ignored and instead
    a collection of artifacts whose hash data could not be found is explicitly checked
    by trying to download the `.sha256` files directly. A 404 indicates the artifact
    genuinely does not exist and anything else we find is extra data we can retain
    in `known_shas.bzl`.

    Args:
        artifacts: A list of paths within `https://static.rust-lang.org/dist` to download.
        output_dir: The location where the intermediate files should be written.

    Returns:
        A mapping of `artifacts` entries to their sha256 value.
    """
    output_dir.mkdir(exist_ok=True, parents=True)
    status_config = output_dir / "status" / "curl_config.txt"
    sha256_config = output_dir / "sha256" / "curl_config.txt"
    common_config_lines = [
        "--parallel",
        "--create-dirs",
    ]
    status_config_lines = common_config_lines + [
        "--silent",
        "--head",
    ]

    statuses = {}
    for url_path in artifacts:
        output = status_config.parent / f"{url_path}.sha256.status"
        status_config_lines.extend(
            [
                f"--output {output}",
                f"--url https://static.rust-lang.org/dist/{url_path}.sha256",
            ]
        )
        statuses[url_path] = output

    status_config.parent.mkdir(exist_ok=True, parents=True)
    status_config.write_text("\n".join(status_config_lines), encoding="utf-8")

    logging.info("Checking for %s missing artifacts...", len(statuses))
    result = subprocess.run(
        [
            "curl",
            "--config",
            status_config,
        ],
        check=True,
    )
    logging.info("Done.")

    checksums = {}
    missing = []
    for url_path, status_file in statuses.items():
        if not status_file.exists():
            missing.append(f"https://static.rust-lang.org/dist/{url_path}.sha256")
            continue

        if status_file.read_text(encoding="utf-8").startswith("HTTP/2 404"):
            continue

        checksums[url_path] = sha256_config.parent / f"{url_path}.sha256"

    if missing:
        logging.warning(
            "Status not found for %s artifacts:\n%s",
            len(missing),
            json.dumps(sorted(missing), indent=2),
        )

    logging.info("Downloading %s missing artifacts...", len(checksums))
    sha256_config_lines = common_config_lines + [
        "--verbose",
    ]
    for url_path, output in checksums.items():
        sha256_config_lines.extend(
            [
                f"--output {output}",
                f"--url https://static.rust-lang.org/dist/{url_path}.sha256",
            ]
        )

    sha256_config.parent.mkdir(exist_ok=True, parents=True)
    sha256_config.write_text("\n".join(sha256_config_lines), encoding="utf-8")
    result = subprocess.run(
        [
            "curl",
            "--config",
            sha256_config,
        ],
        encoding="utf-8",
        check=False,
        stderr=subprocess.STDOUT,
        stdout=subprocess.PIPE,
    )

    if result.returncode:
        print(result.stdout, file=sys.stderr)
        sys.exit(result.returncode)

    logging.info("Done.")

    return {
        name: file.read_text(encoding="utf-8").split(" ")[0].strip()
        for name, file in checksums.items()
    }


def load_data(file: Path) -> Sequence[str]:
    """Load a `fetch_shas_*.txt` file

    Args:
        file: The file to load

    Returns:
        A list of the file's contents.
    """
    data = []
    for line in file.read_text(encoding="utf-8").splitlines():
        text = line.strip()
        if not text:
            continue
        data.append(text)

    return sorted(set(data))


def main() -> None:
    """The  main entrypoint."""
    if "BUILD_WORKSPACE_DIRECTORY" in os.environ:
        workspace_dir = Path(os.environ["BUILD_WORKSPACE_DIRECTORY"])
    else:
        workspace_dir = Path(__file__).parent.parent.parent

    logging.basicConfig(
        level=(
            logging.INFO
            if "RULES_RUST_FETCH_SHAS_DEBUG" not in os.environ
            else logging.DEBUG
        ),
        format="%(asctime)s - %(levelname)s - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    logging.info("Fetching known sha256 data...")

    tools = load_data(workspace_dir / "util/fetch_shas/fetch_shas_TOOLS.txt")
    host_tools = load_data(workspace_dir / "util/fetch_shas/fetch_shas_HOST_TOOLS.txt")
    targets = load_data(workspace_dir / "util/fetch_shas/fetch_shas_TARGETS.txt")
    stable_versions = load_data(
        workspace_dir / "util/fetch_shas/fetch_shas_VERSIONS.txt"
    )
    nightly_iso_dates = load_data(
        workspace_dir / "util/fetch_shas/fetch_shas_NIGHTLY_ISO_DATES.txt"
    )

    # Allow this directory to be optionally cleaned up.
    tmp_dir = Path(tempfile.mkdtemp(prefix="rules-rust-fetch-shas-"))
    logging.debug("Temp dir: %s", tmp_dir)
    try:
        manifest_data = download_manifest_data(
            stable_versions=stable_versions,
            nightly_versions=nightly_iso_dates,
            output_dir=tmp_dir,
        )

        file_key_to_sha = {}

        artifacts = []

        logging.info("Parsing artifacts...")
        for channel, versioned_info in manifest_data.items():
            for version, info in versioned_info.items():
                if channel == "stable":
                    tool_tpl = f"{{pkg}}-{version}-{{target}}.{{ext}}"
                    host_tool_tpl = f"{{pkg}}-{version}.{{ext}}"
                else:
                    tool_tpl = f"{version}/{{pkg}}-{channel}-{{target}}.{{ext}}"
                    host_tool_tpl = f"{version}/{{pkg}}-{channel}.{{ext}}"

                # Artifacts are commonly referred to with an internal (to the Rust org) name.
                # In order to correctly evaluate whether or not a particular `channel-rust-*.toml`
                # has an artifact, a mapping is made from what the internal Rust names are to
                # what they're advertised as externally (e.g. what users would use when trying
                # to download files directly).
                renames = {
                    "clippy-preview": "clippy",
                    "llvm-tools-preview": "llvm-tools",
                    "rustfmt-preview": "rustfmt",
                }
                if "renames" in info:
                    for pkg, rename in info["renames"].items():
                        if "to" in rename:
                            renames[rename["to"]] = pkg

                logging.debug("Renames (%s %s): %s", channel, version, renames)
                for pkg, pkg_data in info["pkg"].items():
                    pkg_name = renames.get(pkg, pkg)
                    if pkg_name in tools:
                        tool_template = tool_tpl
                    elif pkg_name in host_tools:
                        tool_template = host_tool_tpl
                    else:
                        logging.debug("Skipping %s %s %s", channel, version, pkg_name)
                        continue

                    drain_targets = list(targets)
                    for target, target_data in pkg_data["target"].items():
                        # One variant of the template has an extra format string field.
                        # Replace is used to account for that.
                        template = tool_template.replace("{target}", target)

                        if target not in drain_targets:
                            continue

                        drain_targets.remove(target)

                        if "hash" in target_data:
                            file_key_to_sha[
                                template.format(pkg=pkg_name, ext="tar.gz")
                            ] = target_data["hash"]
                        if "xz_hash" in target_data:
                            file_key_to_sha[
                                template.format(pkg=pkg_name, ext="tar.xz")
                            ] = target_data["xz_hash"]

                    # If an artifact is not advertised to be available for a particular
                    # target then we track this and see if the sha256 data can be
                    # downloaded directly.
                    for target in drain_targets:
                        # One variant of the template has an extra format string field.
                        # Replace is used to account for that.
                        template = tool_template.replace("{target}", target)

                        # See if we can download the file directly.
                        artifacts.extend(
                            [
                                template.format(pkg=pkg_name, ext="tar.gz"),
                                template.format(pkg=pkg_name, ext="tar.xz"),
                            ]
                        )

        logging.info("Done. Identified %s artifacts.", len(file_key_to_sha))

        # Do a brute force check to find additional sha256 values.
        file_key_to_sha.update(
            download_direct_sha256s(
                artifacts=sorted(set(artifacts)),
                output_dir=tmp_dir / "retries",
            )
        )

    finally:
        if not "RULES_RUST_FETCH_SHAS_DEBUG" in os.environ:
            shutil.rmtree(tmp_dir)

    known_shas_file = workspace_dir / "rust/known_shas.bzl"
    known_shas_file.write_text(
        KNOWN_SHAS_TEMPLATE.format(
            json.dumps(file_key_to_sha, sort_keys=True, indent=4).replace(
                '"\n}', '",\n}'
            )
        )
    )
    logging.info("Done. Wrote %s", known_shas_file.relative_to(workspace_dir))


if __name__ == "__main__":
    main()
