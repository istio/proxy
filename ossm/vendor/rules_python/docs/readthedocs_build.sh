#!/usr/bin/env bash

set -eou pipefail

declare -a extra_env
while IFS='=' read -r -d '' name value; do
  if [[ "$name" == READTHEDOCS* ]]; then
    extra_env+=("--//sphinxdocs:extra_env=$name=$value")
  fi
done < <(env -0)

# In order to get the build number, we extract it from the host name
extra_env+=("--//sphinxdocs:extra_env=HOSTNAME=$HOSTNAME")

export RULES_PYTHON_ENABLE_PIPSTAR=1

set -x
export RULES_PYTHON_ENABLE_PIPSTAR=1
bazel run \
  --config=rtd \
  "--//sphinxdocs:extra_defines=version=$READTHEDOCS_VERSION" \
  "${extra_env[@]}" \
  //docs:readthedocs_install
