#!/bin/bash
docker run -v $PWD:/work -w /work -v $(realpath $PWD/../../extensions):/work/extensions wasmsdk:v1 bash /build_wasm.sh
rmdir extensions
