#!/bin/bash

set -x
set -euo pipefail

# Change to the top dir
cd "$(dirname "$0")/.."

# Create a scratch directory
SCRATCH_DIR="$(mktemp -d)"
trap 'rm -rf -- "$SCRATCH_DIR"' EXIT

# Create our extended builder image, based on upstream's builder image.
docker build --pull --iidfile "${SCRATCH_DIR}/iid" -f - "${SCRATCH_DIR}" << EOF
    FROM $(./ci/run_envoy_docker.sh 'echo $ENVOY_BUILD_IMAGE')

    # Install the missing Kitware public key
    RUN wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg
    RUN sed -i "s|^deb.*kitware.*$|deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ \$(lsb_release -cs) main|g" /etc/apt/sources.list
    RUN apt update

    # Install OpenSSL 3.0.x
    ENV OPENSSL_VERSION=3.0.8
    ENV OPENSSL_ROOTDIR=/usr/local/openssl-\$OPENSSL_VERSION
    RUN apt install -y build-essential checkinstall zlib1g-dev
    RUN wget -qO- https://github.com/openssl/openssl/releases/download/openssl-\$OPENSSL_VERSION/openssl-\$OPENSSL_VERSION.tar.gz | tar xz -C /
    RUN cd /openssl-\$OPENSSL_VERSION && ./config -d --prefix=\$OPENSSL_ROOTDIR --openssldir=\$OPENSSL_ROOTDIR
    RUN make -C /openssl-\$OPENSSL_VERSION -j && make -C /openssl-\$OPENSSL_VERSION install_sw
    RUN echo "\$OPENSSL_ROOTDIR/lib64" > /etc/ld.so.conf.d/openssl-\$OPENSSL_VERSION.conf
    RUN ldconfig
EOF


# Build with libstdc++ rather than libc++ because the bssl-compat prefixer tool
# is linked against some of the LLVM libraries which require libstdc++
export ENVOY_STDLIB=libstdc++

# Tell the upstream run_envoy_docker.sh script to us our builder image
export IMAGE_NAME=$(cat "${SCRATCH_DIR}/iid" | cut -d ":" -f 1)
export IMAGE_ID=$(cat "${SCRATCH_DIR}/iid" | cut -d ":" -f 2)

# Hand off to the upstream run_envoy_docker.sh script
exec ./ci/run_envoy_docker.sh "$@"