# Images containing Java applications

Users are typically migrating from [java_image](https://github.com/bazelbuild/rules_docker#java_image)
or [war_image](https://github.com/bazelbuild/rules_docker#war_image) in rules_docker.

You can request the `*_deploy.jar` output of a `java_binary` target, which is a single, self-contained launcher that includes all the dependencies.
This can then be added to a container with a base image such as `gcr.io/distroless/java17` and then executed directly, for example with

```bazel
oci_image(
    name = "java_image",
    base = "@distroless_java",
    entrypoint = [
        "java",
        "-jar",
        "/path/to/Application_deploy.jar",
    ],
    ...
)
```

## Example

[A simple example using a deploy.jar](https://github.com/aspect-build/bazel-examples/tree/main/oci_java_image)
