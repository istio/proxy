:::{default-domain} bzl
:::

# Read the Docs integration

The {obj}`readthedocs_install` rule provides support for making it easy
to build for, and deploy to, Read the Docs. It does this by having Bazel do
all the work of building, and then the outputs are copied to where Read the Docs
expects served content to be placed. By having Bazel do the majority of work,
you have more certainty that the docs you generate locally will match what
is created in the Read the Docs build environment.

Setting this up is conceptually simple: make the Read the Docs build call `bazel
run` with the appropriate args. To do this, it requires gluing a couple things
together, most of which can be copy/pasted from the examples below.

## `.readthedocs.yaml` config

In order for Read the Docs to call our custom commands, we have to use the
advanced `build.commands` setting of the config file. This needs to do two key
things:
1. Install Bazel
2. Call `bazel run` with the appropriate args.

In the example below, `npm` is used to install Bazelisk and a helper shell
script, `readthedocs_build.sh` is used to construct the Bazel invocation.

The key purpose of the shell script it to set the
`--@rules_python//sphinxdocs:extra_env` and
`--@rules_python//sphinxdocs:extra_defines` flags. These are used to communicate
`READTHEDOCS*` environment variables and settings to the Bazel invocation.

## BUILD config

In your build file, the {obj}`readthedocs_install` rule handles building the
docs and copying the output to the Read the Docs output directory
(`$READTHEDOCS_OUTPUT` environment variable). As input, it takes a `sphinx_docs`
target (the generated docs).

## conf.py config

Normally, readthedocs will inject extra content into your `conf.py` file
to make certain integration available (e.g. the version selection flyout).
However, because our yaml config uses the advanced `build.commands` feature,
those config injections are disabled and we have to manually re-enable them.

To do this, we modify `conf.py` to detect `READTHEDOCS=True` in the environment
and perform some additional logic. See the example code below for the
modifications.

Depending on your theme, you may have to tweak the conf.py; the example is
based on using the sphinx_rtd_theme.

## Example

```
# File: .readthedocs.yaml
version: 2

build:
  os: "ubuntu-22.04"
  tools:
    nodejs: "19"
  commands:
    - env
    - npm install -g @bazel/bazelisk
    - bazel version
    # Put the actual action behind a shell script because it's
    # easier to modify than the yaml config.
    - docs/readthedocs_build.sh
```

```
# File: docs/BUILD

load("@rules_python//sphinxdocs:readthedocs.bzl.bzl", "readthedocs_install")
readthedocs_install(
    name = "readthedocs_install",
    docs = [":docs"],
)
```

```
# File: docs/readthedocs_build.sh

#!/bin/bash

set -eou pipefail

declare -a extra_env
while IFS='=' read -r -d '' name value; do
  if [[ "$name" == READTHEDOCS* ]]; then
    extra_env+=("--@rules_python//sphinxdocs:extra_env=$name=$value")
  fi
done < <(env -0)

# In order to get the build number, we extract it from the host name
extra_env+=("--@rules_python//sphinxdocs:extra_env=HOSTNAME=$HOSTNAME")

set -x
bazel run \
  --stamp \
  "--@rules_python//sphinxdocs:extra_defines=version=$READTHEDOCS_VERSION" \
  "${extra_env[@]}" \
  //docs:readthedocs_install
```

```
# File: docs/conf.py

# Adapted from the template code:
# https://github.com/readthedocs/readthedocs.org/blob/main/readthedocs/doc_builder/templates/doc_builder/conf.py.tmpl
if os.environ.get("READTHEDOCS") == "True":
    # Must come first because it can interfere with other extensions, according
    # to the original conf.py template comments
    extensions.insert(0, "readthedocs_ext.readthedocs")

    if os.environ.get("READTHEDOCS_VERSION_TYPE") == "external":
        # Insert after the main extension
        extensions.insert(1, "readthedocs_ext.external_version_warning")
        readthedocs_vcs_url = (
            "http://github.com/bazel-contrib/rules_python/pull/{}".format(
                os.environ.get("READTHEDOCS_VERSION", "")
            )
        )
        # The build id isn't directly available, but it appears to be encoded
        # into the host name, so we can parse it from that. The format appears
        # to be `build-X-project-Y-Z`, where:
        # * X is an integer build id
        # * Y is an integer project id
        # * Z is the project name
        _build_id = os.environ.get("HOSTNAME", "build-0-project-0-rules-python")
        _build_id = _build_id.split("-")[1]
        readthedocs_build_url = (
            f"https://readthedocs.org/projects/rules-python/builds/{_build_id}"
        )

html_context = {
    # This controls whether the flyout menu is shown. It is always false
    # because:
    # * For local builds, the flyout menu is empty and doesn't show in the
    #   same place as for RTD builds. No point in showing it locally.
    # * For RTD builds, the flyout menu is always automatically injected,
    #   so having it be True makes the flyout show up twice.
    "READTHEDOCS": False,
    "github_version": os.environ.get("READTHEDOCS_GIT_IDENTIFIER", ""),
    # For local builds, the github link won't work. Disabling it replaces
    # it with a "view source" link to view the source Sphinx saw, which
    # is useful for local development.
    "display_github": os.environ.get("READTHEDOCS") == "True",
    "commit": os.environ.get("READTHEDOCS_GIT_COMMIT_HASH", "unknown commit"),
    # Used by readthedocs_ext.external_version_warning extension
    # This is the PR number being built
    "current_version": os.environ.get("READTHEDOCS_VERSION", ""),
}
```
