# Dockerfile + rules_oci

Here's some good reasons why you should not consider Dockerfiles

- It depends on a functioning container runtime which is not always available, for example on RBE.
- Dockerfiles are fundamentally non-reproducible, you could hack around this fact, but you will always be fighting it.
- Reproducible builds are important for Bazel, Dockerfiles might become the reason for slower CI.
- `RUN` instruction is a perfect foot-gun for non-reprocubile builds, a simple command `RUN apt-get install curl` is non-hermetic by default.
- Building the same Dockerfile one month apart may yield different results for simple cases such as `FROM python:3.11.9-bullseye`.

And some good reasons why you should

- Easy to write, well understood by engineers as opposed to BUILD files.
- Plenty of examples out there that makes it easy to find good patterns.

# Resources

- https://reproducible-builds.org/
- https://github.com/bazel-contrib/rules_oci/issues/35#issuecomment-1285954483
- https://github.com/bazel-contrib/rules_oci/blob/main/docs/compare_dockerfile.md
- https://github.com/moby/moby/issues/43124
- https://github.com/moby/buildkit/blob/master/docs/build-repro.md
- https://medium.com/nttlabs/bit-for-bit-reproducible-builds-with-dockerfile-7cc2b9faed9f
