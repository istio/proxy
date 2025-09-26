"Helper utility for working with pnpm lockfile"

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load(":utils.bzl", "utils")

def gather_transitive_closure(packages, package, no_optional, cache = {}):
    """Walk the dependency tree, collecting the transitive closure of dependencies and their versions.

    This is needed to resolve npm dependency cycles.
    Note: Starlark expressly forbids recursion and infinite loops, so we need to iterate over a large range of numbers,
    where each iteration takes one item from the stack, and possibly adds many new items to the stack.

    Args:
        packages: dictionary from pnpm lock
        package: the package to collect deps for
        no_optional: whether to exclude optionalDependencies
        cache: a dictionary of results from previous invocations

    Returns:
        A dictionary of transitive dependencies, mapping package names to dependent versions.
    """
    root_package = packages[package]

    transitive_closure = {}
    transitive_closure[root_package["name"]] = [root_package["version"]]

    stack = [_get_package_info_deps(root_package, no_optional)]
    iteration_max = 999999
    for i in range(0, iteration_max + 1):
        if not len(stack):
            break
        if i == iteration_max:
            msg = "gather_transitive_closure exhausted the iteration limit of {} - please report this issue".format(iteration_max)
            fail(msg)
        deps = stack.pop()
        for name in deps.keys():
            version = deps[name]
            if version[0].isdigit():
                package_key = utils.pnpm_name(name, version)
            elif version.startswith("/"):
                # an aliased dependency
                package_key = version[1:]
                name, version = utils.parse_pnpm_package_key(name, version)
            else:
                package_key = version
            transitive_closure[name] = transitive_closure.get(name, [])
            if version in transitive_closure[name]:
                continue
            transitive_closure[name].append(version)
            if package_key.startswith("link:"):
                # we don't need to drill down through first-party links for the transitive closure since there are no cycles
                # allowed in first-party links
                continue

            if package_key in cache:
                # Already computed for this dep, merge the cached results
                for transitive_name in cache[package_key].keys():
                    transitive_closure[transitive_name] = transitive_closure.get(transitive_name, [])
                    for transitive_version in cache[package_key][transitive_name]:
                        if transitive_version not in transitive_closure[transitive_name]:
                            transitive_closure[transitive_name].append(transitive_version)
            else:
                # Recurse into the next level of dependencies
                stack.append(_get_package_info_deps(packages[package_key], no_optional))

    result = dict()
    for key in sorted(transitive_closure.keys()):
        result[key] = sorted(transitive_closure[key])

    return result

def _get_package_info_deps(package_info, no_optional):
    return package_info["dependencies"] if no_optional else dicts.add(package_info["dependencies"], package_info["optional_dependencies"])

def _gather_package_info(package_path, package_snapshot):
    if package_path.startswith("/"):
        # an aliased dependency
        package = package_path[1:]
        name, version = utils.parse_pnpm_package_key("", package_path)
        friendly_version = utils.strip_peer_dep_or_patched_version(version)
        package_key = package
    elif package_path.startswith("file:") and utils.is_vendored_tarfile(package_snapshot):
        if "name" not in package_snapshot:
            fail("expected package %s to have a name field" % package_path)
        name = package_snapshot["name"]
        package = package_snapshot["name"]
        version = package_path
        if "version" in package_snapshot:
            version = package_snapshot["version"]
        package_key = "{}/{}".format(package, version)
        friendly_version = version
    elif package_path.startswith("file:"):
        package = package_path
        if "name" not in package_snapshot:
            msg = "expected package {} to have a name field".format(package_path)
            fail(msg)
        name = package_snapshot["name"]
        version = package_path
        friendly_version = package_snapshot["version"] if "version" in package_snapshot else version
        package_key = package
    else:
        package = package_path
        if "name" not in package_snapshot:
            msg = "expected package {} to have a name field".format(package_path)
            fail(msg)
        if "version" not in package_snapshot:
            msg = "expected package {} to have a version field".format(package_path)
            fail(msg)
        name = package_snapshot["name"]
        version = package_path
        friendly_version = package_snapshot["version"]
        package_key = package

    if "resolution" not in package_snapshot:
        msg = "package {} has no resolution field".format(package_path)
        fail(msg)
    id = package_snapshot["id"] if "id" in package_snapshot else None
    resolution = package_snapshot["resolution"]

    return package_key, {
        "name": name,
        "id": id,
        "version": version,
        "friendly_version": friendly_version,
        "resolution": resolution,
        "dependencies": package_snapshot.get("dependencies", {}),
        "optional_dependencies": package_snapshot.get("optionalDependencies", {}),
        "dev": package_snapshot.get("dev", False),
        "optional": package_snapshot.get("optional", False),
        "patched": package_snapshot.get("patched", False),
        "has_bin": package_snapshot.get("hasBin", False),
        "requires_build": package_snapshot.get("requiresBuild", False),
    }

def translate_to_transitive_closure(lock_importers, lock_packages, prod = False, dev = False, no_optional = False):
    """Implementation detail of translate_package_lock, converts pnpm-lock to a different dictionary with more data.

    Args:
        lock_importers: lockfile importers dict
        lock_packages: lockfile packages dict
        prod: If true, only install dependencies
        dev: If true, only install devDependencies
        no_optional: If true, optionalDependencies are not installed

    Returns:
        Nested dictionary suitable for further processing in our repository rule
    """

    # All package info mapped by package name
    packages = {}

    # Packages resolved to a different version
    package_version_map = {}

    for package_path, package_snapshot in lock_packages.items():
        package, package_info = _gather_package_info(package_path, package_snapshot)
        packages[package] = package_info

        # tarbal versions
        if package_info["resolution"].get("tarball", None) and package_info["resolution"]["tarball"].startswith("file:"):
            package_version_map[package] = package_info

    # Collect deps of each importer (workspace projects)
    importers = {}
    for importPath in lock_importers.keys():
        lock_importer = lock_importers[importPath]
        prod_deps = {} if dev else lock_importer.get("dependencies", {})
        dev_deps = {} if prod else lock_importer.get("devDependencies", {})
        opt_deps = {} if no_optional else lock_importer.get("optionalDependencies", {})

        deps = dicts.add(prod_deps, opt_deps)
        all_deps = dicts.add(prod_deps, dev_deps, opt_deps)

        # Package versions mapped to alternate versions
        for info in package_version_map.values():
            if info["name"] in deps:
                deps[info["name"]] = info["version"]
            if info["name"] in all_deps:
                all_deps[info["name"]] = info["version"]

        importers[importPath] = {
            # deps this importer should pass on if it is linked as a first-party package; this does
            # not include devDependencies
            "deps": deps,
            # all deps of this importer to link in the node_modules folder of that Bazel package and
            # make available to all build targets; this includes devDependencies
            "all_deps": all_deps,
        }

    # Collect transitive dependencies for each package
    cache = {}
    for package in packages.keys():
        package_info = packages[package]

        package_info["transitive_closure"] = gather_transitive_closure(
            packages,
            package,
            no_optional,
            cache,
        )

        cache[package] = package_info["transitive_closure"]

    return (importers, packages)
