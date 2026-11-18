## Setup test repositories
load(":fetch.bzl", "fetch_images", "fetch_test_repos")

fetch_images()

fetch_test_repos()

### Setup rules_oci cosign
load("//cosign:repositories.bzl", "cosign_register_toolchains")

cosign_register_toolchains(name = "oci_cosign")
