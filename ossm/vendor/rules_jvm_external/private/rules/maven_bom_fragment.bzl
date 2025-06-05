load("@rules_java//java:defs.bzl", "JavaInfo")
load(":has_maven_deps.bzl", "MavenInfo", "has_maven_deps")

MavenBomFragmentInfo = provider(
    fields = {
        "coordinates": "Maven coordinates for this part of the BOM",
        "artifact": "The `maven_project_jar` that forms the main artifact",
        "srcs": "The src-jar of the artifact",
        "javadocs": "The javadocs of the artifact. May be `None`",
        "pom": "The `pom.xml` template file",
        "maven_info": "The `MavenInfo` of `artifact`",
    },
)

def _maven_bom_fragment_impl(ctx):
    java_info = ctx.attr.artifact[JavaInfo]

    # Expand maven coordinates for any variables to be replaced.
    coordinates = ctx.expand_make_variables("coordinates", ctx.attr.maven_coordinates, ctx.var)

    # Since Bazel 5.0.0
    if "java_outputs" in dir(java_info):
        artifact_jar = java_info.java_outputs[0].class_jar
        if len(java_info.java_outputs) > 1:
            print("Maven BOM may not be correct. Expected one jar, got %s for %s" % (len(java_info.java_outputs), ctx.label))
    elif len(java_info.outputs.jars):
        # Bazel 4.x
        artifact_jar = java_info.outputs.jars[0]
        if len(java_info.outputs.jars) > 1:
            print("Maven BOM may not be correct. Expected one jar, got %s for %s" % (len(java_info.outputs.jars), ctx.label))
    else:
        artifact_jar = None

    return [
        MavenBomFragmentInfo(
            coordinates = coordinates,
            artifact = artifact_jar,
            srcs = ctx.file.src_artifact,
            javadocs = ctx.file.javadoc_artifact,
            pom = ctx.file.pom,
            maven_info = ctx.attr.artifact[MavenInfo],
        ),
    ]

maven_bom_fragment = rule(
    _maven_bom_fragment_impl,
    attrs = {
        "maven_coordinates": attr.string(
            doc = """The maven coordinates that should be used for the generated artifact""",
            mandatory = True,
        ),
        "artifact": attr.label(
            doc = """The `maven_project_jar` that forms the primary artifact of the maven coordinates""",
            mandatory = True,
            providers = [
                [JavaInfo],
            ],
            aspects = [
                has_maven_deps,
            ],
        ),
        "src_artifact": attr.label(
            doc = """The source jar generated from `artifact`""",
            allow_single_file = True,
            mandatory = True,
        ),
        "javadoc_artifact": attr.label(
            doc = """The javadoc jar generated from the `artifact`""",
            allow_single_file = True,
        ),
        "pom": attr.label(
            doc = "The pom file of the generated `artifact`",
            allow_single_file = True,
        ),
    },
    provides = [
        MavenBomFragmentInfo,
    ],
)
