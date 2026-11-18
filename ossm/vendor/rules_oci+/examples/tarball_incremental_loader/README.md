# oci_tarball with an incremental loader

Currently rules_oci doesn't have incremental loader for docker tarball for reasons specified in https://github.com/bazel-contrib/rules_oci/issues/454#issuecomment-1875920593

This an example demonstrating a hack different than the rules_docker and https://github.com/google/go-containerregistry/pull/559

- Spawns a local registry on Docker
- Uses crane to push the tarball efficiently to the local registry. (loses the repo_tags)
- Pulls the image from the local registry to Docker daemon.

Demo:

```
INFO: Analyzed target //examples/incremental_load:tarball (0 packages loaded, 0 targets configured).
INFO: Found 1 target...
Target //examples/incremental_load:tarball up-to-date:
  bazel-bin/examples/incremental_load/tarball/tarball.tar
INFO: Elapsed time: 0.079s, Critical Path: 0.00s
INFO: 1 process: 1 internal.
INFO: Build completed successfully, 1 total action
INFO: Running command line: bazel-bin/examples/incremental_load/tarball.sh
2024/01/03 21:25:04 existing manifest: latest@sha256:065294fa481b167724c861a328a60dc198bbd7b7ceba3a4cd7e2f95eb5c11393
localhost:6000/image@sha256:065294fa481b167724c861a328a60dc198bbd7b7ceba3a4cd7e2f95eb5c11393: Pulling from image
Digest: sha256:065294fa481b167724c861a328a60dc198bbd7b7ceba3a4cd7e2f95eb5c11393
Status: Image is up to date for localhost:6000/image@sha256:065294fa481b167724c861a328a60dc198bbd7b7ceba3a4cd7e2f95eb5c11393
localhost:6000/image@sha256:065294fa481b167724c861a328a60dc198bbd7b7ceba3a4cd7e2f95eb5c11393

bazel run examples/incremental_load:tarball  0.17s user 0.12s system 34% cpu 0.821 total
```

> Disclaimer: This hasn't been tested with Remote Execution.
