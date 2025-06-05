load("//:specs.bzl", "parse", _json = "json")
load("//private:constants.bzl", "DEFAULT_REPOSITORY_NAME")
load("//private/rules:coursier.bzl", "DEFAULT_AAR_IMPORT_LABEL", "coursier_fetch", "pinned_coursier_fetch")
load("//private/rules:generate_pin_repository.bzl", "generate_pin_repository")

def maven_install(
        name = DEFAULT_REPOSITORY_NAME,
        repositories = [],
        artifacts = [],
        boms = [],
        resolver = "coursier",
        fail_on_missing_checksum = True,
        fetch_sources = False,
        fetch_javadoc = False,
        excluded_artifacts = [],
        generate_compat_repositories = False,
        version_conflict_policy = "default",
        maven_install_json = None,
        override_targets = {},
        strict_visibility = False,
        strict_visibility_value = ["//visibility:private"],
        resolve_timeout = 600,
        additional_netrc_lines = [],
        use_credentials_from_home_netrc_file = False,
        fail_if_repin_required = False,
        use_starlark_android_rules = False,
        aar_import_bzl_label = DEFAULT_AAR_IMPORT_LABEL,
        duplicate_version_warning = "warn",
        repin_instructions = None,
        ignore_empty_files = False,
        additional_coursier_options = []):
    """Resolves and fetches artifacts transitively from Maven repositories.

    This macro runs a repository rule that invokes the Coursier CLI to resolve
    and fetch Maven artifacts transitively.

    Args:
      name: A unique name for this Bazel external repository.
      repositories: A list of Maven repository URLs, specified in lookup order.

        Supports URLs with HTTP Basic Authentication, e.g. "https://username:password@example.com".
      boms: A list of Maven artifact coordinates in the form of `group:artifact:version` which refer to Maven BOMs.
      artifacts: A list of Maven artifact coordinates in the form of `group:artifact:version`.
      resolver: Which resolver to use. One of `coursier`, or `maven`.
      fail_on_missing_checksum: fail the fetch if checksum attributes are not present.
      fetch_sources: Additionally fetch source JARs.
      fetch_javadoc: Additionally fetch javadoc JARs.
      excluded_artifacts: A list of Maven artifact coordinates in the form of `group:artifact` to be
        excluded from the transitive dependencies.
      generate_compat_repositories: Additionally generate repository aliases in a .bzl file for all JAR
        artifacts. For example, `@maven//:com_google_guava_guava` can also be referenced as
        `@com_google_guava_guava//jar`.
      version_conflict_policy: Policy for user-defined vs. transitive dependency version
        conflicts.  If "pinned", choose the user's version unconditionally.  If "default", follow
        Coursier's default policy.
      maven_install_json: A label to a `maven_install.json` file to use pinned artifacts for generating
        build targets. e.g `//:maven_install.json`.
      override_targets: A mapping of `group:artifact` to Bazel target labels. All occurrences of the
        target label for `group:artifact` will be an alias to the specified label, therefore overriding
        the original generated `jvm_import` or `aar_import` target.
      strict_visibility: Controls visibility of transitive dependencies. If `True`, transitive dependencies
        are private and invisible to user's rules. If `False`, transitive dependencies are public and
        visible to user's rules.
      strict_visibility_value: Allows changing transitive dependencies strict visibility scope from private
        to specified scopes list.
      resolve_timeout: The execution timeout of resolving and fetching artifacts.
      additional_netrc_lines: Additional lines prepended to the netrc file used by `http_file` (with `maven_install_json` only).
      use_credentials_from_home_netrc_file: Whether to pass machine login credentials from the ~/.netrc file to coursier.
      fail_if_repin_required: Whether to fail the build if the required maven artifacts have been changed but not repinned. Requires the `maven_install_json` to have been set.
      use_starlark_android_rules: Whether to use the native or Starlark version
        of the Android rules. Default is False.
      aar_import_bzl_label: The label (as a string) to use to import aar_import
        from. This is usually needed only if the top-level workspace file does
        not use the typical default repository name to import the Android
        Starlark rules. Default is
        "@build_bazel_rules_android//rules:rules.bzl".
      duplicate_version_warning: What to do if an artifact is specified multiple times. If "error" then
        fail the build, if "warn" then print a message and continue, if "none" then do nothing. The default
        is "warn".
      repin_instructions: Instructions to re-pin dependencies in your repository. Will be shown when re-pinning is required.
      ignore_empty_files: Treat jars that are empty as if they were not found.
      additional_coursier_options: Additional options that will be passed to coursier.
    """
    if resolver != "coursier" and not maven_install_json:
        fail("Only the coursier resolver supports build time resolution. Please set `maven_install_json`. An empty file will work.")

    repositories_json_strings = []
    for repository in parse.parse_repository_spec_list(repositories):
        repositories_json_strings.append(_json.write_repository_spec(repository))

    artifacts_json_strings = []
    for artifact in parse.parse_artifact_spec_list(artifacts):
        artifacts_json_strings.append(_json.write_artifact_spec(artifact))

    boms_json_strings = []
    for bom in parse.parse_artifact_spec_list(boms):
        boms_json_strings.append(_json.write_artifact_spec(bom))

    excluded_artifacts_json_strings = []
    for exclusion in parse.parse_exclusion_spec_list(excluded_artifacts):
        excluded_artifacts_json_strings.append(_json.write_exclusion_spec(exclusion))

    if additional_netrc_lines and maven_install_json == None:
        fail("`additional_netrc_lines` is only supported with `maven_install_json` specified", "additional_netrc_lines")

    # The first coursier_fetch generates the @unpinned_maven
    # repository, which executes Coursier.
    #
    # The second coursier_fetch generates the @maven repository generated from
    # maven_install.json.
    #
    # We don't want the two repositories to have edges between them. This allows users
    # to update the maven_install() declaration in the WORKSPACE, run
    # @unpinned_maven//:pin / Coursier to update maven_install.json, and bazel build
    # //... immediately after with the updated artifacts.
    if resolver == "coursier":
        coursier_fetch(
            # Name this repository "unpinned_{name}" if the user specified a
            # maven_install.json file. The actual @{name} repository will be
            # created from the maven_install.json file in the coursier_fetch
            # invocation after this.
            name = name if maven_install_json == None else "unpinned_" + name,
            pinned_repo_name = None if maven_install_json == None else name,
            repositories = repositories_json_strings,
            artifacts = artifacts_json_strings,
            boms = boms_json_strings,
            fail_on_missing_checksum = fail_on_missing_checksum,
            fetch_sources = fetch_sources,
            fetch_javadoc = fetch_javadoc,
            excluded_artifacts = excluded_artifacts_json_strings,
            generate_compat_repositories = generate_compat_repositories,
            version_conflict_policy = version_conflict_policy,
            override_targets = override_targets,
            strict_visibility = strict_visibility,
            strict_visibility_value = strict_visibility_value,
            maven_install_json = maven_install_json,
            resolve_timeout = resolve_timeout,
            use_credentials_from_home_netrc_file = use_credentials_from_home_netrc_file,
            use_starlark_android_rules = use_starlark_android_rules,
            aar_import_bzl_label = aar_import_bzl_label,
            duplicate_version_warning = duplicate_version_warning,
            ignore_empty_files = ignore_empty_files,
            additional_coursier_options = additional_coursier_options,
        )

    else:
        generate_pin_repository(
            name = "unpinned_" + name,
            unpinned_name = name,
        )

    if maven_install_json != None:
        # Create the repository generated from a maven_install.json file.
        pinned_coursier_fetch(
            name = name,
            resolver = resolver,
            repositories = repositories_json_strings,
            artifacts = artifacts_json_strings,
            boms = boms_json_strings,
            maven_install_json = maven_install_json,
            fetch_sources = fetch_sources,
            fetch_javadoc = fetch_javadoc,
            generate_compat_repositories = generate_compat_repositories,
            override_targets = override_targets,
            strict_visibility = strict_visibility,
            strict_visibility_value = strict_visibility_value,
            additional_netrc_lines = additional_netrc_lines,
            fail_if_repin_required = fail_if_repin_required,
            duplicate_version_warning = duplicate_version_warning,
            use_credentials_from_home_netrc_file = use_credentials_from_home_netrc_file,
            use_starlark_android_rules = use_starlark_android_rules,
            aar_import_bzl_label = aar_import_bzl_label,
            repin_instructions = repin_instructions,
            # Extra arguments only used for hash generation
            excluded_artifacts = excluded_artifacts_json_strings,
        )
