load("@aspect_bazel_lib//lib:diff_test.bzl", "diff_test")
load("@rules_java//java:java_library.bzl", "java_library")
load("//:defs.bzl", "artifact", "java_export")

java_export(
    name = "pom-example",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "//tests/integration/maven_bom:one-dep",
    ],
)

diff_test(
    name = "validate-pom",
    file1 = ":pom-example-pom",
    file2 = "pom.golden.xml",
)

java_export(
    name = "pom-example-with-runtime-dep",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    runtime_deps = [
        "//tests/integration/maven_bom:one-dep",
    ],
    deps = [
        "//tests/integration/maven_bom:one-dep",
        artifact("com.google.guava:guava"),
    ],
)

diff_test(
    name = "validate-pom-with-runtime-dep",
    file1 = ":pom-example-with-runtime-dep-pom",
    file2 = "pom-with-runtime-dep.golden.xml",
)

java_export(
    name = "pom-example-with-classifier-dep",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    runtime_deps = [
        "@maven//:io_netty_netty_tcnative_boringssl_static_linux_aarch_64",
    ],
    deps = [
        "@maven//:io_netty_netty_tcnative_boringssl_static_linux_aarch_64",
        "@maven//:io_netty_netty_tcnative_boringssl_static_linux_x86_64",
    ],
)

diff_test(
    name = "validate-pom-example-with-classifier-dep",
    file1 = ":pom-example-with-classifier-dep-pom",
    file2 = "pom-with-classifier-dep.golden.xml",
)

java_export(
    name = "pom-example-with-compile-only-dep",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    # This dep can be any neverlink dep.
    deps = ["@regression_testing_coursier//:com_squareup_javapoet"],
)

diff_test(
    name = "validate-pom-example-with-compile-only-dep",
    file1 = ":pom-example-with-compile-only-dep-pom",
    file2 = "pom-with-compile-only-dep.golden.xml",
)

java_export(
    name = "pom-example-with-exported-dep",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    exports = [
        "//tests/integration/maven_bom:one-dep",
    ],
    deps = [
        "//tests/integration/maven_bom:one-dep",
    ],
)

diff_test(
    name = "validate-pom-example-with-exported-dep",
    file1 = ":pom-example-with-exported-dep-pom",
    file2 = "pom-with-exported-dep.golden.xml",
)

java_export(
    name = "pom-example-with-exported-compile-only-dep",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    exports = [
        "@regression_testing_coursier//:com_squareup_javapoet",
    ],
    deps = ["@regression_testing_coursier//:com_squareup_javapoet"],
)

diff_test(
    name = "validate-pom-example-exported-compile-only-dep",
    file1 = ":pom-example-with-exported-compile-only-dep-pom",
    file2 = "pom-example-with-exported-compile-only-dep.golden.xml",
)

####
# Exclusion Tests
####
java_export(
    name = "pom-example-with-exclusions-coursier",
    srcs = ["PomExample.java"],
    # A java_export specific exclusion
    exclusions = {
        "@pom_exclusion_testing_coursier//:com_google_guava_guava": [
            "com.google.guava:failureaccess",
        ],
    },
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@pom_exclusion_testing_coursier//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-exclusions-coursier",
    file1 = ":pom-example-with-exclusions-coursier-pom",
    file2 = "pom-with-exclusions.golden.xml",
)

java_export(
    name = "pom-example-with-exclusions-maven",
    srcs = ["PomExample.java"],
    # A java_export specific exclusion
    exclusions = {
        "@pom_exclusion_testing_maven//:com_google_guava_guava": [
            "com.google.guava:failureaccess",
        ],
    },
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@pom_exclusion_testing_maven//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-exclusions-maven",
    file1 = ":pom-example-with-exclusions-maven-pom",
    file2 = "pom-with-exclusions.golden.xml",
)

java_export(
    name = "pom-example-with-exclusions-gradle",
    srcs = ["PomExample.java"],
    # A java_export specific exclusion
    exclusions = {
        "@pom_exclusion_testing_gradle//:com_google_guava_guava": [
            "com.google.guava:failureaccess",
        ],
    },
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@pom_exclusion_testing_gradle//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-exclusions-gradle",
    file1 = ":pom-example-with-exclusions-gradle-pom",
    file2 = "pom-with-exclusions.golden.xml",
)

java_export(
    name = "pom-example-with-specific-exclusions",
    srcs = ["PomExample.java"],
    # A java_export specific exclusion
    exclusions = {
        "@maven//:com_google_guava_guava": [
            "com.google.guava:failureaccess",
        ],
    },
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@maven//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-specific-exclusions",
    file1 = ":pom-example-with-specific-exclusions-pom",
    file2 = "pom-with-specific-exclusions.golden.xml",
)

java_export(
    name = "pom-example-with-maven-defined-exclusions-coursier",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@pom_exclusion_testing_coursier//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-maven-defined-exclusions-coursier",
    file1 = ":pom-example-with-maven-defined-exclusions-coursier-pom",
    file2 = "pom-with-maven-defined-exclusions.golden.xml",
)

java_export(
    name = "pom-example-with-maven-defined-exclusions-maven",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@pom_exclusion_testing_maven//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-maven-defined-exclusions-maven",
    file1 = ":pom-example-with-maven-defined-exclusions-maven-pom",
    file2 = "pom-with-maven-defined-exclusions.golden.xml",
)

java_export(
    name = "pom-example-with-maven-defined-exclusions-gradle",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        "@pom_exclusion_testing_gradle//:com_google_guava_guava",
    ],
)

diff_test(
    name = "validate-pom-example-with-maven-defined-exclusions-gradle",
    file1 = ":pom-example-with-maven-defined-exclusions-gradle-pom",
    file2 = "pom-with-maven-defined-exclusions.golden.xml",
)

####
# Test for transitive dependency with maven.artifact() exclusions
####

# Scenario 1: java_export -> java_export -> guava (with exclusions)
# The intermediate java_export should have guava with exclusions in ITS pom.
# The top-level java_export should only have the intermediate library in its pom.

java_export(
    name = "intermediate-library-export",
    srcs = ["OtherLibrary.java"],
    maven_coordinates = "com.example:intermediate:1.0.0",
    deps = [
        "@pom_exclusion_testing_coursier//:com_google_guava_guava",
    ],
)

# Verify that the intermediate library's POM has guava WITH exclusions
diff_test(
    name = "validate-intermediate-library-pom",
    file1 = ":intermediate-library-export-pom",
    file2 = "pom-intermediate-library.golden.xml",
)

java_export(
    name = "pom-transitive-dep-with-exclusions",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        ":intermediate-library-export",
    ],
)

# Verify that the top-level POM only has the intermediate library (not guava)
diff_test(
    name = "validate-pom-transitive-dep-with-exclusions",
    file1 = ":pom-transitive-dep-with-exclusions-pom",
    file2 = "pom-transitive-dep-with-exclusions.golden.xml",
)

# Scenario 2: java_export -> java_library (no maven_coordinates) -> guava
# The java_library is bundled into the java_export, so guava becomes a
# direct dependency of the java_export's POM (with exclusions).

java_library(
    name = "other-library",
    srcs = ["OtherLibrary.java"],
    deps = [
        "@pom_exclusion_testing_coursier//:com_google_guava_guava",
    ],
)

java_export(
    name = "pom-bundled-dep-with-exclusions",
    srcs = ["PomExample.java"],
    maven_coordinates = "com.example:app:1.0.0",
    deps = [
        ":other-library",
    ],
)

# Verify that guava (with exclusions) appears in the POM since the helper is bundled
diff_test(
    name = "validate-pom-bundled-dep-with-exclusions",
    file1 = ":pom-bundled-dep-with-exclusions-pom",
    file2 = "pom-with-maven-defined-exclusions.golden.xml",
)
