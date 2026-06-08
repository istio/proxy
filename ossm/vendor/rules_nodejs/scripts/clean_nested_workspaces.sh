#!/usr/bin/env bash

set -eu -o pipefail
# -e: exits if a command fails
# -u: errors if an variable is referenced before being set
# -o pipefail: causes a pipeline to produce a failure return code if any command errors

echo_and_run() { echo "+ $@" ; "$@" ; }

readonly workspaceRoots=("e2e" "packages")
for workspaceRoot in ${workspaceRoots[@]} ; do
  (
    readonly workspaceFiles=($(find ./${workspaceRoot} -type f -name WORKSPACE -prune -maxdepth 3))
    for workspaceFile in ${workspaceFiles[@]} ; do
      (
        readonly workspaceDir=$(dirname ${workspaceFile})
        printf "\n\nCleaning ${workspaceDir}\n"
        cd ${workspaceDir}
        echo_and_run rm -rf `find . -type d -name node_modules -prune -maxdepth 1`
        echo_and_run bazel clean --expunge
      )
    done
  )
done
