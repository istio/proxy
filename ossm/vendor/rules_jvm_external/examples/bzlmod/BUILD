load("@rules_jvm_external//:defs.bzl", "artifact")

# You can load artifacts from different `maven.install` tags by using the
# `repository_name` parameter of the `artifact` macro.
alias(
    name = "junit-api",
    actual = artifact(
        "org.junit.jupiter:junit-jupiter-api",
        repository_name = "alternative_resolver",
    ),
)
