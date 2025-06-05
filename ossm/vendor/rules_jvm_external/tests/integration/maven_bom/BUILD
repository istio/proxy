load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("//:defs.bzl", "artifact", "java_export", "maven_bom")

maven_bom(
    name = "bom",
    dependencies_maven_coordinates = "com.example:bom-deps:1.0.0",
    java_exports = [
        ":one-dep",
        ":two-deps",
    ],
    maven_coordinates = "com.example:bom:1.0.0",
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
    maven_coordinates = "com.example:two-deps:1.0.0",
    deps = [
        artifact("com.google.guava:guava"),
        artifact("org.hamcrest:hamcrest"),
    ],
)

java_export(
    name = "one-dep",
    srcs = ["OneDep.java"],
    maven_coordinates = "com.example:one-dep:1.0.0",
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
)

java_export(
    name = "transitive-dep",
    srcs = ["TransitiveDep.java"],
    maven_coordinates = "com.example:transitive:1.0.0",
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

string_flag(
    name = "version",
    build_setting_default = "0.0.0-dev",
    make_variable = "VERSION",
)

maven_bom(
    name = "substitution-bom",
    java_exports = [],
    maven_coordinates = "com.example:bom-substitution:$(VERSION)",
    toolchains = [":version"],
)

diff_test(
    name = "validate-substitution-bom",
    file1 = ":substitution-bom",
    file2 = "substitution-bom.golden.xml",
)
