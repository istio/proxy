load("@bazel_features//:features.bzl", "bazel_features")
load("//private/lib:coordinates.bzl", "unpack_coordinates")
load(":maven_bom_fragment.bzl", "MavenBomFragmentInfo")
load(":maven_publish.bzl", "maven_publish")
load(":maven_utils.bzl", "generate_pom")

def _label(label_or_string):
    if type(label_or_string) == "Label":
        return label_or_string

    workspace_prefix = "@@" if bazel_features.external_deps.is_bzlmod_enabled else "@"

    if type(label_or_string) == "string":
        # We may have a target of the form: `@bar//foo`, `//foo`, `//foo:foo`, `:foo`, `foo`
        if label_or_string.startswith("@"):
            # We have an absolute target. Great!
            return Label(label_or_string)
        elif label_or_string.startswith("//"):
            return Label("%s%s" % (workspace_prefix, label_or_string))
        else:
            if label_or_string.startswith(":"):
                label_or_string = label_or_string[1:]
            return Label("%s//%s:%s" % (workspace_prefix, native.package_name(), label_or_string))

    fail("Can only convert either labels or strings: %s" % label_or_string)

def _maven_bom_impl(ctx):
    fragments = [f[MavenBomFragmentInfo] for f in ctx.attr.fragments]
    dep_coordinates = [f.coordinates for f in fragments]

    # Expand maven coordinates for any variables to be replaced.
    coordinates = ctx.expand_make_variables("coordinates", ctx.attr.maven_coordinates, {})

    bom = generate_pom(
        ctx,
        coordinates = coordinates,
        is_bom = True,
        versioned_dep_coordinates = dep_coordinates,
        pom_template = ctx.file.pom_template,
        out_name = "%s.xml" % ctx.label.name,
    )

    return [
        DefaultInfo(files = depset([bom])),
    ]

_maven_bom = rule(
    _maven_bom_impl,
    doc = """Create a Maven BOM file (`pom.xml`) for the given targets.""",
    attrs = {
        "maven_coordinates": attr.string(
            mandatory = True,
        ),
        "pom_template": attr.label(
            doc = "Template file to use for the BOM pom.xml",
            default = "//private/templates:bom.tpl",
            allow_single_file = True,
        ),
        "fragments": attr.label_list(
            providers = [
                [MavenBomFragmentInfo],
            ],
        ),
    },
)

def _maven_dependencies_bom_impl(ctx):
    fragments = [f[MavenBomFragmentInfo] for f in ctx.attr.fragments]

    # We want to include all the dependencies that aren't
    # included in the main BOM
    first_order_deps = [f[MavenBomFragmentInfo].coordinates for f in ctx.attr.fragments]
    all_deps = depset(transitive = [f.maven_info.maven_deps for f in fragments]).to_list()
    combined_deps = [a for a in all_deps if a not in first_order_deps]

    unpacked = unpack_coordinates(ctx.attr.bom_coordinates)
    dependencies_bom = generate_pom(
        ctx,
        coordinates = ctx.attr.maven_coordinates,
        is_bom = True,
        versioned_dep_coordinates = combined_deps + ["%s:%s:%s@pom" % (unpacked.group, unpacked.artifact, unpacked.version)],
        pom_template = ctx.file.pom_template,
        out_name = "%s.xml" % ctx.label.name,
        indent = 12,
    )

    return [
        DefaultInfo(files = depset([dependencies_bom])),
    ]

_maven_dependencies_bom = rule(
    _maven_dependencies_bom_impl,
    doc = """Create a Maven dependencies `pom.xml` for the given targets.""",
    attrs = {
        "maven_coordinates": attr.string(
            mandatory = True,
        ),
        "pom_template": attr.label(
            doc = "Template file to use for the pom.xml",
            default = "//private/templates:dependencies-bom.tpl",
            allow_single_file = True,
        ),
        "fragments": attr.label_list(
            providers = [
                [MavenBomFragmentInfo],
            ],
        ),
        "bom_coordinates": attr.string(
            doc = "Coordinates of the bom to include",
            mandatory = True,
        ),
    },
)

def maven_bom(
        name,
        maven_coordinates,
        java_exports,
        bom_pom_template = None,
        dependencies_maven_coordinates = None,
        dependencies_pom_template = None,
        tags = None,
        testonly = None,
        visibility = None,
        toolchains = []):
    """Generates a Maven BOM `pom.xml` file and an optional "dependencies" `pom.xml`.

    The generated BOM will contain a list of all the coordinates of the
    `java_export` targets in the `java_exports` parameters. An optional
    dependencies artifact will be created if the parameter
    `dependencies_maven_coordinates` is set.

    Both the BOM and dependencies artifact can be templatised to support
    customisation, but a sensible default template will be used if none is
    provided. The template used is derived from the (optional)
    `pom_template` argument, and the following substitutions are performed on
    the template file:

      * `{groupId}`: Replaced with the maven coordinates group ID.
      * `{artifactId}`: Replaced with the maven coordinates artifact ID.
      * `{version}`: Replaced by the maven coordinates version.
      * `{dependencies}`: Replaced by a list of maven dependencies directly relied upon
        by java_library targets within the artifact.

    To publish, call the implicit `*.publish` target(s).

    The maven repository may accessed locally using a `file://` URL, or
    remotely using an `https://` URL. The following flags may be set
    using `--define`:

      * `gpg_sign`: Whether to sign artifacts using GPG
      * `maven_repo`: A URL for the repo to use. May be "https" or "file".
      * `maven_user`: The user name to use when uploading to the maven repository.
      * `maven_password`: The password to use when uploading to the maven repository.

    When signing with GPG, the current default key is used.

    Generated rules:
      * `name`: The BOM file itself.
      * `name.publish`: To be executed by `bazel run` to publish the BOM to a maven repo
      * `name-dependencies`: The BOM file for the dependencies `pom.xml`. Only generated if `dependencies_maven_coordinates` is set.
      * `name-dependencies.publish`: To be executed by `bazel run` to publish the dependencies `pom.xml` to a maven rpo. Only generated if `dependencies_maven_coordinates` is set.

    Args:
      name: A unique name for this rule.
      maven_coordinates: The maven coordinates of this BOM in `groupId:artifactId:version` form.
      bom_pom_template: A template used for generating the `pom.xml` of the BOM at `maven_coordinates` (optional)
      dependencies_maven_coordinates: The maven coordinates of a dependencies artifact to generate in GAV format. If empty, none will be generated. (optional)
      dependencies_pom_template: A template used for generating the `pom.xml` of the dependencies artifact at `dependencies_maven_coordinates` (optional)
      java_exports: A list of `java_export` targets that are used to generate the BOM.
    """
    fragments = []
    labels = [_label(je) for je in java_exports]

    # `same_package_label` doesn't exist in Bazel 5, but we still support it
    # so we check the version here to call a non-deprecated API in recent
    # Bazel versions, or the older (deprecated) API in Bazel 5.
    feature_check_label = Label("//:doesnotexistinrulesjvmexternal")
    if hasattr(feature_check_label, "same_package_label"):
        fragments = [l.same_package_label("%s.bom-fragment" % l.name) for l in labels]
    else:
        # TODO: Drop this branch once we drop Bazel 5 support
        fragments = [l.relative(":%s.bom-fragment" % l.name) for l in labels]

    _maven_bom(
        name = name,
        maven_coordinates = maven_coordinates,
        pom_template = bom_pom_template,
        fragments = fragments,
        tags = tags,
        testonly = testonly,
        visibility = visibility,
        toolchains = toolchains,
    )

    maven_publish(
        name = "%s.publish" % name,
        coordinates = maven_coordinates,
        pom = name,
        tags = tags,
        testonly = testonly,
        visibility = visibility,
        toolchains = toolchains,
    )

    if dependencies_maven_coordinates:
        _maven_dependencies_bom(
            name = "%s-dependencies" % name,
            maven_coordinates = dependencies_maven_coordinates,
            pom_template = dependencies_pom_template,
            fragments = fragments,
            bom_coordinates = maven_coordinates,
            tags = tags,
            testonly = testonly,
            visibility = visibility,
            toolchains = toolchains,
        )

        maven_publish(
            name = "%s-dependencies.publish" % name,
            coordinates = dependencies_maven_coordinates,
            pom = "%s-dependencies" % name,
            tags = tags,
            testonly = testonly,
            visibility = visibility,
            toolchains = toolchains,
        )
