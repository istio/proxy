#!/bin/bash
#
# This script spins up the OCPDiag docker container with the contents of the
# current folder, executes bazel with the specified commands, and then moves
# all generated files out of the container. This will work like running bazel
# normally, with all of the docker operations handled by this file.
#
# Example usages:
#   Build everything in the current dir - ./bazel-docker.sh
#   Test everything in the current dir - ./bazel-docker.sh test ...


# If no args are specified, build everything in the current folder
if [ $# == 0 ]; then
  args="build ..."
else
  args=$@
fi

dir_name=${PWD##*/}
dir_name=${dir_name:-/}
internal_dir="/workspace/git/${dir_name}"

# Update the latest image before running
docker pull "gcr.io/ocpdiag-kokoro/ocpdiag-build:latest"

# Take ownership of the bazel cache, execute bazel, and then release ownership
docker run --rm -it \
  --volume ~/.gitcookies:/root/.gitcookies \
  --volume ~/.cache/bazel:/root/.cache/bazel \
  --volume "${PWD}:${internal_dir}" \
  --workdir ${internal_dir} \
  --env USER \
  "gcr.io/ocpdiag-kokoro/ocpdiag-build:latest" \
  /bin/bash \
  -c "chown -R root:root /root/.cache/bazel; bazel ${args}; chown -R $(id -u):$(id -g) /root/.cache/bazel"

# Function that updates broken symlinks to point to paths outside of the docker
# container
update_symlink() {
    path_end=$(readlink $1 | cut -d '/' -f  3-)
    new_path="${HOME}/${path_end}"
    rm $1
    ln -s $new_path $1
}

# Fix the symlinks for all generated folders
ls | grep "bazel-" | while read -r line ; do
    update_symlink "$line"
done
