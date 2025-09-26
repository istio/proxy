# Maistra: Envoy Proxy with OpenSSL

## Rationale about this repository

This repository is a fork of the [Istio Proxy project](https://github.com/istio/proxy), and it is modified to meet certain criteria so it can be used as the base for the OpenShift Service Mesh product by Red Hat.

Some of the changes from upstream:

- Use of the fork [maistra/envoy](https://github.com/maistra/envoy/) as the Envoy dependency, as well as other minor dependency changes;
- Some modifications to allow building on s390x and powerpc platforms and changes in the build system.
- Vendored dependencies: All dependencies are stored in the [maistra/vendor](./vendor) subdirectory. This allows an offline build.

---
**HINT**

See also this [README file](https://github.com/maistra/envoy/blob/maistra-2.2/maistra/README.md) in our [maistra/envoy](https://github.com/maistra/envoy/) repository, which contains relevant information about our Envoy fork.

---

## Versions

We base our versions based on the Istio project releases:

- Maistra 2.2 comes with Istio 1.12 which comes with Proxy 1.12. The [maistra/envoy](https://github.com/maistra/envoy/) used is also 2.2 which in turn is based on upstream Envoy 1.20.
- Maistra 2.3 comes with Istio 1.14 which comes with Proxy 1.14. The [maistra/envoy](https://github.com/maistra/envoy/) used is also 2.3 which in turn is based on upstream Envoy 1.22.
- And so on so forth.

## Build

We use a docker builder image to build and run the tests on this repository. This image contains all necessary tools to build and run the tests, like, bazel, clang, golang, etc. See <https://github.com/maistra/test-infra/tree/main/docker/> for more information on this builder image.

In order to run a full build & tests, you can use the script [pre-submit.sh](./ci/pre-submit.sh), like this:

```sh
$ cd /path/to/proxy # Directory where you cloned this repo

$ docker run --rm -it \
        -v $(pwd):/work \
        -u $(id -u):$(id -g) \
        --entrypoint bash \
        quay.io/maistra-dev/maistra-builder:2.2 # Make sure to use the appropriate tag, matching the branch you are working on

[user@3883abd15af2 work] ./maistra/ci/pre-submit.sh  # This runs inside the container shell
```

Inspect the [pre-submit.sh](./ci/pre-submit.sh) to see what it does and feel free to tweak it locally while doing your development builds. For instance, you can change or remove the bazel command lines that limit the number of concurrent jobs, if your machine can handle more that what's defined in that file.

An useful hint is to make use of a build cache for bazel. The [pre-submit.sh](./ci/pre-submit.sh) already supports it, but you need to perform 2 steps before invoking it:

- Create a directory in your machine that will store the cache artifacts, for example `/path/to/bazel-cache`.
- Set an ENV variable in the builder container pointing to that directory. Add this to the `docker run` command above: `-e BAZEL_DISK_CACHE=/path/to/bazel-cache`.

The [pre-submit.sh](./ci/pre-submit.sh) is the one that runs on our CI (more on this later), it builds Proxy and runs all the tests. This might take a while. Again, feel free to tweak it locally while doing your development builds. You can also totally ignore this script and run the bazel commands by hand, inside the container shell.

### Build Flags

Note that we changed a bunch of compilation flags, when comparing to upstream. Most (but not all) of these changes are on the [bazelrc](./bazelrc) file, which is included from the [main .bazelrc](../.bazelrc) file. Again, feel free to tweak these files during local development.

### Relationship with Envoy and Istio: Automation

Envoy is the main and biggest dependency of this project. Proxy can be seen as a wrapper around Envoy. That said, our final build is based on Proxy, not Envoy. Because we need the ability to run builds in an offline environment, we are vendoring all the dependencies - including Envoy. This means that, any change made in the Envoy repository needs to be copied into this repository as a vendored dependency update. This is actually true for every dependency, but it's worth mention Envoy because the vast majority of the Proxy dependencies come from Envoy dependencies.

As an example, let's say that one issue was fixed in Envoy. In order to that issue be fixed, a .cc file was modified there, but also one Envoy dependency had their version bumped. Once these changes are merged in the Envoy repository, they are propagated to this Proxy repository under the [maistra/vendor](./vendor/) directory. Although it is possible to perform this copy manually, with the help of [some](./scripts/update-envoy-sha.sh) [scripts](./scripts/update-deps.sh), it is usually done automatically by a post-submit job that runs on CI after every merge in the Envoy repository.

After a PR is merged in this Proxy repository, two other automated processes happen:

- One job builds Proxy and uploads the artifacts to the cloud, so that they could be downloaded by Istio integration tests
- Another job creates a PR in Istio, updating the SHA of the Proxy dependency there, so that all Istio integration tests run with this new build of Proxy.

Here's the workflow:

```text
PR In Envoy is merged → Post-submit job runs, updating the Envoy dependencies → PR is created
Or an ordinary, manual PR is created in Proxy

         ↓

Tests pass and PR is approved → PR is merged in Proxy

         ↓

Post submit jobs upload the new binary to the cloud and create a PR in Istio repository
```

---
**NOTE**

To learn more about the automation process and the jobs described above, take a look at the [test-infra](https://github.com/maistra/test-infra/) repository

---

## New versions

The next version is chosen based on the version the Istio project will use. For example, if Maistra 2.3 is going to ship Istio 1.14, then we are going to use Proxy 1.14. That will be our `maistra-2.3` branch.

After deciding what version we are going to work on next, we start the work of porting our changes on top of the new base version - a.k.a the "rebase process".

### New builder image

We need to create a new builder image, that is capable of building the chosen Proxy version. To do that, go to the [test-infra repository](https://github.com/maistra/test-infra/tree/main/docker/), and create a new `Dockerfile` file, naming it after the new branch name. For example, if we are working on the new `maistra-2.3` branch, the filename will be `maistra-builder_2.3.Dockerfile`. We usually copy the previous file (e.g. `2.2`) and rename it. Then we look at upstream and figure out what changes should be necessary to make, for example, bumping the bazel or compiler versions, etc.

Play with this new image locally, making adjustments to the Dockerfile; build the image; run the Proxy build and tests using it; repeat until everything works. Then create a pull request on [test-infra repository](https://github.com/maistra/test-infra/) with the new Dockerfile.

---
**NOTE**

We should go through this process early in the new branch development, compiling upstream code and running upstream tests. We should not wait until the rebase process is completed to start working on the builder image. This is to make sure that our image is able to build upstream without errors. By doing this we guarantee that any error that shows up after the rebase process is done, it is our [rebase] fault.

---

### Creating the new branch and CI (Prow) jobs

We need to create jobs for the new branch in our CI (prow) system. We can split this into two tasks:

1. Create the new branch on GitHub. In our example, we are going to create the `maistra-2.3` branch that initially will be a copy of the Proxy `release-1.14` branch.
2. Go to [test-infra repository](https://github.com/maistra/test-infra/tree/main/prow/), create new [pre and postsubmit] jobs for the `maistra-2.3` branch. Open a pull request with your changes.

After the test-infra PR above is merged, you can create a fake, trivial pull request (e.g., updating a README file) in the Proxy project, targeting the new branch. The CI job should be triggered and it must pass. If it does, close the PR. If not, figure out what's wrong (e.g., it might be the builder image is missing a build dependency), fix it (this might involve submitting a PR to the test-infra repository and wait for the new image to be pushed to quay.io after the PR is merged) and comment `/retest` in this fake PR to trigger the CI again.

### Preparing a major release

The rebase process is the process where we pick up a base branch (e.g., Proxy `release-1.14`) and apply our changes on top of it. This is a non-trivial process as there are lots of conflicts mainly due to the way BoringSSL is integrated within the Envoy and Proxy code base. This process may take several weeks. The end result should be:

- The new maistra branch (e.g., `maistra-2.3`) is created and contains the code from the desired upstream branch (e.g. Proxy `release-1.14`) plus our changes
- Code should build and unit and integration tests must pass on our CI platform (Prow) using our builder image

It's acceptable to disable some tests, or tweak some compiler flags (e.g., disabling some `-Werror=`) in this stage, in order to keep things moving on. We should document everything that we did in order to get the build done. For instance, by adding `FIXME's` in the code and linking them to an issue in our [Jira tracker](https://issues.redhat.com/browse/OSSM).

Once the rebase is done locally, it's time to open a pull request and see how it behaves in CI. At this point we are already sure that the CI is able to run upstream code and tests successfully (see the previous topics). This means that any error caught by CI should be a legit error, caused by the changes in the rebase itself (this PR). Figure out what's wrong and keep pushing changes until CI is happy.

At the time this document is being written, an effort to automate this rebase process is being worked on. Once it finishes, this document must be updated to reflect the new process.
