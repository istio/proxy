"""External fetches for OCI base images.

This file is similar to how bazel_gazelle can manage go_repository calls
by writing them to a generated macro in a .bzl file.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@rules_oci//oci:pull.bzl", "oci_pull")
load("//examples:setup_assertion_repos.bzl", "configure_docker")

def fetch_images():
    "fetch external images"

    # A single-arch base image
    oci_pull(
        name = "distroless_java",
        digest = "sha256:161a1d97d592b3f1919801578c3a47c8e932071168a96267698f4b669c24c76d",
        image = "gcr.io/distroless/java17",
    )

    # A multi-arch base image
    oci_pull(
        name = "distroless_static",
        digest = "sha256:c3c3d0230d487c0ad3a0d87ad03ee02ea2ff0b3dcce91ca06a1019e07de05f12",
        image = "gcr.io/distroless/static",
        platforms = [
            "linux/amd64",
            "linux/arm",
            "linux/arm64",
            "linux/ppc64le",
            "linux/s390x",
        ],
    )

    # Pull an image from public ECR.
    # When --credential_helper is provided, see .bazelrc at workspace root, it will take precende over
    # auth from oci_pull. However, pulling from public ECR works out of the box so this will never fail
    # unless oci_pull's authentication mechanism breaks and --credential_helper is absent.
    oci_pull(
        name = "ecr_lambda_python",
        image = "public.ecr.aws/lambda/python",
        tag = "3.11.2024.01.25.10",
        platforms = [
            "linux/amd64",
            "linux/arm64/v8",
        ],
    )

    # Show that the digest is optional.
    # In this case, the dependency is "floating" and our build could break when a new
    # image is pushed to gcr.io with the 'debug' tag, so we document this by setting
    # reproducible = False.
    # This is more convenient, so you might decide the trade-off is worth it.
    oci_pull(
        name = "distroless_python",
        image = "gcr.io/distroless/python3",
        platforms = ["linux/amd64"],
        # Don't make a distroless_python_unpinned repo and print a warning about the tag
        reproducible = False,
        tag = "debug",
    )

    # You can copy-paste a typical image string from dockerhub search results.
    oci_pull(
        name = "debian_latest",
        image = "debian:latest",
        reproducible = False,
        platforms = ["linux/amd64", "linux/arm64/v8"],
    )

    # You can use a digest on the image name
    # https://hub.docker.com/layers/library/debian/stable/images/sha256-e822570981e13a6ef1efcf31870726fbd62e72d9abfdcf405a9d8f566e8d7028?context=explore
    oci_pull(
        name = "debian_stable",
        image = "debian@sha256:e822570981e13a6ef1efcf31870726fbd62e72d9abfdcf405a9d8f566e8d7028",
    )

    # Show the simple case of migrating from rules_docker, like
    # container_pull(
    #     name = "base",
    #     registry = "gcr.io",
    #     repository = "my-project/my-base",
    #     # 'tag' is also supported, but digest is encouraged for reproducibility.
    #     digest = "sha256:deadbeef",
    # )
    oci_pull(
        name = "from_rules_docker",
        registry = "gcr.io",
        repository = "distroless/nodejs18",
        digest = "sha256:1fd03807e02eeb78efaacb0e38e8e68ead0639733e92e7cc9a9e017cd9b50bbf",
        platforms = ["linux/amd64", "linux/arm64"],
    )

    oci_pull(
        name = "aws_lambda_python",
        # tag = "3.8"
        digest = "sha256:46b3b8614b31761b24f56be1bb8c7ba191d9b9b4624bbf7f53ed7ddc696c928b",
        image = "public.ecr.aws/lambda/python",
    )

    oci_pull(
        name = "debian",
        # Omits the "image." CNAME for dockerhub
        image = "docker.io/library/debian",
        platforms = [
            "linux/amd64",
            "linux/arm/v5",
            "linux/arm/v7",
            "linux/arm64/v8",
            "linux/386",
            "linux/mips64le",
            "linux/ppc64le",
            "linux/s390x",
        ],
        # It will print a warning on every build about the "latest" tag being non-reproducible.
        # Un-comment the following line to suppress the warning:
        # reproducible = False,
        tag = "latest",
    )

    oci_pull(
        name = "ubuntu",
        image = "ubuntu",
        platforms = [
            "linux/arm64/v8",
            "linux/amd64",
        ],
        digest = "sha256:67211c14fa74f070d27cc59d69a7fa9aeff8e28ea118ef3babc295a0428a6d21",
    )

    oci_pull(
        name = "apollo_router",
        # tag = "v1.14.0",
        digest = "sha256:237c4d6a477b5013bae88549bfc50aaafd68974cab7d2dde2ba5431345e9c95d",
        image = "ghcr.io/apollographql/router",
        platforms = [
            "linux/amd64",
            "linux/arm64",
        ],
    )

    oci_pull(
        name = "fluxcd_flux",
        image = "docker.io/fluxcd/flux:1.25.4",
    )

    oci_pull(
        name = "chainguard_static",
        image = "cgr.dev/chainguard/static",
        platforms = [
            "linux/amd64",
            "linux/arm",
            "linux/arm64",
            "linux/ppc64le",
            "linux/riscv64",
            "linux/s390x",
        ],
        tag = "latest",
        reproducible = False,
    )

    oci_pull(
        name = "gitlab_assets_ce",
        # tag = "v15-11-0-ee",
        image = "registry.gitlab.com/gitlab-org/gitlab/gitlab-assets-ce",
        digest = "sha256:78fc30603a49cdabba4cd1c3a94f71cb78e606a98b5b17c690c781c5c8955f29",
    )

    oci_pull(
        name = "es_kibana_image",
        # tag = "7.16.2",
        image = "docker.elastic.co/kibana/kibana",
        digest = "sha256:9a83bce5d337e7e19d789ee7f952d36d0d514c80987c3d76d90fd1afd2411a9a",
        platforms = [
            "linux/amd64",
            "linux/arm64",
        ],
    )

    oci_pull(
        name = "quay_clair_image",
        # tag = "4.7.2",
        image = "quay.io/projectquay/clair",
        digest = "sha256:8d38ffa8fad72f4bc2647644284c16491cc2d375602519a1f963f96ccc916276",
        platforms = [
            "linux/amd64",
            "linux/arm64",
        ],
    )

    oci_pull(
        name = "nvidia_k8s_device_plugin_image",
        # tag = "v0.14.4",
        digest = "sha256:19c696958fe8a63676ba26fa57114c33149168bbedfea246fc55e0e665c03098",
        image = "nvcr.io/nvidia/k8s-device-plugin",
    )

    # Helm chart lacking `mediaType` in manifest.
    # https://github.com/bazel-contrib/rules_oci/issues/839
    oci_pull(
        name = "envoyproxy_gateway_helm",
        digest = "sha256:905eced000d4b2acb78f802f5d03af32a08d30478808c20d522ffa735476bc5d",
        image = "docker.io/envoyproxy/gateway-helm",
    )

    _DEB_TO_LAYER = """\
genrule(
    name = "layer",
    srcs = [":data.tar.xz"],
    outs = ["data.tar.zst"],
    cmd = "$(BSDTAR_BIN) --zstd -cf $@ @$<",
    toolchains = ["@bsd_tar_toolchains//:resolved_toolchain"],
    visibility = ["//visibility:public"],
)
"""

    http_archive(
        name = "bash_amd64",
        build_file_content = _DEB_TO_LAYER,
        urls = [
            "http://ftp.us.debian.org/debian/pool/main/b/bash/bash_5.1-2+deb11u1_amd64.deb",
        ],
        sha256 = "f702ef058e762d7208a9c83f6f6bbf02645533bfd615c54e8cdcce842cd57377",
    )

    http_archive(
        name = "bash_arm64",
        build_file_content = _DEB_TO_LAYER,
        urls = [
            "http://ftp.us.debian.org/debian/pool/main/b/bash/bash_5.1-2+deb11u1_arm64.deb",
        ],
        sha256 = "d7c7af5d86f43a885069408a89788f67f248e8124c682bb73936f33874e0611b",
    )

# buildifier: disable=unnamed-macro
def fetch_test_repos():
    "fetch repos needed for testing"

    # For sign_external test
    native.new_local_repository(
        name = "empty_image",
        build_file = "//examples/assertion/sign_external:BUILD.template",
        path = "examples/assertion/sign_external/workspace",
    )

    # For attest_external test
    native.new_local_repository(
        name = "example_sbom",
        build_file = "//examples/assertion/attest_external:BUILD.template",
        path = "examples/assertion/attest_external/workspace",
    )

    # Setup steps for docker testing
    configure_docker(name = "docker_configure")
