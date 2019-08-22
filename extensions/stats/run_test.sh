WD=$(dirname $0)
WD=$(cd $WD; pwd)

BAZEL_BIN="${WD}/../../bazel-bin"

set -ex

${BAZEL_BIN}/src/envoy/envoy -c ${WD}/testdata/client.yaml --concurrency 2 --allow-unknown-fields
${BAZEL_BIN}/src/envoy/envoy -c ${WD}/testdata/server.yaml --concurrency 2 --allow-unknown-fields
