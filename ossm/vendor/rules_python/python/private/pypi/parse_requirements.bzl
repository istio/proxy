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
load(":pep508_requirement.bzl", "requirement")
load(":select_whl.bzl", "select_whl")

def parse_requirements(
        ctx,
        *,
        requirements_by_platform = {},
        extra_pip_args = [],
        platforms = {},
        get_index_urls = None,
        evaluate_markers = None,
        extract_url_srcs = True,
        logger):
    """Get the requirements with platforms that the requirements apply to.

    Args:
        ctx: A context that has .read function that would read contents from a label.
        platforms: The target platform descriptions.
        requirements_by_platform (label_keyed_string_dict): a way to have
            different package versions (or different packages) for different
            os, arch combinations.
        extra_pip_args (string list): Extra pip arguments to perform extra validations and to
            be joined with args found in files.
        get_index_urls: Callable[[ctx, list[str]], dict], a callable to get all
            of the distribution URLs from a PyPI index. Accepts ctx and
            distribution names to query.
        evaluate_markers: A function to use to evaluate the requirements.
            Accepts a dict where keys are requirement lines to evaluate against
            the platforms stored as values in the input dict. Returns the same
            dict, but with values being platforms that are compatible with the
            requirements line.
        extract_url_srcs: A boolean to enable extracting URLs from requirement
            lines to enable using bazel downloader.
        logger: repo_utils.logger, a simple struct to log diagnostic messages.

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
    evaluate_markers = evaluate_markers or (lambda _ctx, _requirements: {})
    options = {}
    requirements = {}
    for file, plats in requirements_by_platform.items():
        logger.trace(lambda: "Using {} for {}".format(file, plats))
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
        # Lines with different markers are not condidered duplicates.
        requirements_dict = {}
        for entry in sorted(
            parse_result.requirements,
            # Get the longest match and fallback to original WORKSPACE sorting,
            # which should get us the entry with most extras.
            #
            # FIXME @aignas 2024-05-13: The correct behaviour might be to get an
            # entry with all aggregated extras, but it is unclear if we
            # should do this now.
            key = lambda x: (len(x[1].partition("==")[0]), x),
        ):
            req = requirement(entry[1])
            requirements_dict[(req.name, req.version, req.marker)] = entry

        tokenized_options = []
        for opt in parse_result.options:
            for p in opt.split(" "):
                tokenized_options.append(p)

        pip_args = tokenized_options + extra_pip_args
        for plat in plats:
            requirements[plat] = requirements_dict.values()
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
    logger.trace(lambda: "Evaluated env markers from:\n{}\n\nTo:\n{}".format(
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
                if not req.srcs.url
            }),
        )

    ret = []
    for name, reqs in sorted(requirements_by_platform.items()):
        requirement_target_platforms = {}
        for r in reqs.values():
            target_platforms = env_marker_target_platforms.get(r.requirement_line, r.target_platforms)
            for p in target_platforms:
                requirement_target_platforms[p] = None

        item = struct(
            # Return normalized names
            name = normalize_name(name),
            is_exposed = len(requirement_target_platforms) == len(requirements),
            is_multiple_versions = len(reqs.values()) > 1,
            srcs = _package_srcs(
                name = name,
                reqs = reqs,
                index_urls = index_urls,
                platforms = platforms,
                env_marker_target_platforms = env_marker_target_platforms,
                extract_url_srcs = extract_url_srcs,
                logger = logger,
            ),
        )
        ret.append(item)
        if not item.is_exposed and logger:
            logger.trace(lambda: "Package '{}' will not be exposed because it is only present on a subset of platforms: {} out of {}".format(
                name,
                sorted(requirement_target_platforms),
                sorted(requirements),
            ))

    logger.debug(lambda: "Will configure whl repos: {}".format([w.name for w in ret]))

    return ret

def _package_srcs(
        *,
        name,
        reqs,
        index_urls,
        platforms,
        logger,
        env_marker_target_platforms,
        extract_url_srcs):
    """A function to return sources for a particular package."""
    srcs = {}
    for r in sorted(reqs.values(), key = lambda r: r.requirement_line):
        if ";" in r.requirement_line:
            target_platforms = env_marker_target_platforms.get(r.requirement_line, [])
        else:
            target_platforms = r.target_platforms
        extra_pip_args = tuple(r.extra_pip_args)

        for target_platform in target_platforms:
            if platforms and target_platform not in platforms:
                fail("The target platform '{}' could not be found in {}".format(
                    target_platform,
                    platforms.keys(),
                ))

            dist = _add_dists(
                requirement = r,
                target_platform = platforms.get(target_platform),
                index_urls = index_urls.get(name),
                logger = logger,
            )
            logger.debug(lambda: "The whl dist is: {}".format(dist.filename if dist else dist))

            if extract_url_srcs and dist:
                req_line = r.srcs.requirement
            else:
                dist = struct(
                    url = "",
                    filename = "",
                    sha256 = "",
                    yanked = False,
                )
                req_line = r.srcs.requirement_line

            key = (
                dist.filename,
                req_line,
                extra_pip_args,
            )
            entry = srcs.setdefault(
                key,
                struct(
                    distribution = name,
                    extra_pip_args = r.extra_pip_args,
                    requirement_line = req_line,
                    target_platforms = [],
                    filename = dist.filename,
                    sha256 = dist.sha256,
                    url = dist.url,
                    yanked = dist.yanked,
                ),
            )

            if target_platform not in entry.target_platforms:
                entry.target_platforms.append(target_platform)

    return srcs.values()

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

def _add_dists(*, requirement, index_urls, target_platform, logger = None):
    """Populate dists based on the information from the PyPI index.

    This function will modify the given requirements_by_platform data structure.

    Args:
        requirement: The result of parse_requirements function.
        index_urls: The result of simpleapi_download.
        target_platform: The target_platform information.
        logger: A logger for printing diagnostic info.
    """

    if requirement.srcs.url:
        if not requirement.srcs.filename:
            logger.debug(lambda: "Could not detect the filename from the URL, falling back to pip: {}".format(
                requirement.srcs.url,
            ))
            return None

        # Handle direct URLs in requirements
        dist = struct(
            url = requirement.srcs.url,
            filename = requirement.srcs.filename,
            sha256 = requirement.srcs.shas[0] if requirement.srcs.shas else "",
            yanked = False,
        )

        if dist.filename.endswith(".whl"):
            return dist
        else:
            return dist

    if not index_urls:
        return None

    whls = []
    sdist = None

    # First try to find distributions by SHA256 if provided
    shas_to_use = requirement.srcs.shas
    if not shas_to_use:
        version = requirement.srcs.version
        shas_to_use = index_urls.sha256s_by_version.get(version, [])
        logger.warn(lambda: "requirement file has been generated without hashes, will use all hashes for the given version {} that could find on the index:\n    {}".format(version, shas_to_use))

    for sha256 in shas_to_use:
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

    if not target_platform:
        # The pipstar platforms are undefined here, so we cannot do any matching
        return sdist

    # Select a single wheel that can work on the target_platform
    return select_whl(
        whls = whls,
        python_version = target_platform.env["python_full_version"],
        implementation_name = target_platform.env["implementation_name"],
        whl_abi_tags = target_platform.whl_abi_tags,
        whl_platform_tags = target_platform.whl_platform_tags,
        logger = logger,
    ) or sdist
