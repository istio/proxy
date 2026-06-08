"""A hub repository builder for incrementally building the hub configuration."""

load("//python/private:full_version.bzl", "full_version")
load("//python/private:normalize_name.bzl", "normalize_name")
load("//python/private:repo_utils.bzl", "repo_utils")
load("//python/private:text_util.bzl", "render")
load("//python/private:version.bzl", "version")
load("//python/private:version_label.bzl", "version_label")
load(":attrs.bzl", "use_isolated")
load(":evaluate_markers.bzl", "evaluate_markers_py", evaluate_markers_star = "evaluate_markers")
load(":parse_requirements.bzl", "parse_requirements")
load(":pep508_env.bzl", "env")
load(":pep508_evaluate.bzl", "evaluate")
load(":python_tag.bzl", "python_tag")
load(":requirements_files_by_platform.bzl", "requirements_files_by_platform")
load(":whl_config_setting.bzl", "whl_config_setting")
load(":whl_repo_name.bzl", "pypi_repo_name", "whl_repo_name")

def _major_minor_version(version_str):
    ver = version.parse(version_str)
    return "{}.{}".format(ver.release[0], ver.release[1])

def hub_builder(
        *,
        name,
        module_name,
        config,
        whl_overrides,
        minor_mapping,
        available_interpreters,
        simpleapi_download_fn,
        evaluate_markers_fn,
        logger,
        simpleapi_cache = {}):
    """Return a hub builder instance

    Args:
        name: {type}`str`, the name of the hub.
        module_name: {type}`str`, the module name that has created the hub.
        config: The platform configuration.
        whl_overrides: {type}`dict[str, struct]` - per-wheel overrides.
        minor_mapping: {type}`dict[str, str]` the mapping between minor and full versions.
        evaluate_markers_fn: the override function used to evaluate the markers.
        available_interpreters: {type}`dict[str, Label]` The dictionary of available
            interpreters that have been registered using the `python` bzlmod extension.
            The keys are in the form `python_{snake_case_version}_host`. This is to be
            used during the `repository_rule` and must be always compatible with the host.
        simpleapi_download_fn: the function used to download from SimpleAPI.
        simpleapi_cache: the cache for the download results.
        logger: the logger for this builder.
    """

    # buildifier: disable=uninitialized
    self = struct(
        name = name,
        module_name = module_name,

        # public methods, keep sorted and to minimum
        build = lambda: _build(self),
        pip_parse = lambda *a, **k: _pip_parse(self, *a, **k),

        # Build output related

        # The set of package names to expose. Dict acting as set
        # dict[str package, None]
        _exposed_packages = {},  # modified by _add_exposed_packages
        # Map of the per-package aliases.
        # The nested dict is a dict acting as a set.
        # dict[str whl_name, dict[str alias_name, bool]]
        _extra_aliases = {},  # modified by _add_extra_aliases
        # dict[str group_name, list[str]]
        _group_map = {},  # modified by _add_group_map
        # Mapping of whl_library repo names and their kwargs.
        # dict[str repo_name, dict[str, object] kwargs]
        _whl_libraries = {},  # modified by _add_whl_library
        # Map of repos and their config settings, and repo the config
        # setting originated from.
        # dict[str whl_name, dict[str config_setting, str repo_name]]
        _whl_map = {},  # modified by _add_whl_library

        # Internal

        # dict[str python_version, dict[str platform, PlatformInfo]]
        # where `PlatformInfo` is from `_platforms()`
        _platforms = {},
        # Supplemental index of `_group_map`
        # dict[str whl_name, str group_name]
        _group_name_by_whl = {},
        # Functions to download according to the config
        # dict[str python_version, callable]
        _get_index_urls = {},
        # Tells whether to use the downloader for a package.
        # dict[str python_version, dict[str package_name, bool use_downloader]]
        _use_downloader = {},
        _simpleapi_cache = simpleapi_cache,

        # Instance constants passed in by callers
        _config = config,
        _whl_overrides = whl_overrides,
        _evaluate_markers_fn = evaluate_markers_fn,
        _logger = logger,
        _minor_mapping = minor_mapping,
        _available_interpreters = available_interpreters,
        _simpleapi_download_fn = simpleapi_download_fn,
    )

    # buildifier: enable=uninitialized
    return self

### PUBLIC methods

def _build(self):
    ret = struct(
        whl_map = {},
        group_map = {},
        extra_aliases = {},
        exposed_packages = [],
        whl_libraries = {},
    )
    if self._logger.failed():
        return ret

    whl_map = {}
    for key, settings in self._whl_map.items():
        for setting, repo in settings.items():
            whl_map.setdefault(key, {}).setdefault(repo, []).append(setting)

    return struct(
        # The config settings for matching repo spokes.
        # dict[str repo_name, dict[str repo_name, list[str]]]
        whl_map = whl_map,
        # Maps a wheel to a list of groups
        # dict[str group_name, list[str]]
        group_map = self._group_map,
        # The per-package aliases for the hub to create.
        # dict[str package, list[str]]
        extra_aliases = {
            whl: sorted(aliases)
            for whl, aliases in self._extra_aliases.items()
        },
        # The list of exposed packages in the hub.
        # list[str]
        exposed_packages = sorted(self._exposed_packages),

        # Mapping of whl_library repo names and their kwargs.
        # dict[str repo_name, dict[str, object] kwargs]
        whl_libraries = self._whl_libraries,
    )

def _pip_parse(self, module_ctx, pip_attr):
    python_version = pip_attr.python_version
    if python_version in self._platforms:
        fail((
            "Duplicate pip python version '{version}' for hub " +
            "'{hub}' in module '{module}': the Python versions " +
            "used for a hub must be unique"
        ).format(
            hub = self.name,
            module = self.module_name,
            version = python_version,
        ))

    full_python_version = full_version(
        version = python_version,
        minor_mapping = self._minor_mapping,
        fail_on_err = False,
    )
    if not full_python_version:
        # NOTE @aignas 2025-11-18: If the python version is not present in our
        # minor_mapping, then we will not register any packages and then the
        # select in the hub repository will fail, which will prompt the user to
        # configure the toolchain correctly and move forward.
        self._logger.info(lambda: (
            "Ignoring pip python version '{version}' for hub " +
            "'{hub}' in module '{module}' because there is no registered " +
            "toolchain for it."
        ).format(
            hub = self.name,
            module = self.module_name,
            version = python_version,
        ))
        return

    default_cross_setup = _set_get_index_urls(self, pip_attr)
    self._platforms[python_version] = _platforms(
        module_ctx,
        python_version = full_python_version,
        config = self._config,
        # TODO @aignas 2025-12-09: flip or part to default to 'os_arch' after
        # 1.8.0 is released and set the default of the `target_platforms` attribute
        # to `{os}_{arch}`.
        target_platforms = pip_attr.target_platforms or ([] if default_cross_setup else ["{os}_{arch}"]),
    )
    _add_group_map(self, pip_attr.experimental_requirement_cycles)
    _add_extra_aliases(self, pip_attr.extra_hub_aliases)
    _create_whl_repos(
        self,
        module_ctx,
        pip_attr = pip_attr,
        enable_pipstar = self._config.enable_pipstar or self._get_index_urls.get(pip_attr.python_version),
        enable_pipstar_extract = self._config.enable_pipstar_extract or self._get_index_urls.get(pip_attr.python_version),
    )

### end of PUBLIC methods
### setters for build outputs

def _add_exposed_packages(self, exposed_packages):
    """Add packages that are exposed.

    Args:
        self: implicitly added
        exposed_packages: {type}`dict[str package, None]` a dict acting as
            a set. The set of packages that should be exposed.
    """
    if self._exposed_packages:
        intersection = {}
        for pkg in exposed_packages:
            if pkg not in self._exposed_packages:
                continue
            intersection[pkg] = None
        self._exposed_packages.clear()
        exposed_packages = intersection

    self._exposed_packages.update(exposed_packages)

def _add_group_map(self, group_map):
    """Adds a group mapping for cycle breaking.

    Args:
        self: implicitly added.
        group_map: {type}`dict[str name, list[str]]`
    """

    # TODO @aignas 2024-04-05: how do we support different requirement
    # cycles for different abis/oses? For now we will need the users to
    # assume the same groups across all versions/platforms until we start
    # using an alternative cycle resolution strategy.
    group_map = {
        name: [normalize_name(whl_name) for whl_name in whls]
        for name, whls in group_map.items()
    }
    self._group_map.clear()
    self._group_name_by_whl.clear()

    self._group_map.update(group_map)
    self._group_name_by_whl.update({
        whl_name: group_name
        for group_name, group_whls in self._group_map.items()
        for whl_name in group_whls
    })

def _add_extra_aliases(self, extra_hub_aliases):
    """Adds per-package aliases to the hub.

    Args:
        self: Implicitly added
        extra_hub_aliases: {type}`dict[str package, list[str]]` Alias target
            names to add to a package's hub BUILD file.
    """
    for whl_name, aliases in extra_hub_aliases.items():
        self._extra_aliases.setdefault(whl_name, {}).update(
            {alias: True for alias in aliases},
        )

def _diff_dict(first, second):
    """A simple utility to shallow compare dictionaries.

    Args:
        first: The first dictionary to compare.
        second: The second dictionary to compare.

    Returns:
        A dictionary containing the differences, with keys "common", "different",
        "extra", and "missing", or None if the dictionaries are identical.
    """
    missing = {}
    extra = {
        key: value
        for key, value in second.items()
        if key not in first
    }
    common = {}
    different = {}

    for key, value in first.items():
        if key not in second:
            missing[key] = value
        elif value == second[key]:
            common[key] = value
        else:
            different[key] = (value, second[key])

    if missing or extra or different:
        return {
            "common": common,
            "different": different,
            "extra": extra,
            "missing": missing,
        }
    else:
        return None

def _add_whl_library(self, *, python_version, whl, repo, enable_pipstar):
    """Add a whl_library and kwargs to call it with for the hub.

    Args:
        self: implicitly added
        python_version: {type}`str` the python version to assume
        whl: struct from `_whl_library_args()`
        repo: struct from `_whl_repo`
        enable_pipstar: {type}`bool` if pipstar is enabled.
    """
    if repo == None:
        # NOTE @aignas 2025-07-07: we guard against an edge-case where there
        # are more platforms defined than there are wheels for and users
        # disallow building from sdist.
        return

    platforms = self._platforms[python_version]

    # TODO @aignas 2025-06-29: we should not need the version in the repo_name if
    # we are using pipstar and we are downloading the wheel using the downloader
    #
    # However, for that we should first have a different way to reference closures with
    # extras. For example, if some package depends on `foo[extra]` and another depends on
    # `foo`, we should have 2 py_library targets.
    repo_name = "{}_{}_{}".format(self.name, version_label(python_version), repo.repo_name)

    if repo_name in self._whl_libraries:
        diff = _diff_dict(self._whl_libraries[repo_name], repo.args)
        if diff:
            self._logger.fail(lambda: (
                "Attempting to create a duplicate library {repo_name} for {whl_name} with different arguments. Already existing declaration has:\n".format(
                    repo_name = repo_name,
                    whl_name = whl.name,
                ) + "\n".join([
                    "    {}: {}".format(key, render.indent(render.dict(value)).lstrip())
                    for key, value in diff.items()
                    if value
                ])
            ))
            return
    self._whl_libraries[repo_name] = repo.args

    if not enable_pipstar and "experimental_target_platforms" in repo.args:
        self._whl_libraries[repo_name] |= {
            "experimental_target_platforms": sorted({
                # TODO @aignas 2025-07-07: this should be solved in a better way
                platforms[candidate].triple.partition("_")[-1]: None
                for p in repo.args["experimental_target_platforms"]
                for candidate in platforms
                if candidate.endswith(p)
            }),
        }

    mapping = self._whl_map.setdefault(whl.name, {})
    if repo.config_setting in mapping and mapping[repo.config_setting] != repo_name:
        fail(
            "attempting to override an existing repo '{}' for config setting '{}' with a new repo '{}'".format(
                mapping[repo.config_setting],
                repo.config_setting,
                repo_name,
            ),
        )
    else:
        mapping[repo.config_setting] = repo_name

### end of setters, below we have various functions to implement the public methods

def _set_get_index_urls(self, pip_attr):
    if not pip_attr.experimental_index_url:
        if pip_attr.experimental_extra_index_urls:
            fail("'experimental_extra_index_urls' is a no-op unless 'experimental_index_url' is set")
        elif pip_attr.experimental_index_url_overrides:
            fail("'experimental_index_url_overrides' is a no-op unless 'experimental_index_url' is set")
        elif pip_attr.simpleapi_skip:
            fail("'simpleapi_skip' is a no-op unless 'experimental_index_url' is set")
        elif pip_attr.netrc:
            fail("'netrc' is a no-op unless 'experimental_index_url' is set")
        elif pip_attr.auth_patterns:
            fail("'auth_patterns' is a no-op unless 'experimental_index_url' is set")

        # parallel_download is set to True by default, so we are not checking/validating it
        # here
        return False

    python_version = pip_attr.python_version
    self._use_downloader.setdefault(python_version, {}).update({
        normalize_name(s): False
        for s in pip_attr.simpleapi_skip
    })
    self._get_index_urls[python_version] = lambda ctx, distributions: self._simpleapi_download_fn(
        ctx,
        attr = struct(
            index_url = pip_attr.experimental_index_url,
            extra_index_urls = pip_attr.experimental_extra_index_urls or [],
            index_url_overrides = pip_attr.experimental_index_url_overrides or {},
            sources = [
                d
                for d in distributions
                if _use_downloader(self, python_version, d)
            ],
            envsubst = pip_attr.envsubst,
            # Auth related info
            netrc = pip_attr.netrc,
            auth_patterns = pip_attr.auth_patterns,
        ),
        cache = self._simpleapi_cache,
        parallel_download = pip_attr.parallel_download,
    )
    return True

def _detect_interpreter(self, pip_attr):
    python_interpreter_target = pip_attr.python_interpreter_target
    if python_interpreter_target == None and not pip_attr.python_interpreter:
        python_name = "python_{}_host".format(
            pip_attr.python_version.replace(".", "_"),
        )
        if python_name not in self._available_interpreters:
            fail((
                "Unable to find interpreter for pip hub '{hub_name}' for " +
                "python_version={version}: Make sure a corresponding " +
                '`python.toolchain(python_version="{version}")` call exists.' +
                "Expected to find {python_name} among registered versions:\n  {labels}"
            ).format(
                hub_name = self.name,
                version = pip_attr.python_version,
                python_name = python_name,
                labels = "  \n".join(self._available_interpreters),
            ))
        python_interpreter_target = self._available_interpreters[python_name]

    return struct(
        target = python_interpreter_target,
        path = pip_attr.python_interpreter,
    )

def _platforms(module_ctx, *, python_version, config, target_platforms):
    platforms = {}
    python_version = version.parse(
        python_version,
        strict = True,
    )

    target_platforms = sorted({
        p.format(
            os = repo_utils.get_platforms_os_name(module_ctx),
            arch = repo_utils.get_platforms_cpu_name(module_ctx),
        ): None
        for p in target_platforms
    })

    for platform, values in config.platforms.items():
        if target_platforms and platform not in target_platforms:
            continue

        # TODO @aignas 2025-07-07: this is probably doing the parsing of the version too
        # many times.
        abi = "{}{}{}.{}".format(
            python_tag(values.env["implementation_name"]),
            python_version.release[0],
            python_version.release[1],
            python_version.release[2],
        )
        key = "{}_{}".format(abi, platform)

        env_ = env(
            env = values.env,
            os = values.os_name,
            arch = values.arch_name,
            python_version = python_version.string,
        )

        if values.marker and not evaluate(values.marker, env = env_):
            continue

        platforms[key] = struct(
            env = env_,
            triple = "{}_{}_{}".format(abi, values.os_name, values.arch_name),
            whl_abi_tags = [
                v.format(
                    major = python_version.release[0],
                    minor = python_version.release[1],
                )
                for v in values.whl_abi_tags
            ],
            whl_platform_tags = values.whl_platform_tags,
        )
    return platforms

def _evaluate_markers(self, pip_attr, enable_pipstar):
    if self._evaluate_markers_fn:
        return self._evaluate_markers_fn

    if enable_pipstar:
        return lambda _, requirements: evaluate_markers_star(
            requirements = requirements,
            platforms = self._platforms[pip_attr.python_version],
        )

    interpreter = _detect_interpreter(self, pip_attr)

    # NOTE @aignas 2024-08-02: , we will execute any interpreter that we find either
    # in the PATH or if specified as a label. We will configure the env
    # markers when evaluating the requirement lines based on the output
    # from the `requirements_files_by_platform` which should have something
    # similar to:
    # {
    #    "//:requirements.txt": ["cp311_linux_x86_64", ...]
    # }
    #
    # We know the target python versions that we need to evaluate the
    # markers for and thus we don't need to use multiple python interpreter
    # instances to perform this manipulation. This function should be executed
    # only once by the underlying code to minimize the overhead needed to
    # spin up a Python interpreter.
    return lambda module_ctx, requirements: evaluate_markers_py(
        module_ctx,
        requirements = {
            k: {
                p: self._platforms[pip_attr.python_version][p].triple
                for p in plats
            }
            for k, plats in requirements.items()
        },
        python_interpreter = interpreter.path,
        python_interpreter_target = interpreter.target,
        srcs = pip_attr._evaluate_markers_srcs,
        logger = self._logger,
    )

def _create_whl_repos(
        self,
        module_ctx,
        *,
        pip_attr,
        enable_pipstar = False,
        enable_pipstar_extract = False):
    """create all of the whl repositories

    Args:
        self: the builder.
        module_ctx: {type}`module_ctx`.
        pip_attr: {type}`struct` - the struct that comes from the tag class iteration.
        enable_pipstar: {type}`bool` - enable the pipstar or not.
        enable_pipstar_extract: {type}`bool` - enable the pipstar extraction or not.
    """
    logger = self._logger
    platforms = self._platforms[pip_attr.python_version]
    requirements_by_platform = parse_requirements(
        module_ctx,
        requirements_by_platform = requirements_files_by_platform(
            requirements_by_platform = pip_attr.requirements_by_platform,
            requirements_linux = pip_attr.requirements_linux,
            requirements_lock = pip_attr.requirements_lock,
            requirements_osx = pip_attr.requirements_darwin,
            requirements_windows = pip_attr.requirements_windows,
            extra_pip_args = pip_attr.extra_pip_args,
            platforms = sorted(platforms),  # here we only need keys
            python_version = full_version(
                version = pip_attr.python_version,
                minor_mapping = self._minor_mapping,
            ),
            logger = logger,
        ),
        platforms = platforms,
        extra_pip_args = pip_attr.extra_pip_args,
        get_index_urls = self._get_index_urls.get(pip_attr.python_version),
        evaluate_markers = _evaluate_markers(self, pip_attr, enable_pipstar),
        logger = logger,
    )

    _add_exposed_packages(self, {
        whl.name: None
        for whl in requirements_by_platform
        if whl.is_exposed
    })

    whl_modifications = {}
    if pip_attr.whl_modifications != None:
        for mod, whl_name in pip_attr.whl_modifications.items():
            whl_modifications[normalize_name(whl_name)] = mod

    common_args = _common_args(
        self,
        module_ctx,
        pip_attr = pip_attr,
        enable_pipstar = enable_pipstar,
    )

    interpreter = _detect_interpreter(self, pip_attr)

    for whl in requirements_by_platform:
        whl_library_args = common_args | _whl_library_args(
            self,
            whl = whl,
            whl_modifications = whl_modifications,
        )
        for src in whl.srcs:
            repo = _whl_repo(
                src = src,
                whl_library_args = whl_library_args,
                download_only = pip_attr.download_only,
                netrc = self._config.netrc or pip_attr.netrc,
                use_downloader = _use_downloader(self, pip_attr.python_version, whl.name),
                auth_patterns = self._config.auth_patterns or pip_attr.auth_patterns,
                python_version = _major_minor_version(pip_attr.python_version),
                is_multiple_versions = whl.is_multiple_versions,
                interpreter = interpreter,
                enable_pipstar = enable_pipstar,
                enable_pipstar_extract = enable_pipstar_extract,
            )
            _add_whl_library(
                self,
                python_version = pip_attr.python_version,
                whl = whl,
                repo = repo,
                enable_pipstar = enable_pipstar,
            )

def _common_args(self, module_ctx, *, pip_attr, enable_pipstar):
    # Construct args separately so that the lock file can be smaller and does not include unused
    # attrs.
    whl_library_args = dict(
        dep_template = "@{}//{{name}}:{{target}}".format(self.name),
        config_load = "@{}//:config.bzl".format(self.name),
    )
    maybe_args = dict(
        # The following values are safe to omit if they have false like values
        add_libdir_to_library_search_path = pip_attr.add_libdir_to_library_search_path,
        download_only = pip_attr.download_only,
        enable_implicit_namespace_pkgs = pip_attr.enable_implicit_namespace_pkgs,
        environment = pip_attr.environment,
        envsubst = pip_attr.envsubst,
        pip_data_exclude = pip_attr.pip_data_exclude,
    )
    if not enable_pipstar:
        maybe_args["experimental_target_platforms"] = pip_attr.experimental_target_platforms

    whl_library_args.update({k: v for k, v in maybe_args.items() if v})
    maybe_args_with_default = dict(
        # The following values have defaults next to them
        isolated = (use_isolated(module_ctx, pip_attr), True),
        quiet = (pip_attr.quiet, True),
        timeout = (pip_attr.timeout, 600),
    )
    whl_library_args.update({
        k: v
        for k, (v, default) in maybe_args_with_default.items()
        if v != default
    })
    return whl_library_args

def _whl_library_args(self, *, whl, whl_modifications):
    group_name = self._group_name_by_whl.get(whl.name)
    group_deps = self._group_map.get(group_name, [])

    # Construct args separately so that the lock file can be smaller and does not include unused
    # attrs.
    whl_library_args = dict(
        dep_template = "@{}//{{name}}:{{target}}".format(self.name),
    )
    maybe_args = dict(
        # The following values are safe to omit if they have false like values
        annotation = whl_modifications.get(whl.name),
        group_deps = group_deps,
        group_name = group_name,
        whl_patches = {
            p: json.encode(args)
            for p, args in self._whl_overrides.get(whl.name, {}).items()
        },
    )

    whl_library_args.update({k: v for k, v in maybe_args.items() if v})
    return whl_library_args

def _whl_repo(
        *,
        src,
        whl_library_args,
        is_multiple_versions,
        download_only,
        netrc,
        auth_patterns,
        python_version,
        use_downloader,
        interpreter,
        enable_pipstar = False,
        enable_pipstar_extract = False):
    args = dict(whl_library_args)
    args["requirement"] = src.requirement_line
    is_whl = src.filename.endswith(".whl")

    if src.extra_pip_args and not is_whl:
        # pip is not used to download wheels and the python
        # `whl_library` helpers are only extracting things, however
        # for sdists, they will be built by `pip`, so we still
        # need to pass the extra args there, so only pop this for whls
        args["extra_pip_args"] = src.extra_pip_args

    if "whl_patches" in args or not (enable_pipstar_extract and is_whl):
        if interpreter.path:
            args["python_interpreter"] = interpreter.path
        if interpreter.target:
            args["python_interpreter_target"] = interpreter.target

    if not src.url or (not is_whl and download_only):
        if download_only and use_downloader:
            # If the user did not allow using sdists and we are using the downloader
            # and we are not using simpleapi_skip for this
            return None
        else:
            # Fallback to a pip-installed wheel
            target_platforms = src.target_platforms if is_multiple_versions else []
            return struct(
                repo_name = pypi_repo_name(
                    normalize_name(src.distribution),
                    *target_platforms
                ),
                args = args,
                config_setting = whl_config_setting(
                    version = python_version,
                    target_platforms = target_platforms or None,
                ),
            )

    # This is no-op because pip is not used to download the wheel.
    args.pop("download_only", None)

    if netrc:
        args["netrc"] = netrc
    if auth_patterns:
        args["auth_patterns"] = auth_patterns

    args["urls"] = [src.url]
    args["sha256"] = src.sha256
    args["filename"] = src.filename
    if not enable_pipstar:
        args["experimental_target_platforms"] = [
            # Get rid of the version for the target platforms because we are
            # passing the interpreter any way. Ideally we should search of ways
            # how to pass the target platforms through the hub repo.
            p.partition("_")[2]
            for p in src.target_platforms
        ]

    # TODO @aignas 2025-11-02: once we have pipstar enabled we can add extra
    # targets to each hub for each extra combination and solve this more cleanly as opposed to
    # duplicating whl_library repositories.
    target_platforms = src.target_platforms if is_multiple_versions else []

    return struct(
        repo_name = whl_repo_name(src.filename, src.sha256, *target_platforms),
        args = args,
        config_setting = whl_config_setting(
            version = python_version,
            target_platforms = src.target_platforms,
        ),
    )

def _use_downloader(self, python_version, whl_name):
    return self._use_downloader.get(python_version, {}).get(
        normalize_name(whl_name),
        self._get_index_urls.get(python_version) != None,
    )
