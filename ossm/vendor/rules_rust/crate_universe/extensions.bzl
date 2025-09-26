"""Module extension for generating third-party crates for use in bazel."""

load("@bazel_features//:features.bzl", "bazel_features")
load("@bazel_skylib//lib:structs.bzl", "structs")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//crate_universe/private:crates_vendor.bzl", "CRATES_VENDOR_ATTRS", "generate_config_file", "generate_splicing_manifest")
load("//crate_universe/private:generate_utils.bzl", "CARGO_BAZEL_GENERATOR_SHA256", "CARGO_BAZEL_GENERATOR_URL", "GENERATOR_ENV_VARS", "render_config")
load("//crate_universe/private:local_crate_mirror.bzl", "local_crate_mirror")
load("//crate_universe/private:urls.bzl", "CARGO_BAZEL_SHA256S", "CARGO_BAZEL_URLS")
load("//rust/platform:triple.bzl", "get_host_triple")
load("//rust/platform:triple_mappings.bzl", "system_to_binary_ext")
load(":defs.bzl", _crate_universe_crate = "crate")

# A list of labels which may be relative (and if so, is within the repo the rule is generated in).
#
# If I were to write ":foo", with attr.label_list, it would evaluate to
# "@@//:foo". However, for a tag such as deps, ":foo" should refer to
# "@@rules_rust~crates~<crate>//:foo".
_relative_label_list = attr.string_list

_OPT_BOOL_VALUES = {
    "auto": None,
    "off": False,
    "on": True,
}

def _get_or_insert(d, key, value):
    if key not in d:
        d[key] = value
    return d[key]

def _generate_repo_impl(repo_ctx):
    for path, contents in repo_ctx.attr.contents.items():
        repo_ctx.file(path, contents)

_generate_repo = repository_rule(
    doc = "A utility for generating a hub repo.",
    implementation = _generate_repo_impl,
    attrs = {
        "contents": attr.string_dict(
            doc = "A mapping of file names to text they should contain.",
            mandatory = True,
        ),
    },
)

def _annotations_for_repo(module_annotations, repo_specific_annotations):
    """Merges the set of global annotations with the repo-specific ones

    Args:
        module_annotations (dict): The annotation tags that apply to all repos, keyed by crate.
        repo_specific_annotations (dict): The annotation tags that apply to only this repo, keyed by crate.
    """

    if not repo_specific_annotations:
        return module_annotations

    annotations = dict(module_annotations)
    for crate, values in repo_specific_annotations.items():
        _get_or_insert(annotations, crate, []).extend(values)
    return annotations

def _generate_hub_and_spokes(*, module_ctx, cargo_bazel, cfg, annotations, cargo_lockfile = None, manifests = {}, packages = {}):
    """Generates repositories for the transitive closure of crates defined by manifests and packages.

    Args:
        module_ctx (module_ctx): The module context object.
        cargo_bazel (function): A function that can be called to execute cargo_bazel.
        cfg (object): The module tag from `from_cargo` or `from_specs`
        annotations (dict): The set of annotation tag classes that apply to this closure, keyed by crate name.
        cargo_lockfile (path): Path to Cargo.lock, if we have one. This is optional for `from_specs` closures.
        manifests (dict): The set of Cargo.toml manifests that apply to this closure, if any, keyed by path.
        packages (dict): The set of extra cargo crate tags that apply to this closure, if any, keyed by package name.
    """

    tag_path = module_ctx.path(cfg.name)

    rendering_config = json.decode(render_config(
        regen_command = "Run 'cargo update [--workspace]'",
    ))
    config_file = tag_path.get_child("config.json")
    module_ctx.file(
        config_file,
        executable = False,
        content = generate_config_file(
            module_ctx,
            mode = "remote",
            annotations = annotations,
            generate_build_scripts = cfg.generate_build_scripts,
            supported_platform_triples = cfg.supported_platform_triples,
            generate_target_compatible_with = True,
            repository_name = cfg.name,
            output_pkg = cfg.name,
            workspace_name = cfg.name,
            generate_binaries = cfg.generate_binaries,
            render_config = rendering_config,
            repository_ctx = module_ctx,
        ),
    )

    splicing_manifest = tag_path.get_child("splicing_manifest.json")
    module_ctx.file(
        splicing_manifest,
        executable = False,
        content = generate_splicing_manifest(
            packages = packages,
            splicing_config = cfg.splicing_config,
            cargo_config = cfg.cargo_config,
            manifests = manifests,
            manifest_to_path = module_ctx.path,
        ),
    )

    nonhermetic_root_bazel_workspace_dir = module_ctx.path(Label("@@//:MODULE.bazel")).dirname

    splicing_output_dir = tag_path.get_child("splicing-output")
    splice_args = [
        "splice",
        "--output-dir",
        splicing_output_dir,
        "--config",
        config_file,
        "--splicing-manifest",
        splicing_manifest,
        "--nonhermetic-root-bazel-workspace-dir",
        nonhermetic_root_bazel_workspace_dir,
    ]
    if cargo_lockfile:
        splice_args.extend([
            "--cargo-lockfile",
            cargo_lockfile,
        ])
    cargo_bazel(splice_args)

    # Create a lockfile, since we need to parse it to generate spoke
    # repos.
    lockfile_path = tag_path.get_child("lockfile.json")
    module_ctx.file(lockfile_path, "")

    paths_to_track_file = module_ctx.path("paths-to-track")
    warnings_output_file = module_ctx.path("warnings-output-file")

    cargo_bazel([
        "generate",
        "--cargo-lockfile",
        cargo_lockfile or splicing_output_dir.get_child("Cargo.lock"),
        "--config",
        config_file,
        "--splicing-manifest",
        splicing_manifest,
        "--repository-dir",
        tag_path,
        "--metadata",
        splicing_output_dir.get_child("metadata.json"),
        "--repin",
        "--lockfile",
        lockfile_path,
        "--nonhermetic-root-bazel-workspace-dir",
        nonhermetic_root_bazel_workspace_dir,
        "--paths-to-track",
        paths_to_track_file,
        "--warnings-output-path",
        warnings_output_file,
    ])

    paths_to_track = json.decode(module_ctx.read(paths_to_track_file))
    for path in paths_to_track:
        # This read triggers watching the file at this path and invalidates the repository_rule which will get re-run.
        # Ideally we'd use module_ctx.watch, but it doesn't support files outside of the workspace, and we need to support that.
        module_ctx.read(path)

    warnings_output_file = json.decode(module_ctx.read(warnings_output_file))
    for warning in warnings_output_file:
        # buildifier: disable=print
        print("WARN: {}".format(warning))

    crates_dir = tag_path.get_child(cfg.name)
    _generate_repo(
        name = cfg.name,
        contents = {
            "BUILD.bazel": module_ctx.read(crates_dir.get_child("BUILD.bazel")),
            "defs.bzl": module_ctx.read(crates_dir.get_child("defs.bzl")),
        },
    )

    contents = json.decode(module_ctx.read(lockfile_path))

    for crate in contents["crates"].values():
        repo = crate["repository"]
        if repo == None:
            continue
        name = crate["name"]
        version = crate["version"]

        # "+" isn't valid in a repo name.
        crate_repo_name = "{repo_name}__{name}-{version}".format(
            repo_name = cfg.name,
            name = name,
            version = version.replace("+", "-"),
        )

        build_file_content = module_ctx.read(crates_dir.get_child("BUILD.%s-%s.bazel" % (name, version)))
        if "Http" in repo:
            # Replicates functionality in repo_http.j2.
            repo = repo["Http"]
            http_archive(
                name = crate_repo_name,
                patch_args = repo.get("patch_args", None),
                patch_tool = repo.get("patch_tool", None),
                patches = repo.get("patches", None),
                remote_patch_strip = 1,
                sha256 = repo.get("sha256", None),
                type = "tar.gz",
                urls = [repo["url"]],
                strip_prefix = "%s-%s" % (crate["name"], crate["version"]),
                build_file_content = build_file_content,
            )
        elif "Git" in repo:
            # Replicates functionality in repo_git.j2
            repo = repo["Git"]
            kwargs = {}
            for k, v in repo["commitish"].items():
                if k == "Rev":
                    kwargs["commit"] = v
                else:
                    kwargs[k.lower()] = v
            new_git_repository(
                name = crate_repo_name,
                init_submodules = True,
                patch_args = repo.get("patch_args", None),
                patch_tool = repo.get("patch_tool", None),
                patches = repo.get("patches", None),
                shallow_since = repo.get("shallow_since", None),
                remote = repo["remote"],
                build_file_content = build_file_content,
                strip_prefix = repo.get("strip_prefix", None),
                **kwargs
            )
        elif "Path" in repo:
            options = {
                "config": rendering_config,
                "crate_context": crate,
                "platform_conditions": contents["conditions"],
                "supported_platform_triples": cfg.supported_platform_triples,
            }
            kwargs = {}
            if len(CARGO_BAZEL_URLS) == 0:
                kwargs["generator"] = "@cargo_bazel_bootstrap//:cargo-bazel"
            local_crate_mirror(
                name = crate_repo_name,
                options_json = json.encode(options),
                path = repo["Path"]["path"],
                **kwargs
            )
        else:
            fail("Invalid repo: expected Http or Git to exist for crate %s-%s, got %s" % (name, version, repo))

def _package_to_json(p):
    # Avoid adding unspecified properties.
    # If we add them as empty strings, cargo-bazel will be unhappy.
    return json.encode({
        k: v
        for k, v in structs.to_dict(p).items()
        if v or k == "default_features"
    })

def _get_generator(module_ctx):
    """Query Network Resources to local a `cargo-bazel` binary.

    Based off get_generator in crates_universe/private/generate_utils.bzl

    Args:
        module_ctx (module_ctx):  The rules context object

    Returns:
        tuple(path, dict) The path to a 'cargo-bazel' binary. The pairing (dict)
            may be `None` if there is not need to update the attribute
    """
    host_triple = get_host_triple(module_ctx)
    use_environ = False
    for var in GENERATOR_ENV_VARS:
        if var in module_ctx.os.environ:
            use_environ = True

    if use_environ:
        generator_sha256 = module_ctx.os.environ.get(CARGO_BAZEL_GENERATOR_SHA256)
        generator_url = module_ctx.os.environ.get(CARGO_BAZEL_GENERATOR_URL)
    elif len(CARGO_BAZEL_URLS) == 0:
        return module_ctx.path(Label("@cargo_bazel_bootstrap//:cargo-bazel"))
    else:
        generator_sha256 = CARGO_BAZEL_SHA256S.get(host_triple.str)
        generator_url = CARGO_BAZEL_URLS.get(host_triple.str)

    if not generator_url:
        fail((
            "No generator URL was found either in the `CARGO_BAZEL_GENERATOR_URL` " +
            "environment variable or for the `{}` triple in the `generator_urls` attribute"
        ).format(host_triple.str))

    output = module_ctx.path("cargo-bazel.exe" if "win" in module_ctx.os.name else "cargo-bazel")

    # Download the file into place
    download_kwargs = {
        "executable": True,
        "output": output,
        "url": generator_url,
    }

    if generator_sha256:
        download_kwargs.update({"sha256": generator_sha256})

    module_ctx.download(**download_kwargs)
    return output

def _get_host_cargo_rustc(module_ctx):
    """A helper function to get the path to the host cargo and rustc binaries.

    Args:
        module_ctx: The module extension's context.
    Returns:
        A tuple of path to cargo, path to rustc.
    """
    host_triple = get_host_triple(module_ctx)
    binary_ext = system_to_binary_ext(host_triple.system)

    cargo_path = str(module_ctx.path(Label("@rust_host_tools//:bin/cargo{}".format(binary_ext))))
    rustc_path = str(module_ctx.path(Label("@rust_host_tools//:bin/rustc{}".format(binary_ext))))
    return cargo_path, rustc_path

def _get_cargo_bazel_runner(module_ctx, cargo_bazel):
    """A helper function to allow executing cargo_bazel in module extensions.

    Args:
        module_ctx: The module extension's context.
        cargo_bazel: Path The path to a `cargo-bazel` binary
    Returns:
        A function that can be called to execute cargo_bazel.
    """
    cargo_path, rustc_path = _get_host_cargo_rustc(module_ctx)

    # Placing this as a nested function allows users to call this right at the
    # start of a module extension, thus triggering any restarts as early as
    # possible (since module_ctx.path triggers restarts).
    def run(args, env = {}, timeout = 600):
        final_args = [cargo_bazel]
        final_args.extend(args)
        final_args.extend([
            "--cargo",
            cargo_path,
            "--rustc",
            rustc_path,
        ])
        result = module_ctx.execute(
            final_args,
            environment = dict(CARGO = cargo_path, RUSTC = rustc_path, **env),
            timeout = timeout,
        )
        if result.return_code != 0:
            if result.stdout:
                print("Stdout:", result.stdout)  # buildifier: disable=print
            pretty_args = " ".join([str(arg) for arg in final_args])
            fail("%s returned with exit code %d:\n%s" % (pretty_args, result.return_code, result.stderr))
        return result

    return run

def _crate_impl(module_ctx):
    # Preload all external repositories. Calling `module_ctx.path` will cause restarts of the implementation
    # function of the module extension, so we want to trigger all restarts before we start the actual work.
    # Once https://github.com/bazelbuild/bazel/issues/22729 has been fixed, this code can be removed.
    _get_host_cargo_rustc(module_ctx)
    for mod in module_ctx.modules:
        for cfg in mod.tags.from_cargo:
            module_ctx.path(cfg.cargo_lockfile)
            for m in cfg.manifests:
                module_ctx.path(m)

    cargo_bazel_output = _get_generator(module_ctx)
    cargo_bazel = _get_cargo_bazel_runner(module_ctx, cargo_bazel_output)

    all_repos = []
    reproducible = True

    for mod in module_ctx.modules:
        module_annotations = {}
        repo_specific_annotations = {}
        for annotation_tag in mod.tags.annotation:
            annotation_dict = structs.to_dict(annotation_tag)
            repositories = annotation_dict.pop("repositories")
            crate = annotation_dict.pop("crate")

            # The crate.annotation function can take in either a list or a bool.
            # For the tag-based method, because it has type safety, we have to
            # split it into two parameters.
            if annotation_dict.pop("gen_all_binaries"):
                annotation_dict["gen_binaries"] = True
            annotation_dict["gen_build_script"] = _OPT_BOOL_VALUES[annotation_dict["gen_build_script"]]

            # Process the override targets for the annotation.
            # In the non-bzlmod approach, this is given as a dict
            # with the possible keys "`proc_macro`, `build_script`, `lib`, `bin`".
            # With the tag-based approach used in Bzlmod, we run into an issue
            # where there is no dict type that takes a string as a key and a Label as the value.
            # To work around this, we split the override option into four, and reconstruct the
            # dictionary here during processing
            annotation_dict["override_targets"] = dict()
            replacement = annotation_dict.pop("override_target_lib")
            if replacement:
                annotation_dict["override_targets"]["lib"] = str(replacement)

            replacement = annotation_dict.pop("override_target_proc_macro")
            if replacement:
                annotation_dict["override_targets"]["proc_macro"] = str(replacement)

            replacement = annotation_dict.pop("override_target_build_script")
            if replacement:
                annotation_dict["override_targets"]["build_script"] = str(replacement)

            replacement = annotation_dict.pop("override_target_bin")
            if replacement:
                annotation_dict["override_targets"]["bin"] = str(replacement)

            annotation = _crate_universe_crate.annotation(**{
                k: v
                for k, v in annotation_dict.items()
                # Tag classes can't take in None, but the function requires None
                # instead of the empty values in many cases.
                # https://github.com/bazelbuild/bazel/issues/20744
                if v != "" and v != [] and v != {}
            })
            if not repositories:
                _get_or_insert(module_annotations, crate, []).append(annotation)
            for repo in repositories:
                _get_or_insert(
                    _get_or_insert(repo_specific_annotations, repo, {}),
                    crate,
                    [],
                ).append(annotation)

        local_repos = []

        for cfg in mod.tags.from_cargo + mod.tags.from_specs:
            if cfg.name in local_repos:
                fail("Defined two crate universes with the same name in the same MODULE.bazel file. Use the name tag to give them different names.")
            elif cfg.name in all_repos:
                fail("Defined two crate universes with the same name in different MODULE.bazel files. Either give one a different name, or use use_extension(isolate=True)")
            all_repos.append(cfg.name)
            local_repos.append(cfg.name)

        for cfg in mod.tags.from_cargo:
            annotations = _annotations_for_repo(
                module_annotations,
                repo_specific_annotations.get(cfg.name),
            )

            cargo_lockfile = module_ctx.path(cfg.cargo_lockfile)
            manifests = {str(module_ctx.path(m)): str(m) for m in cfg.manifests}
            _generate_hub_and_spokes(
                module_ctx = module_ctx,
                cargo_bazel = cargo_bazel,
                cfg = cfg,
                annotations = annotations,
                cargo_lockfile = cargo_lockfile,
                manifests = manifests,
            )

        for cfg in mod.tags.from_specs:
            # We don't have a Cargo.lock so the resolution can change.
            # We could maybe make this reproducible by using `-minimal-version` during resolution.
            # See https://doc.rust-lang.org/nightly/cargo/reference/unstable.html#minimal-versions
            reproducible = False

            annotations = _annotations_for_repo(
                module_annotations,
                repo_specific_annotations.get(cfg.name),
            )

            packages = {p.package: _package_to_json(p) for p in mod.tags.spec}
            _generate_hub_and_spokes(
                module_ctx = module_ctx,
                cargo_bazel = cargo_bazel,
                cfg = cfg,
                annotations = annotations,
                packages = packages,
            )

        for repo in repo_specific_annotations:
            if repo not in local_repos:
                fail("Annotation specified for repo %s, but the module defined repositories %s" % (repo, local_repos))

    metadata_kwargs = {}
    if bazel_features.external_deps.extension_metadata_has_reproducible:
        metadata_kwargs["reproducible"] = reproducible

    return module_ctx.extension_metadata(**metadata_kwargs)

_from_cargo = tag_class(
    doc = "Generates a repo @crates from a Cargo.toml / Cargo.lock pair.",
    # Ordering is controlled for readability in generated docs.
    attrs = {
        "name": attr.string(
            doc = "The name of the repo to generate",
            default = "crates",
        ),
    } | {
        "manifests": CRATES_VENDOR_ATTRS["manifests"],
    } | {
        "cargo_config": CRATES_VENDOR_ATTRS["cargo_config"],
        "cargo_lockfile": CRATES_VENDOR_ATTRS["cargo_lockfile"],
        "generate_binaries": CRATES_VENDOR_ATTRS["generate_binaries"],
        "generate_build_scripts": CRATES_VENDOR_ATTRS["generate_build_scripts"],
        "splicing_config": CRATES_VENDOR_ATTRS["splicing_config"],
        "supported_platform_triples": CRATES_VENDOR_ATTRS["supported_platform_triples"],
    },
)

# This should be kept in sync with crate_universe/private/crate.bzl.
_annotation = tag_class(
    attrs = {
        "additive_build_file": attr.label(
            doc = "A file containing extra contents to write to the bottom of generated BUILD files.",
        ),
        "additive_build_file_content": attr.string(
            doc = "Extra contents to write to the bottom of generated BUILD files.",
        ),
        "alias_rule": attr.string(
            doc = "Alias rule to use instead of `native.alias()`.  Overrides [render_config](#render_config)'s 'default_alias_rule'.",
        ),
        "build_script_data": _relative_label_list(
            doc = "A list of labels to add to a crate's `cargo_build_script::data` attribute.",
        ),
        "build_script_data_glob": attr.string_list(
            doc = "A list of glob patterns to add to a crate's `cargo_build_script::data` attribute",
        ),
        "build_script_deps": _relative_label_list(
            doc = "A list of labels to add to a crate's `cargo_build_script::deps` attribute.",
        ),
        "build_script_env": attr.string_dict(
            doc = "Additional environment variables to set on a crate's `cargo_build_script::env` attribute.",
        ),
        "build_script_proc_macro_deps": _relative_label_list(
            doc = "A list of labels to add to a crate's `cargo_build_script::proc_macro_deps` attribute.",
        ),
        "build_script_rundir": attr.string(
            doc = "An override for the build script's rundir attribute.",
        ),
        "build_script_rustc_env": attr.string_dict(
            doc = "Additional environment variables to set on a crate's `cargo_build_script::env` attribute.",
        ),
        "build_script_toolchains": attr.label_list(
            doc = "A list of labels to set on a crates's `cargo_build_script::toolchains` attribute.",
        ),
        "build_script_tools": _relative_label_list(
            doc = "A list of labels to add to a crate's `cargo_build_script::tools` attribute.",
        ),
        "compile_data": _relative_label_list(
            doc = "A list of labels to add to a crate's `rust_library::compile_data` attribute.",
        ),
        "compile_data_glob": attr.string_list(
            doc = "A list of glob patterns to add to a crate's `rust_library::compile_data` attribute.",
        ),
        "crate": attr.string(
            doc = "The name of the crate the annotation is applied to",
            mandatory = True,
        ),
        "crate_features": attr.string_list(
            doc = "A list of strings to add to a crate's `rust_library::crate_features` attribute.",
        ),
        "data": _relative_label_list(
            doc = "A list of labels to add to a crate's `rust_library::data` attribute.",
        ),
        "data_glob": attr.string_list(
            doc = "A list of glob patterns to add to a crate's `rust_library::data` attribute.",
        ),
        "deps": _relative_label_list(
            doc = "A list of labels to add to a crate's `rust_library::deps` attribute.",
        ),
        "disable_pipelining": attr.bool(
            doc = "If True, disables pipelining for library targets for this crate.",
        ),
        "extra_aliased_targets": attr.string_dict(
            doc = "A list of targets to add to the generated aliases in the root crate_universe repository.",
        ),
        "gen_all_binaries": attr.bool(
            doc = "If true, generates `rust_binary` targets for all of the crates bins",
        ),
        "gen_binaries": attr.string_list(
            doc = "As a list, the subset of the crate's bins that should get `rust_binary` targets produced.",
        ),
        "gen_build_script": attr.string(
            doc = "An authorative flag to determine whether or not to produce `cargo_build_script` targets for the current crate. Supported values are 'on', 'off', and 'auto'.",
            values = _OPT_BOOL_VALUES.keys(),
            default = "auto",
        ),
        "override_target_bin": attr.label(
            doc = "An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
        ),
        "override_target_build_script": attr.label(
            doc = "An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
        ),
        "override_target_lib": attr.label(
            doc = "An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
        ),
        "override_target_proc_macro": attr.label(
            doc = "An optional alternate taget to use when something depends on this crate to allow the parent repo to provide its own version of this dependency.",
        ),
        "patch_args": attr.string_list(
            doc = "The `patch_args` attribute of a Bazel repository rule. See [http_archive.patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)",
        ),
        "patch_tool": attr.string(
            doc = "The `patch_tool` attribute of a Bazel repository rule. See [http_archive.patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)",
        ),
        "patches": attr.label_list(
            doc = "The `patches` attribute of a Bazel repository rule. See [http_archive.patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)",
        ),
        "proc_macro_deps": _relative_label_list(
            doc = "A list of labels to add to a crate's `rust_library::proc_macro_deps` attribute.",
        ),
        "repositories": attr.string_list(
            doc = "A list of repository names specified from `crate.from_cargo(name=...)` that this annotation is applied to. Defaults to all repositories.",
            default = [],
        ),
        "rustc_env": attr.string_dict(
            doc = "Additional variables to set on a crate's `rust_library::rustc_env` attribute.",
        ),
        "rustc_env_files": _relative_label_list(
            doc = "A list of labels to set on a crate's `rust_library::rustc_env_files` attribute.",
        ),
        "rustc_flags": attr.string_list(
            doc = "A list of strings to set on a crate's `rust_library::rustc_flags` attribute.",
        ),
        "shallow_since": attr.string(
            doc = "An optional timestamp used for crates originating from a git repository instead of a crate registry. This flag optimizes fetching the source code.",
        ),
        "version": attr.string(
            doc = "The versions of the crate the annotation is applied to. Defaults to all versions.",
            default = "*",
        ),
    },
)

_from_specs = tag_class(
    doc = "Generates a repo @crates from the defined `spec` tags.",
    attrs = {
        "name": attr.string(
            doc = "The name of the repo to generate.",
            default = "crates",
        ),
    } | {
        "cargo_config": CRATES_VENDOR_ATTRS["cargo_config"],
        "generate_binaries": CRATES_VENDOR_ATTRS["generate_binaries"],
        "generate_build_scripts": CRATES_VENDOR_ATTRS["generate_build_scripts"],
        "splicing_config": CRATES_VENDOR_ATTRS["splicing_config"],
        "supported_platform_triples": CRATES_VENDOR_ATTRS["supported_platform_triples"],
    },
)

# This should be kept in sync with crate_universe/private/crate.bzl.
_spec = tag_class(
    attrs = {
        "artifact": attr.string(
            doc = "Set to 'bin' to pull in a binary crate as an artifact dependency. Requires a nightly Cargo.",
        ),
        "branch": attr.string(
            doc = "The git branch of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.",
        ),
        "default_features": attr.bool(
            doc = "Maps to the `default-features` flag.",
            default = True,
        ),
        "features": attr.string_list(
            doc = "A list of features to use for the crate.",
        ),
        "git": attr.string(
            doc = "The Git url to use for the crate. Cannot be used with `version`.",
        ),
        "lib": attr.bool(
            doc = "If using `artifact = 'bin'`, additionally setting `lib = True` declares a dependency on both the package's library and binary, as opposed to just the binary.",
        ),
        "package": attr.string(
            doc = "The explicit name of the package.",
            mandatory = True,
        ),
        "rev": attr.string(
            doc = "The git revision of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified.",
        ),
        "tag": attr.string(
            doc = "The git tag of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.",
        ),
        "version": attr.string(
            doc = "The exact version of the crate. Cannot be used with `git`.",
        ),
    },
)

_conditional_crate_args = {
    "arch_dependent": True,
    "os_dependent": True,
} if bazel_features.external_deps.module_extension_has_os_arch_dependent else {}

crate = module_extension(
    doc = "Crate universe module extensions.",
    implementation = _crate_impl,
    tag_classes = {
        "annotation": _annotation,
        "from_cargo": _from_cargo,
        "from_specs": _from_specs,
        "spec": _spec,
    },
    **_conditional_crate_args
)
