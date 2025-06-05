load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private/lib:coordinates.bzl", "unpack_coordinates")

def _group_and_artifact_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "", unpacked.version)

    return unittest.end(env)

group_and_artifact_test = unittest.make(_group_and_artifact_impl)

def _group_artifact_and_version_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact:1.2.3")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "1.2.3", unpacked.version)

    return unittest.end(env)

group_artifact_and_version_test = unittest.make(_group_artifact_and_version_impl)

def _complete_original_format_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact:type:scope:1.2.3")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "1.2.3", unpacked.version)
    asserts.equals(env, "type", unpacked.packaging)
    asserts.equals(env, "scope", unpacked.classifier)

    return unittest.end(env)

complete_original_format_test = unittest.make(_complete_original_format_impl)

def _original_format_omitting_scope_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact:test-jar:1.2.3")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "1.2.3", unpacked.version)
    asserts.equals(env, "test-jar", unpacked.packaging)
    asserts.equals(env, None, unpacked.classifier)

    return unittest.end(env)

original_format_omitting_scope_test = unittest.make(_original_format_omitting_scope_impl)

def _gradle_format_without_type_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact:1.2.3:classifier")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "1.2.3", unpacked.version)
    asserts.equals(env, None, unpacked.packaging)
    asserts.equals(env, "classifier", unpacked.classifier)

    return unittest.end(env)

gradle_format_without_type_test = unittest.make(_gradle_format_without_type_impl)

def _gradle_format_with_type_and_classifier_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact:1.2.3:classifier@type")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "1.2.3", unpacked.version)
    asserts.equals(env, "type", unpacked.packaging)
    asserts.equals(env, "classifier", unpacked.classifier)

    return unittest.end(env)

gradle_format_with_type_and_classifier_test = unittest.make(_gradle_format_with_type_and_classifier_impl)

def _gradle_format_with_type_but_no_classifier_impl(ctx):
    env = unittest.begin(ctx)

    unpacked = unpack_coordinates("group:artifact:1.2.3@type")
    asserts.equals(env, "group", unpacked.group)
    asserts.equals(env, "artifact", unpacked.artifact)
    asserts.equals(env, "1.2.3", unpacked.version)
    asserts.equals(env, "type", unpacked.packaging)
    asserts.equals(env, None, unpacked.classifier)

    return unittest.end(env)

gradle_format_with_type_but_no_classifier_test = unittest.make(_gradle_format_with_type_but_no_classifier_impl)

def _multiple_formats_impl(ctx):
    env = unittest.begin(ctx)

    coords_to_structs = {
        "groupId:artifactId:1.2.3": struct(group = "groupId", artifact = "artifactId", version = "1.2.3", classifier = None, packaging = None),
        "groupId:artifactId:test-jar:1.2.3": struct(group = "groupId", artifact = "artifactId", version = "1.2.3", classifier = None, packaging = "test-jar"),
        "groupId:artifactId:type:classifier:1.2.3": struct(group = "groupId", artifact = "artifactId", version = "1.2.3", classifier = "classifier", packaging = "type"),
        "groupId:artifactId:1.2.3@type": struct(group = "groupId", artifact = "artifactId", version = "1.2.3", classifier = None, packaging = "type"),
        "groupId:artifactId:1.2.3:classifier@type": struct(group = "groupId", artifact = "artifactId", version = "1.2.3", classifier = "classifier", packaging = "type"),
        "io.netty:netty-transport-native-unix-common:jar:linux-aarch_64:4.1.100.Final": struct(group = "io.netty", artifact = "netty-transport-native-unix-common", version = "4.1.100.Final", classifier = "linux-aarch_64", packaging = "jar"),
    }

    for (coords, expected) in coords_to_structs.items():
        unpacked = unpack_coordinates(coords)
        asserts.equals(env, expected, unpacked)

    return unittest.end(env)

multiple_formats_test = unittest.make(_multiple_formats_impl)

def _unusual_version_format_impl(ctx):
    env = unittest.begin(ctx)

    # Unusual version formats
    versions = [
        "809c471bf94f09bf4699ba53eb337768d5d9882f",  # A git sha
        "FY21R16",    # Based on year and release number
        "PRERELEASE", # Just a nice name
        "VX.2.5.0.0",  # Also seen in the wild
        "jcef-7f53d6d+cef-100.0.14+g4e5ba66+chromium-100.0.4896.75", # Seen in the wild
    ]

    for version in versions:
        unpacked = unpack_coordinates("group:artifact:%s" % version)

        asserts.equals(env, version, unpacked.version)

    return unittest.end(env)

unusual_version_format_test = unittest.make(_unusual_version_format_impl)

def coordinates_test_suite():
    unittest.suite(
        "coordinates_tests",
        group_and_artifact_test,
        group_artifact_and_version_test,
        complete_original_format_test,
        original_format_omitting_scope_test,
        gradle_format_without_type_test,
        gradle_format_with_type_and_classifier_test,
        gradle_format_with_type_but_no_classifier_test,
        multiple_formats_test,
        unusual_version_format_test,
    )
