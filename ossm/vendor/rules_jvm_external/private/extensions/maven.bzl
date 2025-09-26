load("@bazel_features//:features.bzl", "bazel_features")
load("@bazel_skylib//lib:new_sets.bzl", "sets")
load("//:specs.bzl", "parse", _json = "json")
load("//private:compat_repository.bzl", "compat_repository")
load(
    "//private:coursier_utilities.bzl",
    "contains_git_conflict_markers",
    "escape",
    "strip_packaging_and_classifier_and_version",
)
load("//private/lib:coordinates.bzl", "to_external_form", "unpack_coordinates")
load("//private/lib:toml_parser.bzl", "parse_toml")
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
    doc = "Used to define a single artifact where the simple coordinates are insufficient. Will be added to the other artifacts declared by tags with the same `name` attribute.",
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
    doc = "Combines artifact and bom declarations with setting the location of lock files to use, and repositories to download artifacts from. There can only be one `install` tag with a given `name` per module. `install` tags with the same name across multiple modules will be merged, with the root module taking precedence.",
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
        "resolver": attr.string(doc = "The resolver to use. Only honoured for the root module.", values = ["coursier", "gradle", "maven"], default = _DEFAULT_RESOLVER),

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
        "fail_if_repin_required": attr.bool(doc = "Whether to fail the build if the maven_artifact inputs have changed but the lock file has not been repinned.", default = True),
        "lock_file": attr.label(),
        "repositories": attr.string_list(default = DEFAULT_REPOSITORIES),
        "generate_compat_repositories": attr.bool(
            doc = "Additionally generate repository aliases in a .bzl file for all JAR artifacts. For example, `@maven//:com_google_guava_guava` can also be referenced as `@com_google_guava_guava//jar`.",
        ),
        "known_contributing_modules": attr.string_list(
            doc = "List of Bzlmod modules that are known to be contributing to this repository. Only honoured for the root module.",
            default = [],
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
    doc = "Allows specific maven coordinates to be redirected elsewhere. Commonly used to replace an external dependency with another, or a compatible implementation from within this module.",
    attrs = {
        "name": attr.string(default = DEFAULT_NAME),
        "coordinates": attr.string(doc = "Maven artifact tuple in `artifactId:groupId` format", mandatory = True),
        "target": attr.label(doc = "Target to use in place of maven coordinates", mandatory = True),
    },
)

from_toml = tag_class(
    doc = "Allows a project to import dependencies from a Gradle format `libs.versions.toml` file.",
    attrs = {
        "name": attr.string(default = DEFAULT_NAME),
        "libs_versions_toml": attr.label(doc = "Gradle `libs.versions.toml` file to use", mandatory = True),
        "bom_modules": attr.string_list(doc = "List of modules in `group:artifact` format to treat as BOMs, not artifacts"),
    },
)

amend_artifact = tag_class(
    doc = "Modifies an artifact with `coordinates` defined in other tags with additional properties.",
    attrs = {
        "name": attr.string(default = DEFAULT_NAME),
        "coordinates": attr.string(doc = "Coordinates of the artifact to amend. Only `group:artifact` are used for matching.", mandatory = True),
        "force_version": attr.bool(default = False),
        "neverlink": attr.bool(),
        "testonly": attr.bool(),
        "exclusions": attr.string_list(doc = "Maven artifact tuples, in `artifactId:groupId` format", allow_empty = True),
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
    contributing_module_names = repo_name_2_module_name.get(repo_name, [])
    if module_name in contributing_module_names:
        return
    contributing_module_names.append(module_name)
    repo_name_2_module_name[repo_name] = contributing_module_names

def _warn_if_multiple_contributing_modules(repo_name_2_module_name, repos):
    for (repo_name, contributing_module_names) in repo_name_2_module_name.items():
        if len(contributing_module_names) == 1:
            continue
        known_contributing_modules = repos[repo_name].get("known_contributing_modules", sets.make())
        new_contributing_modules = sets.difference(sets.make(contributing_module_names), known_contributing_modules)
        if sets.length(new_contributing_modules) == 0:
            continue
        print("The maven repository '%s' has contributions from multiple bzlmod modules, and will be resolved together: %s." % (
                  repo_name,
                  sorted(contributing_module_names),
              ) + "\nSee https://github.com/bazel-contrib/rules_jvm_external/blob/master/docs/bzlmod.md#module-dependency-layering" +
              " for more information. \n" +
              " To suppress this warning review the contributions from the other modules and add the following attribute" +
              " in the root MODULE.bazel file: \n" +
              "maven.install(\n" +
              ("  name = \"{0}\"\n".format(repo_name) if repo_name != DEFAULT_NAME else "") +
              "  known_contributing_modules = {0},\n".format(sorted(contributing_module_names)) +
              "  ...\n" +
              ")")

def _generate_compat_repos(name, existing_compat_repos, artifacts):
    seen = []

    for artifact in artifacts:
        coords = to_external_form(artifact)
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

def _get_maven_module(artifact_spec):
    """Extract group:artifact (maven module) coordinates from an artifact spec."""
    return "%s:%s" % (artifact_spec.group, artifact_spec.artifact)

def _find_duplicate_artifacts_across_submodules(non_root_artifacts, root_maven_modules):
    """Find artifacts that appear in multiple sub-modules with detailed version info."""

    # Group artifacts by maven module (group:artifact)
    maven_module_to_versions = {}

    for artifact in non_root_artifacts:
        maven_module = _get_maven_module(artifact)
        if maven_module not in maven_module_to_versions:
            maven_module_to_versions[maven_module] = []

        # Extract version information
        version = getattr(artifact, "version", "unspecified")
        maven_module_to_versions[maven_module].append(version)

    # Find duplicates that aren't overridden by root
    duplicates = {}
    for maven_module, versions in maven_module_to_versions.items():
        # Only warn if:
        # 1. There are multiple artifacts with the same group:artifact
        # 2. This maven module is NOT overridden by the root module
        if len(versions) > 1 and maven_module not in root_maven_modules:
            # Deduplicate versions for cleaner output
            unique_versions = {v: True for v in versions}.keys()
            duplicates[maven_module] = sorted(unique_versions)

    return duplicates

def _deduplicate_artifacts_with_root_priority(root_artifacts, non_root_artifacts):
    """Deduplicate artifacts, giving priority to root module artifacts."""

    # Collect maven modules from root artifacts (handle mixed types)
    root_maven_modules = []
    for artifact in root_artifacts:
        maven_module = _get_maven_module(artifact)
        if maven_module not in root_maven_modules:
            root_maven_modules.append(maven_module)

    # Find duplicates across sub-modules that aren't overridden by root
    duplicate_submodule_artifacts = _find_duplicate_artifacts_across_submodules(
        non_root_artifacts,
        root_maven_modules,
    )

    # Filter non-root artifacts that conflict with root artifacts
    filtered_non_root = []
    for artifact in non_root_artifacts:
        maven_module = _get_maven_module(artifact)
        if not maven_module in root_maven_modules:
            filtered_non_root.append(artifact)

    # Log detailed warning for duplicate sub-module artifacts
    if len(duplicate_submodule_artifacts):
        warning_parts = []
        for maven_module, versions in duplicate_submodule_artifacts.items():
            if len(versions) > 1:
                warning_parts.append("%s (versions: %s)" % (maven_module, ", ".join(versions)))
            else:
                warning_parts.append(maven_module)

        print("WARNING: The following maven modules appear in multiple sub-modules with potentially different versions. " +
              "Consider adding one of these to your root module to ensure consistent versions:\n\t%s" %
              "\n\t".join(sorted(warning_parts)))

    return root_artifacts + filtered_non_root

def _amend_artifact(original_artifact, amend):
    """Apply amendments to an artifact struct, returning a new amended struct."""

    # Handle exclusions by merging with existing ones
    existing_exclusions = getattr(original_artifact, "exclusions", []) or []
    final_exclusions = existing_exclusions
    if amend.exclusions:
        new_exclusions = _add_exclusions(amend.exclusions)
        final_exclusions = existing_exclusions + new_exclusions

    # Create new struct with amendments applied
    return struct(
        group = original_artifact.group,
        artifact = original_artifact.artifact,
        version = getattr(original_artifact, "version", None),
        packaging = getattr(original_artifact, "packaging", None),
        classifier = getattr(original_artifact, "classifier", None),
        force_version = amend.force_version if amend.force_version else getattr(original_artifact, "force_version", None),
        neverlink = amend.neverlink if amend.neverlink else getattr(original_artifact, "neverlink", None),
        testonly = amend.testonly if amend.testonly else getattr(original_artifact, "testonly", None),
        exclusions = final_exclusions if final_exclusions else None,
    )

def _coordinates_match(artifact, coordinates):
    """Check if an artifact's `group` and `artifact` matches the given coordinate string."""
    coords = unpack_coordinates(coordinates)
    return (artifact.group == coords.group and
            artifact.artifact == coords.artifact)

def _process_gradle_versions_file(parsed, bom_modules):
    artifacts = []
    boms = []

    for value in parsed.get("libraries", {}).values():
        if not "module" in value.keys():
            continue
        coords = value["module"]

        if "version.ref" in value.keys():
            version = parsed.get("versions", {}).get(value["version.ref"])
            if not version:
                fail("Unable to resolve version.ref %s" % value["version.ref"])
            coords += ":%s" % version
        elif "version" in value.keys():
            coords += ":%s" % value["version"]

        if value["module"] in bom_modules:
            boms.append(unpack_coordinates(coords))
        else:
            artifacts.append(unpack_coordinates(coords))

    return artifacts, boms

def _process_module_tags(mctx, mod, target_repos, repo_name_2_module_name):
    """Process artifact and install tags for a single module."""

    # Process from_toml tags
    for from_toml_tag in mod.tags.from_toml:
        _check_repo_name(repo_name_2_module_name, from_toml_tag.name, mod.name)

        repo = target_repos.get(from_toml_tag.name, {})

        content = mctx.read(mctx.path(from_toml_tag.libs_versions_toml))
        parsed = parse_toml(content)

        (new_artifacts, new_boms) = _process_gradle_versions_file(parsed, from_toml_tag.bom_modules)

        repo["artifacts"] = repo.get("artifacts", []) + new_artifacts
        repo["boms"] = repo.get("boms", []) + new_boms
        target_repos[from_toml_tag.name] = repo

    for artifact in mod.tags.artifact:
        _check_repo_name(repo_name_2_module_name, artifact.name, mod.name)

        repo = target_repos.get(artifact.name, {})
        existing_artifacts = repo.get("artifacts", [])
        existing_artifacts.append(struct(
            group = artifact.group,
            artifact = artifact.artifact,
            version = artifact.version,
            packaging = artifact.packaging,
            classifier = artifact.classifier,
            force_version = artifact.force_version,
            neverlink = artifact.neverlink,
            testonly = artifact.testonly,
            exclusions = _add_exclusions(artifact.exclusions),
        ))

        repo["artifacts"] = existing_artifacts
        target_repos[artifact.name] = repo

    for install in mod.tags.install:
        _check_repo_name(repo_name_2_module_name, install.name, mod.name)

        repo = target_repos.get(install.name, {})

        repo["resolver"] = install.resolver

        artifacts = repo.get("artifacts", [])
        repo["artifacts"] = artifacts + [unpack_coordinates(a) for a in install.artifacts]

        boms = repo.get("boms", [])
        repo["boms"] = boms + [unpack_coordinates(b) for b in install.boms]

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
            repo["known_contributing_modules"] = sets.make(install.known_contributing_modules)

        repo["additional_coursier_options"] = repo.get("additional_coursier_options", []) + getattr(install, "additional_coursier_options", [])

        target_repos[install.name] = repo

    # Process amend_artifact tags
    for amend in mod.tags.amend_artifact:
        _check_repo_name(repo_name_2_module_name, amend.name, mod.name)

        repo = target_repos.get(amend.name, {})
        artifacts = repo.get("artifacts", [])

        # Find matching artifacts and amend them
        amended = False
        for i, artifact in enumerate(artifacts):
            if _coordinates_match(artifact, amend.coordinates):
                artifacts[i] = _amend_artifact(artifact, amend)
                amended = True

        if not amended:
            # If no matching artifact found, this might be an error or we could create a placeholder
            fail("No artifact found matching coordinates '%s' for amendment" % amend.coordinates)

        repo["artifacts"] = artifacts
        target_repos[amend.name] = repo

def _merge_repo_lists(root_list, non_root_list):
    """Merge two lists, removing duplicates while preserving order, root items first."""
    seen = []
    merged_list = []
    for item in root_list + non_root_list:
        if item not in seen:
            merged_list.append(item)
            seen.append(item)
    return merged_list

def remove_fields(s):
    """Used for reducing an artifact struct down to only those fields that have values"""
    return {
        k: getattr(s, k)
        for k in dir(s)
        if k != "to_json" and k != "to_proto" and getattr(s, k, None)
    } | {"version": getattr(s, "version", "")}

def maven_impl(mctx):
    repos = {}
    overrides = {}
    http_files = []
    compat_repos = []

    # Separate tracking for root vs non-root artifacts
    root_module_repos = {}
    non_root_module_repos = {}
    repo_name_2_module_name = {}

    # Process overrides first (they don't need deduplication)
    # The order of the transitive overrides do not matter, but the root
    # overrides take precedence over all transitive ones.
    for idx, mod in enumerate(reversed(mctx.modules)):
        # Rotate the root module to the last to be visited.
        is_root_module = idx == (len(mctx.modules) - 1)
        for override in mod.tags.override:
            if not override.name in overrides:
                overrides[override.name] = {}
            value = str(override.target)
            if is_root_module:
                # Allow the root module's overrides to take precedence over any transitive overrides.
                to_use = value
            else:
                current = overrides[override.name].get(override.coordinates)
                to_use = _fail_if_different("Target of override for %s" % override.coordinates, current, value, [None])
            overrides[override.name].update({override.coordinates: to_use})

    # First pass: process the module tags, but keep root and non-root modules separately
    for mod in mctx.modules:
        collection = root_module_repos if mod.is_root else non_root_module_repos
        _process_module_tags(mctx, mod, collection, repo_name_2_module_name)

    # Second pass: merge and deduplicate repositories
    all_repo_names = {name: True for name in root_module_repos.keys() + non_root_module_repos.keys()}.keys()

    for repo_name in all_repo_names:
        root_repo = root_module_repos.get(repo_name, {})
        non_root_repo = non_root_module_repos.get(repo_name, {})

        # Start with non-root repo as base, then override with root repo settings
        merged_repo = {}
        merged_repo.update(non_root_repo)
        merged_repo.update(root_repo)

        # Special handling for artifacts and boms - deduplicate with root priority
        root_artifacts = root_repo.get("artifacts", [])
        non_root_artifacts = non_root_repo.get("artifacts", [])
        merged_repo["artifacts"] = _deduplicate_artifacts_with_root_priority(
            root_artifacts,
            non_root_artifacts,
        )

        root_boms = root_repo.get("boms", [])
        non_root_boms = non_root_repo.get("boms", [])
        merged_repo["boms"] = _deduplicate_artifacts_with_root_priority(
            root_boms,
            non_root_boms,
        )

        # For list attributes, concatenate but avoid duplicates (root items first)
        for list_attr in ["repositories", "excluded_artifacts", "additional_netrc_lines", "additional_coursier_options"]:
            root_list = root_repo.get(list_attr, [])
            non_root_list = non_root_repo.get(list_attr, [])
            merged_repo[list_attr] = _merge_repo_lists(root_list, non_root_list)

        repos[repo_name] = merged_repo

    # Warn users if multiple modules contribute to the same maven `name`
    _warn_if_multiple_contributing_modules(repo_name_2_module_name, repos)

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
        boms_json = [json.encode(remove_fields(b)) for b in repo.get("boms", [])]
        artifacts_json = [json.encode(remove_fields(a)) for a in repo.get("artifacts", [])]

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
                override_targets = overrides.get(name),
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

        if repo.get("generate_compat_repositories"):
            seen = _generate_compat_repos(name, compat_repos, repo.get("artifacts", []))
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
                override_targets = overrides.get(name),
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
                # Convert lock file artifacts (which are dicts) to structs
                lock_file_artifacts = [unpack_coordinates(a["coordinates"]) for a in artifacts]
                seen = _generate_compat_repos(name, compat_repos, lock_file_artifacts)
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
        "amend_artifact": amend_artifact,
        "artifact": artifact,
        "from_toml": from_toml,
        "install": install,
        "override": override,
    },
)
