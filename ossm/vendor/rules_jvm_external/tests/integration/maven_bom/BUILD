load("@aspect_bazel_lib//lib:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("//:defs.bzl", "artifact", "java_export", "maven_bom")

string_flag(
    name = "version",
    build_setting_default = "0.0.0-dev",
    make_variable = "VERSION",
)

string_flag(
    name = "lib-version",
    build_setting_default = "2.0.0",
    make_variable = "LIB_VERSION",
)

# Basic BOM test that also validates make variable substitution:
# - $(VERSION) in BOM coordinates
# - $(LIB_VERSION) in java_export coordinates (two-deps)
maven_bom(
    name = "bom",
    dependencies_maven_coordinates = "com.example:bom-deps:$(VERSION)",
    java_exports = [
        ":one-dep",
        ":two-deps",
    ],
    maven_coordinates = "com.example:bom:$(VERSION)",
    toolchains = [":version"],
)

diff_test(
    name = "validate-bom",
    file1 = ":bom",
    file2 = "bom.golden.xml",
)

diff_test(
    name = "validate-dependencies-bom",
    file1 = ":bom-dependencies",
    file2 = "bom-dependencies.golden.xml",
)

java_export(
    name = "two-deps",
    srcs = ["TwoDeps.java"],
    maven_coordinates = "com.example:two-deps:$(LIB_VERSION)",
    toolchains = [":lib-version"],
    deps = [
        artifact("com.google.guava:guava"),
        artifact("org.hamcrest:hamcrest"),
    ],
)

java_export(
    name = "one-dep",
    srcs = ["OneDep.java"],
    maven_coordinates = "com.example:one-dep:1.0.0",
    visibility = [
        ":__pkg__",
        "//tests/integration/pom_file:__pkg__",
    ],
    deps = [
        artifact("com.google.guava:guava"),
    ],
)

maven_bom(
    name = "transitive-bom",
    dependencies_maven_coordinates = "com.example:bom-transitive-deps:1.0.0",
    java_exports = [
        # This dep depends on `:one-dep` and `:two-dep`, but we include `:one-dep`
        # here. That means that we expect the BOM to include the two deps listed
        # here, and the dependencies-bom should contain `:two-dep` plus guava
        ":transitive-dep",
        ":one-dep",
        # But not the second!
    ],
    maven_coordinates = "com.example:bom-transitive:1.0.0",
    toolchains = [":lib-version"],
)

java_export(
    name = "transitive-dep",
    srcs = ["TransitiveDep.java"],
    maven_coordinates = "com.example:transitive:1.0.0",
    toolchains = [":lib-version"],
    deps = [
        ":one-dep",
        ":two-deps",
    ],
)

diff_test(
    name = "validate-transitive-bom",
    file1 = ":transitive-bom",
    file2 = "transitive-bom.golden.xml",
)

diff_test(
    name = "validate-transitive-dependencies-bom",
    file1 = ":transitive-bom-dependencies",
    file2 = "transitive-bom-dependencies.golden.xml",
)

# Combined exclusion test:
# Tests BOTH maven_exclusion= tags AND java_export exclusions attribute
java_export(
    name = "with-combined-exclusions",
    srcs = ["OneDep.java"],
    # Source 1: maven_exclusion= tag on the artifact (defined in MODULE.bazel)
    # excludes com.google.errorprone:error_prone_annotations
    # Source 2: java_export exclusions attribute
    exclusions = {
        "@pom_exclusion_testing_coursier//:com_google_guava_guava": [
            "com.google.guava:failureaccess",
        ],
    },
    maven_coordinates = "com.example:with-combined-exclusions:1.0.0",
    deps = [
        "@pom_exclusion_testing_coursier//:com_google_guava_guava",
    ],
)

maven_bom(
    name = "combined-exclusion-bom",
    dependencies_maven_coordinates = "com.example:combined-exclusion-bom-deps:1.0.0",
    java_exports = [
        ":with-combined-exclusions",
    ],
    maven_coordinates = "com.example:combined-exclusion-bom:1.0.0",
)

diff_test(
    name = "validate-combined-exclusion-bom",
    file1 = ":combined-exclusion-bom",
    file2 = "combined-exclusion-bom.golden.xml",
)

diff_test(
    name = "validate-combined-exclusion-dependencies-bom",
    file1 = ":combined-exclusion-bom-dependencies",
    file2 = "combined-exclusion-bom-dependencies.golden.xml",
)
