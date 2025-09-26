# Stamping

Bazel is generally only a build tool, and is unaware of your version control system.
However, when publishing releases, you may want to embed version information in the resulting distribution.
Bazel supports this with the concept of a "Workspace status" which is evaluated before each build.
See [the Bazel workspace status docs](https://docs.bazel.build/versions/master/user-manual.html#workspace_status)

To stamp a build, you must pass the `--stamp` argument to Bazel.

Stamping is typically performed on a later action in the graph, like on a packaging rule (`pkg_*`). This means that
a changed status variable only causes re-packaging, not re-compilation and thus does not cause cascading re-builds.

Bazel provides a couple of statuses by default, such as `BUILD_EMBED_LABEL` which is the value of the `--embed_label`
argument, as well as `BUILD_HOST` and `BUILD_USER`. You can supply more with the workspace status script, see below.

Some rules accept an attribute that uses the status variables.

## Stamping with a Workspace status script

To define additional statuses, pass the `--workspace_status_command` argument to `bazel`.
The value of this flag is a path to a script that prints space-separated key/value pairs, one per line, such as

```bash
#!/usr/bin/env bash
echo STABLE_GIT_COMMIT $(git rev-parse HEAD)
```

Make sure you set the executable bit, eg. `chmod 755 tools/bazel_stamp_vars.sh`.

> **NOTE** keys that start with `STABLE_` will cause a re-build when they change.
> Other keys will NOT cause a re-build, so stale values can appear in your app.
> Non-stable (volatile) keys should typically be things like timestamps that always vary between builds.

You might like to encode your setup using an entry in `.bazelrc` such as:

```sh
# This tells Bazel how to interact with the version control system
# Enable this with --config=release
build:release --stamp --workspace_status_command=./tools/bazel_stamp_vars.sh
```

## Release script

If you publish more than one package from your workspace, you might want a release script around Bazel.
A nice pattern is to do a `bazel query` to find publishable targets, build them in parallel, then publish in a loop.
Here is a template to get you started:

```sh
#!/usr/bin/env bash

set -u -e -o pipefail

# Call the script with argument "pack" or "publish"
readonly PKG_COMMAND=${1:-publish}
# Don't rely on $PATH to have the right version
readonly BAZEL=./node_modules/.bin/bazel
# Find all the npm packages in the repo
readonly PKG_LABELS=`$BAZEL query --output=label 'kind("pkg_tar", //...)'`
# Build them in one command to maximize parallelism
$BAZEL build --config=release $PKG_LABELS
# publish one package at a time to make it easier to spot any errors or warnings
for pkg in $PKG_LABELS ; do
  $BAZEL run --config=release -- ${pkg}.${PKG_COMMAND} --access public --tag latest
done
```

See https://www.kchodorow.com/blog/2017/03/27/stamping-your-builds/ for more background.
Make sure you use a "STABLE_" status key, or else Bazel may use a cached npm artifact rather than
building a new one with your current version info.