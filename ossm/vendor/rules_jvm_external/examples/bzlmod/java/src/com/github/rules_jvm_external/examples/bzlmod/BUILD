load("@rules_jvm_external//:defs.bzl", "artifact")

java_binary(
    name = "bzlmod_example",
    srcs = glob(["*.java"]),
    deps = [
        artifact("com.google.guava:guava"),
        artifact("org.seleniumhq.selenium:selenium-api"),
    ],
)
