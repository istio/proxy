load("@rules_java//java:defs.bzl", "java_library")
load(":javadoc.bzl", "javadoc")
load(":maven_bom_fragment.bzl", "maven_bom_fragment")
load(":maven_project_jar.bzl", "DEFAULT_EXCLUDED_WORKSPACES", "maven_project_jar")
load(":maven_publish.bzl", "maven_publish")
load(":pom_file.bzl", "pom_file")

def java_export(
        name,
        maven_coordinates,
        manifest_entries = {},
        deploy_env = [],
        excluded_workspaces = {name: None for name in DEFAULT_EXCLUDED_WORKSPACES},
        pom_template = None,
        visibility = None,
        tags = [],
        testonly = None,
        classifier_artifacts = {},
        **kwargs):
    """Extends `java_library` to allow maven artifacts to be uploaded.

    This macro can be used as a drop-in replacement for `java_library`, but
    also generates an implicit `name.publish` target that can be run to publish
    maven artifacts derived from this macro to a maven repository. The publish
    rule understands the following variables (declared using `--define` when
    using `bazel run`, or as environment variables in ALL_CAPS form):

      * `maven_repo`: A URL for the repo to use. May be "https" or "file". Can also be set with environment variable `MAVEN_REPO`.
      * `maven_user`: The user name to use when uploading to the maven repository. Can also be set with environment variable `MAVEN_USER`.
      * `maven_password`: The password to use when uploading to the maven repository. Can also be set with environment variable `MAVEN_PASSWORD`.


    This macro also generates a `name-pom` target that creates the `pom.xml` file
    associated with the artifacts. The template used is derived from the (optional)
    `pom_template` argument, and the following substitutions are performed on
    the template file:

      * `{groupId}`: Replaced with the maven coordinates group ID.
      * `{artifactId}`: Replaced with the maven coordinates artifact ID.
      * `{version}`: Replaced by the maven coordinates version.
      * `{type}`: Replaced by the maven coordinates type, if present (defaults to "jar")
      * `{scope}`: Replaced by the maven coordinates type, if present (defaults to "compile")
      * `{dependencies}`: Replaced by a list of maven dependencies directly relied upon
        by java_library targets within the artifact.

    The "edges" of the artifact are found by scanning targets that contribute to
    runtime dependencies for the following tags:

      * `maven_coordinates=group:artifact:type:version`: Specifies a dependency of
        this artifact.
      * `maven:compile-only`: Specifies that this dependency should not be listed
        as a dependency of the artifact being generated.

    To skip generation of the javadoc jar, add the `no-javadocs` tag to the target.

    Generated rules:
      * `name`: A `java_library` that other rules can depend upon.
      * `name-docs`: A javadoc jar file.
      * `name-pom`: The pom.xml file.
      * `name.publish`: To be executed by `bazel run` to publish to a maven repo.

    Args:
      name: A unique name for this target
      maven_coordinates: The maven coordinates for this target.
      pom_template: The template to be used for the pom.xml file.
      manifest_entries: A dict of `String: String` containing additional manifest entry attributes and values.
      deploy_env: A list of labels of Java targets to exclude from the generated jar.
        [`java_binary`](https://bazel.build/reference/be/java#java_binary) targets are *not*
        supported.
      excluded_workspaces: A dict of strings representing the workspace names of artifacts
        that should not be included in the maven jar to a `Label` pointing to the dependency
        that workspace should be replaced by, or `None` if the exclusion shouldn't be replaced
        with an extra dependency.
      classifier_artifacts: A dict of classifier -> artifact of additional artifacts to publish to Maven.
      doc_deps: Other `javadoc` targets that are referenced by the generated `javadoc` target
        (if not using `tags = ["no-javadoc"]`)
      doc_url: The URL at which the generated `javadoc` will be hosted (if not using
        `tags = ["no-javadoc"]`).
      visibility: The visibility of the target
      kwargs: These are passed to [`java_library`](https://bazel.build/reference/be/java#java_library),
        and so may contain any valid parameter for that rule.
    """

    maven_coordinates_tags = ["maven_coordinates=%s" % maven_coordinates]
    lib_name = "%s-lib" % name

    javadocopts = kwargs.pop("javadocopts", None)
    doc_deps = kwargs.pop("doc_deps", [])
    doc_url = kwargs.pop("doc_url", "")
    doc_resources = kwargs.pop("doc_resources", [])
    toolchains = kwargs.pop("toolchains", [])

    # java_library doesn't allow srcs without deps, but users may try to specify deps rather than
    # runtime_deps on java_export to indicate that the generated POM should list the deps as compile
    # deps.
    if kwargs.get("deps") and not kwargs.get("srcs"):
        fail("deps not allowed without srcs; move to runtime_deps (for 'runtime' scope in the generated POM) or exports (for 'compile' scope)")

    # Construct the java_library we'll export from here.
    java_library(
        name = lib_name,
        visibility = visibility,
        tags = tags + maven_coordinates_tags,
        testonly = testonly,
        **kwargs
    )

    maven_export(
        name = name,
        maven_coordinates = maven_coordinates,
        lib_name = lib_name,
        manifest_entries = manifest_entries,
        deploy_env = deploy_env,
        excluded_workspaces = excluded_workspaces,
        pom_template = pom_template,
        visibility = visibility,
        tags = tags,
        testonly = testonly,
        javadocopts = javadocopts,
        classifier_artifacts = classifier_artifacts,
        doc_deps = doc_deps,
        doc_url = doc_url,
        doc_resources = doc_resources,
        toolchains = toolchains,
    )

def maven_export(
        name,
        maven_coordinates,
        lib_name,
        manifest_entries = {},
        deploy_env = [],
        excluded_workspaces = {},
        pom_template = None,
        visibility = None,
        tags = [],
        testonly = False,
        javadocopts = None,
        classifier_artifacts = {},
        *,
        doc_deps = [],
        doc_url = "",
        doc_resources = [],
        toolchains = None):
    """
    All arguments are the same as java_export with the addition of:
      lib_name: Name of the library that has been built.
      javadocopts: The options to be used for javadocs.

    This macro is used by java_export and kt_jvm_export to generate implicit `name.publish`
    targets to publish maven artifacts derived from this macro to a maven repository.

    The publish rule understands the following variables (declared using `--define` when
    using `bazel run`):

      * `maven_repo`: A URL for the repo to use. May be "https" or "file".
      * `maven_user`: The user name to use when uploading to the maven repository.
      * `maven_password`: The password to use when uploading to the maven repository.

    This macro also generates a `name-pom` target that creates the `pom.xml` file
    associated with the artifacts. The template used is derived from the (optional)
    `pom_template` argument, and the following substitutions are performed on
    the template file:

      * `{groupId}`: Replaced with the maven coordinates group ID.
      * `{artifactId}`: Replaced with the maven coordinates artifact ID.
      * `{version}`: Replaced by the maven coordinates version.
      * `{type}`: Replaced by the maven coordinates type, if present (defaults to "jar")
      * `{scope}`: Replaced by the maven coordinates type, if present (defaults to "compile")
      * `{dependencies}`: Replaced by a list of maven dependencies directly relied upon
        by java_library targets within the artifact.

    The "edges" of the artifact are found by scanning targets that contribute to
    runtime dependencies for the following tags:

      * `maven_coordinates=group:artifact:type:version`: Specifies a dependency of
        this artifact.
      * `maven:compile-only`: Specifies that this dependency should not be listed
        as a dependency of the artifact being generated.

    Generated rules:
      * `name-docs`: A javadoc jar file.
      * `name-pom`: The pom.xml file.
      * `name.publish`: To be executed by `bazel run` to publish to a maven repo.

    Args:
      name: A unique name for this target
      maven_coordinates: The maven coordinates for this target.
      pom_template: The template to be used for the pom.xml file.
      manifest_entries: A dict of `String: String` containing additional manifest entry attributes and values.
      deploy_env: A list of labels of Java targets to exclude from the generated jar.
        [`java_binary`](https://bazel.build/reference/be/java#java_binary) targets are *not*
        supported.
      excluded_workspaces: A dict of strings representing the workspace names of artifacts
        that should not be included in the maven jar to a `Label` pointing to the dependency
        that workspace should be replaced by, or `None` if the exclusion shouldn't be replaced
        with an extra dependency.
      doc_deps: Other `javadoc` targets that are referenced by the generated `javadoc` target
        (if not using `tags = ["no-javadoc"]`)
      doc_url: The URL at which the generated `javadoc` will be hosted (if not using
        `tags = ["no-javadoc"]`).
      doc_resources: Resources to be included in the javadoc jar.
      visibility: The visibility of the target
      kwargs: These are passed to [`java_library`](https://bazel.build/reference/be/java#java_library),
        and so may contain any valid parameter for that rule.
    """
    maven_coordinates_tags = ["maven_coordinates=%s" % maven_coordinates]

    # Sometimes users pass `None` as the value for attributes. Guard against this
    manifest_entries = manifest_entries if manifest_entries else {}
    deploy_env = deploy_env if deploy_env else []
    excluded_workspaces = excluded_workspaces if excluded_workspaces else {}
    doc_url = doc_url if doc_url else ""
    doc_deps = doc_deps if doc_deps else []
    tags = tags if tags else []
    classifier_artifacts = classifier_artifacts if classifier_artifacts else {}

    additional_dependencies = {label: name for (name, label) in excluded_workspaces.items() if label}

    classifier_artifacts = dict(classifier_artifacts)  # unfreeze

    # Merge the jars to create the maven project jar
    maven_project_jar(
        name = "%s-project" % name,
        target = ":%s" % lib_name,
        maven_coordinates = maven_coordinates,
        manifest_entries = manifest_entries,
        deploy_env = deploy_env,
        excluded_workspaces = excluded_workspaces.keys(),
        additional_dependencies = additional_dependencies,
        visibility = visibility,
        tags = tags + maven_coordinates_tags,
        testonly = testonly,
        toolchains = toolchains,
    )

    native.filegroup(
        name = "%s-maven-artifact" % name,
        srcs = [
            ":%s-project" % name,
        ],
        output_group = "maven_artifact",
        visibility = visibility,
        tags = tags,
        testonly = testonly,
    )

    if not "no-sources" in tags:
        native.filegroup(
            name = "%s-maven-source" % name,
            srcs = [
                ":%s-project" % name,
            ],
            output_group = "maven_source",
            visibility = visibility,
            tags = tags,
            testonly = testonly,
        )
        classifier_artifacts.setdefault("sources", ":%s-maven-source" % name)

    docs_jar = None
    if not "no-javadocs" in tags:
        docs_jar = "%s-docs" % name
        javadoc(
            name = docs_jar,
            deps = [
                ":%s-project" % name,
            ] + deploy_env,
            javadocopts = javadocopts,
            doc_deps = doc_deps,
            doc_url = doc_url,
            doc_resources = doc_resources,
            excluded_workspaces = excluded_workspaces.keys(),
            additional_dependencies = additional_dependencies,
            visibility = visibility,
            tags = tags,
            testonly = testonly,
            toolchains = toolchains,
        )
        classifier_artifacts.setdefault("javadoc", docs_jar)

    pom_file(
        name = "%s-pom" % name,
        target = ":%s" % lib_name,
        pom_template = pom_template,
        additional_dependencies = additional_dependencies,
        visibility = visibility,
        tags = tags,
        testonly = testonly,
        toolchains = toolchains,
    )

    maven_publish(
        name = "%s.publish" % name,
        coordinates = maven_coordinates,
        pom = "%s-pom" % name,
        artifact = ":%s-maven-artifact" % name,
        classifier_artifacts = {v: k for (k, v) in classifier_artifacts.items() if v},
        visibility = visibility,
        tags = tags,
        testonly = testonly,
        toolchains = toolchains,
    )

    # We may want to aggregate several `java_export` targets into a single Maven BOM POM
    # https://maven.apache.org/guides/introduction/introduction-to-dependency-mechanism.html#bill-of-materials-bom-poms
    maven_bom_fragment(
        name = "%s.bom-fragment" % name,
        maven_coordinates = maven_coordinates,
        artifact = ":%s" % lib_name,
        src_artifact = ":%s-maven-source" % name,
        javadoc_artifact = None if "no-javadocs" in tags else ":%s-docs" % name,
        pom = ":%s-pom" % name,
        testonly = testonly,
        tags = tags,
        visibility = visibility,
        toolchains = toolchains,
    )

    # Finally, alias the primary output
    native.alias(
        name = name,
        actual = ":%s-project" % name,
        visibility = visibility,
        tags = tags,
        testonly = testonly,
    )
