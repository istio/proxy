# Images containing C/C++ applications

Users are typically migrating from [cc_image](https://github.com/bazelbuild/rules_docker#cc_image)
in rules_docker.

## An example of packaging a simple C++ program

Using a minimal example C++ program `example.cc`:

```cpp
#include <iostream>

int main(){
    std::cout<<"This is a C++ example!"<<std::endl;
}
```

To make a container image for this program, the `BUILD.bazel` would have something like this:

```python
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")
load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@tar.bzl", "tar")

package(default_visibility = ["//visibility:public"])

# Normal cc_binary
cc_binary(
    name  = "example_binary",
    srcs = [
        "example.cc",
    ]
)

# Packaging the binary into tar, which is needed by oci_image rule
tar(
    name = "tar",
    srcs = [":example_binary"],
)

# Making image
# C++ programs usually need some fundamental libraries such as glibc, libstdc++, etc.
# Correspondigly, use language-specific distroless images.
# Here we use docker.io/library/ubuntu image for this C++ program.
oci_image(
    name = "image",
    base = "@docker_lib_ubuntu",
    tars = [":tar"],
    entrypoint = ["/example_binary"],
)

# Use with 'bazel run' to load the oci image into a container runtime.
# The image is designated using `repo_tags` attribute.
oci_load(
    name = "image_load",
    image = ":image",
    repo_tags = ["example:latest"],
)
```

In `MODULE.bazel` file, be sure to add the following sections:

```python
# Pull needed base image
oci.pull(
    name = "docker_lib_ubuntu",
    image = "docker.io/library/ubuntu",
    platforms = [
        "linux/arm64/v8",
        "linux/amd64",
    ],
    tag = "rolling",
)
# Expose the base image
use_repo(oci, "docker_lib_ubuntu")
```

```python
bazel_dep(name = "tar.bzl", version = "0.7.0")
```

To load the image, execute:

```bash
bazel run //:image_load
```

Then to run the program with runtime, e.g., Docker:

```bash
docker run --rm example:latest
```
