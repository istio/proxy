load("@bazel_features//:features.bzl", "bazel_features")
load("//:specs.bzl", "parse", _json = "json")
load("//private:compat_repository.bzl", "compat_repository")
load(
    "//private:coursier_utilities.bzl",
    "contains_git_conflict_markers",
    "escape",
    "strip_packaging_and_classifier_and_version",
)
load("//private/rules:coursier.bzl", "DEFAULT_AAR_IMPORT_LABEL", "coursier_fetch", "pinned_coursier_fetch")
load("//private/rules:unpinned_maven_pin_command_alias.bzl", "unpinned_maven_pin_command_alias")
load("//private/rules:v1_lock_file.bzl", "v1_lock_file")
load("//private/rules:v2_lock_file.bzl", "v2_lock_file")
load(":download_pinned_deps.bzl", "download_pinned_deps")

DEFAULT_REPOSITORIES = [
    "https://repo1.maven.org/maven2",
]

DEFAULT_NAME = "maven"

_DEFAULT_RESOLVER = "coursier"

artifact = tag_class(
    attrs = {
        "name": attr.string(default = DEFAULT_NAME),
        "group": attr.string(mandatory = True),
        "artifact": attr.string(mandatory = True),
        "version": attr.string(),
        "packaging": attr.string(),
        "classifier": attr.string(),
        "force_version": attr.bool(default = False),
        "neverlink": attr.bool(),
        "testonly": attr.bool(),
        "exclusions": attr.string_list(doc = "Maven artifact tuples, in `artifactId:groupId` format", allow_empty = True),
    },
)

install = tag_class(
    attrs = {
        "name": attr.string(default = DEFAULT_NAME),

        # Actual artifacts and overrides
        "artifacts": attr.string_list(doc = "Maven artifact tuples, in `artifactId:groupId:version` format", allow_empty = True),
        "boms": attr.string_list(doc = "Maven BOM tuples, in `artifactId:groupId:version` format", allow_empty = True),
        "exclusions": attr.string_list(doc = "Maven artifact tuples, in `artifactId:groupId` format", allow_empty = True),

        # What do we fetch?
        "fetch_javadoc": attr.bool(default = False),
        "fetch_sources": attr.bool(default = False),

        # How do we do artifact resolution?
        "resolver": attr.string(doc = "The resolver to use. Only honoured for the root module.", values = ["coursier", "maven"], default = _DEFAULT_RESOLVER),

        # Controlling visibility
        "strict_visibility": attr.bool(
            doc = """Controls visibility of transitive dependencies.

            If "True", transitive dependencies are private and invisible to user's rules.
            If "False", transitive dependencies are public and visible to user's rules.
            """,
            default = False,
        ),
        "strict_visibility_value": attr.label_list(default = ["//visibility:private"]),

        # Android support
        "aar_import_bzl_label": attr.string(default = DEFAULT_AAR_IMPORT_LABEL, doc = "The label (as a string) to use to import aar_import from"),
        "use_starlark_android_rules": attr.bool(default = False, doc = "Whether to use the native or Starlark version of the Android rules."),

        # Configuration "stuff"
        "additional_netrc_lines": attr.string_list(doc = "Additional lines prepended to the netrc file used by `http_file` (with `maven_install_json` only).", default = []),
        "use_credentials_from_home_netrc_file": attr.bool(doc = "Whether to pass machine login credentials from the ~/.netrc file to coursier.", default = False),
        "duplicate_version_warning": attr.string(
            doc = """What to do if there are duplicate artifacts

            If "error", then print a message and fail the build.
            If "warn", then print a warning and continue.
            If "none", then do nothing.
            """,
            default = "warn",
            values = [
                "error",
                "warn",
                "none",
            ],
        ),
        "fail_if_repin_required": attr.bool(doc = "Whether to fail the build if the maven_artifact inputs have changed but the lock file has not been repinned.", default = False),
        "lock_file": attr.label(),
        "repositories": attr.string_list(default = DEFAULT_REPOSITORIES),
        "generate_compat_repositories": attr.bool(
            doc = "Additionally generate repository aliases in a .bzl file for all JAR artifacts. For example, `@maven//:com_google_guava_guava` can also be referenced as `@com_google_guava_guava//jar`.",
        ),

        # When using an unpinned repo
        "excluded_artifacts": attr.string_list(doc = "Artifacts to exclude, in `artifactId:groupId` format. Only used on unpinned installs", default = []),  # list of artifacts to exclude
        "fail_on_missing_checksum": attr.bool(default = True),
        "resolve_timeout": attr.int(default = 600),
        "version_conflict_policy": attr.string(
            doc = """Policy for user-defined vs. transitive dependency version conflicts

            If "pinned", choose the user-specified version in maven_install unconditionally.
            If "default", follow Coursier's default policy.
            """,
            default = "default",
            values = [
                "default",
                "pinned",
            ],
        ),
        "ignore_empty_files": attr.bool(default = False, doc = "Treat jars that are empty as if they were not found."),
        "repin_instructions": attr.string(doc = "Instructions to re-pin the repository if required. Many people have wrapper scripts for keeping dependencies up to date, and would like to point users to that instead of the default. Only honoured for the root module."),
        "additional_coursier_options": attr.string_list(doc = "Additional options that will be passed to coursier."),
    },
)

override = tag_class(
    attrs = {
        "name": attr.string(default = DEFAULT_NAME),
        "coordinates": attr.string(doc = "Maven artifact tuple in `artifactId:groupId` format", mandatory = True),
        "target": attr.label(doc = "Target to use in place of maven coordinates", mandatory = True),
    },
)

def _logical_or(source, key, default_value, new_value):
    current = source.get(key, default_value)
    source[key] = current or new_value

def _fail_if_different(attribute, current, next, allowed_default_values):
    if current == next:
        return current

    if next in allowed_default_values:
        return current

    if current in allowed_default_values:
        return next

    fail("Expected values for '%s' to be either default or the same. Instead got: %s and %s" % (attribute, current, next))

def _add_exclusions(exclusions):
    to_return = []

    for exclusion in parse.parse_exclusion_spec_list(exclusions):
        if exclusion not in to_return:
            to_return.append(exclusion)
    return to_return

# Each bzlmod module may contribute jars to different rules_jvm_external maven repo namespaces.
# We record this mapping of repo_name to the list of modules that contributed to it, and emit a warning
# to the user if there are more than one module that contributed to the same repo name.
#
# This can be typical for the default @maven namespace, if a bzlmod dependency
# wishes to contribute to the users' jars.
def _check_repo_name(repo_name_2_module_name, repo_name, module_name):
    known_names = repo_name_2_module_name.get(repo_name, [])
    if module_name in known_names:
        return
    known_names.append(module_name)
    repo_name_2_module_name[repo_name] = known_names

def _to_maven_coords(artifact):
    coords = "%s:%s" % (artifact.get("group"), artifact.get("artifact"))

    extension = artifact.get("packaging", "jar")
    if not extension:
        extension = "jar"
    classifier = artifact.get("classifier", "jar")
    if not classifier:
        classifier = "jar"

    if classifier != "jar":
        coords += ":%s:%s" % (extension, classifier)
    elif extension != "jar":
        coords += ":%s" % extension
    coords += ":%s" % artifact.get("version")

    return coords

def _generate_compat_repos(name, existing_compat_repos, artifacts):
    seen = []

    for artifact in artifacts:
        coords = _to_maven_coords(artifact)
        versionless = escape(strip_packaging_and_classifier_and_version(coords))
        if versionless in existing_compat_repos:
            continue
        seen.append(versionless)
        existing_compat_repos.append(versionless)
        compat_repository(
            name = versionless,
            generating_repository = name,
            target_name = versionless,
        )

    return seen

def maven_impl(mctx):
    repos = {}
    overrides = {}
    exclusions = {}
    http_files = []
    compat_repos = []

    # Iterate over all the tags we care about. For each `name` we want to construct
    # a dict with the following keys:

    # - aar_import_bzl_label: string. Build will fail if this is duplicated and different.
    # - additional_netrc_lines: string list. Accumulated from all `install` tags
    # - artifacts: the exploded tuple for each artifact we want to include.
    # - duplicate_version_warning: string. Build will fail if duplicated and different
    # - fail_if_repin_required: bool. A logical OR over all `fail_if_repin_required` for all `install` tags with the same name.
    # - fail_on_missing_checksum: bool. A logical OR over all `fail_on_missing_checksum` for all `install` tags with the same name.
    # - fetch_javadoc: bool. A logical OR over all `fetch_javadoc` for all `install` tags with the same name.
    # - fetch_sources: bool. A logical OR over all `fetch_sources` for all `install` tags with the same name.
    # - generate_compat_repositories: bool. A logical OR over all `generate_compat_repositories` for all `install` tags with the same name.
    # - lock_file: the lock file to use, if present. Multiple lock files will cause the build to fail.
    # - overrides: a dict mapping `artfifactId:groupId` to Bazel label.
    # - repositories: the list of repositories to pull files from.
    # - strict_visibility: bool. A logical OR over all `strict_visibility` for all `install` tags with the same name.
    # - strict_visibility_value: a string list. Build will fail is duplicated and different.
    # - use_starlark_android_rules: bool. A logical OR over all `use_starlark_android_rules` for all `install` tags with the same name.
    # - version_conflict_policy: string. Fails build if different and not a default.
    # - ignore_empty_files: Treat jars that are empty as if they were not found.
    # - additional_coursier_options: Additional options that will be passed to coursier.

    # Mapping of `name`s to a list of `bazel_module.name`. This will allow us to
    # warn users when more than one module attempts to update a maven repo
    # (which is normally undesired behaviour, but supported as multiple modules
    # can intentionally contribute to the default `maven` repo namespace.)
    repo_name_2_module_name = {}

    for mod in mctx.modules:
        for override in mod.tags.override:
            value = str(override.target)
            current = overrides.get(override.coordinates, None)
            to_use = _fail_if_different("Target of override for %s" % override.coordinates, current, value, [None])
            overrides.update({override.coordinates: to_use})

        for artifact in mod.tags.artifact:
            _check_repo_name(repo_name_2_module_name, artifact.name, mod.name)

            repo = repos.get(artifact.name, {})
            existing_artifacts = repo.get("artifacts", [])

            to_add = {
                "group": artifact.group,
                "artifact": artifact.artifact,
            }

            if artifact.version:
                to_add.update({"version": artifact.version})

            if artifact.packaging:
                to_add.update({"packaging": artifact.packaging})

            if artifact.classifier:
                to_add.update({"classifier": artifact.classifier})

            if artifact.force_version:
                to_add.update({"force_version": artifact.force_version})

            if artifact.neverlink:
                to_add.update({"neverlink": artifact.neverlink})

            if artifact.testonly:
                to_add.update({"testonly": artifact.testonly})

            if artifact.exclusions:
                artifact_exclusions = []
                artifact_exclusions = _add_exclusions(artifact.exclusions + artifact_exclusions)
                to_add.update({"exclusions": artifact_exclusions})

            existing_artifacts.append(to_add)
            repo["artifacts"] = existing_artifacts
            repos[artifact.name] = repo

        for install in mod.tags.install:
            _check_repo_name(repo_name_2_module_name, install.name, mod.name)

            repo = repos.get(install.name, {})

            repo["resolver"] = install.resolver

            artifacts = repo.get("artifacts", [])
            repo["artifacts"] = artifacts + install.artifacts

            boms = repo.get("boms", [])
            repo["boms"] = boms + install.boms

            existing_repos = repo.get("repositories", [])
            for repository in parse.parse_repository_spec_list(install.repositories):
                repo_string = _json.write_repository_spec(repository)
                if repo_string not in existing_repos:
                    existing_repos.append(repo_string)
            repo["repositories"] = existing_repos

            repo["excluded_artifacts"] = repo.get("excluded_artifacts", []) + install.excluded_artifacts

            _logical_or(repo, "fetch_sources", False, install.fetch_sources)
            _logical_or(repo, "generate_compat_repositories", False, install.generate_compat_repositories)
            _logical_or(repo, "use_starlark_android_rules", False, install.use_starlark_android_rules)
            _logical_or(repo, "ignore_empty_files", False, install.ignore_empty_files)
            _logical_or(repo, "use_credentials_from_home_netrc_file", False, install.use_credentials_from_home_netrc_file)

            repo["version_conflict_policy"] = _fail_if_different(
                "version_conflict_policy",
                repo.get("version_conflict_policy"),
                install.version_conflict_policy,
                [None, "default"],
            )

            repo["strict_visibility_value"] = _fail_if_different(
                "strict_visibility_value",
                repo.get("strict_visibility_value", []),
                install.strict_visibility_value,
                [None, []],
            )

            additional_netrc_lines = repo.get("additional_netrc_lines", []) + getattr(install, "additional_netrc_lines", [])
            repo["additional_netrc_lines"] = additional_netrc_lines

            repo["aar_import_bzl_label"] = _fail_if_different(
                "aar_import_bzl_label",
                repo.get("aar_import_bzl_label"),
                install.aar_import_bzl_label,
                [DEFAULT_AAR_IMPORT_LABEL, None],
            )

            repo["duplicate_version_warning"] = _fail_if_different(
                "duplicate_version_warning",
                repo.get("duplicate_version_warning"),
                install.duplicate_version_warning,
                [None, "warn"],
            )

            # Get the longest timeout
            timeout = repo.get("resolve_timeout", install.resolve_timeout)
            if install.resolve_timeout > timeout:
                timeout = install.resolve_timeout
            repo["resolve_timeout"] = timeout

            if mod.is_root:
                repo["repin_instructions"] = install.repin_instructions

            repo["additional_coursier_options"] = repo.get("additional_coursier_options", []) + getattr(install, "additional_coursier_options", [])

            repos[install.name] = repo

    for (repo_name, known_names) in repo_name_2_module_name.items():
        if len(known_names) > 1:
            print("The maven repository '%s' has contributions from multiple bzlmod modules, and will be resolved together: %s" % (
                repo_name,  # e.g. "maven"
                sorted(known_names),  # e.g. bzl_module_bar, bzl_module_bar
            ))

    # Breaking out the logic for picking lock files, because it's not terribly simple
    repo_to_lock_file = {}
    for mod in mctx.modules:
        for install in mod.tags.install:
            if install.lock_file:
                entries = repo_to_lock_file.get(install.name, [])
                if not install.lock_file in entries:
                    entries.append(install.lock_file)
                repo_to_lock_file[install.name] = entries

    # The root module always wins when it comes to these values
    for mod in mctx.modules:
        if mod.is_root:
            for install in mod.tags.install:
                repo = repos[install.name]

                # We will always have a lock file, so this is fine
                repo_to_lock_file[install.name] = [install.lock_file]
                repo["fail_if_repin_required"] = install.fail_if_repin_required
                repo["fail_on_missing_checksum"] = install.fail_on_missing_checksum
                repo["fetch_javadoc"] = install.fetch_javadoc
                repo["fetch_sources"] = install.fetch_sources
                repo["resolver"] = install.resolver
                repo["strict_visibility"] = install.strict_visibility
                if len(install.repositories):
                    mapped_repos = []
                    for repository in parse.parse_repository_spec_list(install.repositories):
                        repo_string = _json.write_repository_spec(repository)
                        if repo_string not in mapped_repos:
                            mapped_repos.append(repo_string)
                    repo["repositories"] = mapped_repos

                repos[install.name] = repo

    # There should be at most one lock file per `name`
    for repo_name, lock_files in repo_to_lock_file.items():
        if len(lock_files) > 1:
            fail("There can only be one lock file for the repo %s. Lock files seen were %s" % (
                repo_name,
                ", ".join(lock_files),
            ))
        repos.get(repo_name)["lock_file"] = lock_files[0]

    existing_repos = []
    for (name, repo) in repos.items():
        boms = parse.parse_artifact_spec_list(repo.get("boms", []))
        boms_json = [_json.write_artifact_spec(a) for a in boms]
        artifacts = parse.parse_artifact_spec_list(repo["artifacts"])
        artifacts_json = [_json.write_artifact_spec(a) for a in artifacts]
        excluded_artifacts = parse.parse_exclusion_spec_list(repo.get("excluded_artifacts", []))
        excluded_artifacts_json = [_json.write_exclusion_spec(a) for a in excluded_artifacts]

        if len(repo.get("repositories", [])) == 0:
            existing_repos = []
            for repository in parse.parse_repository_spec_list(DEFAULT_REPOSITORIES):
                repo_string = _json.write_repository_spec(repository)
                if repo_string not in existing_repos:
                    existing_repos.append(repo_string)
            repo["repositories"] = existing_repos

        if repo.get("resolver", _DEFAULT_RESOLVER) == "coursier":
            coursier_fetch(
                # Name this repository "unpinned_{name}" if the user specified a
                # maven_install.json file. The actual @{name} repository will be
                # created from the maven_install.json file in the coursier_fetch
                # invocation after this.
                name = "unpinned_" + name if repo.get("lock_file") else name,
                pinned_repo_name = name if repo.get("lock_file") else None,
                user_provided_name = name,
                repositories = repo.get("repositories"),
                artifacts = artifacts_json,
                boms = boms_json,
                fail_on_missing_checksum = repo.get("fail_on_missing_checksum"),
                fetch_sources = repo.get("fetch_sources"),
                fetch_javadoc = repo.get("fetch_javadoc"),
                excluded_artifacts = excluded_artifacts_json,
                generate_compat_repositories = False,
                version_conflict_policy = repo.get("version_conflict_policy"),
                override_targets = overrides,
                strict_visibility = repo.get("strict_visibility"),
                strict_visibility_value = repo.get("strict_visibility_value"),
                use_credentials_from_home_netrc_file = repo.get("use_credentials_from_home_netrc_file"),
                maven_install_json = repo.get("lock_file"),
                resolve_timeout = repo.get("resolve_timeout"),
                use_starlark_android_rules = repo.get("use_starlark_android_rules"),
                aar_import_bzl_label = repo.get("aar_import_bzl_label"),
                duplicate_version_warning = repo.get("duplicate_version_warning"),
                ignore_empty_files = repo.get("ignore_empty_files"),
                additional_coursier_options = repo.get("additional_coursier_options"),
            )
        else:
            workspace_prefix = "@@" if bazel_features.external_deps.is_bzlmod_enabled else "@"

            # Only the coursier resolver allows the lock file to be omitted.
            unpinned_maven_pin_command_alias(
                name = "unpinned_" + name,
                alias = "%s%s//:pin" % (workspace_prefix, name),
            )

        if repo.get("generate_compat_repositories") and not repo.get("lock_file"):
            seen = _generate_compat_repos(name, compat_repos, artifacts)
            compat_repos.extend(seen)

        if repo.get("lock_file"):
            lock_file_content = mctx.read(mctx.path(repo.get("lock_file")))

            if not len(lock_file_content) or contains_git_conflict_markers(repo["lock_file"], lock_file_content):
                lock_file = {
                    "artifacts": {},
                    "dependencies": {},
                    "repositories": {},
                    "version": "2",
                }
            else:
                lock_file = json.decode(lock_file_content)

            if v2_lock_file.is_valid_lock_file(lock_file):
                artifacts = v2_lock_file.get_artifacts(lock_file)
                importer = v2_lock_file
            elif v1_lock_file.is_valid_lock_file(lock_file):
                artifacts = v1_lock_file.get_artifacts(lock_file)
                importer = v1_lock_file
            else:
                fail("Unable to determine lock file version: %s" % repo.get("lock_file"))

            created = download_pinned_deps(mctx = mctx, artifacts = artifacts, http_files = http_files, has_m2local = importer.has_m2local(lock_file))
            existing_repos.extend(created)

            pinned_coursier_fetch(
                name = name,
                user_provided_name = name,
                repositories = repo.get("repositories"),
                boms = boms_json,
                artifacts = artifacts_json,
                fetch_sources = repo.get("fetch_sources"),
                fetch_javadoc = repo.get("fetch_javadoc"),
                resolver = repo.get("resolver", _DEFAULT_RESOLVER),
                generate_compat_repositories = False,
                maven_install_json = repo.get("lock_file"),
                override_targets = overrides,
                strict_visibility = repo.get("strict_visibility"),
                strict_visibility_value = repo.get("strict_visibility_value"),
                additional_netrc_lines = repo.get("additional_netrc_lines"),
                use_credentials_from_home_netrc_file = repo.get("use_credentials_from_home_netrc_file"),
                fail_if_repin_required = repo.get("fail_if_repin_required"),
                use_starlark_android_rules = repo.get("use_starlark_android_rules"),
                aar_import_bzl_label = repo.get("aar_import_bzl_label"),
                duplicate_version_warning = repo.get("duplicate_version_warning"),
                excluded_artifacts = excluded_artifacts_json,
                repin_instructions = repo.get("repin_instructions"),
            )

            if repo.get("generate_compat_repositories"):
                all_artifacts = parse.parse_artifact_spec_list([(a["coordinates"]) for a in artifacts])
                seen = _generate_compat_repos(name, compat_repos, parse.parse_artifact_spec_list([(a["coordinates"]) for a in artifacts]))
                compat_repos.extend(seen)

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        # The type and attributes of repositories created by this extension are fully deterministic
        # and thus don't need to be included in MODULE.bazel.lock.
        # Note: This ignores get_m2local_url, but that depends on local information and environment
        # variables only. In fact, since it depends on the host OS, *not* including the extension
        # result in the lockfile makes it more portable across different machines.
        return mctx.extension_metadata(reproducible = True)
    else:
        return None

maven = module_extension(
    maven_impl,
    tag_classes = {
        "artifact": artifact,
        "install": install,
        "override": override,
    },
)
