# Bazel rules for OCI container images

Very simple Bazel rules based on the Open Containers Initiative specification: <https://opencontainers.org/>

Status: Unfunded, stable in maintenance mode.

## Design Principles

- Keep a tight complexity budget for the project so we are able to commit to effective maintenance without any dedicated funding.
  - Use only off-the-shelf, pre-built layer and container manipulation tools.
  - Use `DefaultInfo` to hand information between rules, meaning that files and directories are inputs/outputs for each action.
- Stay true to the specification, and only the specification.
  - Don't write language-specific rules, as we cannot be experts on all languages, nor can users deal with the versioning issues
  that come with dependencies we would be forced to take on the rules for those languages.
  - Don't be docker-specific, now that it has a commercial license and other container runtimes exist ([podman](https://podman.io/) for example).
  - Use our toolchain hermetically: don't assume there is a docker pre-installed on the machine.

## Comparison with rules_img

As described above, rules_oci is built for maintainability on top of standard container tools.
This means it doesn't attempt to make all the optimizations that are possible with rules_img.

For use with remote cache and remote execution, the use of files and directories as action input/output means that lots of bytes are sent over the network.
For these cases, we recommend trying rules_img.
See https://github.com/bazel-contrib/rules_img/blob/main/docs/migration-from-rules_oci.md to migrate from rules_oci to rules_img.

## Comparison with rules_docker

This ruleset is not intended as a complete replacement for [rules_docker].
Most use cases can be accomodated, and we know many users who have completely replaced rules_docker.
However, some other use cases such as `container_run_and*\*` rules have no equivalent.
You might still decide to use rules_docker, and perhaps even volunteer to help maintain it.

You can find a migration guide at <https://docs.aspect.build/guides/rules_oci_migration>.

[rules_docker]: https://github.com/bazelbuild/rules_docker

## Installation

Requires Bazel 7 or later.

See the install instructions on the release notes: <https://github.com/bazel-contrib/rules_oci/releases>

To use a commit rather than a release, you can point at any SHA of the repo.

With bzlmod, you can use `archive_override` or `git_override`. For `WORKSPACE`, you modify the `http_archive` call; for example to use commit `abc123` with a `WORKSPACE` file:

1. Replace `url = "https://github.com/bazel-contrib/rules_oci/releases/download/v0.1.0/rules_oci-v0.1.0.tar.gz"`
   with a GitHub-provided source archive like `url = "https://github.com/bazel-contrib/rules_oci/archive/abc123.tar.gz"`
1. Replace `strip_prefix = "rules_oci-0.1.0"` with `strip_prefix = "rules_oci-abc123"`
1. Update the `sha256`. The easiest way to do this is to comment out the line, then Bazel will
   print a message with the correct value.

> Note that GitHub source archives don't have a strong guarantee on the sha256 stability, see
> <https://github.blog/2023-02-21-update-on-the-future-stability-of-source-code-archives-and-hashes>

## Usage

rules_oci does not contain language-specific rules, but we do have limited documentation on how to accomplish typical tasks.

- [C/C++](docs/cpp.md)
- [Go](docs/go.md)
- [Java](docs/java.md)
- [JavaScript](docs/javascript.md)
- [Python](docs/python.md)
- [Rust](docs/rust.md)
- [Scala](docs/scala.md)
- [WASM](https://github.com/bazel-contrib/rules_oci/tree/main/e2e/wasm) (see https://docs.docker.com/desktop/wasm/)
- [Static Content](docs/static_content.md) (such as a html/javascript frontend)

> [!NOTE]
> Your language not listed above? Please contribute engineering resources or financially through our Sponsor link!

There are some generic examples of usage in the [examples](https://github.com/bazel-contrib/rules_oci/tree/main/examples) folder.
Note that these examples rely on the setup code in the `/WORKSPACE` file in the root of this repo.

## Public API Docs

### Install system packages

- Alpine: we recommend <https://github.com/chainguard-dev/rules_apko> to install [apk](https://wiki.alpinelinux.org/wiki/Package_management) packages.
- Debian: we recommend <https://github.com/GoogleContainerTools/rules_distroless> to install [deb](https://www.debian.org/distrib/packages) packages.
- RHEL/CentOS/Amazon Linux: we don't have any support for this yet. Please consider donating to the project!

### Construct image layers

- [oci_image](docs/image.md) Build an OCI compatible container image.
- [oci_image_index](docs/image_index.md) Build a multi-architecture OCI compatible container image.
- [oci_load](docs/load.md) Loads an `oci_image` into a container daemon. Can optionally produce a loadable tarball.

### Pull and Push

- [oci_pull](docs/pull.md) Pull image layers using Bazel's downloader. Falls back to using `curl` in some cases.
- [oci_push](docs/push.md) Push an `oci_image` or `oci_image_index` to a remote registry.

### Testing

- We recommend [container_structure_test](https://github.com/GoogleContainerTools/container-structure-test#running-structure-tests-through-bazel) to run tests

### Signing

> [!WARNING]  
> Signing images is a developer preview, not part of public API yet.

- [cosign_sign](https://github.com/bazel-contrib/rules_oci/blob/main/cosign/private/sign.bzl): Sign an `oci_image` using `cosign` binary at a remote registry.
- [cosign_attest](https://github.com/bazel-contrib/rules_oci/blob/main/cosign/private/attest.bzl) Add an attachment to an `oci_image` at a remote registry using `cosign`.

### 3rd Party Extensions/Tools

- [ociupdate](https://github.com/theoremlp/ociupdate): CLI and basic module extension to help with automating updates to image digests for ECR-based images with immutable tags.
- [rules_buildx](https://github.com/aspect-build/rules_buildx): Build container images from Dockerfiles using Docker Buildx.
