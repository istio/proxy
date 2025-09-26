load("@bazel_common//tools/maven:pom_file.bzl", "pom_file")

java_library(
    name = "my_library",
    srcs = ["java/com/example/bazel/Main.java"],
    deps = [
        "@maven//:com_google_guava_guava",
        "@maven//:com_google_inject_guice",
    ],
)

pom_file(
    name = "my_library_pom",
    targets = [
        "//:my_library",
    ],
    template_file = "pom_template.xml",
)
