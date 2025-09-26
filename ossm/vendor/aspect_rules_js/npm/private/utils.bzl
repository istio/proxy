"Utility functions for npm rules"

load("@aspect_bazel_lib//lib:utils.bzl", "is_bazel_6_or_greater")
load("@aspect_bazel_lib//lib:paths.bzl", "relative_file")
load("@aspect_bazel_lib//lib:repo_utils.bzl", "repo_utils")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//lib:types.bzl", "types")
load(":yaml.bzl", _parse_yaml = "parse")

INTERNAL_ERROR_MSG = "ERROR: rules_js internal error, please file an issue: https://github.com/aspect-build/rules_js/issues"
DEFAULT_REGISTRY_PROTOCOL = "https"
DEFAULT_EXTERNAL_REPOSITORY_ACTION_CACHE = ".aspect/rules/external_repository_action_cache"

def _sanitize_string(string):
    # Workspace names may contain only A-Z, a-z, 0-9, '-', '_' and '.'
    result = ""
    for i in range(0, len(string)):
        c = string[i]
        if c == "@" and (not result or result[-1] == "_"):
            result += "at"
        if not c.isalnum() and c != "-" and c != "_" and c != ".":
            c = "_"
        result += c
    return result

def _bazel_name(name, version = None):
    "Make a bazel friendly name from a package name and (optionally) a version that can be used in repository and target names"
    escaped_name = _sanitize_string(name)
    if not version:
        return escaped_name
    version_segments = version.split("_")
    escaped_version = _sanitize_string(version_segments[0])
    peer_version = "_".join(version_segments[1:])
    if peer_version:
        escaped_version = "%s__%s" % (escaped_version, _sanitize_string(peer_version))
    return "%s__%s" % (escaped_name, escaped_version)

def _strip_peer_dep_or_patched_version(version):
    "Remove peer dependency or patched syntax from version string"

    # 21.1.0_rollup@2.70.2 becomes 21.1.0
    # 1.0.0_o3deharooos255qt5xdujc3cuq becomes 1.0.0
    index = version.find("_")
    if index != -1:
        return version[:index]
    return version

def _pnpm_name(name, version):
    "Make a name/version pnpm-style name for a package name and version"
    return "%s/%s" % (name, version)

def _parse_pnpm_package_key(pnpm_name, pnpm_version):
    if pnpm_version.startswith("link:") or pnpm_version.startswith("file:"):
        return pnpm_name, "0.0.0"

    if not pnpm_version.startswith("/"):
        if not pnpm_name:
            fail("parse_pnpm_package_key: pnpm_name is empty for non-versioned package %s" % pnpm_version)

        return pnpm_name, pnpm_version

    # Parse a package key such as:
    #    /name/version
    #    /@scope/name/version
    #    registry.com/name/version
    #
    # return a (name, version) tuple. This format is found in pnpm lock file v5.
    _, pnpm_version = pnpm_version.split("/", 1)

    segments = pnpm_version.rsplit("/", 1)
    if len(segments) != 2:
        msg = "unexpected pnpm versioned name {}".format(pnpm_version)
        fail(msg)
    return (segments[0], segments[1])

def _convert_pnpm_v6_version_peer_dep(version):
    # Covert a pnpm lock file v6 version string of the format
    # version(@scope/peer@version)(@scope/peer@version)
    # to a version_peer_version that is compatible with rules_js.
    if version[-1] == ")":
        # There is a peer dep if the string ends with ")"
        peer_dep_index = version.find("(")
        peer_dep = version[peer_dep_index:]
        if len(peer_dep) > 32:
            # Prevent long paths. The pnpm lockfile v6 no longer hashes long sequences of
            # peer deps so we must hash here to prevent extremely long file paths that lead to
            # "File name too long) build failures.
            peer_dep = "_" + _hash(peer_dep)
        version = version[0:peer_dep_index] + _sanitize_string(peer_dep)
        version = version.rstrip("_")
    return version

def _convert_pnpm_v6_package_name(package_name):
    # Covert a pnpm lock file v6 name/version string of the format
    # @scope/name@version(@scope/name@version)(@scope/name@version)
    # to a @scope/name/version_peer_version that is compatible with rules_js.
    if package_name.startswith("/"):
        package_name = _convert_pnpm_v6_version_peer_dep(package_name)
        segments = package_name.rsplit("@", 1)
        if len(segments) != 2:
            msg = "unexpected pnpm versioned name {}".format(package_name)
            fail(msg)
        return "%s/%s" % (segments[0], segments[1])
    else:
        return _convert_pnpm_v6_version_peer_dep(package_name)

def _convert_v6_importers(importers):
    # Convert pnpm lockfile v6 importers to a rules_js compatible format.
    result = {}
    for import_path, importer in importers.items():
        result[import_path] = {}
        for key in ["dependencies", "optionalDependencies", "devDependencies"]:
            deps = importer.get(key, None)
            if deps != None:
                result[import_path][key] = {}
                for name, attributes in deps.items():
                    result[import_path][key][name] = _convert_pnpm_v6_package_name(attributes.get("version"))
    return result

def _convert_v6_packages(packages):
    # Convert pnpm lockfile v6 importers to a rules_js compatible format.
    result = {}
    for package, package_info in packages.items():
        # dependencies
        dependencies = {}
        for dep_name, dep_version in package_info.get("dependencies", {}).items():
            dependencies[dep_name] = _convert_pnpm_v6_package_name(dep_version)
        package_info["dependencies"] = dependencies

        # optionalDependencies
        optional_dependencies = {}
        for dep_name, dep_version in package_info.get("optionalDependencies", {}).items():
            optional_dependencies[dep_name] = _convert_pnpm_v6_package_name(dep_version)
        package_info["optionalDependencies"] = optional_dependencies
        result[_convert_pnpm_v6_package_name(package)] = package_info
    return result

def _parse_pnpm_lock_yaml(content):
    """Parse the content of a pnpm-lock.yaml file.

    Args:
        content: lockfile content

    Returns:
        A tuple of (importers dict, packages dict, patched_dependencies dict, error string)
    """
    parsed, err = _parse_yaml(content)
    return _parse_pnpm_lock_common(parsed, err)

def _parse_pnpm_lock_json(content):
    """Parse the content of a pnpm-lock.yaml file.

    Args:
        content: lockfile content as json

    Returns:
        A tuple of (importers dict, packages dict, patched_dependencies dict, error string)
    """
    return _parse_pnpm_lock_common(json.decode(content) if content else None, None)

def _parse_pnpm_lock_common(parsed, err):
    """Helper function used by _parse_pnpm_lock_yaml and _parse_pnpm_lock_json.

    Args:
        parsed: lockfile content object
        err: any errors from pasring

    Returns:
        A tuple of (importers dict, packages dict, patched_dependencies dict, error string)
    """
    if err != None or parsed == None or parsed == {}:
        return {}, {}, {}, err

    if not types.is_dict(parsed):
        return {}, {}, {}, "lockfile should be a starlark dict"
    if "lockfileVersion" not in parsed.keys():
        return {}, {}, {}, "expected lockfileVersion key in lockfile"

    # Lockfile version may be a float such as 5.4 or a string such as '6.0'
    lockfile_version = str(parsed["lockfileVersion"])
    lockfile_version = lockfile_version.lstrip("'")
    lockfile_version = lockfile_version.rstrip("'")
    lockfile_version = lockfile_version.lstrip("\"")
    lockfile_version = lockfile_version.rstrip("\"")
    lockfile_version = float(lockfile_version)
    _assert_lockfile_version(lockfile_version)

    importers = parsed.get("importers", {
        ".": {
            "dependencies": parsed.get("dependencies", {}),
            "optionalDependencies": parsed.get("optionalDependencies", {}),
            "devDependencies": parsed.get("devDependencies", {}),
        },
    })

    packages = parsed.get("packages", {})

    if lockfile_version >= 6.0:
        # special handling for lockfile v6 which had breaking changes
        importers = _convert_v6_importers(importers)
        packages = _convert_v6_packages(packages)

    patched_dependencies = parsed.get("patchedDependencies", {})

    return importers, packages, patched_dependencies, None

def _assert_lockfile_version(version, testonly = False):
    if type(version) != type(1.0):
        fail("version should be passed as a float")

    # Restrict the supported lock file versions to what this code has been tested with:
    #   5.3 - pnpm v6.x.x
    #   5.4 - pnpm v7.0.0 bumped the lockfile version to 5.4
    #   6.0 - pnpm v8.0.0 bumped the lockfile version to 6.0; this included breaking changes
    #   6.1 - pnpm v8.6.0 bumped the lockfile version to 6.1
    min_lock_version = 5.3
    max_lock_version = 6.1
    msg = None

    if version < min_lock_version:
        msg = "npm_translate_lock requires lock_version at least {min}, but found {actual}. Please upgrade to pnpm v6 or greater.".format(
            min = min_lock_version,
            actual = version,
        )
    if version > max_lock_version:
        msg = "npm_translate_lock currently supports a maximum lock_version of {max}, but found {actual}. Please file an issue on rules_js".format(
            max = max_lock_version,
            actual = version,
        )
    if msg and not testonly:
        fail(msg)
    return msg

def _friendly_name(name, version):
    "Make a name@version developer-friendly name for a package name and version"
    return "%s@%s" % (name, version)

def _virtual_store_name(name, version):
    "Make a virtual store name for a given package and version"
    if version.startswith("@"):
        # Special case where the package name should _not_ be included in the virtual store name.
        # See https://github.com/aspect-build/rules_js/issues/423 for more context.
        return version.replace("/", "+")
    else:
        escaped_name = name.replace("/", "+")
        escaped_version = version.replace("/", "+")
        return "%s@%s" % (escaped_name, escaped_version)

def _make_symlink(ctx, symlink_path, target_file):
    files = []
    if ctx.attr.use_declare_symlink:
        symlink = ctx.actions.declare_symlink(symlink_path)
        ctx.actions.symlink(
            output = symlink,
            target_path = relative_file(target_file.path, symlink.path),
        )
        files.append(target_file)
    else:
        if _is_at_least_bazel_6() and target_file.is_directory:
            # BREAKING CHANGE in Bazel 6 requires you to use declare_directory if your target_file
            # in ctx.actions.symlink is a directory artifact
            symlink = ctx.actions.declare_directory(symlink_path)
        else:
            symlink = ctx.actions.declare_file(symlink_path)
        ctx.actions.symlink(
            output = symlink,
            target_file = target_file,
        )
    files.append(symlink)
    return files

def _is_at_least_bazel_6():
    # Hacky way to check if the we're using at least Bazel 6. Would be nice if there was a ctx.bazel_version instead.
    # native.bazel_version only works in repository rules.
    return "apple_binary" not in dir(native)

def _parse_package_name(package):
    # Parse a @scope/name string and return a (scope, name) tuple
    segments = package.split("/", 1)
    if len(segments) == 2 and segments[0].startswith("@"):
        return (segments[0], segments[1])
    return ("", segments[0])

def _npm_registry_url(package, registries, default_registry):
    (package_scope, _) = _parse_package_name(package)

    return registries[package_scope] if package_scope in registries else default_registry

def _npm_registry_download_url(package, version, registries, default_registry):
    "Make a registry download URL for a given package and version"

    (_, package_name_no_scope) = _parse_package_name(package)
    registry = _npm_registry_url(package, registries, default_registry)

    return "{0}/{1}/-/{2}-{3}.tgz".format(
        registry.removesuffix("/"),
        package,
        package_name_no_scope,
        _strip_peer_dep_or_patched_version(version),
    )

def _is_git_repository_url(url):
    return url.startswith("git+ssh://") or url.startswith("git+https://") or url.startswith("git@")

def _to_registry_url(url):
    return "{}://{}".format(DEFAULT_REGISTRY_PROTOCOL, url) if url.find("//") == -1 else url

def _default_registry():
    return _to_registry_url("registry.npmjs.org/")

def _hash(s):
    # Bazel's hash() resolves to a 32-bit signed integer [-2,147,483,648 to 2,147,483,647].
    # NB: There has been discussion of adding a sha256 built-in hash function to Starlark but no
    # work has been done to date.
    # See https://github.com/bazelbuild/starlark/issues/36#issuecomment-1115352085.
    return str(hash(s))

def _dicts_match(a, b):
    if len(a) != len(b):
        return False
    for key in a.keys():
        if not key in b:
            return False
        if a[key] != b[key]:
            return False
    return True

# Generate a consistent label string between Bazel versions.
def _consistent_label_str(label):
    return "//{}:{}".format(
        # Starting in Bazel 6, the workspace name is empty for the local workspace and there's no other way to determine it.
        # This behavior differs from Bazel 5 where the local workspace name was fully qualified in str(label).
        label.package,
        label.name,
    )

# Copies a file from the external repository to the same relative location in the source tree
def _reverse_force_copy(rctx, label, dst = None):
    if type(label) != "Label":
        fail(INTERNAL_ERROR_MSG)
    dst = dst if dst else str(rctx.path(label))
    src = str(rctx.path(paths.join(label.package, label.name)))
    if repo_utils.is_windows(rctx):
        fail("Not yet implemented for Windows")
        #         rctx.file("_reverse_force_copy.bat", content = """
        # @REM needs a mkdir dirname(%2)
        # xcopy /Y %1 %2
        # """, executable = True)
        #         result = rctx.execute(["cmd.exe", "/C", "_reverse_force_copy.bat", src.replace("/", "\\"), dst.replace("/", "\\")])

    else:
        rctx.file("_reverse_force_copy.sh", content = """#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
mkdir -p $(dirname $2)
cp -f $1 $2
""", executable = True)
        result = rctx.execute(["./_reverse_force_copy.sh", src, dst])
    if result.return_code != 0:
        msg = """

ERROR: failed to copy file from {src} to {dst}:
STDOUT:
{stdout}
STDERR:
{stderr}
""".format(
            src = src,
            dst = dst,
            stdout = result.stdout,
            stderr = result.stderr,
        )
        fail(msg)

# This uses `rctx.execute` to check if the file exists since `rctx.exists` does not exist.
def _exists(rctx, p):
    if type(p) == "Label":
        fail("ERROR: dynamic labels not accepted since they should be converted paths at the top of the repository rule implementation to avoid restarts after rctx.execute() calls")
    p = str(p)
    if repo_utils.is_windows(rctx):
        fail("Not yet implemented for Windows")
        #         rctx.file("_exists.bat", content = """IF EXIST %1 (
        #     EXIT /b 0
        # ) ELSE (
        #     EXIT /b 42
        # )""", executable = True)
        #         result = rctx.execute(["cmd.exe", "/C", "_exists.bat", str(p).replace("/", "\\")])

    else:
        rctx.file("_exists.sh", content = """#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
if [ ! -f $1 ]; then exit 42; fi
""", executable = True)
        result = rctx.execute(["./_exists.sh", str(p)])
    if result.return_code == 0:  # file exists
        return True
    elif result.return_code == 42:  # file does not exist
        return False
    else:
        fail(INTERNAL_ERROR_MSG)

# TODO(2.0): move this to aspect_bazel_lib
def _home_directory(rctx):
    if "HOME" in rctx.os.environ and not repo_utils.is_windows(rctx):
        return rctx.os.environ["HOME"]
    if "USERPROFILE" in rctx.os.environ and repo_utils.is_windows(rctx):
        return rctx.os.environ["USERPROFILE"]
    return None

def _replace_npmrc_token_envvar(token, npmrc_path, environ):
    # A token can be a reference to an environment variable
    if token.startswith("$"):
        # ${NPM_TOKEN} -> NPM_TOKEN
        # $NPM_TOKEN -> NPM_TOKEN
        token = token.removeprefix("$").removeprefix("{").removesuffix("}")
        if token in environ.keys() and environ[token]:
            token = environ[token]
        else:
            # buildifier: disable=print
            print("""
WARNING: Issue while reading "{npmrc}". Failed to replace env in config: ${{{token}}}
""".format(
                npmrc = npmrc_path,
                token = token,
            ))
    return token

def _is_vendored_tarfile(package_snapshot):
    if "resolution" in package_snapshot:
        return "tarball" in package_snapshot["resolution"]
    return False

def _default_external_repository_action_cache():
    return DEFAULT_EXTERNAL_REPOSITORY_ACTION_CACHE

def _is_tarball_extension(ext):
    # Takes an extension (without leading dot) and return True if the extension
    # is a common tarball extension as per
    # https://en.wikipedia.org/wiki/Tar_(computing)#Suffixes_for_compressed_files
    tarball_extensions = [
        "tar",
        "tar.bz2",
        "tb2",
        "tbz",
        "tbz2",
        "tz2",
        "tar.gz",
        "taz",
        "tgz",
        "tar.lz",
        "tar.lzma",
        "tlz",
        "tar.lzo",
        "tar.xz",
        "txz",
        "tar.Z",
        "tZ",
        "taZ",
        "tar.zst",
        "tzst",
    ]
    return ext in tarball_extensions

utils = struct(
    bazel_name = _bazel_name,
    pnpm_name = _pnpm_name,
    assert_lockfile_version = _assert_lockfile_version,
    parse_pnpm_package_key = _parse_pnpm_package_key,
    parse_pnpm_lock_yaml = _parse_pnpm_lock_yaml,
    parse_pnpm_lock_json = _parse_pnpm_lock_json,
    friendly_name = _friendly_name,
    virtual_store_name = _virtual_store_name,
    strip_peer_dep_or_patched_version = _strip_peer_dep_or_patched_version,
    make_symlink = _make_symlink,
    # Symlinked node_modules structure virtual store path under node_modules
    virtual_store_root = ".aspect_rules_js",
    # Suffix for npm_import links repository
    links_repo_suffix = "__links",
    # Output group name for the package directory of a linked package
    package_directory_output_group = "package_directory",
    npm_registry_url = _npm_registry_url,
    npm_registry_download_url = _npm_registry_download_url,
    parse_package_name = _parse_package_name,
    is_git_repository_url = _is_git_repository_url,
    to_registry_url = _to_registry_url,
    default_external_repository_action_cache = _default_external_repository_action_cache,
    default_registry = _default_registry,
    hash = _hash,
    dicts_match = _dicts_match,
    consistent_label_str = _consistent_label_str,
    bzlmod_supported = is_bazel_6_or_greater(),
    reverse_force_copy = _reverse_force_copy,
    exists = _exists,
    home_directory = _home_directory,
    replace_npmrc_token_envvar = _replace_npmrc_token_envvar,
    is_vendored_tarfile = _is_vendored_tarfile,
    is_tarball_extension = _is_tarball_extension,
)
