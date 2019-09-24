#!/bin/bash
docker run -e uid="$(id -u)" -e gid="$(id -g)" -v $PWD:/work -w /work -v $(realpath $PWD/../../extensions):/work/extensions wasmsdk:v1 bash /build_wasm.sh
rmdir extensions
