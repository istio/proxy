# Development

## Testing

GitHub Actions can be executed locally using the [`act`] tool.

All tests can be executed using:

    act

Individual tests can be executed using `-j` and `--matrix` parameters, e.g.:

    act -j bazel
    act -j stable
    act -j nightly
    act -j examples --matrix example:http_auth_random

By default, all jobs are cached in `~/.cache/actcache`. This can be disabled
using the `--no-cache-server` parameter.

## Updating Bazel dependencies

When adding or updating Cargo dependencies, the existing Bazel `BUILD` files
must be regenerated using the [`bazelisk`] tool:

```sh
bazelisk run //bazel/cargo:crates_vendor -- --repin all
```


[`act`]: https://github.com/nektos/act
[`bazelisk`]: https://github.com/bazelbuild/bazelisk
