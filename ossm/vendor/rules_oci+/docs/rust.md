# Images containing Rust applications

Users are typically migrating from [rust_image](https://github.com/bazelbuild/rules_docker#rust_image)
in rules_docker.

## Base image

First, we'll need a base image.

It's wise to minimize changes by using the same one your current `rust_image` uses.

To check which base image rules_docker use for Rust, we can check logic in [rules_docker repo](https://github.com/bazelbuild/rules_docker) or use `bazel query`. In this docs we'll go with the first way, if you want to see how to use `bazel query`, you can refer to [build go image docs](/docs/go.md).

Logic to choose rust base image is in [rust/image.bzl](https://github.com/bazelbuild/rules_docker/blob/master/rust/image.bzl), we can see that it use an variable called `DEFAULT_BASE` imported from [cc/image.bzl](https://github.com/bazelbuild/rules_docker/blob/fc729d85f284225cfc0b8c6d1d838f4b3e037749/cc/image.bzl). Inspecting that file, we can see that it refer to [`distroless/cc`](https://github.com/GoogleContainerTools/distroless/blob/main/cc/README.md) image.

**TL;DR:** Base image to use is [`distroless/cc`](https://github.com/GoogleContainerTools/distroless/blob/main/cc/README.md). To use it, add below code to WORKSPACE:

```
load("@rules_oci//oci:pull.bzl", "oci_pull")

oci_pull(
    name = "distroless_cc",
    digest = "sha256:8aad707f96620ee89e27febef51b01c6ff244277a3560fcfcfbe68633ef09193",
    image = "gcr.io/distroless/cc",
    platforms = ["linux/amd64","linux/arm64"],
)
```

See more details in the [`oci_pull` docs](/docs/pull.md)

## Note about compatibility

`distroless/cc` is based on [Debian 11 (bullseye)](https://github.com/GoogleContainerTools/distroless#base-operating-system), which contain `glibc 2.31`. So if you run `rust_binary` on a machine that has `glibc > 2.31`, your image may not work and will see error like: `/<binary_name>: /lib/x86_64-linux-gnu/libc.so.6: version GLIBC_2.33 not found `. To avoid this, you can:

- Use a base image that contains newer version of glibc (> 2.31)
- Run bazel build on an environment that contains `glibc <= 2.31`
- Switch to `musl`

For example, if you wanted to use a base image with a newer glibc, you could use the Debian 12 `distroless/cc` image like so:

```
load("@rules_oci//oci:pull.bzl", "oci_pull")

oci_pull(
    name = "distroless_cc",
    digest = "sha256:a9056d2232d16e3772bec3ef36b93a5ea9ef6ad4b4ed407631e534b85832cf40",
    image = "gcr.io/distroless/cc-debian12",
    platforms = ["linux/amd64", "linux/arm64/v8"],
)
```

## Example

For example, we have a `hello.rs` like below.

**hello.rs**

```rust
fn main() {
    println!("Hello, World!");
}
```

Create a `WORKSPACE` file to load required toolchains and pull [`distroless/cc`](https://github.com/GoogleContainerTools/distroless/blob/main/cc/README.md) as base image. For more information, refer to [`rules_rust`](https://github.com/bazelbuild/rules_rust) and [`rules_oci`](https://github.com/bazel-contrib/rules_oci/)

**WORKSPACE**

```
# Name of workspace
workspace(name = "sample-rust-bzl")

# Add rules_rust
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "rules_rust",
    sha256 = "950a3ad4166ae60c8ccd628d1a8e64396106e7f98361ebe91b0bcfe60d8e4b60",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.20.0/rules_rust-v0.20.0.tar.gz"],
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

rules_rust_dependencies()

rust_register_toolchains()

# Add rules_oci
http_archive(
    name = "rules_oci",
    sha256 = "f6125c9a123a2ac58fb6b13b4b8d4631827db9cfac025f434bbbefbd97953f7c",
    strip_prefix = "rules_oci-0.3.9",
    url = "https://github.com/bazel-contrib/rules_oci/releases/download/v0.3.9/rules_oci-v0.3.9.tar.gz",
)

load("@rules_oci//oci:dependencies.bzl", "rules_oci_dependencies")

rules_oci_dependencies()

load("@rules_oci//oci:repositories.bzl", "oci_register_toolchains")

oci_register_toolchains(name = "oci")

# Pull distroless image

load("@rules_oci//oci:pull.bzl", "oci_pull")

oci_pull(
    name = "distroless_cc",
    digest = "sha256:8aad707f96620ee89e27febef51b01c6ff244277a3560fcfcfbe68633ef09193",
    image = "gcr.io/distroless/cc",
    platforms = ["linux/amd64","linux/arm64"],
)
```

Now create `BUILD.bazel`. First we need to build `hello.rs` to binary using `rust_binary`

**BUILD.bazel**

```
load("@rules_rust//rust:defs.bzl", "rust_binary")

package(default_visibility = ["//visibility:public"])

# Step 1: Build to binary
rust_binary(
    name = "hello_bin",
    srcs = [
        "hello.rs",
    ],
    edition = "2021",
)
```

After that, we package that binary into a layer using `tar`

```
load("@tar.bzl", "tar")

# Step 2: Compress it to layer
tar(
    name = "hello_bin_layer",
    srcs = [":hello_bin"],
    compression = "gzip",
)
```

Finally, add that layer to the base image and we're done!

```
load("@rules_oci//oci:defs.bzl", "oci_image")

# Step 3: Build image and add built layer to it
oci_image(
    name = "hello_image",
    base = "@distroless_cc",
    tars = [":hello_bin_layer"],
    entrypoint = ["/hello_bin"],
)

```

We can try to load it into `docker` to see if it work properly.

Complete `BUILD.bazel` file

**BUILD.bazel**

```
load("@rules_rust//rust:defs.bzl", "rust_binary")
load("@rules_oci//oci:defs.bzl", "oci_image")
load("@tar.bzl", "tar")

package(default_visibility = ["//visibility:public"])

# Step 1: Build to binary
rust_binary(
    name = "hello_bin",
    srcs = [
        "hello.rs",
    ],
    edition = "2021",
)

# Step 2: Compress it to layer
tar(
    name = "hello_bin_layer",
    srcs = [":hello_bin"],
    compression = "gzip",
)

# Step 3: Build image and add built layer to it
oci_image(
    name = "hello_image",
    base = "@distroless_cc",
    tars = [":hello_bin_layer"],
    entrypoint = ["/hello_bin"],
)
```
