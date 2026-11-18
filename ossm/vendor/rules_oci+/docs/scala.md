# Images containing Scala applications

Users are typically migrating from [scala_image](https://github.com/bazelbuild/rules_docker#scala_image)
in rules_docker.

You can request the *_deploy.jar output of a scala_binary target, which is a single, self-contained launcher that includes all the dependencies. This can then be added to a container with a base image such as gcr.io/distroless/java17 and then executed directly as `java -jar <your jar>`.

## Example

For this example, we will use `App.scala` like below:

**App.scala**

```scala
object App {
  def main(args: Array[String]): Unit = {
    println("Hello, world!")
  }
}
```

In this example, I will not use bzlmod and fall back to the `WORKSPACE` file, as `rules_scala` doesn't support bzlmod yet. This file setups 
the `rules_scala` according to the documentation so that we can build scala targets. Next, it configures `aspect_bazel_lib` so that we can have access to `tar` rule needed later. Finally, it configures `rules_oci` and pulls the base image with Java 17.

**WORKSPACE**

```python
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "bazel_skylib",
    sha256 = "66ffd9315665bfaafc96b52278f57c7e2dd09f5ede279ea6d39b2be471e7e3aa",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.4.2/bazel-skylib-1.4.2.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.4.2/bazel-skylib-1.4.2.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

http_archive(
    name = "io_bazel_rules_scala",
    sha256 = "3b00fa0b243b04565abb17d3839a5f4fa6cc2cac571f6db9f83c1982ba1e19e5",
    strip_prefix = "rules_scala-6.5.0",
    url = "https://github.com/bazelbuild/rules_scala/releases/download/v6.5.0/rules_scala-v6.5.0.tar.gz",
)

load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")

scala_config(scala_version = "2.13.12")

load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")

scala_repositories()

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")

scala_register_toolchains()

load("@io_bazel_rules_scala//testing:scalatest.bzl", "scalatest_repositories", "scalatest_toolchain")

scalatest_repositories()

scalatest_toolchain()

http_archive(
    name = "aspect_bazel_lib",
    sha256 = "6d758a8f646ecee7a3e294fbe4386daafbe0e5966723009c290d493f227c390b",
    strip_prefix = "bazel-lib-2.7.7",
    url = "https://github.com/aspect-build/bazel-lib/releases/download/v2.7.7/bazel-lib-v2.7.7.tar.gz",
)

load("@aspect_bazel_lib//lib:repositories.bzl", "aspect_bazel_lib_dependencies", "aspect_bazel_lib_register_toolchains")

# Required bazel-lib dependencies

aspect_bazel_lib_dependencies()

# Register bazel-lib toolchains

aspect_bazel_lib_register_toolchains()

http_archive(
    name = "rules_oci",
    sha256 = "647f4c6fd092dc7a86a7f79892d4b1b7f1de288bdb4829ca38f74fd430fcd2fe",
    strip_prefix = "rules_oci-1.7.6",
    url = "https://github.com/bazel-contrib/rules_oci/releases/download/v1.7.6/rules_oci-v1.7.6.tar.gz",
)

load("@rules_oci//oci:dependencies.bzl", "rules_oci_dependencies")

rules_oci_dependencies()

load("@rules_oci//oci:repositories.bzl", "LATEST_CRANE_VERSION", "oci_register_toolchains")

oci_register_toolchains(
    name = "oci",
    crane_version = LATEST_CRANE_VERSION,
)

# You can pull your base images using oci_pull like this:
load("@rules_oci//oci:pull.bzl", "oci_pull")

oci_pull(
    name = "distroless_java",
    digest = "sha256:161a1d97d592b3f1919801578c3a47c8e932071168a96267698f4b669c24c76d",
    image = "gcr.io/distroless/java17",
)
```

Now, let's create *BUILD.bazel* step by step. First, create a `scala_binary` target for our app. It is safe to add dependencies, but they were omitted here for simplicity.

**BUILD.bazel**

```python
load("@io_bazel_rules_scala//scala:scala.bzl", "scala_binary")

scala_binary(
    name = "app",
    srcs = ["App.scala"],
    main_class = "App",
)
```

After that, we can package that binary into a layer using `tar`

```python
load("@tar.bzl", "tar")

tar(
    name = "layer",
    srcs = [":app_deploy.jar"],
)
```

Next, construct the image from base image and our new layer, using `oci_image` rule. The entrypoint is set to `java -jar /app_deploy.jar` so that the image can be run directly.

```python
load("@rules_oci//oci:defs.bzl", "oci_image")

oci_image(
    name = "image",
    base = "@distroless_java",
    entrypoint = ["java", "-jar", "/app_deploy.jar"],
    tars = [":layer"],
)
```

Finally, create a tarball from `oci_image` that can be loaded by a runtime such as docker. We specify `repo_tags` so that the image can be loaded by a registry.

```python
load("@rules_oci//oci:defs.bzl", "oci_load")

oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["my-repository:latest"],
)
```

Test if it works:

```shell
$ bazel run //:load
...
$ docker run --rm my-repository:latest
Hello, world!
```

Complete `BUILD.bazel` file

**BUILD.bazel**

```python
load("@io_bazel_rules_scala//scala:scala.bzl", "scala_binary")
load("@tar.bzl", "tar")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")

scala_binary(
    name = "app",
    srcs = ["App.scala"],
    main_class = "App",
)

tar(
    name = "layer",
    srcs = [":app_deploy.jar"],
)

oci_image(
    name = "image",
    base = "@distroless_java",
    entrypoint = ["java", "-jar", "/app_deploy.jar"],
    tars = [":layer"],
)

oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["my-repository:latest"],
)
```
