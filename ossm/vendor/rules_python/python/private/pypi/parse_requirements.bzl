# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Requirements parsing for whl_library creation.

Use cases that the code needs to cover:
* A single requirements_lock file that is used for the host platform.
* Per-OS requirements_lock files that are used for the host platform.
* A target platform specific requirements_lock that is used with extra
  pip arguments with --platform, etc and download_only = True.

In the last case only a single `requirements_lock` file is allowed, in all
other cases we assume that there may be a desire to resolve the requirements
file for the host platform to be backwards compatible with the legacy
behavior.
"""

load("//python/private:normalize_name.bzl", "normalize_name")
load("//python/private:repo_utils.bzl", "repo_utils")
load(":index_sources.bzl", "index_sources")
load(":parse_requirements_txt.bzl", "parse_requirements_txt")
load(":whl_target_platforms.bzl", "select_whls")

def _extract_version(entry):
    """Extract the version part from the requirement string.


    Args:
        entry: {type}`str` The requirement string.
    """
    version_start = entry.find("==")
    if version_start != -1:
        # Extract everything after '==' until the next space or end of the string
        version, _, _ = entry[version_start + 2:].partition(" ")
        return version
    return None

def parse_requirements(
        ctx,
        *,
        requirements_by_platform = {},
        extra_pip_args = [],
        get_index_urls = None,
        evaluate_markers = None,
        logger = None):
    """Get the requirements with platforms that the requirements apply to.

    Args:
        ctx: A context that has .read function that would read contents from a label.
        requirements_by_platform (label_keyed_string_dict): a way to have
            different package versions (or different packages) for different
            os, arch combinations.
        extra_pip_args (string list): Extra pip arguments to perform extra validations and to
            be joined with args found in files.
        get_index_urls: Callable[[ctx, list[str]], dict], a callable to get all
            of the distribution URLs from a PyPI index. Accepts ctx and
            distribution names to query.
        evaluate_markers: A function to use to evaluate the requirements.
            Accepts the ctx and a dict where keys are requirement lines to
            evaluate against the platforms stored as values in the input dict.
            Returns the same dict, but with values being platforms that are
            compatible with the requirements line.
        logger: repo_utils.logger or None, a simple struct to log diagnostic messages.

    Returns:
        {type}`dict[str, list[struct]]` where the key is the distribution name and the struct
        contains the following attributes:
         * `distribution`: {type}`str` The non-normalized distribution name.
         * `srcs`: {type}`struct` The parsed requirement line for easier Simple
           API downloading (see `index_sources` return value).
         * `target_platforms`: {type}`list[str]` Target platforms that this package is for.
             The format is `cp3{minor}_{os}_{arch}`.
         * `is_exposed`: {type}`bool` `True` if the package should be exposed via the hub
           repository.
         * `extra_pip_args`: {type}`list[str]` pip args to use in case we are
           not using the bazel downloader to download the archives. This should
           be passed to {obj}`whl_library`.
         * `whls`: {type}`list[struct]` The list of whl entries that can be
           downloaded using the bazel downloader.
         * `sdist`: {type}`list[struct]` The sdist that can be downloaded using
           the bazel downloader.

        The second element is extra_pip_args should be passed to `whl_library`.
    """
    evaluate_markers = evaluate_markers or (lambda *_: {})
    options = {}
    requirements = {}
    for file, plats in requirements_by_platform.items():
        if logger:
            logger.debug(lambda: "Using {} for {}".format(file, plats))
        contents = ctx.read(file)

        # Parse the requirements file directly in starlark to get the information
        # needed for the whl_library declarations later.
        parse_result = parse_requirements_txt(contents)

        # Replicate a surprising behavior that WORKSPACE builds allowed:
        # Defining a repo with the same name multiple times, but only the last
        # definition is respected.
        # The requirement lines might have duplicate names because lines for extras
        # are returned as just the base package name. e.g., `foo[bar]` results
        # in an entry like `("foo", "foo[bar] == 1.0 ...")`.
        requirements_dict = {
            (normalize_name(entry[0]), _extract_version(entry[1])): entry
            for entry in sorted(
                parse_result.requirements,
                # Get the longest match and fallback to original WORKSPACE sorting,
                # which should get us the entry with most extras.
                #
                # FIXME @aignas 2024-05-13: The correct behaviour might be to get an
                # entry with all aggregated extras, but it is unclear if we
                # should do this now.
                key = lambda x: (len(x[1].partition("==")[0]), x),
            )
        }.values()

        tokenized_options = []
        for opt in parse_result.options:
            for p in opt.split(" "):
                tokenized_options.append(p)

        pip_args = tokenized_options + extra_pip_args
        for plat in plats:
            requirements[plat] = requirements_dict
            options[plat] = pip_args

    requirements_by_platform = {}
    reqs_with_env_markers = {}
    for target_platform, reqs_ in requirements.items():
        extra_pip_args = options[target_platform]

        for distribution, requirement_line in reqs_:
            for_whl = requirements_by_platform.setdefault(
                normalize_name(distribution),
                {},
            )

            if ";" in requirement_line:
                reqs_with_env_markers.setdefault(requirement_line, []).append(target_platform)

            for_req = for_whl.setdefault(
                (requirement_line, ",".join(extra_pip_args)),
                struct(
                    distribution = distribution,
                    srcs = index_sources(requirement_line),
                    requirement_line = requirement_line,
                    target_platforms = [],
                    extra_pip_args = extra_pip_args,
                ),
            )
            for_req.target_platforms.append(target_platform)

    # This may call to Python, so execute it early (before calling to the
    # internet below) and ensure that we call it only once.
    #
    # NOTE @aignas 2024-07-13: in the future, if this is something that we want
    # to do, we could use Python to parse the requirement lines and infer the
    # URL of the files to download things from. This should be important for
    # VCS package references.
    env_marker_target_platforms = evaluate_markers(ctx, reqs_with_env_markers)
    if logger:
        logger.debug(lambda: "Evaluated env markers from:\n{}\n\nTo:\n{}".format(
            reqs_with_env_markers,
            env_marker_target_platforms,
        ))

    index_urls = {}
    if get_index_urls:
        index_urls = get_index_urls(
            ctx,
            # Use list({}) as a way to have a set
            list({
                req.distribution: None
                for reqs in requirements_by_platform.values()
                for req in reqs.values()
                if req.srcs.shas
            }),
        )

    ret = {}
    for whl_name, reqs in sorted(requirements_by_platform.items()):
        requirement_target_platforms = {}
        for r in reqs.values():
            target_platforms = env_marker_target_platforms.get(r.requirement_line, r.target_platforms)
            for p in target_platforms:
                requirement_target_platforms[p] = None

        is_exposed = len(requirement_target_platforms) == len(requirements)
        if not is_exposed and logger:
            logger.debug(lambda: "Package '{}' will not be exposed because it is only present on a subset of platforms: {} out of {}".format(
                whl_name,
                sorted(requirement_target_platforms),
                sorted(requirements),
            ))

        # Return normalized names
        ret_requirements = ret.setdefault(normalize_name(whl_name), [])

        for r in sorted(reqs.values(), key = lambda r: r.requirement_line):
            whls, sdist = _add_dists(
                requirement = r,
                index_urls = index_urls.get(whl_name),
                logger = logger,
            )

            target_platforms = env_marker_target_platforms.get(r.requirement_line, r.target_platforms)
            ret_requirements.append(
                struct(
                    distribution = r.distribution,
                    srcs = r.srcs,
                    target_platforms = sorted(target_platforms),
                    extra_pip_args = r.extra_pip_args,
                    whls = whls,
                    sdist = sdist,
                    is_exposed = is_exposed,
                ),
            )

    if logger:
        logger.debug(lambda: "Will configure whl repos: {}".format(ret.keys()))

    return ret

def select_requirement(requirements, *, platform):
    """A simple function to get a requirement for a particular platform.

    Only used in WORKSPACE.

    Args:
        requirements (list[struct]): The list of requirements as returned by
            the `parse_requirements` function above.
        platform (str or None): The host platform. Usually an output of the
            `host_platform` function. If None, then this function will return
            the first requirement it finds.

    Returns:
        None if not found or a struct returned as one of the values in the
        parse_requirements function. The requirement that should be downloaded
        by the host platform will be returned.
    """
    maybe_requirement = [
        req
        for req in requirements
        if not platform or [p for p in req.target_platforms if p.endswith(platform)]
    ]
    if not maybe_requirement:
        # Sometimes the package is not present for host platform if there
        # are whls specified only in particular requirements files, in that
        # case just continue, however, if the download_only flag is set up,
        # then the user can also specify the target platform of the wheel
        # packages they want to download, in that case there will be always
        # a requirement here, so we will not be in this code branch.
        return None

    return maybe_requirement[0]

def host_platform(ctx):
    """Return a string representation of the repository OS.

    Only used in WORKSPACE.

    Args:
        ctx (struct): The `module_ctx` or `repository_ctx` attribute.

    Returns:
        The string representation of the platform that we can later used in the `pip`
        machinery.
    """
    return "{}_{}".format(
        repo_utils.get_platforms_os_name(ctx),
        repo_utils.get_platforms_cpu_name(ctx),
    )

def _add_dists(*, requirement, index_urls, logger = None):
    """Populate dists based on the information from the PyPI index.

    This function will modify the given requirements_by_platform data structure.

    Args:
        requirement: The result of parse_requirements function.
        index_urls: The result of simpleapi_download.
        logger: A logger for printing diagnostic info.
    """
    if not index_urls:
        return [], None

    whls = []
    sdist = None

    # TODO @aignas 2024-05-22: it is in theory possible to add all
    # requirements by version instead of by sha256. This may be useful
    # for some projects.
    for sha256 in requirement.srcs.shas:
        # For now if the artifact is marked as yanked we just ignore it.
        #
        # See https://packaging.python.org/en/latest/specifications/simple-repository-api/#adding-yank-support-to-the-simple-api

        maybe_whl = index_urls.whls.get(sha256)
        if maybe_whl and not maybe_whl.yanked:
            whls.append(maybe_whl)
            continue

        maybe_sdist = index_urls.sdists.get(sha256)
        if maybe_sdist and not maybe_sdist.yanked:
            sdist = maybe_sdist
            continue

        if logger:
            logger.warn(lambda: "Could not find a whl or an sdist with sha256={}".format(sha256))

    yanked = {}
    for dist in whls + [sdist]:
        if dist and dist.yanked:
            yanked.setdefault(dist.yanked, []).append(dist.filename)
    if yanked:
        logger.warn(lambda: "\n".join([
            "the following distributions got yanked:",
        ] + [
            "reason: {}\n  {}".format(reason, "\n".join(sorted(dists)))
            for reason, dists in yanked.items()
        ]))

    # Filter out the wheels that are incompatible with the target_platforms.
    whls = select_whls(whls = whls, want_platforms = requirement.target_platforms, logger = logger)

    return whls, sdist
